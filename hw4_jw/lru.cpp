#include "lru.h"

/*

*/

//std::list::size is constant time in C++11
/*
phys framenums      : 0   1   2   3
list node seq       : 2   0   3   1
data in node[seq]   : 1   3   0   2
*/

/*ctor*/ LruPolicy::LruPolicy()
{
    for (Iter& rf : accessIters)//note ref
        rf = ls.end();//a nullptr wrapper or some fixed sentinel node
}

#include <stdio.h>

void LruPolicy::print()
{
    puts("Most recent to least recent frames used:");
    for (pageno_t const x : ls)
        printf("%3u", x);
    putchar('\n');
}

void LruPolicy::UseFreeFrame(pageno_t frameno)
{
    if (accessIters[frameno]==ls.end())
    {
        assert(ls.size()<TotalFrames);

        ls.push_front(frameno);
        accessIters[frameno] = ls.begin();
    }
    else
    {
        assert(ls.size()==TotalFrames);
        UpdateAfterHit(frameno);
    }
}

pageno_t LruPolicy::PickFrameToEvict()
{
    //evict back of list, lru
    //then it becomes the most recently used
    assert(ls.size() == TotalFrames);
    assert(gFreeFrames.empty());

    int const backval = ls.back();
    ls.pop_back();

    ls.push_front(backval);
    accessIters[backval] = ls.begin();

    return backval;
}

void LruPolicy::UpdateAfterHit(pageno_t frameno)
{
    ls.erase(accessIters[frameno]);//O(1): array access for nodeptr and dlist unlink
    ls.push_front(frameno);
    accessIters[frameno] = ls.begin();
}
