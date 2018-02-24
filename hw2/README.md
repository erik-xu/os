2/22

The following four functions need be completed:

Nick:   round robin  
kevin:  shortest job  
Eric:   highest priority (non-preemptive)  
Bryan:  highest priority w/aging (non-preemptive)  

Rob:    write up

See below for an explanation of the functions, in addition to the other files

```C++
struct AlgoRet
{
    int jobsCompleted;
    int lastCompletionTime;
};
/*
    First 2 parameters are input.
    The job[] will be sorted by arrival time, with all arrival times distinct.
    
    In addition to the return value, the last two arrays are outputs.
    
    Index of job is its id, job[i] has an id of i, and corresponds to stats[i].
    The stats each algorithm stores for each job is the time of its first slice, and completion time.
    Jobs that don't get serviced can be left alone; the array is initialized before hand.
    
    gantt[q] is the id+'A' of the job that run for slice q, or if cpu was idle '.'.
    
    See the implementation of fcfs in main.cpp for an example.
*/
AlgoRet round_robin       (const Job* job, int njobs, PerJobStats* stats, char* gantt);

AlgoRet sjf               (const Job* job, int njobs, PerJobStats* stats, char* gantt);
AlgoRet hpf_non_preemptive(const Job* job, int njobs, PerJobStats* stats, char* gantt);
AlgoRet hpf_aging_non_preemptive(const Job* job, int njobs, PerJobStats* stats, char* gantt);
/*
  I think the last three are all very similar, and could all be done in a similar way to
  the preemptive template function. But with many hands a person can do a single one.
*/
```
