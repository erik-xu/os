#ifndef H_UTIL
#define H_UTIL

#include <stdint.h>
#include <assert.h>

enum
{
    TotalFrames = 8,
    TotalDiskPages = 32,
    MaxVirtualPagesPerProcess = 32,
    SimMaxProcBurst = 8,
    SimulationDuration = 100,
    SimulationProcesses = 10
};

using pageno_t = int32_t;//twos complement signed 32-bit integer

inline
bool InRam(pageno_t physno)
{
    return physno>=0;
}

inline
pageno_t DiskIndex(pageno_t physno)//phys <-> frame
{
    return ~physno;
}

struct FreeStack
{
    unsigned cnt;
    pageno_t *const p;
    unsigned const cap;
    constexpr FreeStack(pageno_t *mem, unsigned n)
    : cnt(0u)
    , p(mem)
    , cap(n)
    { }

    void Push(pageno_t newdat)
    {
        p[cnt++] = newdat;
    }

    pageno_t Pop()
    {
        return p[--cnt];
    }

    bool empty()const{return cnt==0u; }
    bool full()const{return cnt==cap; }
};

template<int N>
struct FreeStackN : FreeStack
{
    pageno_t a[N];

    constexpr FreeStackN() : FreeStack(a, N) { }
};

extern FreeStackN<TotalFrames> gFreeFrames;
extern FreeStackN<TotalDiskPages> gFreeDisks;

enum:pageno_t{ NotMapped = -(1 << 20) };


/*
    At the minimum there are 3 abstract member functions to implement
    You may want to add a constructor/destructor or other helper functions
*/
class PagePolicy
{
public:
    PagePolicy& operator=(const PagePolicy&) = delete;//disable copying

    virtual
    /*dtor*/ ~PagePolicy() { };

    virtual
    void UpdateAfterHit(pageno_t frameno) = 0; //abstract

    virtual
    void UseFreeFrame(pageno_t frameno) = 0;//abstract

    virtual
    pageno_t PickFrameToEvict() = 0;//abstract

    virtual
    void print(){}//optional
};

#endif // H_UTIL
