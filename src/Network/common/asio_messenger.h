#ifndef ASIO_MESSENGER
#define ASIO_MESSENGER
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <condition_variable>
#include <memory>
#include <string>
#include "Message.h"
#include "aio_complete.h"
#include "networking_common.h"
namespace hdcs{
namespace networking{
using boost::asio::ip::tcp;

class asio_messenger{
public:
    asio_messenger( IOService& ios, int _role)
        : io_service_(ios)
        , strand_(ios)
        , socket_(ios)
        , header_len(sizeof(MsgHeader))
        , msg_header(new char[sizeof(MsgHeader)])
        , resolver_(ios)
        , role(_role) // 0 is client, and 1 is server
    {
        if(role==1){ 
            is_closed = false;
            //set_socket_option();
        }else{
            is_closed = false;
        }
    }

    ~asio_messenger() {
        if( !is_closed ){
            close();
        }
        delete[] msg_header;
    }

    int set_socket_option(){
        boost::system::error_code ec;
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay, ec);
        if(ec){
            std::cout<<"asio_messenger::set_socket_option failed: "<<ec.message()<<std::endl;
            assert(0);
            return -1;
        }
        return 0;
    }

    void close(){
        if(is_closed){
            return;
        }
        boost::system::error_code ec;
        socket_.close(ec);
        if(ec){
            std::cout<<"asio_messenger::close, close failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        is_closed = true;
    }

    boost::asio::ip::tcp::socket& get_socket() {
        return socket_;
    }

    void cancel(){
        boost::system::error_code ec;
        socket_.cancel(ec);
        if(ec){
            std::cout<<"asio_messenger::cancel, cancle failed: "<<ec.message()<<std::endl;
        }
    }

    /********************connection********************************/
    int sync_connection(std::string ip_address, std::string port){
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_.resolve(boost::asio::ip::tcp::resolver::query(ip_address, port), ec);
        if(ec){
            std::cout<<"asio_messenger::sync_connection, resolver failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        endpoint_ = *iter;
        socket_.connect(endpoint_, ec);
        if(ec){
            std::cout<<"asio_messenger::sync_connection, connect  failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        int ret = set_socket_option();
        if(ret){
            assert(0);
        }
        is_closed = false;
        return 0;
    }

    int async_connection(){
        return 0;
    }

    /**************************send****************************/
    int sync_send( std::string send_buffer ) {
        boost::system::error_code ec;
        uint64_t ret;
        Message msg(send_buffer); // due to sync send, local object can live out sync_send operation.
        uint64_t send_bytes = msg.to_buffer().size();
        ret=boost::asio::write(socket_, boost::asio::buffer(std::move(msg.to_buffer())), ec);
        if(ec){
            std::cout<<"asio_messenger::sync_send failed: "<<ec.message()<<std::endl;
            assert(0);
        }
        if(ret != send_bytes){
            std::cout<<"asio_messenger::sync_send failed: ret != send_bytes "<<std::endl;
            assert(0);
        }
        return 0;
    }

    /*
    void async_send(std::string send_buffer, std::shared_ptr<aio_complete> onfinish) {
        std::string* send_string = new std::string( Message(send_buffer).to_buffer());
        uint64_t ret = send_string->size();
        boost::asio::async_write(socket_, boost::asio::buffer(*send_string, ret),
        //strand_.wrap(        
            [this, onfinish, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                    onfinish->complete(0, cb, NULL, NULL);
                }else{
                    // when socket have been closed, how to handle this case.?????
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                    onfinish->complete(-1, cb, NULL, NULL);
                }
                delete send_string; 
            });
    }
    */
   // implement 1
   void async_send(std::string send_buffer, std::shared_ptr<aio_complete> onfinish) {
        Message msg( send_buffer );
        uint64_t ret = msg.to_buffer().size();
        boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer(), ret),
        //strand_.wrap(     // yuan   
            [this, onfinish, ret](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                    onfinish->complete(0, cb, NULL, NULL);
                }else{
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                    onfinish->complete(-1, cb, NULL, NULL);
                }
            });
           // }))  // yuan
    }
    // implement 2
    int async_send(std::string send_buffer) {
        Message msg( send_buffer );
        uint64_t ret = msg.to_buffer().size();
        boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer(), ret),
            [this, ret](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                }else{
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                }
            });
    }


   /**********************************receive***************************************/
    ssize_t sync_receive( char* &receive_buffer ){
        boost::system::error_code ec;
        uint64_t ret;
        char msg_header[header_len];
        ret = boost::asio::read(socket_, boost::asio::buffer(msg_header, header_len), ec); 
        if(ret<0){
            std::cout<<"asio_session::sync_receive msg_header failed:  "<<ec.message()<<std::endl;
            return -1;
        }
        if(ret != header_len){
            std::cout<<"asio_session::sync_receive msg_header failed: ret!=header_len "<<std::endl;
            return -1;
        }
        uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
        if(content_size>1000){
        	std::cout<<"content_size is "<<content_size<<std::endl;
	}
        receive_buffer = new char[content_size+1]();
        ret = boost::asio::read(socket_, boost::asio::buffer(receive_buffer, content_size), ec); 
        if(ret<0){
            std::cout<<"asio_session::sync_receive content failed: "<<ec.message()<<std::endl;
            delete[] receive_buffer;
            return -1;
        }
        if(ret != content_size){
            std::cout<<"asio_session::sync_receive content failed: ret!= content_size"<<std::endl;
            delete[] receive_buffer;
            return -1;
        }
        return content_size;
   }

    /*
    // chendi: async_read + read_some 
    void async_receive(char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
      char* buffer_ = new char[sizeof(MsgHeader)];
      boost::asio::async_read(socket_, boost::asio::buffer(buffer_, sizeof(MsgHeader)),
          [this, buffer_, onfinish](
              const boost::system::error_code& err, size_t exact_read_bytes) {
          if (!err) {
            assert(exact_read_bytes == sizeof(MsgHeader));
            uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
            //char* data_buffer = (char*)malloc(content_size);
            std::cout<<"content_size is  "<<content_size<<std::endl;
            char* data_buffer = new char[content_size+1]();
            exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
            if (exact_read_bytes == content_size) {
                onfinish->complete(0, exact_read_bytes, data_buffer, NULL);
              //cb((void*)this, std::move(std::string(data_buffer, content_size)));
              //free(data_buffer);
              //aio_read();
            } else {
              printf("received: %lu data, not finished yet\n", exact_read_bytes);
            }
            delete[] buffer_;
          }else{
              std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
          }          
      });
    }
*/

/*
    // lock + async_read + async_read, success...
    void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        char* msg_header = new char[header_len];
        write_lock.lock();
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, msg_header, onfinish](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, data_buffer, content_size, onfinish]( const boost::system::error_code& err, uint64_t cb ){
                            write_lock.unlock();
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                onfinish->complete(0, cb, data_buffer, NULL);
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                                delete[] data_buffer;
                            }
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
              delete[] msg_header; // delete hander memory space.
        });
    }
*/

   // use common  header buffer
   void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, onfinish](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1];
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, data_buffer, content_size, onfinish]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
                                onfinish->complete(0, cb, data_buffer, NULL);
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                                delete[] data_buffer;
                            }
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
        });
    }

   //just receive one times, including header and content..
   //header will be directly origoned. success
   /*
    void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        char* msg_header = new char[header_len + 35];
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len+35),
            [ this, msg_header, onfinish](const boost::system::error_code& err, uint64_t cb) {
                uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                //std::cout<<"contest_size is "<<content_size<<std::endl;
                if(!err){
                    if(cb != (header_len+35)){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    char* data_buffer = new char[35];
                    memcpy(data_buffer, msg_header+header_len, 35);
                    onfinish->complete(0, 35, data_buffer, NULL);
              }else{
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
              delete[] msg_header;
        });
    }
   */

  /*
          // async_read + sync_read, fails...
    void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        char* msg_header = new char[header_len];
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, msg_header, onfinish](const boost::system::error_code& err, uint64_t cb) {
                uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                std::cout<<"contest_size is "<<content_size<<std::endl;
                if(!err){
                    char* data_buffer = new char[content_size];
                    uint64_t ret = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
                    if(ret != content_size){
                        std::cout<<"asio_messenger::read_some failed, cb != content_size" << std::endl;
                        assert(0);
                    }
                    onfinish->complete(0, content_size, data_buffer, NULL);
                }else{
                    std::cout << "asio_messenger::async_receive: async_read failed: " << err.message() << std::endl;
                }
                delete[] msg_header;
        });
    }
*/

/*
    // one sync read, success 
     void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        char* msg_header = new char[header_len + 35];
        uint64_t ret = socket_.read_some(boost::asio::buffer(msg_header, header_len+35));
        if(ret != (header_len+35)){
            std::cout<<"asio_messenger::read_some failed, cb != content_size" << std::endl;
            assert(0);
         }
         char* data_buffer = new char[35];
         memcpy(data_buffer, msg_header+header_len, 35);
         delete[] msg_header;
         onfinish->complete(0, 35, data_buffer, NULL);
    }
*/
/*
  void async_receive( char* receive_buffer, std::shared_ptr<aio_complete> onfinish) {
        char* msg_header = new char[header_len ];
        uint64_t ret = socket_.read_some(boost::asio::buffer(msg_header, header_len));
        if(ret != (header_len)){
            std::cout<<"asio_messenger::read_some failed, cb != content_size" << std::endl;
            assert(0);
         }
         uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
         std::cout<<"contest_size is "<<content_size<<std::endl;
         char* data_buffer = new char[content_size];
         delete[] msg_header;
         onfinish->complete(0, 35, data_buffer, NULL);
    }
*/

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::endpoint endpoint_;
    char* msg_header;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::io_service::strand strand_;
    const ssize_t header_len;
    int counts;
    bool is_closed;
    int role;

public:
    ssize_t communicate(std::string& send_buffer){
        return 1;
    }
    
    void aio_communicate(std::string& send_buffer){
        std::string* send_string = new std::string( Message(send_buffer).to_buffer());
        uint64_t ret = send_string->size();
        boost::asio::async_write(socket_, boost::asio::buffer(*send_string, ret),
            [this, ret, send_string](  
                const boost::system::error_code& ec, uint64_t cb) {
                if (!ec) {
                    if(ret != cb){
                        std::cout<<"asio_session::aync_send failed: ret != cb"<<std::endl;
                        assert(0);
                    }
                }else{
                    std::cout<<"asio_session::aync_send failed: "<<ec.message()<<std::endl;
                }
                delete send_string; 
            });
    }

    void aio_receive(ProcessMsgClient process_msg){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, process_msg](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, data_buffer, content_size, process_msg]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
				aio_receive(process_msg);
				process_msg(data_buffer);
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
        });
    }
    
   void aio_receive(void* arg, ProcessMsg process_msg){
        boost::asio::async_read(socket_, boost::asio::buffer(msg_header, header_len),
            [ this, process_msg, arg](const boost::system::error_code& err, uint64_t cb) {
                if(!err){
                    if(cb != header_len){
                        std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                        assert(0);
                    }
                    uint64_t content_size = ((MsgHeader*)msg_header)->get_data_size();
                    char* data_buffer = new char[content_size+1]();
                    boost::asio::async_read( socket_, boost::asio::buffer(data_buffer, content_size ),
                        [this, arg, data_buffer, content_size, process_msg]( const boost::system::error_code& err, uint64_t cb ){
                            if(!err){
                                if( content_size != cb ){
                                    std::cout<<"asio_messenger::async_receive, firstly async_read failed: cb != header_len" << std::endl;
                                    assert(0);
                                }
				aio_receive(arg, process_msg);
				process_msg(arg, data_buffer);
                            }else{ 
                                std::cout << "asio_messenger::async_receive: secondly async_read failed: " << err.message() << std::endl;
                            }
 			    delete[] data_buffer;
                        });
               }else{ 
                   std::cout << "asio_messenger::async_receive: firstly async_read failed: " << err.message() << std::endl;
              }
        });
    }

};

}
}

#endif

