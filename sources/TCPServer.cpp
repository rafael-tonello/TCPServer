

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

		void TCPServerLib::TCPServer::initialize(vector<int> ports, StartResultFunc on_start_done)
		{
			this->running = true;
			this->nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			this->connectedClients.clear();

			atomic<int> numStarted;
			numStarted = 0;
			vector<int> sucessPorts;
			vector<int> errorPorts;

			for (auto &p: ports)
			{
				thread *th = new thread([&](int _p){
					this->waitClients(_p, [&](bool sucess)
					{ 
						numStarted++;
						if (sucess)
							sucessPorts.push_back(_p);
						else
							errorPorts.push_back(_p);

					});
				}, p);

				th->detach();
				
				this->listenThreads.push_back(th);
			}


			
			//wait for sockets initialization
			while (numStarted < ports.size())
				usleep(100);

			on_start_done(sucessPorts, errorPorts);
		}

		void TCPServerLib::TCPServer::waitClients(int port, function<void(bool sucess)> onStartingFinish)
		{
			//create an socket to await for connections

			int listener, efd, epoll_ctl_result, foundEvents;
			int MAXEVENTS = 128;

			struct sockaddr_in *serv_addr = new sockaddr_in();
			struct sockaddr_in *cli_addr = new sockaddr_in();
			struct epoll_event event;
  			struct epoll_event *events;
			int status;
			socklen_t clientSize = sizeof(cli_addr);;
			char *ip_str;


			listener = socket(AF_INET, SOCK_STREAM, 0);

			if (listener >= 0)
			{
				int reuse = 1;
				if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
				this->debug("setsockopt(SO_REUSEADDR) failed");
				//fill(std::begin(serv_addr), std::end(serv_addr), T{});
				//bzero((char *) &serv_addr, sizeof(serv_addr));
				//setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *) &iOptVal, &iOptLen);

				serv_addr->sin_family = AF_INET;
				serv_addr->sin_addr.s_addr = INADDR_ANY;
				serv_addr->sin_port = htons(port);

				usleep(1000);
				status = bind(listener, (struct sockaddr *) serv_addr, sizeof(*serv_addr));
				usleep(1000);
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
									epoll_ctl_result = epoll_ctl (efd, EPOLL_CTL_ADD, listener, &event);
											
											
									foundEvents = epoll_wait (efd, events, MAXEVENTS, -1);

									
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
															//int client = accept(listener, 0, 0);
												if (theSocket >= 0)
												{
													SetSocketBlockingEnabled(theSocket, false);
													event.data.fd = theSocket;
													event.events = EPOLLIN | EPOLLET;
													auto tmpResult = epoll_ctl (efd, EPOLL_CTL_ADD, theSocket, &event);
													if (tmpResult != -1)
														clientSocketConnected(theSocket, cli_addr);
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

											readDataFromClient(events[i].data.fd);

										}
									}
								}

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

				//n = read(newsockfd,buffer,255);
				//n = write(newsockfd,"I got your message",18);
		}

		void TCPServerLib::TCPServer::clientSocketConnected(int theSocket, struct sockaddr_in *cli_addr)
		{
			//int reuse_opt = 1;
								
			//setsockopt(theSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(int));
			//fcntl(theSocket, F_SETFL, O_NONBLOCK);
			
			//creat ea new client
			ClientInfo *client = new ClientInfo();
			client->socketHandle = theSocket;
			client->server = this;
			client->socket = theSocket;
			char *ip_str = new char[255];
			inet_ntop(AF_INET, &cli_addr->sin_addr, ip_str, 255);
			client->address = string(ip_str);
			delete[] ip_str;
			client->port = ntohs(cli_addr->sin_port);
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
				//this->debug("Detect a disconnection of a not connected client !!!!!!!!!!!!!!!!!!!");
				//clientSocketConnected(theSocket);
				return;
			}

			ClientInfo *client = connectedClients[theSocket];

			this->notifyListeners_connEvent(client, CONN_EVENT::DISCONNECTED);

			connectClientsMutext.lock();
			this->connectedClients.erase(theSocket);
			connectClientsMutext.unlock();

			delete client;
		}


		void TCPServerLib::TCPServer::readDataFromClient(int socket)
		{
			int bufferSize = _CONF_READ_BUFFER_SIZE;
			char readBuffer[bufferSize]; //10k buffer

			bool done = false;
			ssize_t count;


			while (true)
			{

				count = read (socket, readBuffer, sizeof readBuffer);
				//count = recv(socket,readBuffer, sizeof readBuffer, 0);
				if (count > 0)
				{
						
					if (connectedClients.count(socket) == 0)
					{
						this->debug("reading data from a not connect client!!!!!!!!!!!!!!!!!!!");
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
					{
						done = true;
					}
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

	#pragma endregion

	#pragma region public functions


	TCPServerLib::TCPServer::TCPServer(int port, bool &startedWithSucess)
	{
		vector<int> ports = {port};
		this->initialize(ports, [&](vector<int> sucess, vector<int> failure){
			startedWithSucess = sucess.size() > 0;
		});
	}

	TCPServerLib::TCPServer::TCPServer(vector<int> ports, StartResultFunc on_start_done)
	{
		this->initialize(ports, on_start_done);
	}

	TCPServerLib::TCPServer::~TCPServer()
	{
		this->running = false;
	}

	void TCPServerLib::TCPServer::sendData(ClientInfo *client, char* data, size_t size)
	{ 
		client->writeMutex.lock();
		connectClientsMutext.lock();

		if (client->socket == 21)
		{
			int b = 10;
			int c = b;
		}

		if (connectedClients.count(client->socket) > 0)
			if (__SocketIsConnected(client->socket))
			{
				auto bytesWrite = send(client->socket, data, size, 0);
			}
			else
				cout << "Try sendind data to disconnected client" << endl;
		else
			cout << "Try sendind data to unknown client" << endl;
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