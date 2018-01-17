#ifndef RDMA_CLIENT_MESSENGER
#define RDMA_CLIENR_MESSENGER

#include <mutex>
#include <chrono>
#include <memory>
#include <iostream>
#include <boost/system/error_code.hpp>

#include "rdma_messenger/RDMAClient.h"

#include "rdma_session.h"
#include "rdma_internal_callback.h"
#include "../common/Message.h"
#include "../common/wait_event.h"
#include "../common/networking_common.h"
#include "../common/counter.h"


// wraper of rdma_messenger --sdh
//
namespace hdcs{
namespace networking{

class RDMAConnect{
public:
    RDMAConnect()
        : new_session(NULL)
    {

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0;   
    }

    ~RDMAConnect(){}

    SessionPtr connect(std::string ip_address, std::string port_num, ProcessMsg _process_msg){
        int ret;
        ret = to_rdma_address(ip_address, port_num);
        if(ret<0){
            std::cout<<"rdma resolve failed."<<std::endl;
            assert(0);
        }
        rdma_client_ptr.reset(new RDMAClient(t->ai_addr, 1));
        // connection callback
        internal_on_connection_ptr.reset(new InternalOnConnection(new_session, wait_event));
        // reading callback
        internal_process_msg_ptr.reset(new InternalProcessMsg(_process_msg));

        internal_on_connection_ptr->set_process_msg(internal_process_msg_ptr);
        // when connction action fails, how i know it. TODO
        rdma_client_ptr->connect(internal_on_connection_ptr.get());
        // 
        wait_event.Wait();
        // default connection success
        return new_session;
    }  

    void close(){
       // TODO close RDMAClient
    }

private:
    int to_rdma_address(std::string ip_address, std::string port_num ){
        int n = getaddrinfo(ip_address.c_str(), port_num.c_str(), &hints, &res);
        return n<0 ? -1:0;
    }

private:
    std::shared_ptr<RDMAClient> rdma_client_ptr; // connector
    std::shared_ptr<InternalOnConnection> internal_on_connection_ptr;
    std::shared_ptr<InternalProcessMsg> internal_process_msg_ptr;
    struct addrinfo* res;
    struct addrinfo* t;
    struct addrinfo hints;
    WaitEvent wait_event;
    SessionPtr new_session;
  
};

}
}

#endif
