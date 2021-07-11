#include <iostream>
#include<vector>
#include<string>
#include <map>
#include <functional>

#include "tester.h"

#include "./TCPServer.test.h"

using namespace std;
int main(int argc, char* argv[]){
    cout << "Test results:" << endl;
    vector<string> args;
    for (int c = 0; c < argc; c++) args.push_back(string(argv[c]));

    //create testers instances4
    vector<Tester*> testers;

    //***** testers instances
    //***** make your changes only here
        testers.push_back(new TCPServerTester());
    //*****

    //raises the contexts of the testers and gruoup them by specifcs contexts
    map<string, vector<Tester*>> contexts;// = {"all", {}};

    for (auto &c: testers)
    {
        
        auto testerContexts = c->getContexts();
        for (auto &c2: testerContexts)
        {
            if (contexts.count(c2) == 0)
                contexts[c2] = {};
            
            contexts[c2].push_back(c);
        }

        //sets ta special tag in the tester indicating that it is not runned yet. This tag will be used to prevent call more one time a tester.
        //c->setTag("main.tested", "false");
    }

    //identify required contexts (or all if no one is informed)
    //calls the testers according to the required contexts

    //checks if user needs help
    if (args.size() > 1 &&  args[1].find(string("-h")) != string::npos)
    {
        //shows the available context and an example of usage

        cout << "This is a test program. It is make to run tests in the code of another program. You can pass some contexts to run a limited test or use the "
                << "special context 'all' to run all tests." << endl
                << "Avaibale contexts are: "<< endl
                << "    all" << endl;
                for (auto &c: contexts)
                    cout << "    " << c.first << endl;


        return 0;
    }

    function<void(string context)> runContext = [&](string context){
        if (contexts.count(context) > 0)
        {
            for (auto &c: contexts[context])
                c->run(context);
        }
        else
            cerr << "        Context '" << context << "' has not found" << endl;

    };

    if (args.size() == 1 || args.size() == 2 && args[1] == "all")
        for (auto &c : contexts)
        {
            cout << "    Context " << c.first << ":" << endl;
            runContext(c.first);
        }

    else
        for (int c =1; c < args.size(); c++)
        {
            cout << "    Context " << args[c] << ":" << endl;
            runContext(args[c]);
        }

    cout << endl << "Final statistics: " << Tester::testsPassed << " passed and " << Tester::testsFailed << " failed." << endl;;


    return Tester::testsFailed;
}