/*
    NOTE: this is fucked
*/
#ifndef H_LFU
#define H_LFU

#include "util.h"

#include <set>

using std::multiset;
using std::find_if;

struct lfu_page {
  pageno_t frame_no;
  pageno_t times_used;
}; 

struct compare_lfu {
  bool operator() (const lfu_page& lhs, const lfu_page& rhs) const
  { return lhs.times_used < rhs.times_used; }
};

class LfuPolicy : public PagePolicy {
  multiset<lfu_page, compare_lfu> lfu_q; 

public:
    LfuPolicy();
    ~LfuPolicy();
    void print();
    void UseFreeFrame(pageno_t frameno);
    pageno_t PickFrameToEvict();
    void UpdateAfterHit(pageno_t);
};


#endif // H_LFU
