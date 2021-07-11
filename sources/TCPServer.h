//TODO: Create a new socket system using libuv: docs.libuv.org/en/v1.x/guide.html


#ifndef _TCPSERVER_H
#define _TCPSERVER_H

#include <functional>
#include <map>
#include <thread>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include "../ThreadPool/ThreadPool.h"
#include <sstream>
#pragma region include for networking
    #include <sys/types.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
#pragma endregion

namespace TCPServerLib
{
    using namespace std;
    class TCPServer;

    enum CONN_EVENT{CONNECTED, DISCONNECTED};
    
    class ClientInfo;

    class SocketHelper{
        protected:
            vector<function<void(ClientInfo *client, char* data,  size_t size)>> receiveListeners;
            vector<function<void(ClientInfo *client, string data)>> receiveListeners_s;
            vector<function<void(ClientInfo *client, CONN_EVENT event)>> connEventsListeners;
        public:
            int socketHandle;
            map<string, void*> tags;

            void addReceiveListener(function<void(ClientInfo *client, char* data,  size_t size)> onReceive);
            void addReceiveListener_s(function<void(ClientInfo *client, string data)> onReceive);
            void addConEventListener(function<void(ClientInfo *client, CONN_EVENT event)> onConEvent);

            void* __getPrivate(string n){
                if (n == "receiveListeners")
                    return (void*)&this->receiveListeners;
                else if (n == "receiveListeners_s")
                    return (void*)&this->receiveListeners_s;
                else if (n == "connEventsListeners")
                    return (void*)&this->connEventsListeners;
            }
    };

    class ClientInfo: public SocketHelper{
        public:
            void ___notifyListeners_dataReceived(char* data, size_t size, string dataAsStr);
            void ___notifyListeners_connEvent(CONN_EVENT action);
            size_t ___getReceiveListeners_sSize();

            int socket;
            mutex writeMutex;
            ClientInfo();
            TCPServer *server;
            ClientInfo(TCPServer *server);
            void sendData(char* data, size_t size);
            void sendString(string data);
            bool isConnected();
            void disconnect(ClientInfo *client);

            atomic<bool> __reading;

            ClientInfo(){
                __reading = false;
            }
    };

    

    class TCPServer: SocketHelper{
        private:
            const int _CONF_MAX_READ_IN_A_TASK = 5242800;
            const int _CONF_DEFAULT_LOOP_WAIT = 1000;
            const int _CONF_READ_BUFFER_SIZE = 10240;

            std::atomic<bool> running;
            std::atomic<int> nextLoopWait;

            ThreadPool *__tasks = NULL;
            map<int, ClientInfo*> connectedClients;
            std::mutex connectClientsMutext;
            vector<thread*> listenThreads;
            void notifyListeners_dataReceived(ClientInfo *client, char* data, size_t size);
            void notifyListeners_connEvent(ClientInfo *client, CONN_EVENT action);
            void initialize(vector<int> ports, ThreadPool *tasker = NULL);
            void waitClients(int port);
            void debug(string msg){cout << "Debug: " << msg << endl;}
            void chatWithClient(ClientInfo *client);
            bool __SocketIsConnected( int socket);
            void clientsCheckLoop();
        public:
            map<string, void*> tags;
            
            TCPServer(int port, ThreadPool *tasker = NULL);
            TCPServer(vector<int> ports, ThreadPool *tasker = NULL);
            ~TCPServer();

            bool isConnected(ClientInfo *client);

            void disconnect(ClientInfo *client);
            void disconnectAll(vector<ClientInfo*> *clientList = NULL);

            void sendData(ClientInfo *client, char* data, size_t size);
            void sendString(ClientInfo *client, string data);
            void sendBroadcast(char* data, size_t size, vector<ClientInfo*> *clientList = NULL);
            void sendBroadcast(string data, vector<ClientInfo*> *clientList = NULL);

    };
}
#endif