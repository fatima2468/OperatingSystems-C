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

/* @author: Fatima Mushtaq
 * Schedular with Event Driven Approach -- Part of Operating Systems Course Labs
 */

using namespace std;

int processNum = 0;
string schedulingPolicy;

struct Process {
  int arrivalTime;
  int totalCpuTime;
  int cpuBurst;
  int ioBurst;
  int remainingTime;
  int timeInPrevState;
  int state;
  int state_ts;
  int static_priority;
  int dynamic_priority;
  int p_id;
  int finishing_time;
  int io_time;
  int cw_time;
  int currentCpuBurst;

  Process(int arrvlTime, int tlCpuTime, int cpuBrst, int ioBrst, int static_prior) :
  arrivalTime(arrvlTime),
  totalCpuTime(tlCpuTime),
  cpuBurst(cpuBrst),
  ioBurst(ioBrst),
  remainingTime(tlCpuTime),
  static_priority(static_prior),
  dynamic_priority(static_prior-1),
  timeInPrevState(0),
  state(0),
  state_ts(arrvlTime),
  p_id(processNum),
  finishing_time(0),
  io_time(0),
  cw_time(0),
  currentCpuBurst(0)
  {}
};

struct EVENT {
  int evtTimeStamp;
  struct Process* evtProcess;
  int transition;

  EVENT (int timeStamp, struct Process* process, int transTo) :
  evtTimeStamp(timeStamp),
  evtProcess(process),
  transition(transTo) {}
};

bool verbose = false;
bool CALL_SCHEDULER = false;
Process* CURRENT_RUNNING_PROCESS;
int CURRENT_TIME;
enum TRANSITION { TRANS_TO_READY = 1, TRANS_TO_RUN = 2, TRANS_TO_BLOCK = 3, TRANS_TO_PREEMPT = 4 };
enum STATES { CREATED = 0, READY = 1, RUNNING = 2, BLOCKED = 3, };
int quantum;
int processInBlockState = 0;
int blockStateActiveStTime = -1;
int blockStateActiveEndTime = -1;
int totalBlockStateActivity = 0;

string getState(int state_id){
  string state;
  switch (state_id) {
    case 0:
       state = "CREATED";
    break;
    case 1:
       state = "READY";
    break;
    case 2:
       state = "RUNNG";
    break;
    case 3:
       state = "BLOCK";
    break;
    default:
    break;
  }
  return state;
}
class SCHEDULER {
     public: list<struct Process*> readyQueue;

     virtual void add_process(Process* proc) = 0;
     virtual Process* get_next_process() = 0;
};

class FCFS : public SCHEDULER {
public:
    void add_process(Process* proc) {
      readyQueue.push_back(proc);
    }

    Process* get_next_process() {
      Process* proc = readyQueue.front();
      if (proc != nullptr){
        readyQueue.pop_front();
      }
      return proc;
    }
};

class LCFS : public SCHEDULER {
public:
    void add_process(Process* proc) {
      readyQueue.push_back(proc);
    }

    Process* get_next_process() {
      Process* proc = readyQueue.back();
      if (proc != nullptr){
        readyQueue.pop_back();
      }
      return proc;
    }
};

class SJF : public SCHEDULER {
public:
    void add_process(Process* proc) {
      readyQueue.push_back(proc);
    }

    Process* get_next_process() {
      Process* proc = readyQueue.front();

      list<struct Process*> ::iterator p_it = readyQueue.begin();
      for (; p_it != readyQueue.end(); ++p_it){
          if((*p_it)->remainingTime < proc->remainingTime){
            proc = (*p_it);
          }
      }

      if (proc != nullptr) {
        readyQueue.remove(proc);
      }
      return proc;
    }
};

class RR : public SCHEDULER {
public:
    void add_process(Process* proc) {
      readyQueue.push_back(proc);
    }

    Process* get_next_process() {
      Process* proc = readyQueue.front();
      if (proc != nullptr){
        readyQueue.pop_front();
      }
      return proc;
    }
};

class PRIO : public SCHEDULER {
  public: list<struct Process*>* activeQueue;
  public: list<struct Process*>* expiredQueue;

public:
    PRIO() {
      activeQueue = new list<struct Process*>[4];
      expiredQueue = new list<struct Process*>[4];
    }

    void add_process(Process* proc) {
      if(proc->dynamic_priority == -1) {
        proc->dynamic_priority = proc->static_priority-1;
        expiredQueue[proc->dynamic_priority].push_back(proc);
      }
      else {
        activeQueue[proc->dynamic_priority].push_back(proc);
      }
    }

    Process* get_next_process() {
      Process* proc =  activeQueue[3].front();

      for(int k = 0; k < 2; k++) {
        for (int i = 3; i >= 0; i--) {
            proc = activeQueue[i].front();
            if (proc != nullptr) {
              //
              activeQueue[i].pop_front();
              return proc;
            }
        }

        //Swaping Queues
        list<struct Process *>* temp = activeQueue;
        activeQueue = expiredQueue;
        expiredQueue = temp;
    }
    return proc;
  }
};

SCHEDULER *THE_SCHEDULER;

list<struct EVENT*> events;
vector<struct Process*> processList;
vector<long long int> randNums;
int totalRandNums;
int rCount = 0;

bool addEvent(EVENT *event){
  bool evntAdded = false;

  if(events.empty()){
     events.push_back(event);
     evntAdded = true;
  }
  else {
     if(event->evtTimeStamp >= events.back()->evtTimeStamp){
       events.push_back(event);
       evntAdded = true;
     }
     else {
       list<struct EVENT*>::iterator e_it = events.begin();
       for (e_it = events.begin(); e_it != events.end(); ++e_it){
           if((*e_it)->evtTimeStamp > event->evtTimeStamp){
             break;
           }
       }
       events.insert(e_it, event);
       evntAdded  = true;
     }
  }
  return evntAdded;
}

EVENT* get_event() {
  EVENT* evt = events.front();
  if (evt != nullptr){
    events.pop_front();
  }
  return evt;
}

int get_next_event_time() {
  int ts = -1;
  if(nullptr != events.front()) {
    ts = events.front()->evtTimeStamp;
  }
  return ts;
}

long long int getRandomNumber(){
   long long int rNum = randNums.at(rCount);
   rCount++;
   if(rCount >= totalRandNums) {
      rCount = 0;
   }
   return rNum;
}

int randumBurstAt(int burstTime){
   return 1 + (getRandomNumber() % burstTime);
}

// Loads Data from input.txt for Processes and randomNumber from rFile.txt
void _loadProcessData(char* inputFile, char* rFile) {
  // Reading rFile first...
  ifstream ifsRandFile;
  ifsRandFile.open(rFile, ios::in);

  bool isFirstLine = true;
  long long int randNum;
  int count = 0;

  if (ifsRandFile.is_open()){
    string line;

    while ( getline(ifsRandFile, line) ) {
        //cout << line << '\n';
        stringstream linestream(line);
        linestream >> randNum;

        if(isFirstLine) {
          totalRandNums = randNum;
          isFirstLine = false;
        }
        else {
          randNums.push_back(randNum);
          count++;
        }

        if(randNums.size() >= totalRandNums) break;
      }
  }
  ifsRandFile.close();

  // Reading Process File...
  ifstream ifsProcFile;
  ifsProcFile.open(inputFile, ios::in);

  if (ifsProcFile.is_open()) {
    string line;
    int arrvlTime;
    int tlCpuTime;
    int cpuBurst;
    int ioBurst;
    int static_prior;

    while (getline(ifsProcFile, line)) {
      stringstream linestream(line);

      linestream >> arrvlTime >> tlCpuTime >> cpuBurst >> ioBurst;
      static_prior = randumBurstAt(4);
      struct Process *proc = new Process(arrvlTime, tlCpuTime, cpuBurst, ioBurst, static_prior);
      processList.push_back(proc);
      processNum++;

      // create Event for the process as in state Ready
      struct EVENT *event = new EVENT(arrvlTime, proc, TRANS_TO_READY);
      addEvent(event);
    }
  }
  ifsProcFile.close();
}

void _startSimulation() {
  EVENT* evt;
  while( (evt = get_event())) {
        Process *proc = evt->evtProcess;
        CURRENT_TIME = evt->evtTimeStamp;
        evt->evtProcess->timeInPrevState = CURRENT_TIME - proc->state_ts;
        struct EVENT *next_event;
        int cpuBurstAt;
        int ioBurstAt;
        bool preempt = false;
        int newCpuBurst;
        int cb;

        switch(evt->transition) {

        case TRANS_TO_READY:
              // must come from BLOCKED or from PREEMPTION
              if(blockStateActiveStTime != -1 && proc->state != 0) {
                processInBlockState--;

                if(processInBlockState == 0) {
                  blockStateActiveEndTime = CURRENT_TIME;
                  totalBlockStateActivity = totalBlockStateActivity + (blockStateActiveEndTime - blockStateActiveStTime);
                }
              }

              if(verbose){
                cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " << getState(proc->state) << " -> READY" << endl;
              }
              proc->state_ts = CURRENT_TIME;
              proc->io_time = proc->io_time + proc->timeInPrevState;
              proc->state = READY;
              THE_SCHEDULER->add_process(proc); // must add to run queue
              CALL_SCHEDULER = true; // conditional on whether something is run
              break;

        case TRANS_TO_RUN:
              // create event for either preemption or blocking
              proc->state_ts = CURRENT_TIME;
              if(quantum != 0) {
                  newCpuBurst = (proc->currentCpuBurst == 0) ? randumBurstAt(proc->cpuBurst) : proc->currentCpuBurst;

                  if(proc->remainingTime < newCpuBurst) {
                    newCpuBurst = proc->remainingTime;
                    cb = proc->remainingTime;
                  }

                  if(quantum < newCpuBurst) {
                      preempt = true;
                      cpuBurstAt = quantum;
                  }
                  else { cpuBurstAt = newCpuBurst; }

                  proc->currentCpuBurst = newCpuBurst - cpuBurstAt;
                  cb = newCpuBurst;
              }
              else {
                cpuBurstAt = randumBurstAt(proc->cpuBurst);
                cb = cpuBurstAt;

                if(proc->remainingTime < cpuBurstAt) {
                  cpuBurstAt = proc->remainingTime;
                  cb = proc->remainingTime;
                }
              }

              if(verbose){
                cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " << getState(proc->state)
                << " -> RUNNG" << " cb=" << cb << " rem=" << proc->remainingTime << " prio=" << proc->dynamic_priority
                << endl;
              }
              proc->state = RUNNING;
              proc->cw_time = proc->cw_time + proc->timeInPrevState;
              proc->remainingTime = proc->remainingTime - (cpuBurstAt);

              next_event = (preempt) ? new EVENT(CURRENT_TIME + cpuBurstAt, proc, TRANS_TO_PREEMPT) : new EVENT(CURRENT_TIME + cpuBurstAt, proc, TRANS_TO_BLOCK);
              addEvent(next_event);
              break;

        case TRANS_TO_BLOCK:
              //create an event for when process becomes READY again
              proc->dynamic_priority = proc->static_priority-1;
              if(proc->remainingTime == 0) {
                  proc->finishing_time = CURRENT_TIME;
                  if(processInBlockState == 0) {
                      blockStateActiveStTime = -1;
                  }
                  if(verbose) { cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " <<  "Done" << endl; }
              }
              else {
                if(processInBlockState == 0) {
                  blockStateActiveStTime = CURRENT_TIME;
                }
                processInBlockState++;

                ioBurstAt = randumBurstAt(proc->ioBurst);
                if(verbose) {
                  cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " << getState(proc->state)
                  << " -> BLOCK" << "  ib=" << ioBurstAt << " rem=" << proc->remainingTime << endl;
                }
                proc->state_ts = CURRENT_TIME;
                proc->state = BLOCKED;
                next_event = new EVENT(CURRENT_TIME + ioBurstAt, proc, TRANS_TO_READY);
                addEvent(next_event);
              }
              CURRENT_RUNNING_PROCESS = nullptr;
              CALL_SCHEDULER = true;
              break;

        case TRANS_TO_PREEMPT:
              // add to runqueue (no event is generated)
              if(proc->remainingTime == 0) {
                  proc->finishing_time = CURRENT_TIME;
                  if(verbose){ cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " <<  "Done" << endl; }
              }
              else {
                if(verbose){
                  cout << CURRENT_TIME << " " << proc->p_id << " " << proc->timeInPrevState << ": " << getState(proc->state)
                  << " -> READY" << "  cb=" << proc->currentCpuBurst << " rem=" << proc->remainingTime << " prio=" << proc->dynamic_priority << endl;
                }
                proc->dynamic_priority = proc->dynamic_priority-1;
                proc->state = READY;
                proc->state_ts = CURRENT_TIME;
                THE_SCHEDULER->add_process(proc);  // add to runqueue (no event is generated)
              }
              CURRENT_RUNNING_PROCESS = nullptr;
              CALL_SCHEDULER = true;
              break;
        }

        //remove current event object from Memory
        delete evt;
        evt = nullptr;

        if(CALL_SCHEDULER) {
          if (get_next_event_time() == CURRENT_TIME) {
              continue; //process next event from Event queue
            }
          CALL_SCHEDULER = false;

          if (CURRENT_RUNNING_PROCESS == nullptr) {
            CURRENT_RUNNING_PROCESS = THE_SCHEDULER->get_next_process();
            if (CURRENT_RUNNING_PROCESS == nullptr) {
                continue;
              }

           // Create a new event with TRANS_TO_RUNNING for the process that you got from the scheduler (which is the CURRENT_RUNNING_PROCESS)
           next_event = new EVENT(CURRENT_TIME, CURRENT_RUNNING_PROCESS, TRANS_TO_RUN);
           addEvent(next_event);
         }
       }
    }
  }

bool isValidNumber(string text){
  bool validNum = true;

  for(int i = 0; i < text.length(); i++){
      if (!isdigit(text[i])) {
          validNum = false;
          break;
      }
  }
  return validNum;
}

void _printReportSummary(){
   double totalCpuTime = 0;
   double totalTurnAround = 0;
   double totalCpuWait = 0;
   int turnAroundTime;
   int totalProcess = processList.size();

   cout << schedulingPolicy;
   if (schedulingPolicy == "RR" || schedulingPolicy == "PRIO") {
         cout << " " << quantum;
   }
   cout << endl;

   vector<struct Process*> ::iterator p_it = processList.begin();
   int lastProcFnshTime = (*p_it)->finishing_time;

   for (; p_it != processList.end(); ++p_it) {
     turnAroundTime = (*p_it)->finishing_time - (*p_it)->arrivalTime;

     printf("%04d: %4d %4d %4d %4d %1d | %5d %5d %5d %5d\n", (*p_it)->p_id, (*p_it)->arrivalTime, (*p_it)->totalCpuTime, (*p_it)->cpuBurst, (*p_it)->ioBurst, (*p_it)->static_priority,
     (*p_it)->finishing_time, turnAroundTime, (*p_it)->io_time, (*p_it)->cw_time);

     totalCpuTime = totalCpuTime +  (*p_it)->totalCpuTime;
     totalTurnAround = totalTurnAround + turnAroundTime;
     totalCpuWait = totalCpuWait + (*p_it)->cw_time;

     if((*p_it)->finishing_time > lastProcFnshTime){
       lastProcFnshTime = (*p_it)->finishing_time;
     }
   }

   double cpuUtilization = ((double)totalCpuTime/(double)lastProcFnshTime)*100;
   double ioUtilization = ((double)totalBlockStateActivity/(double)lastProcFnshTime)*100;
   double throughPut = ((double)totalProcess/(double)lastProcFnshTime)*100;
   printf("SUM: %d %.2lf %.2lf %.2lf %.2lf %.3lf\n", lastProcFnshTime, cpuUtilization, ioUtilization, (double)totalTurnAround/(double)totalProcess, (double)totalCpuWait/(double)totalProcess, throughPut);
}


int main(int argc, char **argv){
  int s_flag = 0;
  int v_flag = 0;
  string sched;
  string quant;

  int index;
  int option;

  while ((option = getopt (argc, argv, "s:v")) != -1) {
    switch (option) {
      case 's':
        sched = optarg;
        s_flag = 1;
        if(sched.length() >= 1) {
           char schd_policy = sched.at(0);
           switch (toupper(schd_policy)) {
              case 'F':
                schedulingPolicy = "FCFS";
                THE_SCHEDULER = new FCFS();
              break;
              case 'L':
                schedulingPolicy = "LCFS";
                THE_SCHEDULER = new LCFS();
              break;
              case 'S':
                schedulingPolicy = "SJF";
                THE_SCHEDULER = new SJF();
              break;
              case 'P':
              case 'R':
                if(toupper(schd_policy) == 'P') {
                  THE_SCHEDULER = new PRIO();
                  schedulingPolicy = "PRIO";
                } else {
                  THE_SCHEDULER = new RR();
                  schedulingPolicy = "RR";
                }
                quant = sched.substr(1, sched.length());
                if(isValidNumber(quant)){
                  quantum = atoi(quant.c_str());
              }
              break;
           }
         }
        break;
      case 'v':
        v_flag = 1;
        verbose = true;
        break;
      case '?':
        if (optopt == 'R' || optopt == 'P')
          fprintf (stderr, "Option '-%c'requires an argument.\n", optopt);
        return 1;
      default:
        abort ();
      }
  }

  if((schedulingPolicy == "PRIO" || schedulingPolicy == "RR") && quantum == 0){ }
  else {
    _loadProcessData(argv[optind], argv[optind+1]);
    _startSimulation();
    _printReportSummary();
  }

}
