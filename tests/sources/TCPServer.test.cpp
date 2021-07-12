#include "TCPServer.test.h"

vector<string> TCPServerTester::getContexts()
{
    return {"TCPServer"};

}

void TCPServerTester::run(string context)
{
    if (context == "TCPServer")
    {
        TCPServerLib::TCPServer server({5001, 5002});

        this->test("Should connect to the both ports", [&](){

            int socketPort1 = this->connectToServcer("127.0.0.1", 5001).get();
            int socketPort2 = this->connectToServcer("127.0.0.1", 5002).get();

            string expected = "port1: valid, port2: valid";
            string result = "port1: " + string((socketPort1 >= 0 ? "valid" : "invalid (" + to_string(socketPort1) + ")")) + ", " +
                            "port2: " + string((socketPort2 >= 0 ? "valid" : "invalid (" + to_string(socketPort2) + ")"));
                    

            return TestResult{
                result == expected,
                expected,
                result
            };
        });



    }
}

future<int> TCPServerTester::connectToServcer(string host, int port)
{
    return th.enqueue([&](){
        int sock = 0, valread;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }
    
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        
        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, host.c_str(), &serv_addr.sin_addr)<=0) 
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
    
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("\nConnection Failed \n");
            return -2;
        }

        return sock;
    });
}

