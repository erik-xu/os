/*
    common symbols for each algorithm
*/
#ifndef H_SYMS
#define H_SYMS

#define INIT_SEED 0xcafebeef // use same rng seed so each algo uses same data
#define QUANTA 100	         // units of time
#define NJOBS 12	           // total number of jobs/processes ran 
#define MAX_BURST 16
#define MIN_BURST 3
#define BURST_SPAN (MAX_BURST-MIN_BURST+1u)


// Structure for each process job containing:
// 1) arrival time
// 2) burst time
// 3) priority 
struct Job
{
	int arrival;  // arrival time of process in quanta
	int burst;    // quanta needed to fully run, > 0
	int priority; // [1 to 4] inclusive. 1 is highest priority
};

struct PerJobStats
{
    //after algorithm runs, if qend==0, (not overwrite of initial memset before algo)
    //signifies job was not served not serviced. no burst is 0
    int qbegin, qend;
};

/*
Each simulation run should last until the completion of the
last process, even if it goes
beyond QUANTA quanta. No process should get the CPU for the
first time at >= QUANTA

Stats don't include any jobs that do not run.
*/
struct AlgoRet
{
    int jobsCompleted;
    int lastCompletionTime;
};
/*
    The job[] will be sorted by arrival time, with all arrival times distinct;
    Index of job is its id, job[i] has an id of i, and corresponds to stats[i];
    gantt[i] is the id+'A' of the job that run for slice i, or if cpu was idle '.';
*/
AlgoRet fcfs              (const Job* job, int njobs, PerJobStats* stats, char* gantt);
AlgoRet sjf               (const Job* job, int njobs, PerJobStats* stats, char* gantt);
//AlgoRet srt               (const Job* job, int njobs, PerJobStats* stats, char* gantt);preemptive<>
AlgoRet round_robin       (const Job* job, int njobs, PerJobStats* stats, char* gantt);//TODO
AlgoRet hpf_non_preemptive(const Job* job, int njobs, PerJobStats* stats, char* gantt);//TODO
//AlgoRet hpf_preemptive    (const Job* job, int njobs, PerJobStats* stats, char* gantt);preemptive<>
AlgoRet hpf_aging_non_preemptive(const Job* job, int njobs, PerJobStats* stats, char* gantt);//TODO
//hpf_aging_preempt preemptive<>

/*
    preemptive: interrupt and resume later

    (Jonathan):
	To make it easy, you don't have to touch the stats array in your algo impls
	if the corresponding job is not serviced, because the whole stats is memset to 0 before hand.
    Also, all your algorithm needs to store is the service begin and completion time pair
	for each job that gets serviced. Turnaround, wait, and response times are then calculated
    and printed on the same line after the algorithm completes.

    Since the range of priorities and burst span is small, a priority queue based on an array map
    could be neato. Could use bit scan forward to skip to occupied slots
*/

#include <queue>
#include <stdint.h>

//the convention for std::priority_queue and heap functions is
//less(<) is for a max heap, and vice versa. (I guess b/c heapsort with max heap is for sorting by < ?)

//lower integer priorities are more important in this context


//NJOBS is small and we know Q max size in advance,
//so dynamic memory (std::vector) is not optimal.
//can at least reserve on init though.
template<typename T, typename Cmp>
class PriorityQueue : public std::priority_queue<T, std::vector<T>, Cmp>
{
public:
    PriorityQueue() = default;
    PriorityQueue(unsigned n)
    {
        this->c.reserve(n);
    }

    T* ptr_top()
    {
        return this->c.data();
    }
};

/*
    (Jonathan)
    See preemptive<CmpFun, InitFin> in main.cpp
    for explanation of these Functors.

    Here is something neat I figured out for aging variants of HPF
    which turns them into regular HPF:

    For this explanation, convention is larger priority values are
    more important. A jobs priority increases every 5 slices.
    Let q = current slice. To determine if job a is more important relative to job b
    in the queue, do:
        a.priority*5 + (q-a.arrival) > b.priority*5 + (q-b.arrival)
    Note how the q's cancel, and left with a constant value once inserted into the queue.
    But for assignment, convention is lower priority values more important, so store:

    job.priority*5 + arrival
*/

struct QueueData
{
    uint16_t rem;      // remainder of the job left for execution (burst - X)
                       // where X = how long the job has ran
    uint8_t  id;       // id of the job
    uint8_t  bserved;  // variable to track whether job has been served
                       // TODO: set this to false since fillData() defaults this to false
    uint16_t priority; // SRT doesn't need this but don't want to make more types.
};

// Priority Queue Comparison Struct for SRT
struct SrtComp
{
    bool operator() (QueueData a, QueueData b) const
    {
        return a.rem > b.rem;
    }
};

// Priority Queue Comparison Struct for HPF 
struct HpfComp
{
    bool operator() (QueueData a, QueueData b) const
    {
        return a.priority > b.priority;
    }
};

#endif

