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

        this->test("Server should receive packs from a client", [&](){
            string receivedData;
            string message = "This is a message to be received by the server.";
            mutex serverWait;
            serverWait.lock();
            int observerId = server.addReceiveListener_s([&](ClientInfo* cli, string data){
                receivedData = data;
                serverWait.unlock();
            });

            //connect to the server
            int theClient = this->connectToServcer("127.0.0.1", 5002).get();

            //send a pack
            auto start = std::chrono::system_clock::now();
            send(theClient, message.c_str(), message.size(), 0);

            //await for recception by the server
            serverWait.lock();
            auto end = std::chrono::system_clock::now();

            std::chrono::nanoseconds duration = end - start;
            this->blueMessage("Total pack travel time: " + to_string(duration.count()/1000.f/1000.f) + " milisseconds");
            //check the results

            server.removeListener_s(observerId);

            return TestResult{
                receivedData == message,
                message,
                receivedData
            };
        });

        //test write from server
        this->test("Client should receive packs from the server", [&](){
            string message = "This message go to the server and cames back to the client.";
            
            auto observerId = server.addReceiveListener_s([&](ClientInfo* cli, string data){
                //send pack back to the client
                cli->sendString(data);
            });

            //connect to the server
            int theClient = this->connectToServcer("127.0.0.1", 5002).get();

            //send a pack
            auto start = std::chrono::system_clock::now();
            send(theClient, message.c_str(), message.size(), 0);

            string comeBackFromTheSocket = readSocket(theClient, 1000).get();

            //await for recception by the server
            auto end = std::chrono::system_clock::now();
            std::chrono::nanoseconds duration = end - start;
            this->blueMessage("Total pack travel time: " + to_string(duration.count()/1000.f/1000.f) + " milisseconds");
            //check the results

            server.removeListener_s(observerId);

            return TestResult{
                comeBackFromTheSocket == message,
                message,
                comeBackFromTheSocket
            };
        });

        //test broadcast from server



    }

    //cpu usage when server is hiddle
    //https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
}

future<string> TCPServerTester::readSocket(int socket, uint timeout_ms)
{
    return th.enqueue([](int socket2, uint timeout_ms2){
        auto start = std::chrono::system_clock::now();
        double elaspedTime = 0.f;
        char data[1024*1024]; //1k buffer
        size_t readCount = 0;

        while (elaspedTime < timeout_ms2)
        {
            std::chrono::nanoseconds duration = std::chrono::system_clock::now() - start;
            elaspedTime = duration.count()/1000.f/1000.f;

            readCount = recv(socket2,data, 1024*1024, 0);
            if (readCount > 0)
                break;
        }

        string ret = string(data, readCount);
        return ret;
    }, socket, timeout_ms);
}

future<int> TCPServerTester::connectToServcer(string host, int port)
{
    return th.enqueue([](string _host, int _port){
        int sock = 0;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }
    
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(_port);
        
        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, _host.c_str(), &serv_addr.sin_addr)<=0) 
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
    
        int ret = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (ret < 0)
        {
            printf("\nConnection Failed \n");
            return ret;
        }

        return sock;
    }, host, port);
}

