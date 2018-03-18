/*

Simulator:
1. Assume 10 threads, each represents a ticket seller: H1, M1, M2, M3, L1, L2, L3, L4, L5, L6.
2. Each seller has their own queue and customer stands in the queue using FIFO.
3. Initialize the simulation clock – initially to zero (0:00)
4. Create 10 threads and suspend them.
5. Think of the simulation as main() that include:
a. Create the necessary data structures including the 10 queues for the different sellers and initialize each queue with its ticket buyers up front based on the N value.
b. Create the 10 threads and each will be set with a sell() function and seller type as arguments.
c. Wakeupall10threadstoexecuteinparallel;wakeup_all_seller_threads();
d. Wait for all threads to complete
e. Exit

# Resources used
* http://www.yolinux.com/TUTORIALS/LinuxTutorialPosixThreads.html

*/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */
#include <iostream>
#include <vector>
#include <queue>          // std::priority_queue
using std::vector;
using std::priority_queue;
using std::cout;
using std::endl;

#define NUM_SELLERS 10
#define NUM_BUYERS 5   // 5, 10, 15
#define NUM_ROWS 10
#define NUM_SEATS_PER_ROW 10
#define HOUR 60

// Thread condition variable initialization using default MACRO 
pthread_cond_t cond = PTHREAD_COND_INITIALIZER; 
// Thread mutex variable initialization using default MACRO
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


class Seller;
class Buyer;
class mycomparison;
void print_seats(vector<vector<int>>);
void generate_buyers(vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> &buyer_queue);
void sell_tickets(Seller *seller);

class Seller {
  public:
    static vector<vector<int>> *seats;
    static vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> *buyer_queue;
    static int *clock;
    char type;
    int id;
    Seller(char type, int id) {
      this->type = type;
      this->id = id;
    }
}; // Seller

vector<vector<int>> *Seller::seats;
vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> *Seller::buyer_queue;
int *Seller::clock;

class Buyer {
  public:
    int arrival_time;
    int serve_time;
    bool served;
    Buyer(int arrival_time, int serve_time) {
      this->arrival_time = arrival_time;
      this->serve_time = serve_time;
    }
}; // Buyer()


// priority queue comparator (default min heap)
class mycomparison {
  bool reverse;
  public:
    mycomparison(const bool& revparam=false) {
      reverse=revparam;
    }
    bool operator() (const Buyer &lhs, const Buyer &rhs) const {
      if (reverse) 
        return (lhs.arrival_time > rhs.arrival_time);
      else 
        return (lhs.arrival_time < rhs.arrival_time);
    }
}; // mycomparison

// prototype for printing seating chart
void print_seats(vector<vector<int>> seats) {
  for (int row = 0; row < NUM_ROWS; row++) {
    cout << "row: " << row << " | ";
    for (int seat = 0; seat < NUM_SEATS_PER_ROW; seat++) {
      cout << seats[row][seat] << " ";
    }
    cout << endl;
  }
} // print_seats()

void generate_buyers(vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> &buyer_queue) {
  // Initialize RNG with seed  
  srand (time(NULL));
 
  // Generate buyer arrival time for each seller
  //for (int seller = 0; seller < NUM_SELLERS; seller++) {
  //  for (int buyer = 0; buyer < NUM_BUYERS; buyer++) {
  //    buyer_queue[seller].push(Buyer(rand() % HOUR));
  //  }
  //}
  
  // Generate serve time for "H" sellers
  for (int buyer = 0; buyer < NUM_BUYERS; buyer++) {
    int arrival_time = rand() % HOUR;
    int serve_time = rand() % 2 + 1;
    buyer_queue[0].push(Buyer(arrival_time, serve_time));
  }
 
  // Generate serve time for "M" sellers
  for (int seller = 1; seller < 4; seller++) { 
    for (int buyer = 0; buyer < NUM_BUYERS; buyer++) {
      int arrival_time = rand() % HOUR;
      int serve_time = rand() % 3 + 2;
      buyer_queue[seller].push(Buyer(arrival_time, serve_time));
    }
  }
 
  // Generate serve time for "L" sellers
  for (int seller = 4; seller < NUM_SELLERS; seller++) { 
    for (int buyer = 0; buyer < NUM_BUYERS; buyer++) {
      int arrival_time = rand() % HOUR;
      int serve_time = rand() % 4 + 4;
      buyer_queue[seller].push(Buyer(arrival_time, serve_time));
    }
  }
} // generate_buyers()


void sell_tickets(Seller *seller) {
  vector<vector<int>> seats = *(seller->seats);

  if (seller->type == 'H') {
    for (int row = 0; row < NUM_ROWS; row++) {
      for (int seat = 0; seat < NUM_SEATS_PER_ROW; seat++) {
        if (seats[row][seat] == 0) {
          // TODO
          // change seats to vector<string>
          //seats[row][seat] = seller.type + row + seat;
          seats[row][seat] = 1;
          return;
        }
      }
    }  
  } else if (seller->type == 'M') {
    // 5, 6, 4, 7, 8, 3, 9, 10, 2, ???
      for (int row = 5; row < NUM_ROWS; row++) {
        for (int seat = 0; seat < NUM_SEATS_PER_ROW; seat++) {
          if (seats[row][seat] == 0) {
            // TODO
            // change seats to vector<string>
            //seats[row][seat] = seller.type + row + seat;
            seats[row][seat] = 1;
            return;
          }
        }
      }
      for (int row = 4; row >= 0; row--) {
        for (int seat = 0; seat < NUM_SEATS_PER_ROW; seat++) {
          if (seats[row][seat] == 0) {
            // TODO
            // change seats to vector<string>
            //seats[row][seat] = seller.type + row + seat;
            seats[row][seat] = 1;
            return;
          }
        }
      }

  } else if (seller->type == 'L') {
      for (int row = NUM_ROWS - 1; row >= 0; row--) {
        for (int seat = 0; seat < NUM_SEATS_PER_ROW; seat++) {
          if (seats[row][seat] == 0) {
            // TODO
            // change seats to vector<string>
            //seats[row][seat] = seller.type + row + seat;
            seats[row][seat] = 1;
            return;
          }
        }
      }  
  }
} // sell_tickets()



// seller thread to serve one time slice (1 minute)
void *sell(void *seller) {
  while (*Seller::clock < HOUR) {
    pthread_mutex_lock(&mutex);
    
    // atomically release mutex and wait on cond until somebody does signal or broadcast. 
    // when you are awaken as a result of signal or broadcast, you acquire the mutex again. 
    pthread_cond_wait(&cond, &mutex);
    Buyer buyer = (*Seller::buyer_queue)[((Seller *)seller)->id].top(); 
    if (buyer.arrival_time + buyer.serve_time < *Seller::clock) {
      (*Seller::clock)++;
      pthread_cond_signal(&cond);
    } else {
      //clock++;
      //buyer_queue[seller.id].pop();
      sell_tickets(((Seller *)seller));
    } 

    pthread_mutex_unlock(&mutex);
    // Serve any buyer available in this seller queue that is ready
    // now to buy ticket till done with all relevant buyers in their queue ..................
  }
    return NULL; // thread exits 
} // sell()



void wakeup_all_seller_threads() {
  // get the lock to have predictable scheduling
  pthread_mutex_lock(&mutex);
  // wakeup all threads waiting on the cond variable
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mutex); 
} // wakeup_all_seller_threads()


int main(int argc, char* argv[]) {
  int i;
  pthread_t tids[10]; 
  char seller_type;

  // Create data structure for the seats
  // 0: empty seat
  // 1: seat occupied
  vector<vector<int>> seats(NUM_ROWS, vector<int>(NUM_SEATS_PER_ROW,0)); 
  Seller::seats = &seats;

  // data structure for buyer queues
  //vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> buyer_queue(NUM_SELLERS, priority_queue<Buyer, vector<Buyer>, mycomparison>);
  //vector<priority_queue<Buyer>> buyer_queue(NUM_SELLERS, priority_queue<Buyer, vector<Buyer>, mycomparison>);
  vector<priority_queue<Buyer, vector<Buyer>, mycomparison>> buyer_queue(NUM_SELLERS);
  for (int seller = 0; seller < NUM_SELLERS; seller++) {
    buyer_queue.push_back(priority_queue<Buyer, vector<Buyer>, mycomparison>());
  }
  Seller::buyer_queue = &buyer_queue;
  generate_buyers(buyer_queue);
  
  // Clock variable used to determine minutes elapsed
  int clock = 0;
  Seller::clock = &clock;
 
  vector<Seller> sellers;
 
  // Create necessary data structures for the simulator.
  // Create buyers list for each seller ticket queue based on the 
  // N value within an hour and have them in the seller queue.
  // Create 10 threads representing the 10 sellers. 
  seller_type = 'H';
  sellers.push_back(Seller(seller_type, 0));
  pthread_create(&tids[0], NULL, sell, (void *)&sellers[0]);
  
  seller_type = 'M'; 
  for (i = 1; i < 4; i++) {
    sellers.push_back(Seller(seller_type, i));
    pthread_create(&tids[i], NULL, sell, (void *)&sellers[i]);
  }  

  seller_type = 'L';
  for (i = 4; i < 10; i++) {
    sellers.push_back(Seller(seller_type, i));
    pthread_create(&tids[i], NULL, sell, (void *)&sellers[i]);
  }


  // wakeup all seller threads
  wakeup_all_seller_threads();

  // wait for all seller threads to exit
  for (i = 0 ; i < 10 ; i++) 
    pthread_join(tids[i], NULL);

  // Printout simulation results
  // ............
  exit(0); 


} // main ()
