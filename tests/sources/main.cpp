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

    Tester::runTests(testers, argc, argv);


    while (true)
        usleep (100000);
}