#include "util.h"
#include <stdio.h>
#include <list>
#include <queue>
using namespace std;

class FIFOPolicy : public PagePolicy {
	std::queue<pageno_t> q;

	public:
		FIFOPolicy() {
			//nothing needed
		}

	public:
		void UseFreeFrame(pageno_t frameno)
		{
			q.push(frameno);
		}

	public:
		//If a page needs to be evicted
		//@return evicted page
		pageno_t PickFrameToEvict()
		{
			if (!q.empty()) {
				pageno_t temp = q.front();
				q.pop();
				return temp;
			}
		}

	public:
		//If page is already loaded
		void UpdateAfterHit(pageno_t frameno)
		{
			//do nothing
		}

};
