/*
    Author: Jonathan Weinstein

    This file does not contain tailored
    functions to this assignment, but general
    purpose/generic routines. Examples: millisecond timer,
    a millisecond sleep, and an RNG with local state.
*/
#ifndef H_UTIL
#define H_UTIL

#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
#elif (_POSIX_C_SOURCE) < 199309L
    #error compile with -D_POSIX_C_SOURCE=200809L
#endif

/*
#if !defined(_POSIX_C_SOURCE) || (_POSIX_C_SOURCE) < 199309L
    #error compile with -D_POSIX_C_SOURCE=199309L
#endif
*/

#include <pthread.h>//link with -lpthread
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

inline
void xcreate_pthread_defattr(pthread_t *pres, void* (*func)(void*), void* arg)
{

    if (pthread_create(pres, NULL, func, arg)!=0)
    {
        perror("pthread_create()");
        exit(EXIT_FAILURE);
    }
}

inline
void byte_copy(const void* from, const void* fend, void* to)
{
    memcpy(to, from, (const char *)fend - (const char *)from);//byte distance
}

#define QSORT_TYPED(B,E,F) do{ __typeof(E) _P = (B); qsort(_P, (E)-_P, sizeof(*_P), F); }while(0)

/*
    A small and decent rng is provided here. rand() is not used, as its
    implementation may vary depending on library vendor, and the ability
    to reproduce results across environments is desired.
    Also, rand() making the state global is an unwanted abstraction(and not good for multi-threading).

    ...realized this whole thing is not really reproducible due to threads, locking, and sleeping

    Adapted from following link, with the modification to only return the high 32 bits.
    https://en.wikipedia.org/wiki/Xorshift#xorshift*
    ... "If the generator is modified to only return the high 32 bits, then it passes BigCrush with zero failures.[10]"
*/

typedef struct
{
    uint64_t s;//state
} random32_t;

inline//seed it, state can't be zero
void srandom32(random32_t *prng, uint64_t init)
{
    prng->s = init ? init : 0xf0e1d2c3b4a59687ull;
}

inline
uint32_t random32(random32_t *p)
{
	uint64_t x = p->s;
	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
	p->s = x;
	return (uint32_t)((x * 0x2545F4914F6CDD1D) >> 32);//NB
}

inline
uint32_t random32_xrange(random32_t *p, unsigned ibegin, unsigned iend)
{
    return ibegin + (random32(p) % (iend-ibegin));
}

inline
unsigned long get_millisecs_stamp(void)
{
    struct timespec spec;

    clock_gettime(CLOCK_MONOTONIC, &spec);

    return spec.tv_sec*1000u + spec.tv_nsec/(1000u*1000u);//10e6 millisecs per nanosec
}

inline
void millisleep(unsigned long msecs)
{
    struct timespec a[2];//req, rem

    if (msecs >= 1000u)
    {
        a[0].tv_sec = msecs/1000u;
        a[0].tv_nsec = msecs*1000u;//only by one 1000 because secs consumed
    }
    else
    {
        a[0].tv_sec = 0;
        a[0].tv_nsec = msecs*(1000u*1000u);
    }
    #if 0//untested
    unsigned i=0;
    while (nanosleep(&a[i], &a[i^1])!=0 && errno==EINTR)
        i^=1;
    #else
    nanosleep(a+0, a+1);//const request, remaining (due to thread signal/intr) not a concern for this assignment
    #endif
}

#endif // H_UTIL
