/*
    I'm not doing the min 4 frames before new proc,
    but rather min 1 frame

    For you java enthusiasts, note this if you didn't already:

        In java, can't have a flat structure/class by value (need to "new it up"),
        so all java "references" are like C pointers to structures.
        The java '.' operator is field a dereference,
        which is C's -> operator

    Hit CTRL+F for the "algos" array, currently line 91 in this file,
    Thats how you guys are going to hook in the other policies.
    I did LRU and random already, see them for an example of the
    3 functions you need to implement.

    You dont have to mess with the 2 gFree stacks,
    I made them global b/c lazy and I put asserts in multiple files
*/
#include "util.h"

#include "fifo.cpp"
#include "rand.h"
#include "lru.h"

#include <stdio.h>
#include <stdlib.h>

struct Process// : SNode
{
    int id;
    int remainingTime;
    int lastVirtualRefNo;//init to zero
    int nvpages;

    //maps process-local virtual pageno to system-global physical pageno
    //nonnegative values are RAM frame slots
    //negatives are ~index into disk table
    pageno_t pageTable[MaxVirtualPagesPerProcess];
};

static
void InitProcess_id_burst_nvpages(Process *p, int id_, int burst, int pages)
{
    p->id = id_;
    p->remainingTime = burst;
    p->nvpages = pages;

    p->lastVirtualRefNo = 0;

    for (pageno_t& rf : p->pageTable)
        rf = NotMapped;
}

static//i dunno
pageno_t ReferencePage(Process *p, uint32_t randnum)
{
    //11 is about 70% of 16
    //nvpages always >= 4
    int off;

    if ((randnum & 15u) <= 11u)//11 is about 70% of 16
        off = ((randnum>>4u) % 3u) - 1 ;//get [-1,0,1]
    else
    {
        unsigned const span = p->nvpages - 2u;
        off = ((randnum>>4u) % span) + 2u;//get [2, nvpages) exclusive
        if (randnum & 1) off *= -1;
    }

    int res = p->lastVirtualRefNo + off;
    if (res<0) res += p->nvpages; else if (res >= p->nvpages) res-=p->nvpages;
    return (p->lastVirtualRefNo = res);
}

struct PageInfo
{
    int ownerId;//if -1 is not in use by a process
    int theirVirtNo;
};

struct AlgoDescriptor
{
    PagePolicy* (*fpNewUp)();
    const char* name;
};

template<class T>
PagePolicy* NewUp()
{
    return static_cast<PagePolicy*>(new T);
}

const AlgoDescriptor algos[]=
{

	{ &NewUp<FIFOPolicy>, "FIFO: First In First Out" },
    { &NewUp<LruPolicy>, "LRU: Least Recently Used" },
    { &NewUp<RandPolicy>, "Random" }
	//, your other stuff here...
};

struct ProcessInit
{
    int arrival;
    int burst;
    int nvpages;
};

#include <random>
#include <algorithm>
typedef std::minstd_rand Rng;

/*
    Processes are serviced in a round-robin fashion,
    and given a single time unit, where they reference one page.
    Determining the pid to service is as follows:
    -first check if a new process has arrived or has been waiting to start
    -else if queue not empty, extract pid of front. Note extract only happens here
    Then, just serviced pid is pushed back to tail of queue if it has remaining life
*/
struct RingBuf
{
    unsigned ibegin=0;
    unsigned iend=0;
    enum:unsigned{N=SimulationProcesses};
    int cnt=0;
    int a[N];

    int ExtractFront()
    {
        int const ret = a[ibegin++];
        --cnt;
        if (ibegin==N) ibegin = 0;
        return ret;
    }

    void PushBack(int newdata)
    {
        a[iend++] = newdata;
        ++cnt;
        if (iend==N) iend = 0;
    }

    bool empty() const { return !cnt; }
};

template<class T, size_t N>
T* endof(T (&a)[N]) { return a+N; }

static void printMem(const PageInfo (&frames)[TotalFrames], const PageInfo(&disks)[TotalDiskPages]);

static void InitFreeStacks();

static int ReleaseProcessPages(const Process& pc);

Process processes[SimulationProcesses];//pool

ProcessInit simdat[SimulationProcesses];
PageInfo diskinfo[TotalDiskPages];
PageInfo frameinfo[TotalFrames];

int main(int argc, char** argv)
{
    int const printPeriod = argc == 2 ? atoi(argv[1]) : 5;
    uint32_t printcntr = printPeriod > 0 ? 1u : 0u;//decs to big num so never prints

    for (const AlgoDescriptor *algo=algos; algo!=endof(algos); ++algo)
    {
        Rng rng(0xabcdefu);
        unsigned arv = 0u;//always start something at 0
        for (ProcessInit& rf : simdat)//note ref
        {
            rf.arrival = arv;
            rf.burst = (rng() % 16u) + 1;//[1, 17)
            rf.nvpages = (rng() % (MaxVirtualPagesPerProcess-4u)) + 4u;//give at least 4 pages

            arv = rng() % SimulationDuration;
        }
        //sort by ascending arrival times
        auto const compar=[](const ProcessInit& a, const ProcessInit& b){ return a.arrival<b.arrival; };
        std::sort(simdat, endof(simdat), compar);
        if (algo == algos)//if first time around
        {
            puts("# Workload\n"
                 " process id, arrival time, burst, max vpages");
            for (int i=0; i!=SimulationProcesses; ++i)
                printf("          %c,%13u,%6u,%11u\n",
                       char(i+'A'), simdat[i].arrival, simdat[i].burst, simdat[i].nvpages);
        }

        for (PageInfo& info : diskinfo) info.ownerId = -1;
        for (PageInfo& info : frameinfo) info.ownerId = -1;
        InitFreeStacks();

        printf("\n *** Testing Policy [%s] ***\n", algo->name);
        PagePolicy *const policy = (*algo->fpNewUp)();
        //simulation begins here
        int piditer = 0;
        //int notMappedCnt = 0;
        int faults = 0;
        int hits = 0;
        int evictions = 0;
        int comboAvail = TotalDiskPages + TotalFrames;
        RingBuf rrq;
        int elapsed=0;
        for (; ; ++elapsed)
        {
            printf(" -- Time %2u --\n", elapsed);
            int serviceId;
            int refno;

            if (piditer < SimulationProcesses)
            {
                if (comboAvail <= 0)
                {
                    puts("There is no system RAM or backing store to start another process");
                    assert(gFreeDisks.full());
                    assert(gFreeFrames.full());
                    goto LGetFront;
                }

                if (elapsed >= simdat[piditer].arrival)//check for incoming processes to service first
                {
                    Process *const pnew = processes + piditer;
                    InitProcess_id_burst_nvpages(pnew, piditer, simdat[piditer].burst, simdat[piditer].nvpages);
                    printf("new process %c arrived\n", char(piditer + 'A')) ;

                    serviceId = piditer++;
                    refno = 0;
                }
                else if (rrq.empty())
                {
                    puts("nothing to do...");
                    continue;//process iter < end so more coming in future
                }
                else
                    goto LGetFront;
            }
            else if (rrq.empty())
                break;//process iter == end, the future holds nothing
            else
            {
            LGetFront:
                serviceId = rrq.ExtractFront();
                refno = ReferencePage(processes+serviceId, rng());
            }
            //now have process id that has requested a reference
            printf("[%c] referenced its virtual page [%2u]", char(serviceId+'A'), refno);
            Process *const pserv = processes+serviceId;
            pageno_t mval = pserv->pageTable[refno];
            static_assert(NotMapped<0, "");
            if (mval >= 0)
            {
                ++hits;
                printf(", HIT, resolved to frame: [%2u]\n", mval);
                policy->UpdateAfterHit(mval);
            }
            else
            {
                ++faults;
                if (mval == NotMapped) puts(", PAGE FAULT: [not mapped]");
                else                   puts(", PAGE FAULT: [was paged to disk]");
                //Now 2 cases. 1: have free frame, or 2: need to evict
                --comboAvail;//
                if (! gFreeFrames.empty())//1
                {
                    int const fi = gFreeFrames.Pop();
                    policy->UseFreeFrame(fi);
                    pserv->pageTable[refno] = fi;//& processes[serviceId]
                    frameinfo[fi].ownerId = serviceId;
                    frameinfo[fi].theirVirtNo = refno;
                    printf("no eviction needed, gave frame: [%2u]\n", fi);
                }
                else//2: need evict
                {
                    ++evictions;
                    assert(! gFreeDisks.empty());
                    int const diski = gFreeDisks.Pop();

                    int const fi = policy->PickFrameToEvict();
                    pserv->pageTable[refno] = fi;//& processes[serviceId]

                    PageInfo const prevFrameInfo = frameinfo[fi];
                    frameinfo[fi].ownerId = serviceId;
                    frameinfo[fi].theirVirtNo = refno;
                    //need update diskinfo and prev processes pgtble

                    processes[prevFrameInfo.ownerId].pageTable[prevFrameInfo.theirVirtNo] = ~diski;
                    diskinfo[diski] = prevFrameInfo;
                    //print eviction info
                }
            }

            if (--processes[serviceId].remainingTime==0)
            {
                printf("process %c finished\n", char(serviceId+'A'));
                comboAvail += ReleaseProcessPages(processes[serviceId]);
            }
            else
                rrq.PushBack(serviceId);

            if (--printcntr == 0)
            {
                policy->print();
                printcntr = printPeriod;
                printMem(frameinfo, diskinfo);
            }
        }//the simulation loop
        assert(comboAvail == TotalFrames+TotalDiskPages);
        assert(gFreeDisks.full());
        assert(gFreeFrames.full());

        printf("\n *** [%s] results ***\n", algo->name);
        printf("time elapsed: %u\n", elapsed);
        printf("hits: %u\n", hits);
        printf("eviction: %u\n", evictions);
        printf("faults: %u\n", faults);

        fputs("getchar()", stdout);
        getchar();

        delete policy;
    }//for each replacement policy

    return 0;
}

static void InitFreeStacks()
{
    gFreeFrames.cnt = 0;
    gFreeDisks.cnt = 0;

    int n = TotalFrames;
    while (n--)
        gFreeFrames.Push(n);
    n = TotalDiskPages;
    while (n--)
        gFreeDisks.Push(n);

    assert(gFreeFrames.full());
    assert(gFreeDisks.full());
}

static void pmemhelp(const PageInfo *it, const PageInfo *const pend)
{
    for (;;)
    {
        if (it->ownerId >= 0) printf("%c:%02u", char(it->ownerId+'A'), it->theirVirtNo);
        else                  fputs("free", stdout);

        if (++it==pend)
            break;
        putchar(' ');
    }
    putwchar('\n');
}

static void printMem(const PageInfo (&frames)[TotalFrames], const PageInfo(&disks)[TotalDiskPages])
{
    puts("* frames, then backing store:");
    pmemhelp(frames, endof(frames));
    puts("   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15");
    const PageInfo *const mid = disks + TotalDiskPages/2u;
    pmemhelp(disks, mid);
    pmemhelp(mid, endof(disks));
}

static
int ReleaseProcessPages(const Process& pc)
{
    int cnt=0;
    const int pid = pc.id;
    for (int i=0; i!=TotalFrames; ++i)
    {
        if (frameinfo[i].ownerId==pid)
        {
            frameinfo[i].ownerId = -1;
            ++cnt;
            gFreeFrames.Push(i);
        }
    }
    for (int i=0; i!=TotalDiskPages; ++i)
    {
        if (diskinfo[i].ownerId==pid)
        {
            diskinfo[i].ownerId = -1;
            ++cnt;
            gFreeDisks.Push(i);
        }
    }
    return cnt;
}

FreeStackN<TotalFrames> gFreeFrames;
FreeStackN<TotalDiskPages> gFreeDisks;

/*
Microsoft Windows [Version 10.0.16299.309]
(c) 2017 Microsoft Corporation. All rights reserved.

C:\Dev> cd C:\Users\jw\Desktop\spring18\opsys\os\hw4\release

C:\Users\jw\Desktop\spring18\opsys\os\hw4\release>hw4.exe 1
# Workload
 process id, arrival time, burst, max vpages
          A,            0,    15,         10
          B,           12,    13,         23
          C,           30,     9,          4
          D,           40,     4,         20
          E,           42,    14,          4
          F,           45,    12,         13
          G,           46,    16,          9
          H,           52,    15,         28
          I,           58,    10,          5
          J,           89,     1,          8

 *** Testing Policy [LRU: Least Recently Used] ***
 -- Time  0 --
new process A arrived
[A] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 0]
Most recent to least recent frames used:
  0
* frames, then backing store:
A:00 free free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  1 --
[A] referenced its virtual page [ 0], HIT, resolved to frame: [ 0]
Most recent to least recent frames used:
  0
* frames, then backing store:
A:00 free free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  2 --
[A] referenced its virtual page [ 9], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 1]
Most recent to least recent frames used:
  1  0
* frames, then backing store:
A:00 A:09 free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  3 --
[A] referenced its virtual page [ 0], HIT, resolved to frame: [ 0]
Most recent to least recent frames used:
  0  1
* frames, then backing store:
A:00 A:09 free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  4 --
[A] referenced its virtual page [ 7], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 2]
Most recent to least recent frames used:
  2  0  1
* frames, then backing store:
A:00 A:09 A:07 free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  5 --
[A] referenced its virtual page [ 7], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  0  1
* frames, then backing store:
A:00 A:09 A:07 free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  6 --
[A] referenced its virtual page [ 6], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 3]
Most recent to least recent frames used:
  3  2  0  1
* frames, then backing store:
A:00 A:09 A:07 A:06 free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  7 --
[A] referenced its virtual page [ 5], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 4]
Most recent to least recent frames used:
  4  3  2  0  1
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  8 --
[A] referenced its virtual page [ 9], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  4  3  2  0
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time  9 --
[A] referenced its virtual page [ 0], HIT, resolved to frame: [ 0]
Most recent to least recent frames used:
  0  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 10 --
[A] referenced its virtual page [ 1], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 5]
Most recent to least recent frames used:
  5  0  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 11 --
[A] referenced its virtual page [ 1], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  0  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 12 --
new process B arrived
[B] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 6]
Most recent to least recent frames used:
  6  5  0  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 B:00 free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 13 --
[A] referenced its virtual page [ 0], HIT, resolved to frame: [ 0]
Most recent to least recent frames used:
  0  6  5  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 B:00 free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 14 --
[B] referenced its virtual page [22], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 7]
Most recent to least recent frames used:
  7  0  6  5  1  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 15 --
[A] referenced its virtual page [ 9], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  7  0  6  5  4  3  2
* frames, then backing store:
A:00 A:09 A:07 A:06 A:05 A:01 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 16 --
[B] referenced its virtual page [21], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  2  1  7  0  6  5  4  3
* frames, then backing store:
A:00 A:09 B:21 A:06 A:05 A:01 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
A:07 free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 17 --
[A] referenced its virtual page [ 9], HIT, resolved to frame: [ 1]
process A finished
Most recent to least recent frames used:
  1  2  7  0  6  5  4  3
* frames, then backing store:
free free B:21 free free free B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 18 --
[B] referenced its virtual page [22], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  1  2  0  6  5  4  3
* frames, then backing store:
free free B:21 free free free B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 19 --
[B] referenced its virtual page [ 0], HIT, resolved to frame: [ 6]
Most recent to least recent frames used:
  6  7  1  2  0  5  4  3
* frames, then backing store:
free free B:21 free free free B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 20 --
[B] referenced its virtual page [22], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  6  1  2  0  5  4  3
* frames, then backing store:
free free B:21 free free free B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 21 --
[B] referenced its virtual page [20], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 5]
Most recent to least recent frames used:
  5  7  6  1  2  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 22 --
[B] referenced its virtual page [20], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  7  6  1  2  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 23 --
[B] referenced its virtual page [21], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  5  7  6  1  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 24 --
[B] referenced its virtual page [20], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  2  7  6  1  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 25 --
[B] referenced its virtual page [21], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  5  7  6  1  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 26 --
[B] referenced its virtual page [21], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  5  7  6  1  0  4  3
* frames, then backing store:
free free B:21 free free B:20 B:00 B:22
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 27 --
[B] referenced its virtual page [21], HIT, resolved to frame: [ 2]
process B finished
Most recent to least recent frames used:
  2  5  7  6  1  0  4  3
* frames, then backing store:
free free free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 28 --
nothing to do...
 -- Time 29 --
nothing to do...
 -- Time 30 --
new process C arrived
[C] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 7]
Most recent to least recent frames used:
  7  2  5  6  1  0  4  3
* frames, then backing store:
free free free free free free free C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 31 --
[C] referenced its virtual page [ 1], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 32 --
[C] referenced its virtual page [ 1], HIT, resolved to frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 33 --
[C] referenced its virtual page [ 0], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  6  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 34 --
[C] referenced its virtual page [ 1], HIT, resolved to frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 35 --
[C] referenced its virtual page [ 1], HIT, resolved to frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 36 --
[C] referenced its virtual page [ 0], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  6  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 37 --
[C] referenced its virtual page [ 1], HIT, resolved to frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free C:01 C:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 38 --
[C] referenced its virtual page [ 0], HIT, resolved to frame: [ 7]
process C finished
Most recent to least recent frames used:
  7  6  2  5  1  0  4  3
* frames, then backing store:
free free free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 39 --
nothing to do...
 -- Time 40 --
new process D arrived
[D] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 7]
Most recent to least recent frames used:
  7  6  2  5  1  0  4  3
* frames, then backing store:
free free free free free free free D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 41 --
[D] referenced its virtual page [11], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 6]
Most recent to least recent frames used:
  6  7  2  5  1  0  4  3
* frames, then backing store:
free free free free free free D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 42 --
new process E arrived
[E] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 5]
Most recent to least recent frames used:
  5  6  7  2  1  0  4  3
* frames, then backing store:
free free free free free E:00 D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 43 --
[D] referenced its virtual page [ 7], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 2]
Most recent to least recent frames used:
  2  5  6  7  1  0  4  3
* frames, then backing store:
free free D:07 free free E:00 D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 44 --
[E] referenced its virtual page [ 2], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 4]
Most recent to least recent frames used:
  4  2  5  6  7  1  0  3
* frames, then backing store:
free free D:07 free E:02 E:00 D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 45 --
new process F arrived
[F] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 3]
Most recent to least recent frames used:
  3  4  2  5  6  7  1  0
* frames, then backing store:
free free D:07 F:00 E:02 E:00 D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 46 --
new process G arrived
[G] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 1]
Most recent to least recent frames used:
  1  3  4  2  5  6  7  0
* frames, then backing store:
free G:00 D:07 F:00 E:02 E:00 D:11 D:00
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 47 --
[D] referenced its virtual page [14], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 0]
process D finished
Most recent to least recent frames used:
  0  1  3  4  2  5  6  7
* frames, then backing store:
free G:00 free F:00 E:02 E:00 free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 48 --
[E] referenced its virtual page [ 3], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 7]
Most recent to least recent frames used:
  7  0  1  3  4  2  5  6
* frames, then backing store:
free G:00 free F:00 E:02 E:00 free E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 49 --
[F] referenced its virtual page [12], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 6]
Most recent to least recent frames used:
  6  7  0  1  3  4  2  5
* frames, then backing store:
free G:00 free F:00 E:02 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 50 --
[G] referenced its virtual page [ 8], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 2]
Most recent to least recent frames used:
  2  6  7  0  1  3  4  5
* frames, then backing store:
free G:00 G:08 F:00 E:02 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 51 --
[E] referenced its virtual page [ 0], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  2  6  7  0  1  3  4
* frames, then backing store:
free G:00 G:08 F:00 E:02 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 52 --
new process H arrived
[H] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 0]
Most recent to least recent frames used:
  0  5  2  6  7  1  3  4
* frames, then backing store:
H:00 G:00 G:08 F:00 E:02 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 53 --
[F] referenced its virtual page [ 6], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  4  0  5  2  6  7  1  3
* frames, then backing store:
H:00 G:00 G:08 F:00 F:06 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 54 --
[G] referenced its virtual page [ 8], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  4  0  5  6  7  1  3
* frames, then backing store:
H:00 G:00 G:08 F:00 F:06 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 55 --
[E] referenced its virtual page [ 3], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  2  4  0  5  6  1  3
* frames, then backing store:
H:00 G:00 G:08 F:00 F:06 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 56 --
[H] referenced its virtual page [20], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  3  7  2  4  0  5  6  1
* frames, then backing store:
H:00 G:00 G:08 H:20 F:06 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 57 --
[F] referenced its virtual page [ 4], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  1  3  7  2  4  0  5  6
* frames, then backing store:
H:00 F:04 G:08 H:20 F:06 E:00 F:12 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 58 --
new process I arrived
[I] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  6  1  3  7  2  4  0  5
* frames, then backing store:
H:00 F:04 G:08 H:20 F:06 E:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 59 --
[G] referenced its virtual page [ 0], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  5  6  1  3  7  2  4  0
* frames, then backing store:
H:00 F:04 G:08 H:20 F:06 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 60 --
[E] referenced its virtual page [ 3], HIT, resolved to frame: [ 7]
Most recent to least recent frames used:
  7  5  6  1  3  2  4  0
* frames, then backing store:
H:00 F:04 G:08 H:20 F:06 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 61 --
[H] referenced its virtual page [10], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  0  7  5  6  1  3  2  4
* frames, then backing store:
H:10 F:04 G:08 H:20 F:06 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 62 --
[F] referenced its virtual page [ 5], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  4  0  7  5  6  1  3  2
* frames, then backing store:
H:10 F:04 G:08 H:20 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 63 --
[I] referenced its virtual page [ 4], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  2  4  0  7  5  6  1  3
* frames, then backing store:
H:10 F:04 I:04 H:20 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 64 --
[G] referenced its virtual page [ 0], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  2  4  0  7  6  1  3
* frames, then backing store:
H:10 F:04 I:04 H:20 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 65 --
[E] referenced its virtual page [ 2], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  3  5  2  4  0  7  6  1
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 66 --
[H] referenced its virtual page [10], HIT, resolved to frame: [ 0]
Most recent to least recent frames used:
  0  3  5  2  4  7  6  1
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 67 --
[F] referenced its virtual page [ 4], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  0  3  5  2  4  7  6
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:00 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 68 --
[I] referenced its virtual page [ 2], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  6  1  0  3  5  2  4  7
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:02 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 69 --
[G] referenced its virtual page [ 0], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  6  1  0  3  2  4  7
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:02 E:03
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 70 --
[E] referenced its virtual page [ 1], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  7  5  6  1  0  3  2  4
* frames, then backing store:
H:10 F:04 I:04 E:02 F:05 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 free free free free free
free free free free free free free free free free free free free free free free
 -- Time 71 --
[H] referenced its virtual page [ 9], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  4  7  5  6  1  0  3  2
* frames, then backing store:
H:10 F:04 I:04 E:02 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 free free free free
free free free free free free free free free free free free free free free free
 -- Time 72 --
[F] referenced its virtual page [ 5], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  2  4  7  5  6  1  0  3
* frames, then backing store:
H:10 F:04 F:05 E:02 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 free free free
free free free free free free free free free free free free free free free free
 -- Time 73 --
[I] referenced its virtual page [ 1], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  3  2  4  7  5  6  1  0
* frames, then backing store:
H:10 F:04 F:05 I:01 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 free free
free free free free free free free free free free free free free free free free
 -- Time 74 --
[G] referenced its virtual page [ 0], HIT, resolved to frame: [ 5]
Most recent to least recent frames used:
  5  3  2  4  7  6  1  0
* frames, then backing store:
H:10 F:04 F:05 I:01 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 free free
free free free free free free free free free free free free free free free free
 -- Time 75 --
[E] referenced its virtual page [ 0], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  0  5  3  2  4  7  6  1
* frames, then backing store:
E:00 F:04 F:05 I:01 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 free
free free free free free free free free free free free free free free free free
 -- Time 76 --
[H] referenced its virtual page [ 9], HIT, resolved to frame: [ 4]
Most recent to least recent frames used:
  4  0  5  3  2  7  6  1
* frames, then backing store:
E:00 F:04 F:05 I:01 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 free
free free free free free free free free free free free free free free free free
 -- Time 77 --
[F] referenced its virtual page [ 0], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  1  4  0  5  3  2  7  6
* frames, then backing store:
E:00 F:00 F:05 I:01 H:09 G:00 I:02 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
free free free free free free free free free free free free free free free free
 -- Time 78 --
[I] referenced its virtual page [ 0], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  6  1  4  0  5  3  2  7
* frames, then backing store:
E:00 F:00 F:05 I:01 H:09 G:00 I:00 E:01
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 free free free free free free free free free free free free free free free
 -- Time 79 --
[G] referenced its virtual page [ 2], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  7  6  1  4  0  5  3  2
* frames, then backing store:
E:00 F:00 F:05 I:01 H:09 G:00 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 free free free free free free free free free free free free free free
 -- Time 80 --
[E] referenced its virtual page [ 1], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  2  7  6  1  4  0  5  3
* frames, then backing store:
E:00 F:00 E:01 I:01 H:09 G:00 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 free free free free free free free free free free free free free
 -- Time 81 --
[H] referenced its virtual page [ 9], HIT, resolved to frame: [ 4]
Most recent to least recent frames used:
  4  2  7  6  1  0  5  3
* frames, then backing store:
E:00 F:00 E:01 I:01 H:09 G:00 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 free free free free free free free free free free free free free
 -- Time 82 --
[F] referenced its virtual page [12], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  3  4  2  7  6  1  0  5
* frames, then backing store:
E:00 F:00 E:01 F:12 H:09 G:00 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 free free free free free free free free free free free free
 -- Time 83 --
[I] referenced its virtual page [ 1], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  5  3  4  2  7  6  1  0
* frames, then backing store:
E:00 F:00 E:01 F:12 H:09 I:01 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 free free free free free free free free free free free
 -- Time 84 --
[G] referenced its virtual page [ 1], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  0  5  3  4  2  7  6  1
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 free free free free free free free free free free
 -- Time 85 --
[E] referenced its virtual page [ 1], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  0  5  3  4  7  6  1
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 free free free free free free free free free free
 -- Time 86 --
[H] referenced its virtual page [ 9], HIT, resolved to frame: [ 4]
Most recent to least recent frames used:
  4  2  0  5  3  7  6  1
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 free free free free free free free free free free
 -- Time 87 --
[F] referenced its virtual page [ 0], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  4  2  0  5  3  7  6
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:00 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 free free free free free free free free free free
 -- Time 88 --
[I] referenced its virtual page [ 2], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  6  1  4  2  0  5  3  7
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 free free free free free free free free free
 -- Time 89 --
new process J arrived
[J] referenced its virtual page [ 0], PAGE FAULT: [not mapped]
process J finished
Most recent to least recent frames used:
  7  6  1  4  2  0  5  3
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:02 free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 free free free free free free free free
 -- Time 90 --
[G] referenced its virtual page [ 2], PAGE FAULT: [was paged to disk]
no eviction needed, gave frame: [ 7]
Most recent to least recent frames used:
  7  6  1  4  2  0  5  3
* frames, then backing store:
G:01 F:00 E:01 F:12 H:09 I:01 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 free free free free free free free free
 -- Time 91 --
[E] referenced its virtual page [ 0], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  3  7  6  1  4  2  0  5
* frames, then backing store:
G:01 F:00 E:01 E:00 H:09 I:01 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 free free free free free free free
 -- Time 92 --
[H] referenced its virtual page [ 9], HIT, resolved to frame: [ 4]
Most recent to least recent frames used:
  4  3  7  6  1  2  0  5
* frames, then backing store:
G:01 F:00 E:01 E:00 H:09 I:01 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 free free free free free free free
 -- Time 93 --
[F] referenced its virtual page [ 0], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  4  3  7  6  2  0  5
* frames, then backing store:
G:01 F:00 E:01 E:00 H:09 I:01 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 free free free free free free free
 -- Time 94 --
[I] referenced its virtual page [ 4], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  5  1  4  3  7  6  2  0
* frames, then backing store:
G:01 F:00 E:01 E:00 H:09 I:04 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 I:01 free free free free free free
 -- Time 95 --
[G] referenced its virtual page [ 3], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  0  5  1  4  3  7  6  2
* frames, then backing store:
G:03 F:00 E:01 E:00 H:09 I:04 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 I:01 G:01 free free free free free
 -- Time 96 --
[E] referenced its virtual page [ 1], HIT, resolved to frame: [ 2]
Most recent to least recent frames used:
  2  0  5  1  4  3  7  6
* frames, then backing store:
G:03 F:00 E:01 E:00 H:09 I:04 I:02 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 I:01 G:01 free free free free free
 -- Time 97 --
[H] referenced its virtual page [10], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  6  2  0  5  1  4  3  7
* frames, then backing store:
G:03 F:00 E:01 E:00 H:09 I:04 H:10 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 F:00 G:00 F:12 E:00 H:00 F:06 G:08 H:20 I:00 E:03 F:05 I:04 E:02 H:10 F:04
I:02 E:01 F:05 I:01 G:00 E:00 I:00 G:02 F:12 I:01 G:01 I:02 free free free free
 -- Time 98 --
[F] referenced its virtual page [ 0], HIT, resolved to frame: [ 1]
process F finished
Most recent to least recent frames used:
  1  6  2  0  5  4  3  7
* frames, then backing store:
G:03 free E:01 E:00 H:09 I:04 H:10 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free G:00 free E:00 H:00 free G:08 H:20 I:00 E:03 free I:04 E:02 H:10 free
I:02 E:01 free I:01 G:00 E:00 I:00 G:02 free I:01 G:01 I:02 free free free free
 -- Time 99 --
[I] referenced its virtual page [ 3], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 1]
Most recent to least recent frames used:
  1  6  2  0  5  4  3  7
* frames, then backing store:
G:03 I:03 E:01 E:00 H:09 I:04 H:10 G:02
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free G:00 free E:00 H:00 free G:08 H:20 I:00 E:03 free I:04 E:02 H:10 free
I:02 E:01 free I:01 G:00 E:00 I:00 G:02 free I:01 G:01 I:02 free free free free
 -- Time 100 --
[G] referenced its virtual page [ 4], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  7  1  6  2  0  5  4  3
* frames, then backing store:
G:03 I:03 E:01 E:00 H:09 I:04 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
E:02 free G:00 free E:00 H:00 free G:08 H:20 I:00 E:03 free I:04 E:02 H:10 free
I:02 E:01 free I:01 G:00 E:00 I:00 G:02 G:02 I:01 G:01 I:02 free free free free
 -- Time 101 --
[E] referenced its virtual page [ 0], HIT, resolved to frame: [ 3]
process E finished
Most recent to least recent frames used:
  3  7  1  6  2  0  5  4
* frames, then backing store:
G:03 I:03 free free H:09 I:04 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 I:00 free free I:04 free H:10 free
I:02 free free I:01 G:00 free I:00 G:02 G:02 I:01 G:01 I:02 free free free free
 -- Time 102 --
[H] referenced its virtual page [24], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 3]
Most recent to least recent frames used:
  3  7  1  6  2  0  5  4
* frames, then backing store:
G:03 I:03 free H:24 H:09 I:04 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 I:00 free free I:04 free H:10 free
I:02 free free I:01 G:00 free I:00 G:02 G:02 I:01 G:01 I:02 free free free free
 -- Time 103 --
[I] referenced its virtual page [ 2], PAGE FAULT: [was paged to disk]
no eviction needed, gave frame: [ 2]
process I finished
Most recent to least recent frames used:
  2  3  7  1  6  0  5  4
* frames, then backing store:
G:03 free free H:24 H:09 free H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 free free free free free
 -- Time 104 --
[G] referenced its virtual page [ 6], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 5]
Most recent to least recent frames used:
  5  2  3  7  1  6  0  4
* frames, then backing store:
G:03 free free H:24 H:09 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 free free free free free
 -- Time 105 --
[H] referenced its virtual page [23], PAGE FAULT: [not mapped]
no eviction needed, gave frame: [ 2]
Most recent to least recent frames used:
  2  5  3  7  1  6  0  4
* frames, then backing store:
G:03 free H:23 H:24 H:09 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 free free free free free
 -- Time 106 --
[G] referenced its virtual page [ 1], PAGE FAULT: [was paged to disk]
no eviction needed, gave frame: [ 1]
Most recent to least recent frames used:
  1  2  5  3  7  6  0  4
* frames, then backing store:
G:03 G:01 H:23 H:24 H:09 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 free free free free free
 -- Time 107 --
[H] referenced its virtual page [22], PAGE FAULT: [not mapped]
Most recent to least recent frames used:
  4  1  2  5  3  7  6  0
* frames, then backing store:
G:03 G:01 H:23 H:24 H:22 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 H:09 free free free free
 -- Time 108 --
[G] referenced its virtual page [ 1], HIT, resolved to frame: [ 1]
Most recent to least recent frames used:
  1  4  2  5  3  7  6  0
* frames, then backing store:
G:03 G:01 H:23 H:24 H:22 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 free G:01 H:09 free free free free
 -- Time 109 --
[H] referenced its virtual page [20], PAGE FAULT: [was paged to disk]
Most recent to least recent frames used:
  0  1  4  2  5  3  7  6
* frames, then backing store:
H:20 G:01 H:23 H:24 H:22 G:06 H:10 G:04
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free G:00 free free H:00 free G:08 H:20 free free free free free H:10 free
free free free free G:00 free free G:02 G:02 G:03 G:01 H:09 free free free free
 -- Time 110 --
[G] referenced its virtual page [ 2], PAGE FAULT: [was paged to disk]
process G finished
Most recent to least recent frames used:
  6  0  1  4  2  5  3  7
* frames, then backing store:
H:20 free H:23 H:24 H:22 free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free H:00 free free H:20 free free free free free H:10 free
free free free free free free H:10 free free free free H:09 free free free free
 -- Time 111 --
[H] referenced its virtual page [20], HIT, resolved to frame: [ 0]
process H finished
Most recent to least recent frames used:
  0  6  1  4  2  5  3  7
* frames, then backing store:
free free free free free free free free
   0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
free free free free free free free free free free free free free free free free
free free free free free free free free free free free free free free free free
 -- Time 112 --

 *** [LRU: Least Recently Used] results ***
time elapsed: 112
hits: 46
eviction: 33
faults: 63
getchar()
*/
