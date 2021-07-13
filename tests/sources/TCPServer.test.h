#ifndef TCPSERVER_TEST_H
#define TCPSERVER_TEST_H

#include <string>
#include <regex>
#include <tester.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../sources/TCPServer.h"



using namespace TCPServerLib;
class TCPServerTester: public Tester{
private:
    ThreadPool th;
    future<int> connectToServcer(string host, int port);
    future<string> readSocket(int socket, uint timeout_ms);
public:
    vector<string> getContexts();
    void run(string context);
};

      
#endif