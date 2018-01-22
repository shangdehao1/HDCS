#ifndef ASIO_CONNECT
#define ASIO_CONNECT
#include "asio_session.h"
#include "../io_service/io_service_pool.h"
namespace hdcs{
namespace networking{

class AsioConnect{
public:
    /*
    AsioConnect(int _ios_num, int _thd_num_of_one_ios)
        : io_service_pool_ptr(new io_service_pool(_ios_num, _thd_num_of_one_ios))
    {
        m_io_service_pool.async_run();
    }
    */

    AsioConnect(const ClientOptions& _co)
        : client_options(_co)
        , io_service_pool_ptr(new io_service_pool(_co._io_service_num, _co._thd_num_on_one_session))
    {
        io_service_pool_ptr->async_run();
    }

    ~AsioConnect()
    {
        close();
    }

    int async_connect( std::string ip_address, std::string port )
    {
        // TODO
        return 1;
    }

    void close()
    {
        io_service_pool_ptr->stop();
    }

    SessionPtr sync_connect(std::string ip_address, std::string port)
    {
       asio_session* new_session;
       new_session = new asio_session(io_service_pool_ptr->get_io_service(), 0);
       // must set process_msg before calling sync_connect
       new_session->set_process_msg_client(client_options._process_msg);
       new_session->set_process_msg_arg_client(client_options._process_msg_arg);

       int ret;
       ret = new_session->sync_connection(ip_address, port);
       if(ret != 0){
           std::cout<<"connect:: new_session->sync_connection failed."<<std::endl;
           return NULL; // connect failed
       }
       return (Session*)new_session;
    }
private:
    const ClientOptions& client_options;
    std::shared_ptr<io_service_pool> io_service_pool_ptr;
};
}
}
#endif
