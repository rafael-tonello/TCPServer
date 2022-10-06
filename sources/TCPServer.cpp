

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

		void TCPServerLib::TCPServer::initialize(vector<int> ports, ThreadPool* tasker, StartResultFunc on_start_done)
		{
			this->running = true;
			this->nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			this->connectedClients.clear();

			if (tasker == NULL)
				tasker = new ThreadPool();

			this->__tasks = tasker;

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


			thread *th2 = new thread([this](){
				this->clientsCheckLoop();
			});

			th2->detach();

			//wait for sockets initialization
			while (numStarted < ports.size())
				usleep(100);

			on_start_done(sucessPorts, errorPorts);
		}

		void TCPServerLib::TCPServer::waitClients(int port, function<void(bool sucess)> onStartingFinish)
		{
			//create an socket to await for connections

			int listener;

			struct sockaddr_in *serv_addr = new sockaddr_in();
			struct sockaddr_in *cli_addr = new sockaddr_in();
			int status;
			socklen_t clientSize;
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

						clientSize = sizeof(cli_addr);
						onStartingFinish(true);

						while (true)
						{
							int theSocket = accept(listener, (struct sockaddr *) cli_addr, &clientSize);

							//int client = accept(listener, 0, 0);

							if (theSocket >= 0)
							{
								int reuse_opt = 1;
								
								//setsockopt(theSocket, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(int));
								//fcntl(theSocket, F_SETFL, O_NONBLOCK);

								//creat ea new client
								ClientInfo *client = new ClientInfo();
								client->socketHandle = theSocket;
								client->server = this;
								client->socket = theSocket;
								ip_str = new char[255];
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
							else{
								usleep(5000);
							}

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

		void TCPServerLib::TCPServer::clientsCheckLoop()
		{
			while (this->running)
			{
				//scrolls the list of clients and checks if there is data to be read
				//a for was used instead a 'foreach' to allow connectedClientsMutex lock() and unlock() and allow modificatiosn in the list
				//during execution.
				//for (size_t c = 0; c < this->connectedClients.size(); c++)
				int64_t max = (int64_t)this->connectedClients.size();
				max = max-1;
				for (int64_t c = max; c >= 0; c--)
				{
					connectClientsMutext.lock();
					auto currClientPair = connectedClients.begin();
					std::advance(currClientPair, c);

					auto currClient = currClientPair->second;


				//for (auto &currClient: this->connectedClients)
				//{
					//checks if client is connected
					if (this->__SocketIsConnected(currClient->socket))
					{

						//checks if a reading process is already in progress
						if (!currClient->__reading)
						{
							int availableBytes = 0;
							ioctl(currClient->socket, FIONREAD, &availableBytes);
							//cout << "busy threads: "<< this->__tasks->getTotalBusyThreads() << ", pending: " << this->__tasks->getTaskCount() << endl;

							if (availableBytes > 0)
							{
								nextLoopWait = 0;
								//create a new task in the thread pool (this->__tasks) to read the socket
								currClient->__reading = true;

								this->__tasks->enqueue([this](ClientInfo* __currClient, int __availableBytes){
									this->chatWithClient(__currClient, __availableBytes);
									__currClient->__reading = false;
								}, currClient, availableBytes);

							}
						}
					}
					else
					{
						//send disconnected notifications
						currClient->__reading = false;
						this->connectedClients.erase(currClient->socket);
						this->__tasks->enqueue([this](ClientInfo* __currClient){
							this->notifyListeners_connEvent(__currClient, CONN_EVENT::DISCONNECTED);
							{
								usleep(10000);
								delete __currClient;
							}
						}, currClient);
					}
					connectClientsMutext.unlock();
				}

				//checks if the current loop must waits.. this block allow to prevent waiting, if needed, outside here

				if (nextLoopWait > 0)
					usleep(nextLoopWait);

				nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			}
		}

		void TCPServerLib::TCPServer::chatWithClient(ClientInfo *client, int ammountToRead)
		{
			int bufferSize = _CONF_READ_BUFFER_SIZE;
			char readBuffer[bufferSize]; //10k buffer

			ammountToRead = ammountToRead > bufferSize ? bufferSize : ammountToRead;

			auto readCount = recv(client->socket,readBuffer, ammountToRead, 0);
			if (readCount > 0)
			{
				this->notifyListeners_dataReceived(client, readBuffer, readCount);
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


	TCPServerLib::TCPServer::TCPServer(int port, bool &startedWithSucess, ThreadPool *tasker)
	{
		vector<int> ports = {port};
		this->initialize(ports, tasker, [&](vector<int> sucess, vector<int> failure){
			startedWithSucess = sucess.size() == 0;
		});
	}

	TCPServerLib::TCPServer::TCPServer(vector<int> ports, ThreadPool *tasker, StartResultFunc on_start_done)
	{
		this->initialize(ports, tasker, on_start_done);
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
		{
			this->__tasks->enqueue([data, size](ClientInfo* p){
				p->sendData(data, size);
			
			}, (*clientList)[c]);
		}

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