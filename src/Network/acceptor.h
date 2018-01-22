#ifndef ACCEPTOR
#define ACCEPTOR

#include "common/networking_common.h"
#include "asio_common/asio_acceptor.h"
#include "rdma_common/rdma_acceptor.h"
#include "common/option.h"

namespace hdcs{
namespace networking{

class Acceptor{
private:

    std::shared_ptr<asio_acceptor> asio_acceptor_ptr;
    std::shared_ptr<rdma_acceptor> rdma_acceptor_ptr;
    WaitEvent wait_event;
    bool sync_run_model;
public:

    // must ensure _port_num_vec don't have deplicate elements, 
    // otherwise 'bind' operation fail.
    Acceptor(const ServerOptions& server_options, SessionSet& _set)
        : asio_acceptor_ptr(NULL)
        , rdma_acceptor_ptr(NULL)
        , sync_run_model(true)
    {
        auto temp_port_vec = server_options._port_num_vec;
        auto temp_type_vec = server_options._communication_type_vec;
        for( int i = 0; i < temp_port_vec.size() ; i++)
        {
            if(temp_type_vec[i] == TCP_COMMUNICATION )
            {
                // tcp communication
                asio_acceptor_ptr.reset(new asio_acceptor("0.0.0.0", temp_port_vec[i], _set, 
                            server_options._session_num, server_options._thd_num_on_one_session));
            }
            else if(temp_type_vec[i] == RDMA_COMMUNICATION)
            {
                // rdma communication
                rdma_acceptor_ptr.reset(new rdma_acceptor("0.0.0.0", temp_port_vec[i], _set ));
            }
            else
            {
                // local communication
                std::cout<<"local communication, don't support now!!!!"<<std::endl;
                assert(0);
            }
        }
    }

    ~Acceptor()
    {
        stop();
        asio_acceptor_ptr.reset();
        rdma_acceptor_ptr.reset();
        // TODO
    }

    void stop()
    {
        if(asio_acceptor_ptr != NULL)
        {
            asio_acceptor_ptr->stop();
        }
        if(rdma_acceptor_ptr != NULL)
        {
            rdma_acceptor_ptr->stop();
        }
        if(false)
        {
            //TODO
        }
        if(sync_run_model)
        {
            wait_event.Signal();
        }
    }

    void start(ProcessMsg _process_msg)
    {
        if(asio_acceptor_ptr != NULL)
        {
            asio_acceptor_ptr->start(_process_msg);
        }
        if(rdma_acceptor_ptr != NULL)
        {
            rdma_acceptor_ptr->start(_process_msg);
        }
        if(false)
        {
            //TODO 
        }
    }

    // for sync run, how to start all sync_run
    // TODO TODO TODO TODO TODO
    void sync_run()
    {
        async_run();
        sync_run_model = true;
        wait_event.Wait();
    }

    void async_run()
    {
        if(asio_acceptor_ptr != NULL)
        {
            asio_acceptor_ptr->async_run();
        }
        if(rdma_acceptor_ptr != NULL)
        {
            rdma_acceptor_ptr->async_run();
        }
        if(false)
        {
            //TODO
        }
        sync_run_model = false;
    }

};  //acceptor 

} // networking
} // hdcs

#endif
