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

vector<long long int> randNums;
int totalRandNums;
int rCount = 0;
int numOfFrames = 128;
int freeFrame = 0;
long instCounter = 0;
long ctx_switches = 0;
bool freeFrameAllocation = true;

bool O_Option = false;
bool P_Option = false;
bool F_Option = false;
bool S_Option = false;
bool x_Option = false;
bool y_Option = false;
bool f_Option = false;
bool a_Option = false;

bool isValidNumber(const char *text){
  bool validNum = true;

  for(int i = 0; i < strlen(text); i++){
      if (!isdigit(text[i])) {
          validNum = false;
          break;
      }
  }
  return validNum;
}

long long int getRandomNumber() {
   long long int rNum = randNums.at(rCount);
   rCount++;
   if(rCount >= totalRandNums) {
      rCount = 0;
   }
   return rNum;
}

int getRandomFrame(int numOfFrames){
   return getRandomNumber() % numOfFrames;
}

int readNumber(const char *input) {
  if(NULL == input){
    return -1;
  }
  else {
      if(isValidNumber(input)){
        return atoi(input);
      }
      else { return -1; }
    }
  }

struct Frame {
  int frameNo;
  int processId;
  int pageTableEntry;

  Frame() : frameNo(-1), processId(-1), pageTableEntry(-1) {};
};

struct VMA {
  int start_vpage;
  int end_vpage;
  int write_prtctd;
  int file_mapped;

  VMA(int startVpage, int endVpage, int writePrtctd, int fileMapped) :
    start_vpage(startVpage),
    end_vpage(endVpage),
    write_prtctd(writePrtctd),
    file_mapped(fileMapped)
    {}
};

struct PTE {
    unsigned int valid_bit : 1;
    unsigned int write_prtctd_bit : 1;
    unsigned int modified_bit : 1;
    unsigned int referenced_bit : 1;
    unsigned int paged_out_bit : 1;
    unsigned int frame_num:7;

    PTE() : valid_bit(0),
    write_prtctd_bit(0),
    modified_bit(0),
    referenced_bit(0),
    paged_out_bit(0),
    frame_num(0)
    {}

    PTE(unsigned int v_bit, unsigned int wp_bit, unsigned int m_bit, unsigned int r_bit, unsigned int p_bit,  unsigned int frame_no) :
      valid_bit(v_bit),
      write_prtctd_bit(wp_bit),
      modified_bit(m_bit),
      referenced_bit(r_bit),
      paged_out_bit(p_bit),
      frame_num(frame_no) {}
};

struct PStats {
  long unmaps;
  long maps;
  long ins;
  long outs;
  long fins;
  long fouts;
  long zeros;
  long segv;
  long segprot;

  PStats() :
  unmaps(0),
  maps(0),
  ins(0),
  outs(0),
  fins(0),
  fouts(0),
  zeros(0),
  segv(0),
  segprot(0)
  {}
};

struct Process {
  int process_id;
  PTE pageTable[64];
  list<struct VMA*> virtualMemArea;
  PStats pstats;

  Process() {}

  Process(int procId, list<struct VMA*> vma) :
    process_id(procId),
    virtualMemArea(vma) {}
};

Process* CURRENT_RUNNING_PROCESS;
struct Frame* FRAME_TABLE;
vector<struct Process*> processList;

class Pager {
    public:
       virtual Frame* determine_victum_frame() = 0;
};

class FIFO : public Pager {
int freedFrame;

public:
  FIFO() :
  freedFrame(0) {}

public:
    Frame* determine_victum_frame() {
      Frame *victumframe = NULL;
      if(freedFrame >= numOfFrames) {
        freedFrame = 0;
      }
      victumframe = &FRAME_TABLE[freedFrame];
      freedFrame++;

      return victumframe;
    };
};

class SecondChance : public Pager {
  list<int> framesList;
  list<int> tempFramesList;
  public:
    void initializeFramesList(){
      for(int i = 0; i < numOfFrames; i++){
         framesList.push_back(i);
         tempFramesList.push_back(i);
      }
    }

  public:
    Frame* determine_victum_frame() {
      Frame *victumframe = NULL;
      int frameIndex = 0;

      while(victumframe == NULL) {
        for (list<int>::iterator f_it = framesList.begin(); f_it != framesList.end(); ++f_it) {
            int f = (*f_it);
            Frame *frame = &FRAME_TABLE[f];

            struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];
            if(pte.referenced_bit == 1){
              pte.referenced_bit = 0;
              processList.at(frame->processId)->pageTable[frame->pageTableEntry] = pte;
              tempFramesList.remove(f);
              tempFramesList.push_back(f);
              //cout << "moving on to next frame :: processid=" << frame->processId << ", pte=" << frame->pageTableEntry<< endl;
            }
            else if(pte.referenced_bit == 0) {
              victumframe = frame;
              frameIndex = f;
              break;
            }
          }
      }

      tempFramesList.remove(frameIndex);
      tempFramesList.push_back(frameIndex);
      framesList = tempFramesList;

      return victumframe;
    };
};

class Clock : public Pager {
int clockNeedleAtFrame;

public:
  Clock() :
  clockNeedleAtFrame(0) {}

public:
    Frame* determine_victum_frame() {
      Frame *victumframe = NULL;

      while(victumframe == NULL) {
        if(clockNeedleAtFrame >= numOfFrames) {
           clockNeedleAtFrame = 0;
        }
        for(int f = clockNeedleAtFrame; f < numOfFrames; f++) {
          Frame *frame = &FRAME_TABLE[f];

          struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];
          if(pte.referenced_bit == 1) {
            pte.referenced_bit = 0;
            processList.at(frame->processId)->pageTable[frame->pageTableEntry] = pte;
          }
          else if(pte.referenced_bit == 0) {
            victumframe = frame;
            clockNeedleAtFrame = f;
            break;
          }
          clockNeedleAtFrame++;
        }
      }

    clockNeedleAtFrame++;
    return victumframe;
    };
};

class Random : public Pager {

public:
    Frame* determine_victum_frame() {
      int rFrameNum = getRandomFrame(numOfFrames);
      return &FRAME_TABLE[rFrameNum];
    };
};

class Aging : public Pager {
  std::vector<unsigned int> framesAge;
  unsigned int age_bits : 32;
  unsigned int fage : 32;  // why does it not allow to declare this variable inside for loop

  public:
    void initializeFramesAge(){
      for(int i = 0; i < numOfFrames; i++){
         age_bits = 0;
         framesAge.push_back(age_bits);
      }
    }

public:
    Frame* determine_victum_frame() {
      int frameIndex = 0;
      int firstSmallest = 0;
      int youngestFrame = 0;

      for (vector<unsigned int>::iterator f_it = framesAge.begin(); f_it != framesAge.end(); ++f_it) {
          unsigned int age = (*f_it);
          age = age >> 1;
          Frame *frame = &FRAME_TABLE[frameIndex];
          struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];

          if(pte.referenced_bit == 1) {
            age = age |= 1 << 31;
            pte.referenced_bit = 0;
            processList.at(frame->processId)->pageTable[frame->pageTableEntry] = pte;
          }

          framesAge.at(frameIndex) = age;
          frameIndex++;
        }

        frameIndex = 0;
        youngestFrame = framesAge.at(0);
        for (vector<unsigned int>::iterator f_it = framesAge.begin(); f_it != framesAge.end(); ++f_it) {
            fage = (*f_it);

            if(fage < youngestFrame) {
              youngestFrame = fage;
              firstSmallest = frameIndex;
            }
            frameIndex++;
        }

      return &FRAME_TABLE[firstSmallest];
  };
};


class NRU : public Pager {
  std::vector<int> class0List;   // R=0, M=0
  std::vector<int> class1List;   // R=0, M=1
  std::vector<int> class2List;   // R=1, M=0
  std::vector<int> class3List;   // R=1, M=1
  long nru_counter;

  public :
    NRU() :
    nru_counter(0) {}

  private:
    void prepareLists(){
      class0List.clear();
      class1List.clear();
      class2List.clear();
      class3List.clear();

      for(int i = 0; i < numOfFrames; i++){
        Frame *frame = &FRAME_TABLE[i];
        struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];
        if(pte.referenced_bit == 0 && pte.modified_bit == 0){
          class0List.push_back(i);
        }
        else if(pte.referenced_bit == 0 && pte.modified_bit == 1){
          class1List.push_back(i);
        }
        else if(pte.referenced_bit == 1 && pte.modified_bit == 0){
          class2List.push_back(i);
        }
        else if(pte.referenced_bit == 1 && pte.modified_bit == 1){
          class3List.push_back(i);
        }
      }
    }

public:
    Frame* determine_victum_frame() {
      int rFrameNum = 0;
      int frameNum = 0;
      nru_counter++;
      prepareLists();

      if(!class0List.empty()){
          rFrameNum = getRandomFrame(class0List.size());
          frameNum = class0List.at(rFrameNum);
      }
      else if(!class1List.empty()){
          rFrameNum = getRandomFrame(class1List.size());
          frameNum = class1List.at(rFrameNum);
      }
      else if(!class2List.empty()){
        rFrameNum = getRandomFrame(class2List.size());
        frameNum = class2List.at(rFrameNum);
      }
      else if(!class3List.empty()){
          rFrameNum = getRandomFrame(class3List.size());
          frameNum = class3List.at(rFrameNum);
      }

      if(nru_counter % 10 == 0) { // At each 10th Page Replacement
        for(int i = 0; i < numOfFrames; i++) {
          Frame *frame = &FRAME_TABLE[i];
          struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];
          if(pte.referenced_bit == 1){
            pte.referenced_bit = 0;
            processList.at(frame->processId)->pageTable[frame->pageTableEntry] = pte;
          }
        }
      }
      return &FRAME_TABLE[frameNum];
  };
};


Pager *THE_PAGER;

Frame *allocate_frame_from_free_list(){
  Frame *frame = NULL;
  if(freeFrame < numOfFrames) {
     frame = &FRAME_TABLE[freeFrame];
     frame->frameNo = freeFrame;
     freeFrame++;
     freeFrameAllocation = true;
  }
  return frame;
}

Frame *getFrame() {
  Frame *frame = allocate_frame_from_free_list();
  if(frame == NULL){
    frame = THE_PAGER->determine_victum_frame();
    freeFrameAllocation = false;
  }
  return frame;
}

bool isComment(string line) {
  bool isCommntedLine = false;
  if(line.empty() || (!line.empty() && line.at(0) == '#')){
    isCommntedLine = true;
  }
  return isCommntedLine;
}

bool is_c_Instruction(string line) {
  bool is_c_inst = false;
  if((line.length() >= 3) && (line.at(0) == 'c') && (line.at(1) == ' ') && isValidNumber(line.substr(2, line.length()).c_str())) {
    is_c_inst = true;
  }
  return is_c_inst;
}

bool is_r_Instruction(string line) {
  bool is_r_inst = false;
  if((line.length() >= 3) && (line.at(0) == 'r') && (line.at(1) == ' ') && isValidNumber(line.substr(2, line.length()).c_str())) {
    is_r_inst = true;
  }
  return is_r_inst;
}

bool is_w_Instruction(string line) {
  bool is_w_inst = false;
  if((line.length() >= 3) && (line.at(0) == 'w') && (line.at(1) == ' ') && isValidNumber(line.substr(2, line.length()).c_str())) {
    is_w_inst = true;
  }
  return is_w_inst;
}

int extractVirtualPageNum(string line) {
    return atoi(line.substr(2, line.length()).c_str());
}

bool contains(string str, string val) {
  bool contained = false;
  if(str.find(val) != std::string::npos){
    contained = true;
  }
  return contained;
}

void contextSwitch(string line) {
    int cntxtSwtchToProc = atoi(line.substr(2, line.length()).c_str());
    CURRENT_RUNNING_PROCESS = processList.at(cntxtSwtchToProc);
}

struct VMA* checkFramesAgainstPageEnrty(int page, Process* process){
  struct VMA* vMemAreaForPage = nullptr;

  list<struct VMA*>::iterator vma = process->virtualMemArea.begin();
  for (; vma!= process->virtualMemArea.end(); ++vma){
      //cout << (*vma)->start_vpage << ", " << (*vma)->end_vpage << ", " << (*vma)->write_prtctd << ", " << (*vma)->file_mapped << endl;
      if(page < 64 && page >= (*vma)->start_vpage && page <= (*vma)->end_vpage){
         vMemAreaForPage = (*vma);
         break;
      }
  }
  return vMemAreaForPage;
}

// Loads Data from input.txt for Processes and randomNumber from rFile.txt
void _readInputData(char* inputFile, char* rFile) {
  // Reading rFile first...
  ifstream ifsRandFile;
  ifsRandFile.open(rFile, ios::in);

  bool isFirstLine = true;
  long long int randNum;
  int count = 0;

  if (ifsRandFile.is_open()){
    string line;

    while (getline(ifsRandFile, line) ) {
        stringstream linestream(line);
        linestream >> randNum;

        if(isFirstLine) {
          totalRandNums = randNum-1;
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
  int totalProcesses = 0;
  int processCount = 0;

  if (ifsProcFile.is_open()) {
    string line;
    int strtVpage;
    int endVpage;
    int writePrtctd;
    int fileMapped;

    bool isFirstLine = true;

    while (getline(ifsProcFile, line)) {
        if(!isComment(line)) {

           if(isFirstLine) {
              totalProcesses = readNumber(line.c_str());
              isFirstLine = false;
            }
           else {
              if(processCount < totalProcesses) {
                int vmaCount = readNumber(line.c_str());
                list<struct VMA*> vmaList;

                int j = 0;
                while (j < vmaCount) {
                  getline(ifsProcFile, line);
                  if(!isComment(line)) {
                    //cout << line << endl;
                    stringstream linestream(line);
                    linestream >> strtVpage >> endVpage >> writePrtctd >> fileMapped;
                    struct VMA *vma = new VMA(strtVpage, endVpage, writePrtctd, fileMapped);
                    vmaList.push_back(vma);
                    j++;
                  }
                }

               Process *process = new Process(processCount, vmaList);
               processList.push_back(process);

               processCount++;
            }
            else {
              if(O_Option){
                cout << instCounter << ": ==> " << line << endl;
              }
              instCounter++;
              if(is_c_Instruction(line)) {
                ctx_switches++;
                contextSwitch(line);
              }
              else { // Read/Write Instruction Flow
                int vPage = extractVirtualPageNum(line);
                struct VMA* vMemAreaForPage = checkFramesAgainstPageEnrty(vPage, CURRENT_RUNNING_PROCESS);

                if(vMemAreaForPage == NULL){
                  if(O_Option){
                    cout << "  SEGV" << endl;
                  }
                  CURRENT_RUNNING_PROCESS->pstats.segv = CURRENT_RUNNING_PROCESS->pstats.segv+1;
                }
                else {
                      struct PTE pageTblEntry = CURRENT_RUNNING_PROCESS->pageTable[vPage];

                      if(pageTblEntry.valid_bit == 1) {
                         pageTblEntry.referenced_bit = 1;

                         if(is_w_Instruction(line)) {
                           if(pageTblEntry.write_prtctd_bit == 1) {
                             if(O_Option) { cout << " SEGPROT" << endl; }
                             CURRENT_RUNNING_PROCESS->pstats.segprot = CURRENT_RUNNING_PROCESS->pstats.segprot+1;
                           }
                           else{
                             pageTblEntry.modified_bit = 1;
                           }
                         }
                      }
                      else if(pageTblEntry.valid_bit == 0) {  // PageFault..
                          Frame *frame = getFrame();

                          if(!freeFrameAllocation) {
                            if(O_Option){
                              cout << " UNMAP " << frame->processId << ":" << frame->pageTableEntry << endl;
                            }
                            processList.at(frame->processId)->pstats.unmaps = processList.at(frame->processId)->pstats.unmaps+1;

                            struct PTE pte = processList.at(frame->processId)->pageTable[frame->pageTableEntry];
                            struct VMA* vMAForPage = checkFramesAgainstPageEnrty(frame->pageTableEntry, processList.at(frame->processId));

                            if(pte.modified_bit == 1) {
                              if(O_Option){
                                if(vMAForPage->file_mapped == 1) cout << " FOUT" << endl;
                                else cout << " OUT" << endl;
                              }

                              if(vMAForPage->file_mapped == 1) {
                                processList.at(frame->processId)->pstats.fouts = processList.at(frame->processId)->pstats.fouts+1;
                              }
                              else {
                                processList.at(frame->processId)->pstats.outs = processList.at(frame->processId)->pstats.outs+1;
                              }

                              if(vMAForPage->file_mapped == 0) { pte.paged_out_bit = 1; }
                            }

                            pte.valid_bit = 0;
                            pte.modified_bit = 0;
                            processList.at(frame->processId)->pageTable[frame->pageTableEntry] = pte;
                          }

                          frame->processId = CURRENT_RUNNING_PROCESS->process_id;
                          frame->pageTableEntry = vPage;

                          pageTblEntry.valid_bit = 1;
                          pageTblEntry.referenced_bit = 1;
                          pageTblEntry.frame_num = frame->frameNo;
                          pageTblEntry.write_prtctd_bit = vMemAreaForPage->write_prtctd;

                          if(pageTblEntry.paged_out_bit == 0 && vMemAreaForPage->file_mapped != 1) {
                               if(O_Option){ cout << " ZERO" << endl; }
                               CURRENT_RUNNING_PROCESS->pstats.zeros = CURRENT_RUNNING_PROCESS->pstats.zeros+1;
                          }
                          else {
                            if(O_Option){
                              if(vMemAreaForPage->file_mapped == 1) cout << " FIN" << endl;
                              else cout << " IN" << endl;
                            }

                            if(vMemAreaForPage->file_mapped == 1) {
                              CURRENT_RUNNING_PROCESS->pstats.fins = CURRENT_RUNNING_PROCESS->pstats.fins+1;
                            }
                            else {
                              CURRENT_RUNNING_PROCESS->pstats.ins = CURRENT_RUNNING_PROCESS->pstats.ins+1;
                            }
                          }

                          if(O_Option){  cout << " MAP " << frame->frameNo << endl;  }
                          CURRENT_RUNNING_PROCESS->pstats.maps = CURRENT_RUNNING_PROCESS->pstats.maps+1;

                          if(is_w_Instruction(line)) {
                             if(vMemAreaForPage->write_prtctd == 1){
                               if(O_Option){ cout << " SEGPROT" << endl; }
                               CURRENT_RUNNING_PROCESS->pstats.segprot = CURRENT_RUNNING_PROCESS->pstats.segprot+1;
                               pageTblEntry.referenced_bit = 1;
                             }
                             else{
                               pageTblEntry.modified_bit = 1;
                             }
                          }
                      }
                      CURRENT_RUNNING_PROCESS->pageTable[vPage] = pageTblEntry;
                  }
                   // End Not Segmentation Fault
                } // End Read/Write Flow
              }
            }
          }
        }
      }
  ifsProcFile.close();
}

void _printPageTable(){
    if(P_Option){
      vector<struct Process*>::iterator it1 = processList.begin();
      for (it1=processList.begin(); it1!=processList.end(); ++it1){

          cout << "PT[" << (*it1)->process_id << "]: " ;
          for (int i=0; i < 64; i++){
              string outPTE = "";
              PTE pte = (*it1)->pageTable[i];
              //cout << "vpage=" << i << " :: v=" << pte.valid_bit << ", r=" << pte.referenced_bit << ", m=" << pte.modified_bit << ", pg=" << pte.paged_out_bit << endl;

              if(pte.valid_bit == 0) {
                if(pte.paged_out_bit == 1){
                  outPTE = outPTE + "# ";
                }
                else if(pte.paged_out_bit == 0){
                  outPTE = outPTE + "* ";
                }
              }
              else if(pte.valid_bit == 1){
                outPTE = outPTE + std::to_string(i) + ":";

                if(pte.referenced_bit == 1) {
                  outPTE = outPTE + "R";
                }
                else {
                  outPTE = outPTE + "-";
                }

                if(pte.modified_bit == 1) {
                  outPTE = outPTE + "M";
                }
                else {
                  outPTE = outPTE + "-";
                }

                if(pte.paged_out_bit == 1){
                  outPTE = outPTE + "S ";
                }
                else {
                  outPTE = outPTE + "- ";
                }
              }
              cout << outPTE;
          }
          cout << endl;
      }
    }
}

void _printFrameTable(){
  if(F_Option){
    string outFT = "FT: ";
    for (int i=0; i < numOfFrames; i++){
        Frame *frame = &FRAME_TABLE[i];
        if(frame->pageTableEntry == -1){
          outFT = outFT + "* ";
        }
        else{
          outFT = outFT + std::to_string(frame->processId) + ":" + std::to_string(frame->pageTableEntry) + " ";
        }
    }
    cout << outFT << endl;
  }
}

void _printProcSummary(){
  if(S_Option){
    long totalMaps = 0;
    long totalUnMaps = 0;
    long totalOuts = 0;
    long totalFouts = 0;
    long totalIns = 0;
    long totalFins = 0;
    long totalZeros = 0;
    long totalSegv = 0;
    long totalSegprot = 0;

    vector<struct Process*>::iterator proc = processList.begin();
    for (proc=processList.begin(); proc!=processList.end(); ++proc) {
        printf("PROC[%d]: U=%lu M=%lu I=%lu O=%lu FI=%lu FO=%lu Z=%lu SV=%lu SP=%lu\n", (*proc)->process_id,
        (*proc)->pstats.unmaps, (*proc)->pstats.maps, (*proc)->pstats.ins, (*proc)->pstats.outs,
        (*proc)->pstats.fins, (*proc)->pstats.fouts, (*proc)->pstats.zeros, (*proc)->pstats.segv, (*proc)->pstats.segprot);

        totalMaps = totalMaps + (*proc)->pstats.maps;
        totalUnMaps = totalUnMaps + (*proc)->pstats.unmaps;
        totalOuts = totalOuts + (*proc)->pstats.outs;
        totalFouts = totalFouts + (*proc)->pstats.fouts;
        totalIns = totalIns + (*proc)->pstats.ins;
        totalFins = totalFins + (*proc)->pstats.fins;
        totalZeros = totalZeros + (*proc)->pstats.zeros;
        totalSegv = totalSegv + (*proc)->pstats.segv;
        totalSegprot = totalSegprot + (*proc)->pstats.segprot;
    }

    long long cost = (totalMaps + totalUnMaps)*400 + (totalIns + totalOuts)*3000 +
    (totalFins + totalFouts)*2500 + totalZeros*150 + totalSegv*240 + totalSegprot*300 + ctx_switches*121 + (instCounter-ctx_switches);

    printf("TOTALCOST %lu %lu %llu\n", ctx_switches, instCounter, cost);
  }
}

int main(int argc, char **argv){
  string argmntValue;
  int index;
  int option;
  string argVal;
  bool isSecChance = false;
  bool isAging = false;
  bool no_F_Option = true;

  while ((option = getopt (argc, argv, "a:o:f:xy")) != -1) {
    argVal = optarg;

    switch (option) {
      case 'a':
        if(argVal.length() == 1) {
           char pgng_policy = argVal.at(0);
           switch (toupper(pgng_policy)) {
              case 'F':
                THE_PAGER = new FIFO();
              break;
              case 'S':
                  isSecChance = true;
              break;
              case 'R':
                THE_PAGER = new Random();
              break;
              case 'N':
                THE_PAGER = new NRU();
              break;
              case 'C':
                THE_PAGER = new Clock();
              break;
              case 'A':
                isAging = true;
              break;
           }
         }
        break;
      case 'o':
          if(argVal.length() >= 1) {
            string printOps = argVal;
            if(contains(printOps,"O")){
              O_Option = true;
            }
            if(contains(printOps,"P")){
              P_Option = true;
            }
            if(contains(printOps,"F")){
              F_Option = true;
            }
            if(contains(printOps,"S")){
              S_Option = true;
            }
            if(contains(printOps,"x")){
              x_Option = true;
            }
            if(contains(printOps,"y")){
              y_Option = true;
            }
            if(contains(printOps,"f")){
              a_Option = true;
            }
            if(contains(printOps,"a")){
              a_Option = true;
            }
          }
        break;
      case 'f':
          if(argVal.length() >= 1) {
             numOfFrames = atoi(argVal.c_str());
             no_F_Option = false;
           }
      break;
      default:
        abort ();
      }
  }

  if(no_F_Option){
    numOfFrames = 4;
  }

  if(numOfFrames == 0) {
      cout << "Really funny .. you need at least one frame" << endl;
  }
  else if(numOfFrames < 0 || numOfFrames > 128) {
      cout << "sorry max frames supported = 128" << endl;
  }
  else {
    FRAME_TABLE = new Frame[numOfFrames];

    if(isSecChance){
      SecondChance * sc = new SecondChance();
      sc->initializeFramesList();
      THE_PAGER = sc;
    }

    if(isAging){
      Aging * ag = new Aging();
      ag->initializeFramesAge();
      THE_PAGER = ag;
    }

    _readInputData(argv[optind], argv[optind+1]);
    _printPageTable();
    _printFrameTable();
    _printProcSummary();
  }
}
