#include "lfu.h"

#include <stdio.h>

LfuPolicy::LfuPolicy() {}
LfuPolicy::~LfuPolicy() {}


void LfuPolicy::print() {
  cout << "LFU to MFU" << endl;

  for (multiset<lfu_page, compare_lfu>::iterator i = lfu_q.begin(); i != lfu_q.end(); i++) {
    printf("%3u", i->frame_no); 
  }
  putchar('\n');
 
}

struct check_frameno {
    check_frameno(pageno_t const& f) : frameno(f) { }  
    bool operator () (lfu_page const& p) { return p.frame_no == frameno; }
private:
    pageno_t frameno;
};

void LfuPolicy::UseFreeFrame(pageno_t frameno) {
  // find page frame
  //multiset<lfu_page, compare_lfu>::iterator it = find_if(lfu_q.begin(), lfu_q.end(), [&](lfu_page const& p) { return p.frame_no == frameno; });  
  //multiset<lfu_page, compare_lfu>::iterator it = find_if(lfu_q.begin(), lfu_q.end(), check_frameno(frameno));  
  /*
  multiset<lfu_page, compare_lfu>::iterator it = lfu_q.end();
  for (multiset<lfu_page, compare_lfu>::iterator i = lfu_q.begin(); i != lfu_q.end(); i++) {
    if (i->frame_no == frameno) {
      it = i; 
      break;
    }
  }
  */
  
  lfu_page temp;
  cout << "LOOKING FOR PAGE: " << frameno << endl; 
  // if not found, insert into set 
  //if (it == lfu_q.end()) {
    cout << "did not find " << endl; 
    temp.frame_no = frameno;
    temp.times_used = 0;
  //}
  
  /*
  else if (it != lfu_q.end()) {
    cout << "found " << it->frame_no << endl; 
    temp.frame_no = it->frame_no;
    temp.times_used = it->times_used + 1;
    lfu_q.erase(it);
  }
  */
    
  lfu_q.insert(temp);

  for (multiset<lfu_page, compare_lfu>::iterator i = lfu_q.begin(); i != lfu_q.end(); i++) {
    LfuPolicy::print();    
  }
} // LfuPolicy::UseFreeFrame()

pageno_t LfuPolicy::PickFrameToEvict() {
  pageno_t ret = lfu_q.begin()->frame_no;
  lfu_q.erase(lfu_q.begin()); 
  
  return ret;
} // LfuPolicy::PickFrameToEvict()

void LfuPolicy::UpdateAfterHit(pageno_t frameno) {
  multiset<lfu_page, compare_lfu>::iterator it = find_if(lfu_q.begin(), lfu_q.end(), [&](lfu_page const& p) 
    { return p.frame_no == frameno; }); 
  //multiset<lfu_page, compare_lfu>::iterator it = find_if(lfu_q.begin(), lfu_q.end(), check_frameno(frameno));  
  //multiset<lfu_page, compare_lfu>::iterator it = lfu_q.end();

  cout << "LOOKING FOR PAGE: " << frameno << endl; 

  /*
  for (multiset<lfu_page, compare_lfu>::iterator i = lfu_q.begin(); i != lfu_q.end(); i++) {
    cout << "frame: " << i->frame_no << endl;
    if (i->frame_no == frameno) {
      cout << "FOUND!!!" << endl;
      it = i; 
      break;
    }
  }
  */
  
  assert(it != lfu_q.end());

  lfu_page temp;

  temp.frame_no = it->frame_no;
  temp.times_used = it->times_used + 1;
  
  lfu_q.erase(it);
  lfu_q.insert(temp); 
} // LfuPolicy::UpdateAfterHit()
