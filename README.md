# About
This is an event based TCP Server that uses epoll to wait connections and data. When data is read from sockets, this data retransmited to you application through an event based interface.

The libraray contains helpful methods to read and write data as c++ strigns.

# Using
To Use this library, you can copy the .cpp and .h files or add this repository as a submodule of your project.

# Where is the main.cpp?
This project is a library, so I don't provide a main file to test it. Instead, I wrote a projet to do unity tests classes and functions.
You can enter in the folder 'tests' and run the command 'make all'. After run this command, a file located in tests/build, named 'tests', will
be generated. This file (tests/build/tests) is a binary file that run some tests. 

Along with this code, there is a ".vscode" folder that contains some configurations for the Visual Studio Code. If you use VSCode, you can compile the tests direclty from the 'Debug' section.

# Using

See some examples of how you can use this library.

```c++
#include <iostream>
#include "libs/TCPServer/sources/TCPServer.h"

using namespace std;
using namespace TCPServerLib;
void startMyServer(){

    //starts the server
    TCPServer server(5000);

    //you can also specify a list of ports to the server listen:
    //  TCPServer server({5000, 5001, 5002, 5003, 5004});



    //add an event (a lambda function) to know when clients connects or
    //disconnects from your server
    server.addConEventListeners([](ClientInfo *client, CONN_EVENT event){
        if (event == CONN_EVENT::CONNECTED)
            cout << "A client was connect in the server" << endl;
        else
            cout << "A client was disconnect in the server" << endl;
    });

    //add an event to be called when some data cames from the client
    server.addReceiveListener([&server](ClientInfo* client, char* data, size_t size){
        cout << "Received from client "<< string(data, size) << endl;

        //send a response to the client
        string msg = "OK, I received your data";
        server.send(client, msg.c_str(), msg.size());

        //you can also use directly the client object to send the response
        client->send(msg.c_str(), msg.size());
    });

    //add an event to be called when some data cames from the client. Is
    //very similiar to addReceiveListener method, but this receives the
    //data as a string
    //
    //OBS: addReceiveListener_s is executed after addReceiveListener (both
    // on the ClientInfo and TCPServer objects)
    server.addReceiveListener_s([](ClientInfo* client, string data){
        cout << "Received from client "<< data << endl;

        //you can also send a response as a string to the client
        server.sendString(client, "OK, I received your data");

        //again: you can also use directly the client object to send the response
        client->sendString(client, "OK, I received your data");

        //you can disconnect your client:
        server.disconnect(client);
    });
}
```

Bellow, you can see an example using directly the ClientInfo object, and user the TCPServer object just to handle the incoming clients

```c++
#include <iostream>
#include "libs/TCPServer/sources/TCPServer.h"

using namespace std;
using namespace TCPServerLib;
void startMyServer(){

    //starts the server
    TCPServer server({5000, 5001});



    //add an event (a lambda function) to know when clients connects or
    //disconnects from your server
    server.addConEventListeners([](ClientInfo *client, CONN_EVENT event){
        if (event == CONN_EVENT::CONNECTED)
        {
            cout << "A client was connect in the server" << endl;
            client->addReceiveListener_s([](ClientInfo* clientp2, string data){
                cout << "Received from client the data: " << data << endl;
                clientp2->sendString("Ok, I received your data");

                //you can disconnect the client
                clientp2->disconnect();
            });
        }
        else
            cout << "A client was disconnect in the server" << endl;
    });
}
```

 
