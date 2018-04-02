#ifndef PROCESS_H
#define PROCESS_H

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <string>
#include <iostream>     // cout

using std::string;
using std::cout;
using std::endl;

/* initialize random seed: */
//srand (time(NULL));

class Process {
  public: 
    string process_name;
    int process_size;
    int arrival_time;
    int service_time;
    Process *prev;
    Process *next;

    Process();
    void gen_process_size();
    void gen_service_time();
    
};


#endif
