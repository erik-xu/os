/*
    Author: Jonathan Weinstein

    to build:
    * place sellers.c and util.h in same initially empty directory
    * issue:
        gcc -Wall -Wextra -std=c99 -O2 -s sellers.c -lpthread -o sellers.out
      (alternatively, you may issue "make" and the provided makefile will do it)

    to run:
        ./sellers.out [N customers] [rng seed]

    please make your terminal about 120 characters wide
*/
#include "util.h"

#define DEFAULT_SEED UINT64_C(0xdeadbeefcafe2357)
#define TIME_UNIT_MS 10

//All indexing and id's start at 0.

typedef struct
{
    int arrivalTime;//ms since broadcast

    short serviceBeginTime;//ms since broadcast
    short eventOrd;

    short serviceDuration;//ms
    short seatIdx;

    short sellerTid;
    short custIdent;
} TransactInfo;

//M rows of width(columns) N. [i][j] -> i*N + j
TransactInfo ginfo[100];//zeroed init
#define isSlotEmpty(x) ((x).serviceDuration == 0)//all should have a time longer than, High seller has min 1 "min" (10 ms)

//how many each seller served after simulation ends, may be less than num custs per seller if ncps>10(strict)
int gNumServed[10];
int bcastCounter = 10;//to make sure broadcast is called only when all other threads are in cond_wait()

typedef struct
{
    int arrival;
    int burst;
} InitData;

static
int initDataCompare(const void* a, const void* b)
{
    //no overflow
    return ((const InitData *)a)->arrival - ((const InitData *)b)->arrival;
}

uint64_t seedval = DEFAULT_SEED;
int ncps=7;//same num customers per each seller

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static unsigned gTimeOfBroadcastMs;//set once in main, then const

/*
    A single lock controls all 6 of these.
    "cs" prefix is for critical section
    A thread may not have to touch all of these in
    one pass into and out of the critical section.
*/
int csMidAlternate = 0;
int csEventNo = 0;//used to generate sequence numbers of reservations

int csA = 0;
int csB = 49;
int csC = 50;
int csD = 99;
/*
A->
    <-B
C->
    <-D
*/
typedef int (*fp_sell_t)(void);
//For all these sell_* functions, thread has mutex before hand.
//things that go away from mid, toward front or back
//can return -1: all seats full
int sell_mid_to_front(void)
{
    if (csB >= csA)
        return csB--;
    else
        return -1;//sold out
}

int sell_back_to_mid(void)
{
    if (csD >= csC)
        return csD--;
    else
        return sell_mid_to_front();
}

int sell_mid_to_back(void)
{
    if (csC <= csD)
        return csC++;
    else
        return -1;//sold out
}

int sell_front_to_mid(void)
{
    if (csA <= csB)
        return csA++;
    else
        return sell_mid_to_back();
}
//mid sellers alternate 50,49,51,48,52...
int sell_mid_alternate(void)
{
    if ((csMidAlternate ^= 1) != 0)
    {
        int const res = sell_mid_to_back();
        return res > -1 ? res : sell_mid_to_front();
    }
    else
    {
        int const res = sell_mid_to_front();
        return res > -1 ? res : sell_mid_to_back();
    }
}

typedef struct
{
    fp_sell_t fpSell;
    short minBurstMs;
    short maxBurstMs;
} ThreadParams;

ThreadParams getParams(int tid)//compile with -std=c99
{
    if (tid == 0)       return (ThreadParams){sell_front_to_mid,  1u*TIME_UNIT_MS, 2u*TIME_UNIT_MS};
    else if (tid < 4)   return (ThreadParams){sell_mid_alternate, 2u*TIME_UNIT_MS, 4u*TIME_UNIT_MS};
    else                return (ThreadParams){sell_back_to_mid,   4u*TIME_UNIT_MS, 7u*TIME_UNIT_MS};
}

void* thread_func(void *ctx)
{
    int const tid = (int)(uintptr_t)ctx;
    ThreadParams const params = getParams(tid);
    const int ntotal = ncps;
    InitData custs[99];//NMAX
    //initialize customer arrivals and time to service:
    {
        random32_t rng;
        srandom32(&rng, seedval*tid + tid);//I dunno... as an aside PCG random has concept of multiple streams

        const int ntotal = ncps;
        InitData custs[99];//NMAX

        for (int i=0; i<ntotal; ++i)
            custs[i].arrival = random32_xrange(&rng, 0, 60*TIME_UNIT_MS),//arrive within the 60 "minutes"
            custs[i].burst = random32_xrange(&rng, params.minBurstMs, params.maxBurstMs+1);

        QSORT_TYPED(custs, custs+ntotal, initDataCompare);//sort ascending arrival
    }

    pthread_mutex_lock(&mutex);
    if (--bcastCounter == 0)//memory dec guarded by above mutex
    {
        printf("9 of 10 threads waiting on cv, mutex owner thread %u will broadcast\n", tid);
        gTimeOfBroadcastMs = get_millisecs_stamp();
        pthread_cond_broadcast(&cond);
    }
    else
    {
        printf("Thread %u waiting on cv\n", tid);
        pthread_cond_wait(&cond, &mutex);
    }
    //mtx now acquired
    printf("Thread %u passed wait\n", tid);
    //for (int i=0; i<ntotal; ++i) printf("{%d, %d} ", custs[i].arrival, custs[i].burst); putchar('\n');
    pthread_mutex_unlock(&mutex);//do this now in case cust not arrived yet, dont sleep holding lock!

    int nsold=0;
    int q = get_millisecs_stamp() - gTimeOfBroadcastMs;
    //q is global elapsed ms since broadcast
    for (;;)
    {
        int const arriv = custs[nsold].arrival;
        int const brst = custs[nsold].burst;

        if (q < arriv)
            millisleep(arriv - q);
        //only a few integer loads, stores, and comparisons take place in the critical section
        pthread_mutex_lock(&mutex);//ENTER critical section
        unsigned const i = params.fpSell();//reserve seat position
        int const eventOrder = csEventNo++;
        pthread_mutex_unlock(&mutex);//LEAVE critical section

        if (i >= 100u)//concert full
            break;
        q = get_millisecs_stamp() - gTimeOfBroadcastMs;
        //index i is unique
        ginfo[i].arrivalTime = arriv;
        ginfo[i].serviceBeginTime = q;
        ginfo[i].custIdent = nsold;
        ginfo[i].sellerTid = tid;
        ginfo[i].serviceDuration = brst;
        ginfo[i].eventOrd = eventOrder;
        ginfo[i].seatIdx = i;//duh, but will eventually sort the thing to print events in a list
        //do "work" to make this seam less silly. some kind of i/o...
        millisleep(brst);
        q += brst;
        if (++nsold == ntotal)
            break;
    }

    gNumServed[tid] = nsold;
    printf("Thread %u exiting\n", tid);
    return NULL;
}

static void fprint64x(uint64_t val);
static void printSeatChart(void);
static void printEventList(void);//this does an inplace sort, destroying matrix fmt

//printf("Usage: %s [N customers] [rng seed]\n", argv[0])
int main(int argc, char **argv)
{
    if (argc>1)
    {
        if (argc<=3)
        {
            ncps = atoi(argv[1]);
            if (ncps<=0 || ncps>=100)
            {
                puts("N must be in [1 to 99]");
                return 0;
            }

            if (argc==3)
            {
                seedval = strtoull(argv[2], NULL, 0);//0: determine base, eg 0x prefix
                if (!seedval)
                {
                    puts("invalid seed, using default");
                    seedval = DEFAULT_SEED;
                }
            }
        }
        else
        {
            printf("Usage: %s [N customers] [rng seed]\n", argv[0]);
            return 0;
        }
    }

    printf("n: customers per seller = %u\nseed = 0x", ncps);
    fprint64x(seedval);
    puts("\n");


    pthread_t addthreads[9];//main is hi seller, so only 9 additional. 10 in total with main

    for (unsigned tid = 1; tid<10; ++tid)//maybe each thread should have diff thread func...
        xcreate_pthread_defattr(addthreads+tid-1, thread_func, (void *)(uintptr_t)tid);

    thread_func((void *)0);//run hi seller on main, make use of it

    for (int i=0; i<9; ++i)
        pthread_join(addthreads[i], NULL);

    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);

    puts("\noutput...\n");

    printSeatChart();//do before if no copy data
    printEventList();

    return 0;
}

static
int evCompare(const void* a, const void* b)
{
    return ((const TransactInfo *)a)->eventOrd - ((const TransactInfo *)b)->eventOrd;
}

static
TransactInfo* removeEmpty(TransactInfo* it, TransactInfo*const end)
{
    TransactInfo *w = it;//write pos
    for ( ; it!=end; ++it)
    {
        if (!isSlotEmpty(*it))
            *w++ = *it;
    }
    return w;
}

static unsigned tidToSellerChars(unsigned tid)
{
    if (tid == 0)       return 'H' | (unsigned)'A'<<8;
    else if (tid < 4)   return 'M' | (unsigned)('A'-1u+tid) << 8;
    else                return 'L' | (unsigned)('A'-4u+tid) << 8;
}

static void printEventList(void)//this does an inplace sort, destroying matrix fmt
{
    TransactInfo *const end = removeEmpty(ginfo+0, ginfo+100);
    QSORT_TYPED(ginfo+0, end, evCompare);//destroyed, copy before if need do other things
    int const nSoldAll = end - ginfo;

    //....0         1         2         3         4         5         6         7         8         9
    //....0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0 2 4 6 8 0
    puts("reservation seq no | seat no | seller id | customer id | cust arrival | reservation begin | service duration");
    #define FMT_STR\
         "%18u |"          "%8u |"  "  %2u %c %c   |" "%12u |"   "%13u |"       "%18u |"              "%4u\n"
    for (int i=0; i!=nSoldAll; ++i)
    {
        TransactInfo const x = ginfo[i];
        unsigned const cc = tidToSellerChars(x.sellerTid);

        printf(FMT_STR, i, x.seatIdx, x.sellerTid, (char)cc, (char)(cc>>8), x.custIdent,
                        x.arrivalTime, x.serviceBeginTime, x.serviceDuration);
    }
    #undef FMT_STR

    if (nSoldAll == 100) puts("All 100 seats were filled");
    else                 printf("Not enough buyers to sell out, %d seats unfilled\n", 100-nSoldAll);

    if (ncps > 10)//strict
    {
        unsigned cnt = 0u;
        for (int i=0; i!=10; ++i) cnt+=gNumServed[i];
        printf("More customers than seats, %u customers turned away after selling out:\n", cnt);
        for (int i=0; i!=10; ++i)
            printf(" thread %c turned away %2d\n", i+'0', ncps-gNumServed[i]);
    }
    else
        puts("No customers turned away");
}
//"21MB01:555" means: reservation 21 done by Mid seller B to his customer of id 1 at time-stamp 555
static void printSeatChart(void)
{
    enum{Rows = 10, Width = 10};
    const TransactInfo *prow = ginfo, *const end = ginfo + Rows*Width;
    for (; prow!=end; prow+=Width)
    {
        int j=0;
        for (;;)
        {
            TransactInfo const x = prow[j];
            if (!isSlotEmpty(x))
            {
                unsigned const cc = tidToSellerChars(x.sellerTid);
                printf("%02d%c%c%02d:%03d", x.eventOrd, (char)cc, (char)(cc>>8), x.custIdent, x.serviceBeginTime);
            }
            else
                fputs("...none...", stdout);
            if (++j == Width)
                break;
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

static//this is not interesting to assignment
void fprint64x(uint64_t val)
{
    char buf[23], *p=buf;//23, 16 hex digits, 8 bytes -> 7 seps
    int hpos=15;
    unsigned hval;
    for (;;)
    {
        hval = (val >> (hpos-- * 4u)) & 15;
        *p++ = hval + (hval<10u ? '0' : 'a'-10);

        hval = (val >> (hpos-- * 4u)) & 15;
        *p++ = hval + (hval<10u ? '0' : 'a'-10);
        if (hpos<0)
            break;
        *p++ = ':';
    }
    fwrite(buf, 1, 23, stdout);
}

extern inline//use default attrs, and exit if fails
void xcreate_pthread_defattr(pthread_t *pres, void* func(void*), void* arg);
extern inline//seed it, state can't be zero
void srandom32(random32_t *prng, uint64_t init);
extern inline
uint32_t random32(random32_t *p);
extern inline
unsigned long get_millisecs_stamp(void);
extern inline
void millisleep(unsigned long msecs);
extern inline
uint32_t random32_xrange(random32_t *p, unsigned ibegin, unsigned iend);
extern inline
void byte_copy(const void* from, const void* fend, void* to);
