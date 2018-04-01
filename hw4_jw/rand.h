#ifndef H_RAND
#define H_RAND

#include "util.h"
#include <random>

class RandPolicy : public PagePolicy
{
    std::minstd_rand rng;
public:
    /*ctor*/ RandPolicy() : rng(0xabcdefu) { }

    void UpdateAfterHit(pageno_t)
    {
        //nothing
    }

    void UseFreeFrame(pageno_t)
    {

    }

    pageno_t PickFrameToEvict()
    {
        assert(gFreeFrames.empty());
        assert(! gFreeDisks.empty());

        return (rng() % TotalFrames);
    }
};

#endif // H_RAND
