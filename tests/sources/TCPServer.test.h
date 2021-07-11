#ifndef TCPSERVER_TEST_H
#define TCPSERVER_TEST_H

#include <string>
#include <regex>
#include <tester.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "../../sources/TCPServer.h"

class TCPServerTester: public Tester, public API::ApiMediatorInterface{
public:
    vector<string> getContexts();
    void run(string context);
};

      
#endif