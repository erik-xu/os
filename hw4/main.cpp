#include "process.h"
#include "memory.h"
#include <vector>

using std::vector;

int main(int argc, char* argv[]) {

  srand(time(NULL));
  Process* p_root;
  vector<Memory> main_mem(100);
  Memory* m_root = &main_mem[0];
  m_root->n = 0;

  Memory* m_temp = m_root; 
  for (int i = 1; i < 100; i++) {
    m_temp->next = &main_mem[i];      
    m_temp = m_temp->next;
    m_temp->n = i;
  }



  return 0;
}
