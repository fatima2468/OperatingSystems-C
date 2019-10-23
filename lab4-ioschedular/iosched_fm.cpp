#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <vector>
#include <list>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

using namespace std;


long track = 0;
int tot_movement = 0;
int total_time = 0;

struct IO_Request {
  int ioReqNum;
  int timeStep;
  int trackToAccess;
  int startTime;
  int endTime;

  IO_Request() {}

  IO_Request(int requestNum, int timeStp, int trackAccess) :
    ioReqNum(requestNum),
    timeStep(timeStp),
    trackToAccess(trackAccess),
    startTime(0),
    endTime(0) {}
};


class SCHEDULER {
     public: list<struct IO_Request*> ioQueue;

     virtual void add_IORequest(IO_Request* ioRequest) = 0;
     virtual IO_Request* get_next_IORequest() = 0;
};

class FIFO : public SCHEDULER {
public:
    void add_IORequest(IO_Request* ioRequest) {
      ioQueue.push_back(ioRequest);
    }

    IO_Request* get_next_IORequest() {
      IO_Request* ioRequest = ioQueue.front();
      if (ioRequest != nullptr){
        ioQueue.pop_front();
      }
      return ioRequest;
    }
};

class SSTF : public SCHEDULER {
  public:
  void add_IORequest(IO_Request* ioRequest) {
    ioQueue.push_back(ioRequest);
  }

  IO_Request* get_next_IORequest() {
    IO_Request* ioRequest = nullptr;
    int shortestSeekTime = 0;
    bool firstRequestInQueue = true;

    list<struct IO_Request*>::iterator io_it = ioQueue.begin();
    for (io_it = ioQueue.begin(); io_it != ioQueue.end(); ++io_it){
      int seekTime = std::abs(track - (*io_it)->trackToAccess);
      if(firstRequestInQueue){
        shortestSeekTime = seekTime;
        ioRequest = (*io_it);
        firstRequestInQueue = false;
      }

      if (seekTime < shortestSeekTime) {
        shortestSeekTime  = seekTime;
        ioRequest = (*io_it);
      }
    }

    if (ioRequest != nullptr){
      ioQueue.remove(ioRequest);
    }

    return ioRequest;
  }
};

class LOOK : public SCHEDULER {
  int direction;

  public:
    LOOK() :
    direction(1) {}

  private:
    IO_Request* goForwardDirection(){
      IO_Request* ioRequest = nullptr;
      int nextNearestTrack = 0;
      bool firstRequestHit = true;

      list<struct IO_Request*>::iterator io_it = ioQueue.begin();
      for (io_it = ioQueue.begin(); io_it != ioQueue.end(); ++io_it) {
          int nextTrack = (*io_it)->trackToAccess;
          if(nextTrack >= track){
            int seekTime = std::abs(track - nextTrack);
            if(firstRequestHit){
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
              firstRequestHit = false;
            }

            if (seekTime < nextNearestTrack) {
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
            }
         }
      }

      return ioRequest;
    }

  private:
    IO_Request* goBackwardDirection() {
      IO_Request* ioRequest = nullptr;
      int nextNearestTrack = 0;
      bool firstRequestHit = true;

      list<struct IO_Request*>::iterator io_it = ioQueue.begin();
      for (io_it = ioQueue.begin(); io_it != ioQueue.end(); ++io_it){
          int nextTrack = (*io_it)->trackToAccess;
          if(nextTrack <= track){
          int seekTime = std::abs(track - nextTrack);
           if(firstRequestHit){
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
              firstRequestHit = false;
           }

          if(seekTime < nextNearestTrack) {
             nextNearestTrack = seekTime;
             ioRequest = (*io_it);
          }
        }
      }

    return ioRequest;
  }

  public:
  void add_IORequest(IO_Request* ioRequest) {
    ioQueue.push_back(ioRequest);
  }

  IO_Request* get_next_IORequest() {
      IO_Request* ioRequest = (direction == 1) ? goForwardDirection() : goBackwardDirection();

      if(ioRequest == nullptr){
        direction = (direction == 1) ? -1 : 1; //changeDirection
        ioRequest = (direction == 1) ? goForwardDirection() : goBackwardDirection();
      }

      if(ioRequest != nullptr){
        ioQueue.remove(ioRequest);
      }

    return ioRequest;
  }
};

class CLOOK : public SCHEDULER {
public:
  void add_IORequest(IO_Request* ioRequest) {
    ioQueue.push_back(ioRequest);
  }

private:
  IO_Request* goForwardDirection(int tempTrack){
    IO_Request* ioRequest = nullptr;
    int nextNearestTrack = 0;
    bool firstRequestHit = true;

    list<struct IO_Request*>::iterator io_it = ioQueue.begin();
    for (io_it = ioQueue.begin(); io_it != ioQueue.end(); ++io_it) {
        int nextTrack = (*io_it)->trackToAccess;
        if(nextTrack >= tempTrack){
          int seekTime = std::abs(tempTrack - nextTrack);
          if(firstRequestHit){
            nextNearestTrack = seekTime;
            ioRequest = (*io_it);
            firstRequestHit = false;
          }

          if (seekTime < nextNearestTrack) {
            nextNearestTrack = seekTime;
            ioRequest = (*io_it);
          }
       }
    }

    return ioRequest;
  }

private:
  IO_Request* get_next_IORequest() {
    IO_Request* ioRequest = nullptr;
    ioRequest = goForwardDirection(track);

    if(ioRequest == nullptr){
      ioRequest = goForwardDirection(0);
    }

    if(ioRequest != nullptr){
      ioQueue.remove(ioRequest);
    }
    return ioRequest;
  }
};

class FLOOK : public SCHEDULER {
  private: list<struct IO_Request*>* activeIoQueue;
  private: list<struct IO_Request*>* inActiveIoQueue;
  private: int direction;

  public:
    FLOOK() :
    direction(1) {
      activeIoQueue = new list<struct IO_Request*>;
      inActiveIoQueue = new list<struct IO_Request*>;
    }

  private:
    IO_Request* goForwardDirection(){
      IO_Request* ioRequest = nullptr;
      int nextNearestTrack = 0;
      bool firstRequestHit = true;

      list<struct IO_Request*>::iterator io_it = activeIoQueue->begin();
      for (io_it = activeIoQueue->begin(); io_it != activeIoQueue->end(); ++io_it) {
          int nextTrack = (*io_it)->trackToAccess;
          if(nextTrack >= track){
            int seekTime = std::abs(track - nextTrack);
            if(firstRequestHit){
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
              firstRequestHit = false;
            }

            if (seekTime < nextNearestTrack) {
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
            }
         }
      }
      return ioRequest;
    }

  private:
    IO_Request* goBackwardDirection(){
      IO_Request* ioRequest = nullptr;
      int nextNearestTrack = 0;
      bool firstRequestHit = true;

      list<struct IO_Request*>::iterator io_it = activeIoQueue->begin();
      for (io_it = activeIoQueue->begin(); io_it != activeIoQueue->end(); ++io_it){
          int nextTrack = (*io_it)->trackToAccess;
          if(nextTrack <= track){
          int seekTime = std::abs(track - nextTrack);
           if(firstRequestHit){
              nextNearestTrack = seekTime;
              ioRequest = (*io_it);
              firstRequestHit = false;
           }

          if(seekTime < nextNearestTrack) {
             nextNearestTrack = seekTime;
             ioRequest = (*io_it);
          }
        }
      }

    return ioRequest;
  }

  public:
  void add_IORequest(IO_Request* ioRequest) {
    inActiveIoQueue->push_back(ioRequest);
  }

   IO_Request* get_next_IORequest() {
    if(activeIoQueue->front() == nullptr) {
      //Swaping Queues
      list<struct IO_Request*>* temp = activeIoQueue;
      activeIoQueue = inActiveIoQueue;
      inActiveIoQueue = temp;
    }

    IO_Request* ioRequest = (direction == 1) ? goForwardDirection() : goBackwardDirection();
    if(ioRequest == nullptr){
       direction = (direction == 1) ? -1 : 1; //changeDirection
       ioRequest = (direction == 1) ? goForwardDirection() : goBackwardDirection();
    }

    if(ioRequest != nullptr){
       activeIoQueue->remove(ioRequest);
    }

    return ioRequest;
  }
};

SCHEDULER *THE_SCHEDULER;

bool v_flag = false;
bool q_flag = false;
bool f_flag = false;
int crrntTimeStamp = 1;
int totalRequests = 0;
int allRequestsProcessed = 0;
IO_Request* CURRENT_RUNNING_REQUEST;
list<struct IO_Request*> ioRequestList;
list<struct IO_Request*> finishedIORequestList;


bool isComment(string line) {
  bool isCommntedLine = false;
  if(line.empty() || (!line.empty() && line.at(0) == '#')){
    isCommntedLine = true;
  }
  return isCommntedLine;
}


IO_Request *get_request() {
  IO_Request *request = ioRequestList.front();
  if(request != nullptr){
    finishedIORequestList.push_back(request);
    ioRequestList.pop_front();
  }
  return request;
}


int get_next_request_time() {
  int ts = -1;
  if(nullptr != ioRequestList.front()) {
    ts = ioRequestList.front()->timeStep;
  }
  return ts;
}


// Loads Data from input.txt of all IO_Requests
void loadIORequests(char* inputFile) {
  // Reading Process File...
  ifstream ifsIOFile;
  ifsIOFile.open(inputFile, ios::in);

  if (ifsIOFile.is_open()) {
    string line;
    int timeStep;
    int trackAccessTo;
    int requestNum = 0;

    while (getline(ifsIOFile, line)) {
        if(!isComment(line)) {
          stringstream linestream(line);
          linestream >> timeStep >> trackAccessTo;
          struct IO_Request *ioRequest = new IO_Request(requestNum, timeStep, trackAccessTo);
          ioRequestList.push_back(ioRequest);
          requestNum++;
        }
    }
    totalRequests = requestNum;
  }
  ifsIOFile.close();
}


void processsIORequests() {
   bool allRequestsProcessed = false;
   int numOfRequestsProcessed = 0;

   while(true) {
     if(CURRENT_RUNNING_REQUEST != nullptr) {
        if(CURRENT_RUNNING_REQUEST->trackToAccess == track){
        // current request finished IO
        crrntTimeStamp--;
        CURRENT_RUNNING_REQUEST->endTime = crrntTimeStamp;

        total_time = CURRENT_RUNNING_REQUEST->endTime;
        CURRENT_RUNNING_REQUEST = nullptr;
        numOfRequestsProcessed++;
      }
      else {
          if(CURRENT_RUNNING_REQUEST->trackToAccess > track){ track++; }
          else { track--; }
        }
      }

      if (get_next_request_time() == crrntTimeStamp) {
          THE_SCHEDULER->add_IORequest(get_request());
      }

      if(CURRENT_RUNNING_REQUEST == nullptr) {
        CURRENT_RUNNING_REQUEST = THE_SCHEDULER->get_next_IORequest();

        if (CURRENT_RUNNING_REQUEST != nullptr) {
            CURRENT_RUNNING_REQUEST->startTime = crrntTimeStamp;
            tot_movement = tot_movement + std::abs(CURRENT_RUNNING_REQUEST->trackToAccess - track);
        }
      }

    crrntTimeStamp++;
    if(numOfRequestsProcessed == totalRequests){
      break;  // break for while(true)
    }
  }
}


void printIORequestsSummary() {
  double total_avg_turnaround = 0;
  double total_avg_waitTime = 0;
  double avg_turnaround = 0;
  double avg_waittime = 0;
  int max_waittime = 0;

  list<struct IO_Request*>::iterator io_it = finishedIORequestList.begin();
  for (io_it = finishedIORequestList.begin(); io_it != finishedIORequestList.end(); ++io_it){
    printf("%5d: %5d %5d %5d\n", (*io_it)->ioReqNum, (*io_it)->timeStep, (*io_it)->startTime, (*io_it)->endTime);

    total_avg_turnaround = total_avg_turnaround + ((*io_it)->endTime - (*io_it)->timeStep);
    int waittime = (*io_it)->startTime - (*io_it)->timeStep;
    if (max_waittime < waittime) {
      max_waittime = waittime;
    }
    total_avg_waitTime = total_avg_waitTime + waittime;
  }
  avg_turnaround = total_avg_turnaround/finishedIORequestList.size();
  avg_waittime = total_avg_waitTime/finishedIORequestList.size();
  printf("SUM: %d %d %.2lf %.2lf %d\n", total_time, tot_movement, avg_turnaround, avg_waittime, max_waittime);
}

int main(int argc, char **argv){
  string sched;
  int option;

  while ((option = getopt (argc, argv, "s:vqf")) != -1) {
    switch (option) {
      case 's':
        sched = optarg;
        if(sched.length() >= 1) {
           char schd_policy = sched.at(0);
           switch (schd_policy) {
              case 'i':
                THE_SCHEDULER = new FIFO();
              break;
              case 'j':
                THE_SCHEDULER = new SSTF();
              break;
              case 's':
                THE_SCHEDULER = new LOOK();
              break;
              case 'c':
                THE_SCHEDULER = new CLOOK();
              break;
              case 'f':
                THE_SCHEDULER = new FLOOK();
              break;
           }
         }
        break;
      case 'v':
        v_flag = true;
      case 'q':
        q_flag = true;
      case 'f':
        f_flag = true;
        break;
      default:
        abort ();
    }
  }

  loadIORequests(argv[optind]);
  processsIORequests();
  printIORequestsSummary();
}
