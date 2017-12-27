#ifndef CLIENT 
#define CLIENT 
#include <mutex>
#include <string>
#include <thread>
#include <map>
#include <vector>
#include <atomic>
#include <memory>
#include <atomic>
#include "connect.h"
#include "common/counter.h"
//#include "../io_service/thread_group.h"

namespace hdcs{
namespace networking{
class Connection{   
private:

    std::vector<SessionPtr> session_vec;
    std::atomic<int> session_index;
    std::atomic<bool> is_closed;
    Connect m_connect;
    ProcessMsgClient process_msg;
    void* process_msg_arg;
    int session_num;
    std::mutex session_index_lock;
    AtomicCounter64 _next_sequence_id; // free lock counter
public:

    Connection( ProcessMsgClient _process_msg , int _s_num, int _thd_num)
        : session_index(0)
        , session_num(_s_num)
        , process_msg(_process_msg)
        , m_connect(_s_num, _thd_num)
        , _next_sequence_id(0)
        , is_closed(true)
    {}

    ~Connection(){
        if(!is_closed.load()){
            close();
        }
    }

    void close() {
        for(int i = 0 ; i < session_vec.size(); ++i){
            session_vec[i]->close();
            delete session_vec[i];
        }
        is_closed.store(true);
    }

    void cancel(){
        for(int i = 0; i < session_vec.size(); ++i){
            session_vec[i]->cancel();
        }
    }
    
    void set_session_arg(void* arg){
        process_msg_arg = arg; 
        for(int i = 0; i < session_vec.size(); ++i){
            session_vec[i]->set_session_arg(arg);
        }
    }

    int connect( std::string ip_address, std::string port){
        SessionPtr new_session;
        for(int i=0; i < session_num; i++){
            new_session = m_connect.sync_connect( ip_address, port, process_msg );
            if(new_session != NULL){
                session_vec.push_back( new_session );
            }else{
                std::cout<<"Client::sync_connect failed.."<<std::endl;
            }
        }
        if( session_vec.size() == session_num ){
            std::cout<<"Networking: "<<session_vec.size()<<" sessions have been created..."<<std::endl;
            is_closed.store(true);
        }else{
            assert(0);
        }
        return 0;
    }

    ssize_t communicate( std::string send_buffer){
        int temp_index;
        session_index_lock.lock();
        temp_index = session_index;
        if(++session_index==session_vec.size()){
            session_index = 0;
        }
        session_index_lock.unlock();
        ssize_t ret = session_vec[temp_index]->communicate(send_buffer, generate_sequence_id());
        return ret;
    }

    void aio_communicate(std::string&& send_buffer){
        int temp_index; 
        session_index_lock.lock();
        temp_index = session_index;
        if(++session_index==session_vec.size()){
            session_index = 0;
        }
        session_index_lock.unlock();
        session_vec[temp_index]->aio_communicate( send_buffer, generate_sequence_id() );
    }

private:

    uint64_t generate_sequence_id(){
        return ++_next_sequence_id;
    }

};
} //hdcs
}
#endif
