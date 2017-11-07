#include<iostream>
#include <thread>
#include <string>
#include"../hdcs_networking.h"

using namespace std;
using namespace hdcs::networking;



class test_class{
public:

    test_class(int s_num, int thd_num):
        echo_server(new server(7777, s_num, thd_num)){
    }

    ~test_class(){
        cout<<"test_class will destroy!!!!"<<endl;
    }

    void run(){
        cout<<"....run echo server...."<<endl;
        echo_server->start( [ this ](void* p, string s){ //
                handle_request(p, s);
                });
        echo_server->run();
        sleep(1000000);
    }

    void handle_request(void* s_id, string receive_buffer){
        //cout<<"*****echo server******session id :  "<<s_id<<" : received MSG --->"<<receive_buffer<<"!!!!!!!!"<<endl;

        string xx("this is a test program : i'm server   xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");//<<endl;
        echo_server->send((Session*)s_id, xx, NULL);
    }
private:
    server* echo_server;
};




int main(){

    int session_num = 10; // one session for one io_service
    int thd_num_of_one_ios = 20;
    uint64_t run_time = 100000;

    test_class test(session_num, thd_num_of_one_ios);

    test.run(  );

    return 0;


} 




