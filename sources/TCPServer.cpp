

#include "TCPServer.h"

#pragma region SocketHelper class
	void TCPServerLib::SocketHelper::addReceiveListener(function<void(ClientInfo *client, char* data,  size_t size)> onReceive)
	{
		this->receiveListeners.push_back(onReceive);
	}

	void TCPServerLib::SocketHelper::addReceiveListener_s(function<void(ClientInfo *client, string data)> onReceive)
	{
		this->receiveListeners_s.push_back(onReceive);
	}

	void TCPServerLib::SocketHelper::addConEventListener(function<void(ClientInfo *client, CONN_EVENT event)> onConEvent)
	{
		this->connEventsListeners.push_back(onConEvent);
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
				c(client, data, size);
			}

			for (auto &c: this->receiveListeners_s)
			{
				c(client, dataAsString);
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
				c(client, action);
			}

			client->___notifyListeners_connEvent(action);
			
			//IMPORTANT: if disconnected, the 'client' must be destroyed here (or in the function that calls this function);
			if (action == CONN_EVENT::DISCONNECTED)
			{
				connectClientsMutext.lock();
				this->connectedClients.erase(client->socket);
				connectClientsMutext.unlock();
				delete client;
			}
		}

		void TCPServerLib::TCPServer::initialize(vector<int> ports, ThreadPool* tasker)
		{
			this->running = true;
			this->nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;

			if (tasker == NULL)
				tasker = new ThreadPool();

			this->__tasks = tasker;

			int numStarted = 0;

			for (auto &p: ports)
			{
				thread *th = new thread([&](){
					this->waitClients(p, [&](bool sucess){ numStarted++;});
				});

				

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
		}

		void TCPServerLib::TCPServer::waitClients(int port, function<void(bool sucess)> onStartingFinish)
		{
			//create an socket to await for connections

			int listener;

			struct sockaddr_in *serv_addr = new sockaddr_in();
			struct sockaddr_in *cli_addr = new sockaddr_in();
			int status;
			socklen_t clientSize;

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
					//SetSocketBlockingEnabled(listener, false);
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
								//creat ea new client
								ClientInfo client;
								client.socketHandle = theSocket;
								client.server = this;
								client.socket = theSocket;
								connectClientsMutext.lock();
								this->connectedClients[theSocket] = &client;
								connectClientsMutext.unlock();
								this->notifyListeners_connEvent(&client, CONN_EVENT::CONNECTED);
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
				for (size_t c = 0; c < this->connectedClients.size(); c++)
				{
					connectClientsMutext.lock();
					auto currClient = connectedClients.begin();
					std::advance(currClient, c);
				//for (auto &currClient: this->connectedClients)
				//{
					//checks if a reading process is already in progress
					if (!currClient->second->__reading)
					{
						//checks if client is connected
						if (this->__SocketIsConnected(currClient->second->socket))
						{

							int availableBytes = 0;
							ioctl(currClient->second->socket, FIONREAD, &availableBytes);

							if (availableBytes > 0)
							{
								//create a new task in the thread pool (this->__tasks) to read the socket
								currClient->second->__reading = true;

								this->__tasks->enqueue([this](ClientInfo* __currClient){
									this->chatWithClient(__currClient);
									__currClient->__reading = false;
								}, currClient->second);
							}
						}
						else
						{
							//send disconnected notifications
							this->__tasks->enqueue([this](ClientInfo* __currClient){
								this->notifyListeners_connEvent(__currClient, CONN_EVENT::DISCONNECTED);
							}, currClient->second);
						}
					}
					connectClientsMutext.unlock();
				}

				//checks if the current loop must waits.. this block allow to prevent waiting, if needed, outside here
				if (nextLoopWait > 0)
					usleep(nextLoopWait);

				nextLoopWait = _CONF_DEFAULT_LOOP_WAIT;
			}
		}

		void TCPServerLib::TCPServer::chatWithClient(ClientInfo *client)
		{
			int bufferSize = _CONF_READ_BUFFER_SIZE;
			char readBuffer[bufferSize]; //10k buffer
			int totalRead = 0;

			while(true)
			{
				auto readCount = recv(client->socket,readBuffer, bufferSize, 0);
				if (readCount > 0)
				{
					this->notifyListeners_dataReceived(client, readBuffer, readCount);
					totalRead += readCount;
					//limits the reading in a a maximum of 5MB (to prevent thread monopolization in possible - or not? - very long input streams)
					if (totalRead > _CONF_MAX_READ_IN_A_TASK)
					{
						//thre remaing data will be read in another task
						break;	
					}
				}
				else
					break;
			}
		}

		bool TCPServerLib::TCPServer::__SocketIsConnected(int socket)
		{
			char data;
			int readed = recv(socket,&data,1, MSG_PEEK | MSG_DONTWAIT);//read one byte (but not consume this)

			int error_code;
			socklen_t error_code_size = sizeof(error_code);
			getsockopt(socket, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
			//string desc(strerror(error_code));
			return error_code == 0;
		}

	#pragma endregion

	#pragma region public functions


	TCPServerLib::TCPServer::TCPServer(int port, ThreadPool *tasker)
	{
		vector<int> ports = {port};
		this->initialize(ports, tasker);
	}

	TCPServerLib::TCPServer::TCPServer(vector<int> ports, ThreadPool *tasker)
	{
		this->initialize(ports, tasker);
	}

	TCPServerLib::TCPServer::~TCPServer()
	{
		this->running = false;
	}

	void TCPServerLib::TCPServer::sendData(ClientInfo *client, char* data, size_t size)
	{ 
		client->writeMutex.lock();
		
		auto bytesWrite = send(client->socket, data, size, 0);
		
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
			c(this, data, size);
		}

		for (auto &c: this->receiveListeners_s)
		{
			c(this, dataAsStr);
		}

	}

	void TCPServerLib::ClientInfo::___notifyListeners_connEvent(CONN_EVENT action)
	{
		for (auto &c: this->connEventsListeners)
		{
			c(this, action);
		}
	}

	size_t TCPServerLib::ClientInfo::___getReceiveListeners_sSize()
	{
		return this->receiveListeners_s.size();
	}



#pragma endregion