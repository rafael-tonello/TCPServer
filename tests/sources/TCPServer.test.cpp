#include "PHOMAU.test.h"

vector<string> PhomauTester::getContexts()
{
    return {"APIs.PHOMAU"};

}

void PhomauTester::run(string context)
{
    if (context == "APIs.PHOMAU")
    {

        this->testeWriteFunction();

        this->testeProcessPackFunction();

        //test port ans socket connection
        //send a pack throught the port
        this->testTCPEndPoint();


    }
}

void PhomauTester::testeProcessPackFunction()
{
    this->yellowMessage("PROCESS_PACK function tests");
    //process pack function tests
    this->test("PROCESS_PACK_(SET_VAR, 'var', 'value', 5) should result in a var set in the controller", [&](){
        
        this->setTag("setVar.lastCall.name", "");
        this->setTag("setVar.lastCall.value", "");
        
        SocketInfo s;
        s.socket = 0;
        this->ph->__PROCESS_PACK(PHOMAU_ACTIONS::SET_VAR, "var", "value", 5, s);

        string received = this->getTag("setVar.lastCall.name") + "=" + this->getTag("setVar.lastCall.value");

        return TestResult{
            received == "var=value",
            "var=value",
            received
        };
    });

    this->test("PROCESS_PACK_(GET_VAR var) should result in a var get in the controller", [&](){
        //prepare the pack
        this->setVar("getVar.lastCall.name", "");
        this->setVar("getVar.lastCall.value", "");

        Tester::global_test_result = "";
        
        SocketInfo s;
        s.socket = 0;
        string expected = "vName:'var', defValue:'', result: 'gvr:var=value'";
        this->ph->__PROCESS_PACK(PHOMAU_ACTIONS::GET_VAR, "var", "", 0, s);

        //"2 2 17 0 0 0 0 0 0 17 22 v a r = s a m p l e  v a l u e 0 0"
        string received = "vName:'"+this->getTag("getVar.lastCall.name") + "', defValue:'" + getTag("getVar.lastCall.value") + "', result: '"+Tester::global_test_result+"'";

        return TestResult{
            received == expected,
            expected,
            received
        };
    });
}


//this is an integration test
void PhomauTester::testTCPEndPoint()
{
    this->yellowMessage("General tests in the TCP Server");
    this->test("Connection to PHOMAU Server should result in a valid socket (>0)",[&](){
        clientSocket = this->connectToPHOMAU().get();
        return TestResult {
            clientSocket > 0,
            ">0",
            to_string(clientSocket)
        };
    });

    this->test("Send a setVar to server with 'tcpEndpointTestVar=tcpEndpointTestValue", [&](){
        //parpare the data to be analyzed
        this->setTag("setVar.lastCall.name", "");
        this->setTag("setVar.lastCall.value", "");
        //prepare the buffer to be sent

        string bytes = PHOMAU_ACTIONS::SET_VAR + ":tcpEndpointTestVar=tcpEndpointTestValue\n";
        //write the buffer to the socket
        send(clientSocket, bytes.c_str(), bytes.size(), 0);
        //wait the response
        usleep(1000000);

        //analyse the results
        string expected = "tcpEndpointTestVar=tcpEndpointTestValue";
        string received = this->getTag("setVar.lastCall.name") + "=" + this->getTag("setVar.lastCall.value");

        return TestResult{
            expected == received,
            expected,
            received
        };
    

    });

    this->test("Send a setVar to server with 'tcpEndpointTestVar=tcpEndpoint\\nTestValue", [&](){
        //parpare the data to be analyzed
        this->setTag("setVar.lastCall.name", "");
        this->setTag("setVar.lastCall.value", "");
        //prepare the buffer to be sent

        string bytes = PHOMAU_ACTIONS::SET_VAR + ":tcpEndpointTestVar=tcpEndpoint\\nTestValue\n";
        //write the buffer to the socket
        send(clientSocket, bytes.c_str(), bytes.size(), 0);
        //wait the response
        usleep(1000000);

        //analyse the results
        string expected = "tcpEndpointTestVar=tcpEndpoint\nTestValue";
        string received = this->getTag("setVar.lastCall.name") + "=" + this->getTag("setVar.lastCall.value");

        return TestResult{
            expected == received,
            expected,
            received
        };
    

    });
}

void PhomauTester::testeWriteFunction()
{
    this->yellowMessage("PROTOCOL_PHOMAU_WRITE function tests");
    //write pack function
    this->test("PHOMAU::PROTOCOL_PHOMAU_WRITE(1, VAR_CHANGED, 'var=value', 9)", [&](){
        //need observate function output through the Tester::msgBus system
        //string r = Tester::msgBusWaitNext("API::PHOMAU::__PROTOCOL_PHOMAU_WRITE output signal", [&](){
            //interactc with function __PROTOCOL_PHOMAU_WRIET

        //});

        global_test_result = "";
        //interactc with function __PROTOCOL_PHOMAU_WRIET
        SocketInfo si;
        si.socket = 0;
        this->ph->__PROTOCOL_PHOMAU_WRITE(si, PHOMAU_ACTIONS::VAR_CHANGED, "var=value", 9);


        return TestResult{
            Tester::global_test_result == "vc:var=value",
            "vc:var=value",
            Tester::global_test_result
        };


        //compare the result data
    });
}

future<int> PhomauTester::connectToPHOMAU()
{
    return th.enqueue([&](){
        int sock = 0, valread;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            printf("\n Socket creation error \n");
            return -1;
        }
    
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(5100);
        
        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) 
        {
            printf("\nInvalid address/ Address not supported \n");
            return -1;
        }
    
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            printf("\nConnection Failed \n");
            return -1;
        }

        return sock;
    });
}

string PhomauTester::convertStringToByteList(string s, size_t i){
    string r;
    if (i == 0)
        i = s.size();

    for (size_t a = 0; a < i; a++)
        r += to_string((uint)(char)s[a]) +" ";
    return r;
}

/*ApiMediatorInterface*/
future<void> PhomauTester::createAlias(string name, string dest)
{

}


future<string> PhomauTester::getAliasValue(string aliasName)
{
    
}
future<void> PhomauTester::deleteAlias(string aliasName)
{
    
}

string PhomauTester::observeVar(string varName, observerCallback callback, void* args, string observerId)
{

}

void PhomauTester::stopObservingVar(string observerId)
{

}

future<vector<tuple<string, DynamicVar>>> PhomauTester::getVar(string name, DynamicVar defaultValue)
{
    this->setTag("getVar.lastCall.name", name);
    this->setTag("getVar.lastCall.value", defaultValue.getString());

    vector<tuple<string, DynamicVar>> ret;
    ret.push_back({name, DynamicVar(this->getTag("vars."+name, "var-not-found"))});
    this->setTag("getVar.lastCall.result", get<0>(ret[0]) + "=" + get<1>(ret[0]).getString());

    promise<vector<tuple<string, DynamicVar>>> prom;
    prom.set_value(ret);
    return prom.get_future(); 
}

future<void> PhomauTester::setVar(string name, DynamicVar value)
{
    this->setTag("vars."+name, value.getString());

    this->setTag("setVar.lastCall.name", name);
    this->setTag("setVar.lastCall.value", value.getString());
    

    

    promise<void> prom;
    prom.set_value();
    return prom.get_future();
}

future<void> PhomauTester::delVar(string varname)
{

}

future<vector<string>> PhomauTester::getChildsOfVar(string parentName)
{

}
