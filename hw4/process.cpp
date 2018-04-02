#include "process.h"

Process::Process() {
  cout << "creating process" << endl;
  this->gen_process_size();
  this->gen_service_time();
}

void Process::gen_process_size() {
  cout << "generating process size" << endl;

  int size = rand() % 4 + 0;

  if (size == 0) {
    this->process_size = 5;
  } else if (size == 1) {
    this->process_size = 11;
  } else if (size == 2) {
    this->process_size = 17;
  } else if (size == 3) {
    this->process_size = 31;
  }   

  cout << "process size is: " << this->process_size << endl;
}

void Process::gen_service_time() {
  cout << "generating process duration" << endl;

  this->service_time = rand() % 5 + 1;

  cout << "process duration is: " << this->service_time << endl;
}
 
