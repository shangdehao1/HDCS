#ifndef RDMA_SESSION
#define RDMA_SESSION

#include "../common/Message.h"
#include "../common/session.h"

namespace hdcs{
namespace networking{

class rdma_session : public Session{
private:
    std::shared_ptr<RDMAConnection> rdma_connection;

public:
    rdma_session(RDMAConnection* conn){
        rdma_connection.reset(conn);
    } 

    ~rdma_session(){
    }

    // implement virtual class interface.
    void stop(){
        rdma_connection->fin();
    }

    void cancel(){
        // TODO
    }

    void set_session_arg(void* arg){
        // TODO TODO TODO
    }

    bool start( ProcessMsg _process_msg){
        return true;
    }

    int async_send(std::string send_buffer, uint64_t _seq_id){
        Message msg(send_buffer,_seq_id);
        std::string serialization_msg(std::move(msg.to_buffer()));

        rdma_connection->async_send(serialization_msg.c_str(), serialization_msg.size()+1);
        return 0;
    }

    ssize_t communicate(std::string send_buffer, uint64_t _seq_id){
        //rdma_connection->async_send();//?
        return 0;
    }

    void aio_communicate(std::string& send_buffer, uint64_t _seq_id){
        //rdma_connection->async_send();
    }

    void set_option(){
    }

    bool if_timeout(){
        return true;
    }

    // rdma interface
    int sync_connection(std::string ip_address, std::string port, ProcessMsgClient _process_msg){
        return 0;
        //
    }

    int async_connection(){
        return 0;
        //
    }


};//class rdma_session

}//networking
}//hdcs

























#endif
