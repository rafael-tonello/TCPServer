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
#include <ThreadPool.h>
#include <string>
#pragma region include for networking
    #include <sys/types.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <signal.h>
    #include <arpa/inet.h>
    #include <sys/epoll.h>

    #include <openssl/ssl.h>
    #include <openssl/err.h>
#pragma endregion
#include <ThreadPool.h>

namespace TCPServerLib
{
    using namespace std;
    class TCPServer;

    enum CONN_EVENT{CONNECTED, DISCONNECTED};
    
    class ClientInfo;

    class SocketHelper{
        protected:
            #ifdef __TESTING__
                public: 
            #endif
            int listenersIdCounter = 0;
            map<int, function<void(ClientInfo *client, char* data,  size_t size)>> receiveListeners;
            map<int, function<void(ClientInfo *client, string data)>> receiveListeners_s;
            map<int, function<void(ClientInfo *client, CONN_EVENT event)>> connEventsListeners;
        public:
            int socketHandle;
            map<string, string> tags;

            int addReceiveListener(function<void(ClientInfo *client, char* data,  size_t size)> onReceive);
            void removeListener(int id);
            int addReceiveListener_s(function<void(ClientInfo *client, string data)> onReceive);
            void removeListener_s(int id);
            int addConEventListener(function<void(ClientInfo *client, CONN_EVENT event)> onConEvent);
            void removeConEventListener(int id);


    };

    class ClientInfo: public SocketHelper{
        public:
            void ___notifyListeners_dataReceived(char* data, size_t size, string dataAsStr);
            void ___notifyListeners_connEvent(CONN_EVENT action);
            size_t ___getReceiveListeners_sSize();

            int socket;
            mutex writeMutex;
            TCPServer *server;

            string address;
            int port;
            sockaddr_in cli_addr;

            void sendData(char* data, size_t size);
            void sendString(string data);
            bool isConnected();
            void disconnect();
            bool sslTlsEnabled;
            SSL *cSsl;

            atomic<bool> __reading;

            ClientInfo(TCPServer *server)
            {
                this->server = server;
                __reading = false;
            }

            ClientInfo(){
                __reading = false;
            }

            ~ClientInfo(){
                //cout << "client deleted" << endl;
            }
    };

    class TCPServer: public SocketHelper{
        public: struct PortConf{
            int port;
            bool ssl_tls = false;
            string private_cert = "";
            string public_cert = "";
        };
        private:
        #ifdef __TESTING__
            public: 
        #endif
            //const int _CONF_MAX_READ_IN_A_TASK = 10485760;
            //const int _CONF_DEFAULT_LOOP_WAIT = 500;
            const int _CONF_DEFAULT_LOOP_WAIT = 500;
            const int _CONF_READ_BUFFER_SIZE = 10240;

            bool deleteClientesAfterDisconnection = true;

            bool sslWasInited = false;

            std::atomic<bool> running;
            std::atomic<int> nextLoopWait;
            
            map<int, ClientInfo*> connectedClients;
            std::mutex connectClientsMutext;
            vector<thread*> listenThreads;
            void notifyListeners_dataReceived(ClientInfo *client, char* data, size_t size);
            void notifyListeners_connEvent(ClientInfo *client, CONN_EVENT action);
            void waitClients(PortConf portConf, function<void(bool sucess)> onStartingFinish);
            void debug(string msg){cout << "TCPServer library debug: " << msg << endl;}
            bool __SocketIsConnected( int socket);
            bool SetSocketBlockingEnabled(int fd, bool blocking);

            void clientSocketConnected(int theSocket, struct sockaddr_in *cli_addr, bool sslTls = false, SSL* ssl = NULL);
            void clientSocketDisconnected(int theSocket);
            void readDataFromClient(int socket, bool usingSsl_tls, SSL* ssl_obj);

            void ssl_init();
            void ssl_stop();
        public:
            map<string, void*> tags;
            
            TCPServer(int port, bool &startedWithSucess, bool AutomaticallyDeleteClientesAfterDisconnection = true);

            /**
             * @brief Construct a new TCPServer object. To start listen, use 'startListen' method
             * 
             * @param AutomaticallyDeleteClientesAfterDisconnection 
             */
            TCPServer(bool autoDeleteClientsAfterDisconnection);

            /**
             * @brief Construct a new TCPServer object. To start listen, use 'startListen' method
             * 
             */
            TCPServer();
            ~TCPServer();

            struct startListen_Result{ vector<PortConf> startedPorts; vector<PortConf> failedPorts; };
            startListen_Result startListen(vector<PortConf> portConfs);

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