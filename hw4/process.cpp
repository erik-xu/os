#include "process.h"

Process::Process() {

}

void Process::gen_process_size() {
  int size = rand() % 3 + 0;

  if (size == 0) {
    this->process_size = 5;
  } else if (size == 1) {
    this->process_size = 11;
  } else if (size == 2) {
    this->process_size = 17;
  } else if (size == 3) {
    this->process_size = 31;
  }   
}

void Process::gen_service_time() {
  this->service_time = rand() % 5 + 1;
}
 
