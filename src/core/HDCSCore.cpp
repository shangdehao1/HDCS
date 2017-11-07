// Copyright [2017] <Intel>

#include "core/HDCSCore.h"

#if defined(CACHE_POLICY)
#include "core/policy/CachePolicy.h"
#endif
#if defined(TIER_POLICY)
#include "core/policy/TierPolicy.h"
#endif
#include "store/SimpleStore/SimpleBlockStore.h"
#include "store/RBD/RBDImageStore.h"

#include <string>
#include "common/HDCS_REQUEST_HANDLER.h"

namespace hdcs {
namespace core {
HDCSCore::HDCSCore(std::string name, std::string config_name) {
  config = new Config(name, config_name); 

  std::string log_path = config->configValues["log_to_file"];
  std::cout << "log_path: " << log_path << std::endl;
  if( log_path!="false" ){
    int stderr_no = dup(fileno(stderr));
    log_fd = fopen( log_path.c_str(), "w" );
	  if(log_fd==NULL){}
    if(-1==dup2(fileno(log_fd), STDERR_FILENO)){}
  }

  int hdcs_thread_max = stoi(config->configValues["op_threads_num"]);
  hdcs_op_threads = new TWorkQueue( hdcs_thread_max );
  uint64_t total_size = stoull(config->configValues["total_size"]);
  uint64_t block_size = stoull(config->configValues["cache_min_alloc_size"]);
  bool cache_policy_mode = config->configValues["policy_mode"].compare(std::string("cache")) == 0 ? true : false;

  std::string path = config->configValues["cache_dir_run"];
  std::string pool_name = config->configValues["rbd_pool_name"];
  std::string volume_name = config->configValues["rbd_volume_name"];

  block_guard = new BlockGuard(total_size, block_size);
  BlockMap* block_ptr_map = block_guard->get_block_map();

  if (cache_policy_mode) {
#if defined(CACHE_POLICY)
    uint64_t cache_size = stoull(config->configValues["cache_total_size"]);
    float cache_ratio_health = stof(config->configValues["cache_ratio_health"]);
    uint64_t timeout_nanosecond = stoull(config->configValues["cache_dirty_timeout_nanoseconds"]);
    CACHE_MODE_TYPE cache_mode = config->configValues["cache_mode"].compare(std::string("readonly")) == 0 ? CACHE_MODE_READ_ONLY : CACHE_MODE_WRITE_BACK;
    policy = new CachePolicy(total_size, cache_size, block_size, block_ptr_map,
                      new store::SimpleBlockStore(path, total_size, cache_size, block_size),
                      new store::RBDImageStore(pool_name, volume_name, block_size),
                      cache_ratio_health, &request_queue,
                      timeout_nanosecond, cache_mode, hdcs_thread_max);
#endif
  } else {
#if defined(TIER_POLICY)
    policy = new TierPolicy(total_size, block_size, block_ptr_map,
                    new store::SimpleBlockStore(path, total_size, total_size, block_size),
                    new store::RBDImageStore(pool_name, volume_name, block_size),
                    &request_queue, hdcs_thread_max);
#endif
  }

  go = true;
  main_thread = new std::thread(std::bind(&HDCSCore::process, this));

  //connect to its replication_nodes
  if (config->configValues["role"].compare(std::string("hdcs_master")) == 0) {
    //connect_to_replica(name);
  }
}

HDCSCore::~HDCSCore() {
  go = false;
  main_thread->join();
  delete hdcs_op_threads;
  delete policy;
  delete block_guard;
  delete main_thread;
}

void HDCSCore::close() {
#if defined(CACHE_POLICY)
  policy->flush_all();
#endif
}

void HDCSCore::promote_all() {
#if defined(TIER_POLICY)
  ((TierPolicy*)policy)->promote_all();
#endif
}

void HDCSCore::flush_all() {
#if defined(TIER_POLICY)
  ((TierPolicy*)policy)->flush_all();
#endif
}

void HDCSCore::queue_io(Request *req) {
  //request_queue.enqueue((void*)req);
  process_request(req);
}

void HDCSCore::process() {
  while(go){
    Request *req = (Request*)request_queue.dequeue();
    if (req != nullptr) {
      process_request(req);
    }
  }
}

void HDCSCore::process_request(Request *req) {
  if (req->io_type == IO_TYPE_WRITE) {
    int replica_size = 1 + replication_core_map.size();
    void* req_comp = req->comp;
    AioCompletion *comp = new AioCompletionImp([this, req_comp](ssize_t r){
      ((AioCompletion*)req_comp)->complete(r); 
    }, replica_size); 

    hdcs_ioctx_t* io_ctx;
    for (auto& replica_node : replication_core_map) {
      io_ctx = (hdcs_ioctx_t*)replica_node.second;
      hdcs::HDCS_REQUEST_CTX msg_content(HDCS_WRITE, io_ctx->hdcs_inst,
                                         comp, req->offset, req->length, req->data);
      io_ctx->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
    }
    req->comp = comp;
  }
  // Request -> list<BlockRequest> -> list<BlockOp>
  //std::mutex block_request_list_lock;
  //BlockRequestList block_request_list;
  std::lock_guard<std::mutex> lock(block_request_list_lock);
  block_guard->create_block_requests(req, &block_request_list);

  for (BlockRequestList::iterator it = block_request_list.begin(); it != block_request_list.end();) { 
    log_print("block %lu: %lu-%lu", it->block->block_id, it->offset, it->size); 
    if (!it->block->in_discard_process) {
      map_block(std::move(*it));
      block_request_list.erase(it++);
    } else {
      it++;
    }
  }
}

void HDCSCore::map_block(BlockRequest &&block_request) {
  BlockOp *block_ops_end;
  Block* block = block_request.block;
  bool do_process = false;
  block->block_mutex.lock();
  // If this block_request belongs to discard block,
  // append to wait list firstly.
  BlockOp *block_ops_head = policy->map(std::move(block_request), &block_ops_end);
  if (!block->in_process) {
    block->in_process = true;
    block->block_ops_end = block_ops_end;
    do_process = true;
    log_print("Block not in process, block: %lu, new end: %p", block->block_id, block_ops_end);
  } else {
    log_print("Block in process, append request, block: %lu, append BlockOp: %p after BlockOp: %p, new end: %p", block->block_id, block_ops_head, block->block_ops_end, block_ops_end);
    block->block_ops_end->block_op_next = block_ops_head;
    block->block_ops_end = block_ops_end;
  }
  block->block_mutex.unlock();
  if (do_process) {
    hdcs_op_threads->add_task(std::bind(&BlockOp::send, block_ops_head));
    //block_ops_head->send();
  }
}

void HDCSCore::aio_read (char* data, uint64_t offset, uint64_t length,  void* arg) {
  Request *req = new Request(IO_TYPE_READ, data, offset, length, arg);
  request_queue.enqueue((void*)req);
  //process_request(req);
}

void HDCSCore::aio_write (char* data, uint64_t offset, uint64_t length,  void* arg) {
  Request *req = new Request(IO_TYPE_WRITE, data, offset, length, arg);
  request_queue.enqueue((void*)req);
  //process_request(req);
}

void HDCSCore::connect_to_replica (std::string name) {
  std::cout<<"============================connect_to_replica============================="<<std::endl;
  std::string addr;
  std::string port;
  int colon_pos, last_pos;
  char c;

  hdcs_ioctx_t* io_ctx;
  std::istringstream iss(config->configValues["replication_nodes"]);
  std::vector<std::string> replication_nodes((std::istream_iterator<WordDelimitedBy<','>>(iss)),
                                       std::istream_iterator<WordDelimitedBy<','>>());
  int replication_nodes_num = replication_nodes.size();
  for (auto &addr_port_str : replication_nodes) {
    colon_pos = addr_port_str.find(':');
    addr = addr_port_str.substr(0, colon_pos);
    c = addr_port_str.at(addr_port_str.length() - 1);

    if (c == ',') last_pos = 2;
    else last_pos = 1;
    port = addr_port_str.substr(colon_pos + 1, addr_port_str.length() - colon_pos - last_pos);
    std::cout << "Connect to replication_node: " << addr << " : " << port << std::endl;

    io_ctx = (hdcs_ioctx_t*)malloc(sizeof(hdcs_ioctx_t));
    replication_core_map[addr_port_str] = (void*)io_ctx;
    /* param 2: io_service number in io_pool.
       param 3: thread number at one io_service.
    */
    io_ctx->conn = new hdcs::networking::Connection(([](void* p, std::string s){client::request_handler(p, s);}), 5, 5);
    io_ctx->conn->connect(addr, port);
    io_ctx->conn->set_session_arg((void*)io_ctx);

    hdcs::HDCS_REQUEST_CTX msg_content(HDCS_CONNECT, nullptr, nullptr, 0, name.length(), const_cast<char*>(name.c_str()));
    io_ctx->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  }
}

}// core
}// hdcs
