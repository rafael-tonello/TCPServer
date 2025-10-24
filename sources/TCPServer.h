//TODO: Create a new socket system using libuv: docs.libuv.org/en/v1.x/guide.html


#ifndef _TCPSERVER_H
#define _TCPSERVER_H

#include <functional>
#include <map>
#include <unordered_set>
#include <thread>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <cstring>
#pragma region include for networking
    #include <sys/types.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <errno.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <sys/un.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <signal.h>
    #include <arpa/inet.h>
    #include <sys/epoll.h>

    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <netinet/tcp.h>
#include <memory>
#pragma endregion

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
            map<int, function<void(shared_ptr<ClientInfo> client, char* data,  size_t size)>> receiveListeners;
            map<int, function<void(shared_ptr<ClientInfo> client, string data)>> receiveListeners_s;
            map<int, function<void(shared_ptr<ClientInfo> client, CONN_EVENT event)>> connEventsListeners;
        public:
            int socketHandle;
            map<string, string> tags;

            //liten to data received. The function will be called with raw data (char*) and its size. The id of 
            //observation is returned and could be used to stop listening
            int addReceiveListener(function<void(shared_ptr<ClientInfo> client, char* data,  size_t size)> onReceive);
            void removeListener(int id);

            //liten to data received. The function will be called with the data in a string format. The id of
            //observation is returned and could be used to stop listening
            int addReceiveListener_s(function<void(shared_ptr<ClientInfo> client, string data)> onReceive);
            void removeListener_s(int id);

            //listen to connection events. The function will be called with the client and the event. The id of
            //observation is returned and could be used to stop listening
            int addConEventListener(function<void(shared_ptr<ClientInfo> client, CONN_EVENT event)> onConEvent);
            void removeConEventListener(int id);
    };


    class TCPServer_SocketInputConf { 
    protected:
        int type;
    public:
        bool ssl_tls = false;
        string private_cert = "";
        string public_cert = "";

        TCPServer_SocketInputConf(int type): type(type){};

        virtual string ToString(){ return ""; };

    
        int GetType(){ return type; };

    };

    class TCPServer_PortConf: public TCPServer_SocketInputConf{ public:
        static const int TYPE_NUMBER = 1;
        string ip;
        int port;

        string ToString() override;



        //ip = "" means all interfaces
        TCPServer_PortConf(int port, string ip="", bool enableSslTls = false, string privateCertificateFile="", string publicCertificateFile="");
    };

    class TCPServer_UnixSocketConf: public TCPServer_SocketInputConf{ public:
        static const int TYPE_NUMBER = 2;
        string path;
        TCPServer_UnixSocketConf(string path, bool enableSslTls = false, string privateCertificateFile="", string publicCertificateFile="");
        string ToString();
    };

    class ClientInfo: public SocketHelper, public std::enable_shared_from_this<ClientInfo>{
        public:
            void ___notifyListeners_dataReceived(char* data, size_t size, string dataAsStr);
            void ___notifyListeners_connEvent(CONN_EVENT action);
            size_t ___getReceiveListeners_sSize();

            int socket;
            mutex writeMutex;
            TCPServer *server;

            string address = "";
            std::shared_ptr<TCPServer_SocketInputConf> inputSocketInfo;
            sockaddr cli_addr;

            void sendData(char* data, size_t size);
            void sendString(string data);
            bool isConnected();
            void disconnect();
            bool sslTlsEnabled;
            SSL *cSsl; //TODO: use shared_ptr

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
                //cout << "client " << ((int64_t)(this)) << " deleted" << endl;
            }
    };

    class TCPServer: public SocketHelper{
        private:
        #ifdef __TESTING__
            public: 
        #endif
            //const int _CONF_MAX_READ_IN_A_TASK = 10485760;
            //const int _CONF_DEFAULT_LOOP_WAIT = 500;
            const int _CONF_DEFAULT_LOOP_WAIT = 500;
            const int _CONF_READ_BUFFER_SIZE = 10240;

            mutex clientsToDeleteMutex;
            vector<tuple<int64_t, ClientInfo*>> clientsToDelete;
            int64_t timeToWaitBeforeDelete = 5000; //5 seconds

            bool sslWasInited = false;

            std::atomic<bool> running;
            std::atomic<int> nextLoopWait;
            
            map<int, shared_ptr<ClientInfo>> connectedClients;
            std::mutex connectClientsMutext;
            vector<thread*> listenThreads;
            
            bool debugMode = false;

            

            void notifyListeners_dataReceived(shared_ptr<ClientInfo> client, char* data, size_t size);
            void notifyListeners_connEvent(shared_ptr<ClientInfo> client, CONN_EVENT action);
            void waitClients(shared_ptr<TCPServer_SocketInputConf> portConf, function<void(string error)> onStartingFinish);
            void debug(string msg);
            bool __SocketIsConnected( int socket);
            bool SetSocketBlockingEnabled(int fd, bool blocking);

            void clientSocketConnected(int theSocket, struct sockaddr *cli_addr, bool sslTls = false, SSL* ssl = NULL);
            void clientSocketDisconnected(int theSocket);
            void readDataFromClient(int socket, bool usingSsl_tls, SSL* ssl_obj);

            void ssl_init();
            void ssl_stop();
        public:
            map<string, void*> tags; //TODO: use shared_ptr
            
            //start the server and listen to the port. If the port is already in use, the 'startedWithSucess' will be false.
            //Calls startListen function with a single port
            TCPServer(int port, bool &startedWithSucess);

            //start the server and listen to a unix socket. If the socket is already in use, the 'startedWithSucess' will be false.
            //Calls startListen function with a single unix socket
            TCPServer(string unixsocketpath, bool &startedWithSucess);

            /**
             * @brief Construct a new TCPServer object. To start listen, use 'startListen' method
             * 
             */
            TCPServer();
            ~TCPServer();

            struct startListen_Result{ vector<shared_ptr<TCPServer_SocketInputConf>> startedPorts; vector<tuple<shared_ptr<TCPServer_SocketInputConf>, string>> failedPorts; };
            //star listen in the 'portConfs' ports/unix sockets. You can mix ports and unix sockets in the same call
            startListen_Result startListen(vector<shared_ptr<TCPServer_SocketInputConf>> portConfs);

            bool isConnected(shared_ptr<ClientInfo> client);//TODO: use shared_ptr
            bool _isConnected(ClientInfo &client);//TODO: use shared_ptr

            void disconnect(shared_ptr<ClientInfo> client);//TODO: use shared_ptr
            void _disconnect(ClientInfo &client);//TODO: use shared_ptr

            //disconnect all clients
            void disconnectAll(vector<shared_ptr<ClientInfo>> clientList = {});

            void _sendData(ClientInfo &client, char* data, size_t size);//TODO: use shared_ptr
            void _sendString(ClientInfo &client, string data);//TODO: use shared_ptr

            void sendData(shared_ptr<ClientInfo> client, char* data, size_t size);//TODO: use shared_ptr
            void sendString(shared_ptr<ClientInfo> client, string data);//TODO: use shared_ptr

            //send data to 'clientList' or to all connected clients. Receives raw data (char *) and its size.
            //If clienList is NULL, all connected clients will receive the data
            void sendBroadcast(char* data, size_t size, vector<shared_ptr<ClientInfo>> clientList = {});//TODO: use shared_ptr

            //send data to 'clientList' or all connected clients. Receives a string.
            //If clienList is NULL, all connected clients will receive the data
            void sendBroadcast(string data, vector<shared_ptr<ClientInfo>> clientList = {});//TODO: use shared_ptr

            void enableDebug(bool debugEnabled);

    };
}

#endif
