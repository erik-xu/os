#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <string>
#include <iostream>     // cout

using std::string;
using std::cout;
using std::endl;


class Memory {
  public:
    Memory* prev = NULL;
    Memory* next = NULL;
    int n;
    int process;
    int process_page; 

    Memory();
};


#endif
