/*
    NOTE: this is fucked
*/
#ifndef H_LRU
#define H_LRU

#include "util.h"

#include <list>//a doubly-linked list that allocates. used for lru

class LruPolicy : public PagePolicy
{
    using ListType = std::list<pageno_t>;
    using Iter = ListType::iterator;

    Iter accessIters[TotalFrames];//indexed by physical frame numbers
    std::list<pageno_t> ls;//front is more recent then back
public:
    LruPolicy();//ctor
    void print();
    void UseFreeFrame(pageno_t frameno);
    pageno_t PickFrameToEvict();
    void UpdateAfterHit(pageno_t);
};


#endif // H_LRU
