#ifndef SERVER_H
#define SERVER_H

#include <sstream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>

#include "common/session.h"
#include "common/networking_common.h"
#include "common/option.h"
#include "acceptor.h"

namespace hdcs{
namespace networking{

class server{
private:
    SessionSet session_set; 
    std::unique_ptr<Acceptor> acceptor_ptr;
    std::atomic<bool> is_stop;
    ServerOptions server_options;

public:

    /*
    server(std::string _ip_address, std::string _port_num, int s_num=10, int thd_num=10): 
        is_stop(false){
        acceptor_ptr.reset(new Acceptor( _ip_address, _port_num, session_set , s_num, thd_num));
        //std::cout<<"server construction success.."<<std::endl;
    }
    */

    // this construction: server just support TCP communication
    // old interface.
    server(std::string ip_address, std::string port_num, int s_num=10, int thd_num=10)
        : is_stop(false)
    {
        server_options._io_service_num = s_num;
        server_options._session_num = s_num;
        server_options._thd_num_on_one_session = thd_num;
        server_options._port_num_vec.push_back(port_num);
        server_options._communication_type_vec.push_back(RDMA_COMMUNICATION);

        //acceptor_ptr.reset(new Acceptor(ip_address, port_num, session_set , s_num, thd_num));
        acceptor_ptr.reset(new Acceptor(server_options, session_set));
    }

    // totally depend on server_options to setup server.
    server(const ServerOptions& _server_options)
        : is_stop(false)
    {
        server_options._io_service_num =   _server_options._io_service_num;
        server_options._session_num = _server_options._session_num;
        server_options._thd_num_on_one_session = _server_options._thd_num_on_one_session;
        server_options._port_num_vec = _server_options._port_num_vec;
        server_options._communication_type_vec = _server_options._communication_type_vec;

        acceptor_ptr.reset(new Acceptor(server_options,session_set));
    }

    ~server(){
        stop();
    }

    void stop(){
        if(is_stop.load()){
            return;
        }
        is_stop.store(true);
        for(auto it=session_set.begin(); it!=session_set.end(); ++it){
            (*it)->stop(); 
            session_set.erase(it);
        }
        acceptor_ptr->stop();
    }

    void sync_run(){
        acceptor_ptr->sync_run();
    }

    void async_run(){
        acceptor_ptr->async_run();
    }

    // start listen 
    bool start( ProcessMsg process_msg )
    {
        //std::cout<<"server::start, begin..."<<std::endl;
        acceptor_ptr->start( process_msg );
    }

    //this is async send, but hdcs use this interface name.
    void send(void* session_arg, std::string send_buffer, OnSentServer _callback=NULL)
    {
        async_send(session_arg, send_buffer);   
    }

    void async_send(void* session_arg, std::string& send_buffer )
    {
        ///////
        /*
        std::cout<<"server::async_send: session is is "<<session_arg<<std::endl;
        std::cout<<"sessionset have the following elements: "<<std::endl;
        for(auto it=session_set.begin(); it!=session_set.end(); it++){
            std::cout<<"session id is "<<*it<<std::endl;
        }
        //////////
        Session* temp_s_id = (Session*)(((SessionArg*)session_arg)->get_session_id());
        std::cout<<"++++++"<<temp_s_id<<"++++++"<<std::endl;
        if(session_set.find(temp_s_id)==session_set.end()){
            std::cout<<"Networking::server: finding session_id failed. Maybe need to re-connction "<<std::endl;
            //assert(0);
        }
        */
        // TODO TODO TODO TODO
        //((Session*)session_arg)->async_send(send_buffer, ((SessionArg*)session_arg)->get_seq_id());
        ((Session*)session_arg)->async_send(send_buffer, 0);
    }
}; 
}
}
#endif
/*
    asio::error::operation_aborted  -1
    asio::error::connection_aborted -2
    asio::error::connection_reset -3
    asio::error::bad_descriptor -4
    asio::error::interrupted -5
    asio::error::network_down -6
    asio::error::not_connected -7
    asio::error::shut_down -8

*/
