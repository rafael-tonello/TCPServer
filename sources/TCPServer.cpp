#include "TCPServer.h"

#pragma region SocketHelper class
	int TCPServerLib::SocketHelper::addReceiveListener(function<void(shared_ptr<ClientInfo> client, char* data,  size_t size)> onReceive)
	{
		int id = listenersIdCounter++;
		this->receiveListeners[id] = onReceive;
		return id;
	}

	int TCPServerLib::SocketHelper::addReceiveListener_s(function<void(shared_ptr<ClientInfo> client, string data)> onReceive)
	{
		int id = listenersIdCounter++;
		this->receiveListeners_s[id] = onReceive;
		return id;
	}

	int TCPServerLib::SocketHelper::addConEventListener(function<void(shared_ptr<ClientInfo> client, CONN_EVENT event)> onConEvent)
	{
		int id = listenersIdCounter++;
		this->connEventsListeners[id] = onConEvent;
		return id;
	}

	void TCPServerLib::SocketHelper::removeListener(int id)
	{
		if (this->receiveListeners.count(id))
			this->receiveListeners.erase(id);
	}

	void TCPServerLib::SocketHelper::removeListener_s(int id)
	{
		if (this->receiveListeners_s.count(id))
			this->receiveListeners_s.erase(id);
	}

	void TCPServerLib::SocketHelper::removeConEventListener(int id)
	{
		if (this->connEventsListeners.count(id))
			this->connEventsListeners.erase(id);
	}
#pragma endregion

#pragma region TCPServer_SocketInputConf class
	TCPServerLib::TCPServer_PortConf::TCPServer_PortConf(
		int port, 
		string ip, 
		bool enableSslTls, 
		string privateCertificateFile, 
		string publicCertificateFile)
	: TCPServer_SocketInputConf(TCPServer_PortConf::TYPE_NUMBER)
	{
		this->port = port;
		this->ip = ip;
		this->ssl_tls = enableSslTls;
		this->private_cert = privateCertificateFile;
		this->public_cert = publicCertificateFile;
	}

	std::string TCPServerLib::TCPServer_PortConf::ToString()
	{
		return "PortConf(ip=" + this->ip + ", port=" + std::to_string(this->port) + ", ssl_tls=" + (this->ssl_tls ? "true" : "false") + ")";
	}

	TCPServerLib::TCPServer_UnixSocketConf::TCPServer_UnixSocketConf(
		string path, 
		bool enableSslTls, 
		string privateCertificateFile, 
		string publicCertificateFile)
	: TCPServer_SocketInputConf(TCPServer_UnixSocketConf::TYPE_NUMBER)
	{
		this->path = path;
		this->ssl_tls = enableSslTls;
		this->private_cert = privateCertificateFile;
		this->public_cert = publicCertificateFile;
	}

	std::string TCPServerLib::TCPServer_UnixSocketConf::ToString()
	{
		return "UnixSocketConf(path=" + this->path + ", ssl_tls=" + (this->ssl_tls ? "true" : "false") + ")";
	}
#pragma region TCPServer class
	#pragma region private functions
		void TCPServerLib::TCPServer::notifyListeners_dataReceived(shared_ptr<ClientInfo> client, char* data, size_t size)
		{
			string dataAsString = "";
			if ((this->receiveListeners_s.size() > 0) || (client->___getReceiveListeners_sSize() > 0))
			{
				dataAsString.resize(size);
				for (size_t i = 0; i < size; i++)
					dataAsString[i] = data[i];
			}

			//notify the events in the TCPServer
			for (auto &c: this->receiveListeners)
			{
				c.second(client, data, size);
			}

			for (auto &c: this->receiveListeners_s)
			{
				c.second(client, dataAsString);
			}

			//notify the events in the 'client'
			client->___notifyListeners_dataReceived(data, size, dataAsString);


			//dataAsString.resize(0);
			dataAsString = "";
		}

		void TCPServerLib::TCPServer::notifyListeners_connEvent(shared_ptr<ClientInfo> client, CONN_EVENT action)
		{
			//notify the events in the TCPServer
			for (auto &c: this->connEventsListeners)
			{
				c.second(client, action);
			}

			client->___notifyListeners_connEvent(action);
			
			//IMPORTANT: if disconnected, the 'client' must be destroyed here (or in the function that calls this function);
		}

		TCPServerLib::TCPServer::startListen_Result TCPServerLib::TCPServer::startListen(vector<shared_ptr<TCPServer_SocketInputConf>> ports)
		{
			startListen_Result result{};
			this->running = true;
			this->nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			this->connectedClients.clear();

			atomic<int> numStarted = 0;
			numStarted = 0;

			for (auto &p: ports)
			{
				thread *th = new thread([&](shared_ptr<TCPServer_SocketInputConf> _p){
					this->waitClients(_p, [&](string error)
					{ 
						if (error == "")
						{
							cout << "adding things to result" << endl;
							result.startedPorts.push_back(_p);
							cout << "after adding things to result, the amount of items is " << result.startedPorts.size() << endl;
						}
						else
						{
							cerr << "Failed to start port " << _p->ToString() << ", error: " << error << endl;
							result.failedPorts.push_back({_p, error});
						}
						numStarted++;
					});
				}, p);

				th->detach();
				
				this->listenThreads.push_back(th);
			}


			
			//wait for sockets initialization
			while (numStarted < ports.size())
				usleep(100);
			return result;
		}

		void TCPServerLib::TCPServer::waitClients(
			std::shared_ptr<TCPServer_SocketInputConf> portConf,
			std::function<void(std::string error)> onStartingFinish)
		{
			if (portConf->ssl_tls)
				TCPServerLib::TCPServer::ssl_init();

			int listener, efd;
			const int MAXEVENTS = 128;

			sockaddr_in serv_addr{};
			sockaddr_un serv_addr_unix{};
			sockaddr_storage cli_addr{};
			socklen_t clientSize = sizeof(cli_addr);

			epoll_event event;
			epoll_event* events = nullptr;
			int status;

			if (portConf->GetType() == TCPServer_PortConf::TYPE_NUMBER)
				listener = socket(AF_INET, SOCK_STREAM, 0);
			else if (portConf->GetType() == TCPServer_UnixSocketConf::TYPE_NUMBER)
				listener = socket(AF_UNIX, SOCK_STREAM, 0);
			else {
				this->debug("Invalid port configuration");
				onStartingFinish("Invalid port configuration");
				return;
			}

			if (listener < 0) {
				this->debug("General error opening socket");
				onStartingFinish("General error opening socket");
				return;
			}

			int reuse = 1;
			setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
			int value = 1;
			setsockopt(listener, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));

			if (portConf->GetType() == TCPServer_PortConf::TYPE_NUMBER) {
				serv_addr.sin_family = AF_INET;

				if (((TCPServer_PortConf*)portConf.get())->ip.empty())
					serv_addr.sin_addr.s_addr = INADDR_ANY;
				else
					inet_pton(AF_INET, ((TCPServer_PortConf*)portConf.get())->ip.c_str(),
							&serv_addr.sin_addr);

				serv_addr.sin_port = htons(((TCPServer_PortConf*)portConf.get())->port);
				status = bind(listener, (sockaddr*)&serv_addr, sizeof(serv_addr));
			} else {
				serv_addr_unix.sun_family = AF_UNIX;
				strncpy(serv_addr_unix.sun_path,
						((TCPServer_UnixSocketConf*)portConf.get())->path.c_str(),
						sizeof(serv_addr_unix.sun_path) - 1);
				status = bind(listener, (sockaddr*)&serv_addr_unix,
							sizeof(serv_addr_unix));
			}

			if (status < 0) {
				this->debug("Failure to start socket system");
				onStartingFinish("Failure to start socket system");
				close(listener);
				return;
			}

			SetSocketBlockingEnabled(listener, false);

			if (listen(listener, 5) < 0) {
				this->debug("Failure to open socket");
				onStartingFinish("Failure to open socket");
				close(listener);
				return;
			}

			efd = epoll_create1(0);
			if (efd == -1) {
				this->debug("Server socket epoll_create1 error");
				onStartingFinish("Server socket epoll_create1 error");
				close(listener);
				return;
			}

			event.data.fd = listener;
			event.events = EPOLLIN | EPOLLET;

			if (epoll_ctl(efd, EPOLL_CTL_ADD, listener, &event) == -1) {
				this->debug("Server socket epoll_ctl error");
				onStartingFinish("Server socket epoll_ctl error");
				close(listener);
				close(efd);
				return;
			}

			onStartingFinish("");

			events = (epoll_event*)calloc(MAXEVENTS, sizeof(event));

			while (true) {
				int foundEvents = epoll_wait(efd, events, MAXEVENTS, 1000);

				for (int i = 0; i < foundEvents; i++) {
					if ((events[i].events & (EPOLLERR | EPOLLHUP)) ||
						!(events[i].events & EPOLLIN)) {
						this->debug("epoll error");
						close(events[i].data.fd);
						continue;
					} else if (listener == events[i].data.fd) {
						// Accept new clients
						while (true) {
							int theSocket =
								accept(listener, (sockaddr*)&cli_addr, &clientSize);

							if (theSocket < 0) {
								if (errno == EAGAIN || errno == EWOULDBLOCK)
									break;
								else
									break;
							}

							SetSocketBlockingEnabled(theSocket, false);

							SSL* cSSL = nullptr;
							if (portConf->ssl_tls) {
								SSL_CTX* sslctx = SSL_CTX_new(SSLv23_server_method());
								SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);
								SSL_CTX_use_certificate_file(sslctx,
															portConf->public_cert.c_str(),
															SSL_FILETYPE_PEM);
								SSL_CTX_use_PrivateKey_file(sslctx,
															portConf->private_cert.c_str(),
															SSL_FILETYPE_PEM);
								cSSL = SSL_new(sslctx);
								SSL_set_fd(cSSL, theSocket);
								if (SSL_accept(cSSL) <= 0) {
									SSL_free(cSSL);
									close(theSocket);
									continue;
								}
							}

							event.data.fd = theSocket;
							event.events = EPOLLIN | EPOLLET;
							if (epoll_ctl(efd, EPOLL_CTL_ADD, theSocket, &event) != -1) {
								clientSocketConnected(theSocket, (sockaddr*)&cli_addr,
													portConf->ssl_tls, cSSL);
							} else {
								this->debug("Client epoll_ctl error");
								if (cSSL) SSL_free(cSSL);
								close(theSocket);
							}
						}
					} else {
						// Data from client
						readDataFromClient(events[i].data.fd, portConf->ssl_tls, nullptr);
					}
				}
			}

			free(events);
			close(listener);
			close(efd);
		}

		void TCPServerLib::TCPServer::clientSocketConnected(
			int theSocket, struct sockaddr* cli_addr, bool sslTls, SSL* ssl)
		{
			auto client = std::make_shared<ClientInfo>();
			client->socketHandle = theSocket;
			client->server = this;
			client->socket = theSocket;
			client->sslTlsEnabled = sslTls;
			client->cSsl = ssl;

			char ip_str[INET6_ADDRSTRLEN] = {0};
			if (cli_addr->sa_family == AF_UNIX) {
				client->address =
					std::string("unixsocket:") + ((sockaddr_un*)cli_addr)->sun_path;
				TCPServer_UnixSocketConf inputSocketInfo(
					((sockaddr_un*)cli_addr)->sun_path);
				client->inputSocketInfo = &inputSocketInfo;
			} else {
				inet_ntop(AF_INET, &((sockaddr_in*)cli_addr)->sin_addr, ip_str,
						sizeof(ip_str));
				client->address = std::string("ip:") + ip_str + ":" +
								std::to_string(ntohs(((sockaddr_in*)cli_addr)->sin_port));
				TCPServer_PortConf inputSocketInfo(
					ntohs(((sockaddr_in*)cli_addr)->sin_port), std::string(ip_str));
				client->inputSocketInfo = &inputSocketInfo;
			}

			client->inputSocketInfo->ssl_tls = sslTls;
			client->cli_addr = *cli_addr;

			{
				std::lock_guard<std::mutex> lk(connectClientsMutext);
				connectedClients[theSocket] = client;
			}

			this->notifyListeners_connEvent(client, CONN_EVENT::CONNECTED);
		}

		void TCPServerLib::TCPServer::clientSocketDisconnected(int theSocket)
		{
			close(theSocket);

			std::shared_ptr<ClientInfo> client;
			{
				std::lock_guard<std::mutex> lk(connectClientsMutext);
				auto it = connectedClients.find(theSocket);
				if (it == connectedClients.end()) {
					this->debug("Detect a disconnection of a not connected client!");
					return;
				}
				client = it->second;
				connectedClients.erase(it);
			}

			this->notifyListeners_connEvent(client, CONN_EVENT::DISCONNECTED);
		}

		void TCPServerLib::TCPServer::readDataFromClient(int socket, bool usingSsl_tls,
														SSL* ssl_obj)
		{
			const int bufferSize = _CONF_READ_BUFFER_SIZE;
			char* readBuffer = new char[bufferSize];
			bool done = false;

			while (__SocketIsConnected(socket)) {
				ssize_t count = usingSsl_tls ? SSL_read(ssl_obj, readBuffer, bufferSize)
											: read(socket, readBuffer, bufferSize);

				if (count > 0) {
					std::shared_ptr<ClientInfo> client;
					{
						std::lock_guard<std::mutex> lk(connectClientsMutext);
						if (connectedClients.count(socket) == 0) {
							this->debug("reading data from a not connected client!");
							delete[] readBuffer;
							return;
						}
						client = connectedClients[socket];
					}
					this->notifyListeners_dataReceived(client, readBuffer, count);
				} else if (count == 0) {
					done = true;
					break;
				} else {
					if (errno != EAGAIN && errno != EWOULDBLOCK)
						done = true;
					break;
				}
			}

			if (done) {
				clientSocketDisconnected(socket);
			}

			delete[] readBuffer;
		}

		bool TCPServerLib::TCPServer::__SocketIsConnected(int socket)
		{
			char data;
			int readed = recv(socket,&data,1, MSG_PEEK | MSG_DONTWAIT);//read one byte (but not consume this)

			int error_code;
			socklen_t error_code_size = sizeof(error_code);
			auto getsockoptRet = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
			//string desc(strerror(error_code));
			//return error_code == 0;


			//in the ser of "'TCPCLientLib", after tests, I received 0 in the var 'readed' when server closes the connection and error_code is always 0, wheter or not connected to the server
			//in the case of "TCPServerLib", the error_code works fine
			
			if (getsockoptRet < 0) {
				return false;
			} else if (error_code == 0) {
				return true;
			} else {
				return false;
			}
		}

		void TCPServerLib::TCPServer::ssl_init()
		{
			if (!sslWasInited)
			{
				SSL_load_error_strings();
				SSL_library_init();			
				OpenSSL_add_all_algorithms();
				sslWasInited = true;
			}
		}

		void TCPServerLib::TCPServer::ssl_stop()
		{
			ERR_free_strings();
    		EVP_cleanup();
		}

		void TCPServerLib::TCPServer::debug(string msg)
		{
			cout << "TCPServer library debug: " << msg << endl;
		}

	#pragma endregion

	#pragma region public functions


	TCPServerLib::TCPServer::TCPServer(int port, bool &startedWithSucess)
	{
		auto startResult = this->startListen({ shared_ptr<TCPServer_SocketInputConf>(new TCPServer_PortConf(port)) });
		startedWithSucess = startResult.startedPorts.size() > 0;
	}

	TCPServerLib::TCPServer::TCPServer(string unixsocketpath, bool &startedWithSucess)
	{  
		auto startResult = this->startListen({ shared_ptr<TCPServer_SocketInputConf>(new TCPServer_UnixSocketConf(unixsocketpath)) });
		startedWithSucess = startResult.startedPorts.size() > 0;
		
	}

	TCPServerLib::TCPServer::TCPServer()
	{
		
	}

	TCPServerLib::TCPServer::~TCPServer()
	{
		this->running = false;
		usleep(100000); //wait 100ms to the threads finish their work
		if (sslWasInited)
		{
			//usleep(10000); //needs to wati the sockets shutdown
			ssl_stop();
		}
	}

	void TCPServerLib::TCPServer::sendData(std::shared_ptr<ClientInfo> client, char* data, size_t size)
	{
		// uses unique_lock to make sure the mutex is released when the function ends
		std::unique_lock<std::mutex> lockWrite(client->writeMutex);
		std::unique_lock<std::mutex> lockClients(connectClientsMutext);

		// Verifica se o cliente ainda está na lista de conectados
		if (connectedClients.count(client->socket) == 0) {
			std::cerr << "[TCPServer] sendData: tentativa de envio para cliente desconhecido (socket "
					<< client->socket << ")" << std::endl;
			return;
		}

		if (!__SocketIsConnected(client->socket)) {
			std::cerr << "[TCPServer] sendData: cliente desconectado detectado no envio (socket "
					<< client->socket << ")" << std::endl;
			clientSocketDisconnected(client->socket);
			return;
		}

		if (client->sslTlsEnabled) {
			int ret = SSL_write(client->cSsl, data, static_cast<int>(size));
			if (ret <= 0) {
				int sslErr = SSL_get_error(client->cSsl, ret);
				std::cerr << "[TCPServer] sendData: SSL_write falhou no socket "
						<< client->socket << " (ssl_error=" << sslErr << ")" << std::endl;
				clientSocketDisconnected(client->socket);
			}
		} else {
			ssize_t sent = send(client->socket, data, size, MSG_NOSIGNAL);
			if (sent < 0) {
				int err = errno;
				std::cerr << "[TCPServer] sendData: erro ao enviar para socket "
						<< client->socket << " (" << strerror(err) << ")" << std::endl;

				if (err == EPIPE || err == ECONNRESET) {
					clientSocketDisconnected(client->socket);
				}
			}
		}
	}

	void TCPServerLib::TCPServer::sendString(shared_ptr<ClientInfo> client, string data)
	{
		this->sendData(client, (char*)data.c_str(), data.size());
	}

	void TCPServerLib::TCPServer::sendBroadcast(char* data, size_t size, vector<shared_ptr<ClientInfo>> clientList)
	{
		if (clientList.size() == 0)
		{
			connectClientsMutext.lock();
			for (auto &c: this->connectedClients)   
				clientList.push_back(c.second);
			connectClientsMutext.unlock();
		}

		for (auto &c: clientList)
			c->sendData(data, size);
	}

	void TCPServerLib::TCPServer::sendBroadcast(string data, vector<shared_ptr<ClientInfo>> clientList)
	{
		this->sendBroadcast((char*)(data.c_str()), data.size(), clientList);
	}

	void TCPServerLib::TCPServer::disconnect(shared_ptr<ClientInfo> client)
	{
		close(client->socket);
		//the observer are notified in TCPServer::the clientsCheckLoop method
	}

	void TCPServerLib::TCPServer::disconnectAll(vector<shared_ptr<ClientInfo>> clientList)
	{
		if (clientList.size() == 0)
		{
			connectClientsMutext.lock();
			for (auto &c: this->connectedClients)   
				clientList.push_back(c.second);
			connectClientsMutext.unlock();
		}

		for (auto c : clientList)
		{
			this->disconnect(c);
		}
	}

	bool TCPServerLib::TCPServer::isConnected(shared_ptr<ClientInfo> client)
	{
		return this->__SocketIsConnected(client->socket);
	}

	bool TCPServerLib::TCPServer::SetSocketBlockingEnabled(int fd, bool blocking)
	{
	   if (fd < 0) return false;

		#ifdef _WIN32
		   unsigned long mode = blocking ? 0 : 1;
		   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
		#else
		   int flags = fcntl(fd, F_GETFL, 0);
		   if (flags < 0) return false;
		   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
		   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
		#endif
	}

	void TCPServerLib::TCPServer::enableDebug(bool debugEnabled)
	{
		this->debugMode = debugEnabled;
	}

	#pragma endregion
#pragma endregion

#pragma region ClientInfo class

	void TCPServerLib::ClientInfo::sendData(char* data, size_t size)
	{
		this->server->sendData(shared_from_this(), data, size);
		
	}

	void TCPServerLib::ClientInfo::sendString(string data)
	{
		this->server->sendString(shared_from_this(), data);
	}

	bool TCPServerLib::ClientInfo::isConnected()
	{
		return this->server->isConnected(shared_from_this());
	}

	void TCPServerLib::ClientInfo::disconnect()
	{
		this->server->disconnect(shared_from_this());
	}

	void TCPServerLib::ClientInfo::___notifyListeners_dataReceived(char* data, size_t size, string dataAsStr)
	{
		//notify the events in the TCPServer
		for (auto &c: this->receiveListeners)
		{
			c.second(shared_from_this(), data, size);
		}

		for (auto &c: this->receiveListeners_s)
		{
			c.second(shared_from_this(), dataAsStr);
		}

	}

	void TCPServerLib::ClientInfo::___notifyListeners_connEvent(CONN_EVENT action)
	{
		for (auto &c: this->connEventsListeners)
		{
			c.second(shared_from_this(), action);
		}
	}

	size_t TCPServerLib::ClientInfo::___getReceiveListeners_sSize()
	{
		return this->receiveListeners_s.size();
	}

#pragma endregion
