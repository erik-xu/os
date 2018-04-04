#include "util.h"
#include <stdio.h>
#include <list>

class FIFOPolicy : public PagePolicy {

	using ListType = std::list<pageno_t>;
	using Iter = ListType::iterator;
	Iter accessIters[TotalFrames];//indexed by physical frame numbers
	std::list<pageno_t> ls;//front is more recent then back

	public:
		FIFOPolicy() {
			for (Iter& rf : accessIters)//note ref
				rf = ls.end();//a nullptr wrapper or some fixed sentinel node
		}

	public:
		void UseFreeFrame(pageno_t frameno)
		{
			ls.push_back(frameno);
			accessIters[frameno] = ls.begin();
		}

	public:
		//If a page needs to be evicted
		//@return evicted page??
		pageno_t PickFrameToEvict()
		{
			int const backval = ls.front(); //FIFO = remove front
			ls.pop_front();
		
			ls.push_back(backval);
			accessIters[backval] = ls.begin();

			return backval;
		}

	public:
		//If page is already loaded
		void UpdateAfterHit(pageno_t frameno)
		{
			//do nothing because FIFO
			//accessIters[frameno] = ls.begin();	//reset??
		}

};
