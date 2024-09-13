#include "TCPServer.h"

#pragma region SocketHelper class
	int TCPServerLib::SocketHelper::addReceiveListener(function<void(ClientInfo *client, char* data,  size_t size)> onReceive)
	{
		int id = listenersIdCounter++;
		this->receiveListeners[id] = onReceive;
		return id;
	}

	int TCPServerLib::SocketHelper::addReceiveListener_s(function<void(ClientInfo *client, string data)> onReceive)
	{
		int id = listenersIdCounter++;
		this->receiveListeners_s[id] = onReceive;
		return id;
	}

	int TCPServerLib::SocketHelper::addConEventListener(function<void(ClientInfo *client, CONN_EVENT event)> onConEvent)
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
	TCPServerLib::TCPServer_PortConf::TCPServer_PortConf(int port, string ip, bool enableSslTls, string privateCertificateFile, string publicCertificateFile)
	{
		this->port = port;
		this->ip = ip;
		this->ssl_tls = enableSslTls;
		this->private_cert = privateCertificateFile;
		this->public_cert = publicCertificateFile;
	}

	TCPServerLib::TCPServer_UnixSocketConf::TCPServer_UnixSocketConf(string path, bool enableSslTls, string privateCertificateFile, string publicCertificateFile)
	{
		this->path = path;
		this->ssl_tls = enableSslTls;
		this->private_cert = privateCertificateFile;
		this->public_cert = publicCertificateFile;
	}

#pragma region TCPServer class
	#pragma region private functions
		void TCPServerLib::TCPServer::notifyListeners_dataReceived(ClientInfo *client, char* data, size_t size)
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

		void TCPServerLib::TCPServer::notifyListeners_connEvent(ClientInfo *client, CONN_EVENT action)
		{
			//notify the events in the TCPServer
			for (auto &c: this->connEventsListeners)
			{
				c.second(client, action);
			}

			client->___notifyListeners_connEvent(action);
			
			//IMPORTANT: if disconnected, the 'client' must be destroyed here (or in the function that calls this function);
		}

		TCPServerLib::TCPServer::startListen_Result TCPServerLib::TCPServer::startListen(vector<TCPServer_SocketInputConf> ports)
		{
			startListen_Result result;

			this->running = true;
			this->nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			this->connectedClients.clear();

			atomic<int> numStarted;
			numStarted = 0;

			for (auto &p: ports)
			{
				thread *th = new thread([&](TCPServer_SocketInputConf _p){
					this->waitClients(_p, [&](bool sucess)
					{ 
						if (sucess)
							result.startedPorts.push_back(_p);
						else
							result.failedPorts.push_back(_p);
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

		void TCPServerLib::TCPServer::waitClients(TCPServer_SocketInputConf portConf, function<void(bool sucess)> onStartingFinish)
		{
			if (portConf.ssl_tls)
				TCPServerLib::TCPServer::ssl_init();
			//create an socket to await for connections

			int listener, efd, epoll_ctl_result, foundEvents;
			int MAXEVENTS = 128;

			struct sockaddr_in *serv_addr = new sockaddr_in();
			struct sockaddr_un *serv_addr_unix = new sockaddr_un();
			struct sockaddr *cli_addr = new sockaddr();
			struct epoll_event event;
  			struct epoll_event *events;
			int status;
			socklen_t clientSize = sizeof(cli_addr);;
			char *ip_str;

			SSL_CTX *sslctx = NULL;
			SSL *cSSL = NULL;


			if (typeid(portConf) == typeid(TCPServer_PortConf) )
				listener = socket(AF_INET, SOCK_STREAM, 0);
			else
				listener = socket(AF_UNIX, SOCK_STREAM, 0);

			if (listener >= 0)
			{
				//reuse address
				int reuse = 1;

				if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
					this->debug("setsockopt(SO_REUSEADDR) failed");

				//no delay (disable Nagle's algorithm)
				int value = 1;
				if (setsockopt(listener, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(int)))
					this->debug("setsockopt(TCP_NODELAY) failed");

				if (typeid(portConf) == typeid(TCPServer_PortConf) ) 
				{
					serv_addr->sin_family = AF_INET;
					if (((TCPServer_PortConf*)&portConf)->ip == "")
						serv_addr->sin_addr.s_addr = INADDR_ANY;
					else
						inet_pton(AF_INET, ((TCPServer_PortConf*)&portConf)->ip.c_str(), &serv_addr->sin_addr);

					//serv_addr->sin_addr.s_addr = INADDR_ANY;
					serv_addr->sin_port = htons(((TCPServer_PortConf*)&portConf)->port);
					usleep(1000);
					status = bind(listener, (struct sockaddr *) serv_addr, sizeof(*serv_addr));
					usleep(1000);
				}
				else
				{
					serv_addr_unix->sun_family = AF_UNIX;
					strcpy(serv_addr_unix->sun_path, ((TCPServer_UnixSocketConf*)&portConf)->path.c_str());
					status = bind(listener, (struct sockaddr *) serv_addr_unix, sizeof(*serv_addr_unix));
				}

				if (status >= 0)
				{
					SetSocketBlockingEnabled(listener, false);
					status = listen(listener, 5);
					if (status >= 0)
					{

						efd = epoll_create1 (0);
						if (efd != -1)
						{
							event.data.fd = listener;
  							event.events = EPOLLIN | EPOLLET;
							
							epoll_ctl_result = epoll_ctl (efd, EPOLL_CTL_ADD, listener, &event);

							if (epoll_ctl_result != -1)
							{
								onStartingFinish(true);

								events = (struct epoll_event*)calloc (MAXEVENTS, sizeof event);

								while (true)
								{
									event.data.fd = listener;
  									event.events = EPOLLIN | EPOLLET;
											
											
									foundEvents = epoll_wait (efd, events, MAXEVENTS, 1000);

									
      								for (auto i = 0; i < foundEvents; i++)
									{
										if ((events[i].events & EPOLLERR) ||
												(events[i].events & EPOLLHUP) ||
												(!(events[i].events & EPOLLIN)))
										{
											/* An error has occured on this fd, or the socket is not
												ready for reading (why were we notified then?) */
											fprintf (stderr, "epoll error\n");
											close (events[i].data.fd);
											continue;
										}
										else if (listener == events[i].data.fd)
										{
											/* We have a notification on the listening socket, which
												means one or more incoming connections. */
											while (1)
											{
												int theSocket = accept(listener, (struct sockaddr *) cli_addr, &clientSize);

												if (theSocket >= 0)
												{
													SetSocketBlockingEnabled(theSocket, false);

													if (portConf.ssl_tls)
													{
														sslctx = SSL_CTX_new( SSLv23_server_method());
														SSL_CTX_set_options(sslctx, SSL_OP_SINGLE_DH_USE);
														
														int use_cert = SSL_CTX_use_certificate_file(sslctx, portConf.public_cert.c_str() , SSL_FILETYPE_PEM);

														int use_prv = SSL_CTX_use_PrivateKey_file(sslctx, portConf.private_cert.c_str(), SSL_FILETYPE_PEM);

														cSSL = SSL_new(sslctx);
														SSL_set_fd(cSSL, theSocket);
														//Here is the SSL Accept portion.  Now all reads and writes must use SSL
														auto ssl_err = SSL_accept(cSSL);
														if(ssl_err <= 0)
														{
															//Error occurred, log and close down ssl
															//ShutdownSSL();
														}
													}


													event.data.fd = theSocket;
													event.events = EPOLLIN | EPOLLET;
													auto tmpResult = epoll_ctl (efd, EPOLL_CTL_ADD, theSocket, &event);
													if (tmpResult != -1)
														clientSocketConnected(theSocket, cli_addr, portConf.ssl_tls, cSSL);
													else
													{
														this->debug("Client epoll_ctl error");
													}
												}
												else if (theSocket == -1)
												{
													if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
													{
													/* We have processed all incoming
														connections. */
														break;
													}
													//else
													//{
													//	perror ("accept");
													break;
												}
											}
										}
										else
										{

											/* We have data on the fd waiting to be read. Read and
                 							display it. We must read whatever data is available
											completely, as we are running in edge-triggered mode
											and won't get a notification again for the same
											data. */

											readDataFromClient(events[i].data.fd, portConf.ssl_tls, cSSL);
										}
									}
								}
								
								SSL_shutdown(cSSL);
    							SSL_free(cSSL);


								free (events);
								close (listener);
							}
							else
							{
								this->debug("Server socket epoll_ctl error");
								onStartingFinish(false);

							}
						}
						else
						{
							this->debug("Server socket epoll_create1 error");
							onStartingFinish(false);
						}
            		}
					else
					{
						this->debug("Failure to open socket");
						onStartingFinish(false);
					}
				}
				else
				{
					this->debug("Failure to start socket system");
					onStartingFinish(false);
				}
			}
			else
			{
				this->debug("General error opening socket");
				onStartingFinish(false);
			}
		}

		void TCPServerLib::TCPServer::clientSocketConnected(int theSocket, struct sockaddr *cli_addr, bool sslTls, SSL* ssl)
		{
			
			//creat ea new client
			ClientInfo *client = new ClientInfo();
			client->socketHandle = theSocket;
			client->server = this;
			client->socket = theSocket;
			client->sslTlsEnabled = sslTls;
			client->cSsl = ssl;
			char *ip_str = new char[255];
			//check if cli_addr is a sockaddr_in or sockaddr_un
			if (cli_addr->sa_family == AF_UNIX)
			{
				client->address = string("unixsocket:")+string(((sockaddr_un*)cli_addr)->sun_path);

				TCPServer_UnixSocketConf inputSocketInfo(((sockaddr_un*)cli_addr)->sun_path);
				client->inputSocketInfo = inputSocketInfo;
			}
			else
			{
				inet_ntop(AF_INET, &((sockaddr_in*)cli_addr)->sin_addr, ip_str, 255);
				client->address = string("ip:")+string(ip_str) + ":" + to_string(ntohs(((sockaddr_in*)cli_addr)->sin_port));

				TCPServer_PortConf inputSocketInfo(ntohs(((sockaddr_in*)cli_addr)->sin_port), string(ip_str));
				client->inputSocketInfo = inputSocketInfo;
			}
			delete[] ip_str;
			
			client->inputSocketInfo.ssl_tls = sslTls;
			client->cli_addr = *cli_addr;

			connectClientsMutext.lock();
			this->connectedClients[theSocket] = client;
			connectClientsMutext.unlock();
			this->notifyListeners_connEvent(client, CONN_EVENT::CONNECTED);
		}

		void TCPServerLib::TCPServer::clientSocketDisconnected(int theSocket)
		{
			close (theSocket);
			if (connectedClients.count(theSocket) == 0)
			{
				this->debug("Detect a disconnection of a not connected client!");
				//clientSocketConnected(theSocket);
				return;
			}

			ClientInfo *client = connectedClients[theSocket];

			connectClientsMutext.lock();
			this->connectedClients.erase(theSocket);
			connectClientsMutext.unlock();

			this->notifyListeners_connEvent(client, CONN_EVENT::DISCONNECTED);
			if (deleteClientesAfterDisconnection)
				delete client;
		}


		void TCPServerLib::TCPServer::readDataFromClient(int socket, bool usingSsl_tls, SSL* ssl_obj)
		{
			int bufferSize = _CONF_READ_BUFFER_SIZE;
			char readBuffer[bufferSize]; //10k buffer

			bool done = false;
			ssize_t count;


			while (__SocketIsConnected(socket))
			{

				if (usingSsl_tls)
					count = SSL_read (ssl_obj, readBuffer, sizeof readBuffer);
				else
					count = read (socket, readBuffer, sizeof readBuffer);
				//count = recv(socket,readBuffer, sizeof readBuffer, 0);
				if (count > 0)
				{
						
					if (connectedClients.count(socket) == 0)
					{
						this->debug("reading data from a not connect client!");
						//clientSocketConnected(socket);
						return;
					}

					ClientInfo *client = connectedClients[socket];
			
					this->notifyListeners_dataReceived(client, readBuffer, count);
				}
				else if (count == 0)
				{
					this->debug("Error reading data from client");
					done = true;
					break;
				}
				else{
					if (errno != EAGAIN)
						done = true;
					break;
				}
			}
			
			if (done ||  connectedClients.count(socket)== 0 || !isConnected(connectedClients[socket]))
			{
				clientSocketDisconnected(socket);
			}
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


	#pragma endregion

	#pragma region public functions


	TCPServerLib::TCPServer::TCPServer(int port, bool &startedWithSucess, bool AutomaticallyDeleteClientesAfterDisconnection)
	{
		this->deleteClientesAfterDisconnection = AutomaticallyDeleteClientesAfterDisconnection;
		auto startResult = this->startListen({ TCPServer_PortConf(port) });
		startedWithSucess = startResult.startedPorts.size() > 0;
	}

	TCPServerLib::TCPServer::TCPServer(string unixsocketpath, bool &startedWithSucess, bool AutomaticallyDeleteClientesAfterDisconnection)
	{
		this->deleteClientesAfterDisconnection = AutomaticallyDeleteClientesAfterDisconnection;
		auto startResult = this->startListen({ TCPServer_UnixSocketConf(unixsocketpath) });
		startedWithSucess = startResult.startedPorts.size() > 0;
		
	}

	TCPServerLib::TCPServer::TCPServer(bool autoDeleteClientsAfterDisconnection)
	{
		this->deleteClientesAfterDisconnection = autoDeleteClientsAfterDisconnection;
	}

	TCPServerLib::TCPServer::TCPServer()
	{
		
	}

	TCPServerLib::TCPServer::~TCPServer()
	{
		this->running = false;
		if (sslWasInited)
		{
			usleep(10000); //needs to wati the sockets shutdown
			ssl_stop();
		}
	}

	void TCPServerLib::TCPServer::sendData(ClientInfo *client, char* data, size_t size)
	{

		client->writeMutex.lock();
		connectClientsMutext.lock();

		if (connectedClients.count(client->socket) > 0)
		{
			if (__SocketIsConnected(client->socket))
			{
				if (client->sslTlsEnabled)
					SSL_write(client->cSsl, data, size);
				else
					send(client->socket, data, size, 0);
			}
			else{
				//client is disconnected, but it disconnection was not detected before. If it happens, the lib heave a bug! :D
				//so, make de disconectio nprocess here
				cerr << "Try sendind data to disconnected client." << endl;
				connectClientsMutext.unlock();
				clientSocketDisconnected(client->socket);
			}
		}
		else
			cerr << "Try sendind data to unknown client" << endl;
		connectClientsMutext.unlock();
		
		client->writeMutex.unlock();
	}

	void TCPServerLib::TCPServer::sendString(ClientInfo *client, string data)
	{
		this->sendData(client, (char*)data.c_str(), data.size());
	}

	void TCPServerLib::TCPServer::sendBroadcast(char* data, size_t size, vector<ClientInfo*> *clientList)
	{
		bool clearList = false;
		if (clientList == NULL)
		{
			vector<ClientInfo*> temp;
			connectClientsMutext.lock();
			for (auto &c: this->connectedClients)   
				temp.push_back(c.second);
			connectClientsMutext.unlock();

			clientList = &temp;
			clearList = true;
		}

		for (int c = 0; c < clientList->size(); c++)
			(*clientList)[c]->sendData(data, size);

		if (clearList)
		{
			(*clientList).clear();
			delete clientList;
		}

	}

	void TCPServerLib::TCPServer::sendBroadcast(string data, vector<ClientInfo*> *clientList)
	{
		this->sendBroadcast((char*)(data.c_str()), data.size(), clientList);
	}

	void TCPServerLib::TCPServer::disconnect(ClientInfo *client)
	{
		close(client->socket);
		//the observer are notified in TCPServer::the clientsCheckLoop method
	}

	void TCPServerLib::TCPServer::disconnectAll(vector<ClientInfo*> *clientList)
	{
		bool clearList = false;
		if (clientList == NULL)
		{
			vector<ClientInfo*> temp;
			connectClientsMutext.lock();
			for (auto &c: this->connectedClients)   
				temp.push_back(c.second);
			connectClientsMutext.unlock();
			clientList = &temp;
			clearList = true;
		}

		for (int c = 0; c < clientList->size(); c++)
		{
			this->disconnect((*clientList)[c]);
		}

		if (clearList)
		{
			(*clientList).clear();
			delete clientList;
		}
	}

	bool TCPServerLib::TCPServer::isConnected(ClientInfo *client)
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


	#pragma endregion
#pragma endregion

#pragma region ClientInfo class

	void TCPServerLib::ClientInfo::sendData(char* data, size_t size)
	{
		this->server->sendData(this, data, size);
		
	}

	void TCPServerLib::ClientInfo::sendString(string data)
	{
		this->server->sendString(this, data);
	}

	bool TCPServerLib::ClientInfo::isConnected()
	{
		return this->server->isConnected(this);
	}

	void TCPServerLib::ClientInfo::disconnect()
	{
		this->server->disconnect(this);
	}

	void TCPServerLib::ClientInfo::___notifyListeners_dataReceived(char* data, size_t size, string dataAsStr)
	{
		//notify the events in the TCPServer
		for (auto &c: this->receiveListeners)
		{
			c.second(this, data, size);
		}

		for (auto &c: this->receiveListeners_s)
		{
			c.second(this, dataAsStr);
		}

	}

	void TCPServerLib::ClientInfo::___notifyListeners_connEvent(CONN_EVENT action)
	{
		for (auto &c: this->connEventsListeners)
		{
			c.second(this, action);
		}
	}

	size_t TCPServerLib::ClientInfo::___getReceiveListeners_sSize()
	{
		return this->receiveListeners_s.size();
	}

	

	



#pragma endregion