#ifndef RDMA_INTERNAL_CALLBACK
#define RDMA_INTERNAL_CALLBACK

/* wrapper for rdma callback function 
 *
 * hdcs_handle_msg can be posted to thread_group --sdh
 */

#include "../common/counter.h"
#include"../common/Message.h"
#include "rdma_session.h"

namespace hdcs{
namespace networking{

// reading of callback.
class InternalProcessMsg : public Callback {
public:
    enum {
        READING_HEADER = 0,
        READING_CONTENT = 1,
    };
    
    InternalProcessMsg(ProcessMsg _process_msg)
        : hdcs_handle_msg(_process_msg)
        , msg_header(new char[sizeof(MsgHeader)]) 
        , read_status(READING_HEADER)
    {}

    ~InternalProcessMsg(){}
    
    // callback 
    virtual void entry(void *param) override {
        //RDMAConnection *con = static_cast<RDMAConnection*>(param); 
        //uint32_t ret;
        // reading header
        switch(read_status){
            case READING_HEADER:
                read_msg_header(param);
                break;
            case READING_CONTENT:
                read_msg_content(param);
                break;
            default:
                std::cout<<"RDMA_InternalProcessMsg: status unknown..."<<std::endl;
                assert(0);
                break;
        }
        return;
    }
  private:

    int read_msg_header(void* param){
        uint32_t ret; 
        RDMAConnection *con = static_cast<RDMAConnection*>(param); 
        ret = con->read_buffer(msg_header, msg_header_size);
        if(0 == ret){
            // in-complete message header
            assert(0);
            return -1;
        }else if(ret < 0){
            // error
            std::cout<<"RDMA_InternalProcessMsg: reading header failed "<<std::endl;
            assert(0);
        }else{
            // complete message header
            msg_content_size = ((MsgHeader*)msg_header)->get_data_size();
            msg_content = new char[msg_content_size+1]();
            if(atomic_swap(&read_status,(int)READING_CONTENT) != READING_HEADER){
                assert(0);
            }
            // when reading successfully, re-reading content.
            read_msg_content(param);
            return 0;
        }
    }

    int read_msg_content(void* param){
        uint32_t ret;
        RDMAConnection *con = static_cast<RDMAConnection*>(param); 
        ret = con->read_buffer(msg_content, msg_content_size); 
        if(0 == ret){
            // in-complete message content
            assert(0);

            return -1;
        }else if(ret < 0){
            std::cout<<"RDMA_InternalProcessMsg: reading content failed "<<std::endl;
            assert(0);
        }else{
            // complete message content, read it, and process it.
            //hdcs_handle_msg(/*param , */ std::move(std::string(msg_content, msg_content_size)));
            delete[] msg_content;
            if(atomic_swap(&read_status,(int)READING_HEADER) != READING_CONTENT){
                assert(0);
            }

            return 0;
        }
    }

private:
    ProcessMsg hdcs_handle_msg;
    volatile int read_status;
    char* msg_header;
    char* msg_content;
    uint32_t msg_header_size;
    uint32_t msg_content_size;
};

// callback of connection
// when this function be called, express that conection action success
// So, create a session
class InternalOnConnection : public Callback {
public:
    InternalOnConnection(SessionPtr& _session, WaitEvent& _wait_event)
        : new_session(_session)
        , wait_event(_wait_event)
    {}

    virtual ~InternalOnConnection(){}

    virtual void entry(void *param) override {
        // create session using RDMAConnection
        RDMAConnection *con = static_cast<RDMAConnection*>(param); 
        assert(_internal_process_msg_ptr);
        con->set_read_callback(_internal_process_msg_ptr.get());

        new_session = new rdma_session(con);
        wait_event.Signal();
        //con->async_send("hello server", 13);
    }

    void set_process_msg(std::shared_ptr<InternalProcessMsg> _p_m){
       _internal_process_msg_ptr = _p_m;
  
    }

private:
    std::shared_ptr<InternalProcessMsg> _internal_process_msg_ptr;
    SessionPtr& new_session;
    WaitEvent& wait_event;
};

// callback of accept
// when this function be called, express that a connection request arrived, and 
// sucessfully estimash a conntion. 
class InternalOnAccept : public Callback{
public:
    InternalOnAccept(SessionSet& s_s)
        : session_set(s_s)
    {}

    ~InternalOnAccept(){}

    virtual void entry(void *param) override {
        RDMAConnection* conn = static_cast<RDMAConnection*>(param);
        assert(_internal_process_msg_ptr);
        conn->set_read_callback(_internal_process_msg_ptr.get());

        new_session = new rdma_session(conn);
        session_set.insert(new_session);
    }

    void set_process_msg(std::shared_ptr<InternalProcessMsg> _p_m){
        _internal_process_msg_ptr = _p_m;
    }

private:

    SessionPtr new_session;
    SessionSet& session_set;
    std::shared_ptr<InternalProcessMsg> _internal_process_msg_ptr;
};


}
}

#endif
