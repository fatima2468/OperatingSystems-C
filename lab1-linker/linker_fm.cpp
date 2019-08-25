#include <iostream>
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <map>

using namespace std;


char* filename;
FILE *fp;
int charCount = 2048;
char charBuffer[2048];
const char* delims = " \t\n";
int lineNum = 0;
int lineOffset = 0;
int linelength = 0;

const char instType_I = 'I';
const char instType_A = 'A';
const char instType_R = 'R';
const char instType_E = 'E';

int baseAddress = 0;
int modulesCount = 0;
int instrsnCount = 0;
int totalInstCount = 0;
int mmCounter = 0;
int machineSize = 512;

bool syntaxErrOcurred = false;
bool eof = false;
bool passOne = true;

//For Pass1
vector<int> mdulsBsAddrsList;
map<string, int> symbolTable;
map<string, string> symbolErrMsgs;
map<string, int> tempDefList;

//For Pass2
map<int, string> useTable;
map<string, bool> usedUseTable;
typedef pair<int, bool> symbolModule;
map<string, symbolModule> usedSymbolTable;
map<string, int> instructionTable;


void __cleanup(){
    ::passOne = false;
    ::eof = false;
    ::baseAddress = 0;
    ::lineNum = 0;
    ::lineOffset = 0;
    ::modulesCount = 0;
}

char* getToken(){
  char *token = strtok(NULL, ::delims);

  while(token == NULL){
    char *line = fgets(::charBuffer, ::charCount, ::fp);
    if(line == NULL){
      //::lineOffset++;
      ::lineOffset = ::linelength;
      ::eof = true;
      return NULL;
    }
    else {
      ::lineNum++;
      ::linelength = strlen(::charBuffer);
      token = strtok(line, ::delims);
    }
  }
  ::lineOffset = (token - ::charBuffer)+1;
  return token;
}

void __parseerror(int errcode) {
    const char *errstr[] = {
        "NUM_EXPECTED",  // Number Expected
        "SYM_EXPECTED",  // Symbol Expected
        "ADDR_EXPECTED", // Addressing Expected which is A/E/I/R
        "SYM_TOO_LONG", // Symbol Name is too long
        "TOO_MANY_DEF_IN_MODULE", // > 16
        "TOO_MANY_USE_IN_MODULE", // > 16
        "TOO_MANY_INSTR",   // total num_instr exceeds memory size (512)
      };
    printf("Parse Error line %d offset %d: %s\n", ::lineNum, ::lineOffset, errstr[errcode]);
}

string __errormsgs(int errcode) {
    const char *errstr[] = {
        "Error: This variable is multiple times defined; first value used",
        "Error: ss is not defined; zero used",
        "Error: External address exceeds length of uselist; treated as immediate",
        "Error: Absolute address exceeds machine size; zero used",
        "Error: Relative address exceeds module size; zero used",
        "Error: Illegal immediate value; treated as 9999",
        "Error: Illegal opcode; treated as 9999",
      };

    return errstr[errcode];
}

void __warningmsgs(int warn_code, int moduleNo, char *symbol, int instAddrs, int maxInstAddrs) {
    if(warn_code == 0) {
      printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", moduleNo, symbol, instAddrs, maxInstAddrs);
    }
    else if(warn_code == 1) {
      printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", moduleNo, symbol);
    }
    else if(warn_code == 2) {
      printf("Warning: Module %d: %s was defined but never used\n", moduleNo, symbol);
    }
}


bool isValidNumber(char *token){
  bool validNum = true;

  for(int i = 0; i < strlen(token); i++){
      if (!isdigit(token[i])) {
          __parseerror(0);
          ::syntaxErrOcurred = true;
          validNum = false;
          break;
      }
  }
  return validNum;
}


bool isValidSymbol(char *token){
  bool validSymbol = true;

  if (strlen(token) > 16) {
     validSymbol = false;
     __parseerror(3);
     ::syntaxErrOcurred = true;
  }
  else if (!isalpha(token[0])){
    __parseerror(1);
    ::syntaxErrOcurred = true;
    validSymbol = false;
  }
  else {
      for(int i = 1; i < strlen(token); i++){
         if (!isalnum(token[i])) {
             validSymbol = false;
             break;
           }
        }
      }
  return validSymbol;
}

int readNumber() {
  char *token = getToken();

  if(NULL == token){
    return -2;
  }
  else {
    if(passOne) {  // Pass 1: SyntaxError Checking
      if(isValidNumber(token)){
        return atoi(token);
      }
      else { return -1; }
    }
    else { // Pass 2
      return atoi(token);
    }
  }
}


char* readSymbol(){
  char *token = getToken();
  if(passOne) {  // Pass 1: SyntaxError Checking
    if(NULL == token) {
      __parseerror(1);
      ::syntaxErrOcurred = true;
      return NULL;
    }
    else {
      if(isValidSymbol(token)){
        return token;
      }
      else { return NULL; }
    }
  }
  else { // Pass 2
    return token;
  }
}


char readInstructionType(){
  char *token = getToken();
  char instrctnType = 'N';

  if(passOne) {  // Pass 1: SyntaxError Checking
    bool parseErr = false;

    if(NULL == token){
      parseErr = true;
    }
    else if(strlen(token) > 1 && !isalpha(token[0])){
      parseErr = true;
    }
    else {
      char insTyp = token[0];
      insTyp = toupper(insTyp);

      if(insTyp != ::instType_I && insTyp != ::instType_A &&
         insTyp != ::instType_R && insTyp != ::instType_E) {
           parseErr = true;
         }
      else {
           instrctnType = insTyp;
      }
    }

    if(parseErr){
      __parseerror(2);
      ::syntaxErrOcurred = true;
    }
  }
  else {  // Pass 2
    instrctnType = toupper(token[0]);
  }

  return instrctnType;
}


int readInstruction(){
  char *token = getToken();

  if(passOne) {  // Pass 1: SyntaxError Checking
    if(NULL == token && ::eof){
      __parseerror(0);
      ::syntaxErrOcurred = true;
      return -2;
    }

    if(isValidNumber(token)){
        return atoi(token);
      }
    else {
      return -1;
    }
  }
  else {  // Pass 2
     return atoi(token);
  }
}

void readDefList(){
  if(!::syntaxErrOcurred && !::eof){
    //readDefList
    int defCount = readNumber();

    if(defCount == -2 && ::eof) {}
    else if(defCount != -1) {
      if(defCount > 16){
        __parseerror(4);     // No more than 16 symbol definitions
        ::syntaxErrOcurred = true;
      }
      else {
        for (int i = 0; i < defCount; i++) {
           char *symbol = readSymbol();

           if(NULL == symbol){
             break;
           }
           else {
             char *saved_symbol = (char *)malloc(sizeof(char) * strlen(symbol));
             strcpy(saved_symbol, symbol);

             if(passOne) { // Get the Symbol and put it in Symbol Table
               int relAddrs = readNumber();

               if(relAddrs == -2 && ::eof){
                 __parseerror(0);     // Number Expected
                 ::syntaxErrOcurred = true;
               }
               else if(relAddrs != -1) {
                 int absAddrs = baseAddress+relAddrs;
                 bool newSymbol = true;

                 map<string,int>::iterator it_s = symbolTable.find(saved_symbol);
                 if(it_s != symbolTable.end()) {
                   symbolErrMsgs.insert(make_pair(saved_symbol,__errormsgs(0)));
                   newSymbol = false;
                 }

                if(newSymbol){
                   symbolTable.insert(make_pair(saved_symbol,absAddrs));
                   tempDefList.insert(make_pair(saved_symbol,relAddrs));
                }
              }
             } //end if Pass1
             else {
               // Pass2 :: Put the symbol with moduleNo and marked used as false in the map
               // and just read the number to move forward..
               usedSymbolTable.insert(make_pair(saved_symbol,make_pair(modulesCount+1,false)));
               readNumber();
             }
             free(saved_symbol);
           } // end else (Symbol != NULL)
         } //end_for
      } //end else // No more than 16 symbol definitions
    }  // end else
  } // end if
}// end proc_defl


void readUseList(){
  if(!::syntaxErrOcurred && !::eof){
      // readUseList
      int useCount = readNumber();
      if(useCount == -2 && ::eof){
        __parseerror(0);     // Number Expected
        ::syntaxErrOcurred = true;
      }
      else if(useCount != -1) {
        if(useCount > 16) {
          __parseerror(5);     // No more than 16 symbols in useList
          ::syntaxErrOcurred = true;
        }
        else {
          for (int i = 0; i < useCount; i++) {
             char *symbol = readSymbol();
             if(NULL == symbol){
               break;
             }
             else {
               if(!passOne){
                  ::useTable.insert(make_pair(i,symbol));
                  ::usedUseTable.insert(make_pair(symbol, false));
               }
             }
          }
        } // end else :  // No more than 16 symbols in useList
      } //end if useCount is Valid
    }
}

void readInstList(){
  if(!::syntaxErrOcurred && !::eof){
    // readInstList
    int instCount = readNumber();

    if(instCount == -2 && ::eof){
      __parseerror(0);     // Number Expected
      ::syntaxErrOcurred = true;
    }
    else if(instCount != -1) {
      if(::totalInstCount+instCount > 512) {
        __parseerror(6);     // No more than 16 symbols in useList
        ::syntaxErrOcurred = true;
      }
      else {
        for (int i = 0; i < instCount; i++) {
           char instType = readInstructionType();
           if('N' == instType){
             break;           // InValid Instruction Type
           }
           else {
             int instruction = readInstruction();
             if(instruction == -1) {
               break;
             }
             else {
               if(!passOne){
                 ostringstream oss;
                 oss << i << instType;
                 instructionTable.insert(make_pair(oss.str(), instruction));
               }
             }
           }
        }

        if(!::syntaxErrOcurred){
          ::instrsnCount = instCount;
        }
      }
    }
  }
}

void printInstMemoryAddrs(string memAddres, int err_code, string err_symbol){
  string counterVal = "";
  ostringstream oss;
  bool appendAtStart = !(err_code == 3 || err_code == 4);

  if(mmCounter < 10){
    oss << "00" << mmCounter;
  }
  else if (mmCounter >= 10 && mmCounter <= 99){
    oss << "0" << mmCounter;
  }
  else {
    oss << mmCounter ;
  }
  counterVal = oss.str();

  if(memAddres.length() == 1){
    if(appendAtStart){
      memAddres =   "000" + memAddres;
    }
    else { memAddres =  memAddres + "000"; }
  }
  else if(memAddres.length() == 2){
    if(appendAtStart){
      memAddres =   "00" + memAddres;
    }
    else { memAddres =  memAddres + "00"; }
  }
  else if(memAddres.length() == 3){
    if(appendAtStart){
      memAddres =   "0" + memAddres;
    }
    else { memAddres =  memAddres + "0"; }
  }

  cout << counterVal << ": " << memAddres;

  if(err_code != -1) {

     if(err_code == 1) {
          ostringstream ess;
          ess << __errormsgs(err_code);
          string err_str = ess.str();
          err_str = err_str.replace(err_str.begin()+7, err_str.begin()+9, err_symbol);

          cout << " " << err_str;
        }
    else {
      cout << " " << __errormsgs(err_code);
    }
  }
  cout << endl;
}


void produceMemoryMap(){
  if(modulesCount < ::mdulsBsAddrsList.size()){
    int baseAddrs = ::mdulsBsAddrsList.at(modulesCount);

    map<string,int>::iterator it1 = instructionTable.begin();
    for (it1=instructionTable.begin(); it1!=instructionTable.end(); ++it1){
        char instType = it1->first[1];
        int instruction = it1->second;

        string instruction_str;
        int error_code = -1;
        string err_symbol = "";

        int opCode = instruction/1000;
        int opernd = instruction%1000;

        if (instType_I == instType){
            if(instruction >= 10000){
              instruction = 9999;
              error_code = 5;
            }
          ostringstream insts;
          insts << instruction;
          instruction_str = insts.str();
        }
        else {
          if(instruction >= 10000){
            instruction = 9999;
            error_code = 6;

            ostringstream insts;
            insts << instruction;
            instruction_str = insts.str();
          }
          else {
            if(instType_R == instType) {
              ostringstream insts;

              if(opernd >= instructionTable.size()){
                instruction = opCode*1000 + baseAddrs;
                error_code = 4;
              }
              else{
                instruction = instruction + baseAddrs;
              }
              insts << instruction;
              instruction_str = insts.str();
            }
            else if (instType_E == instType) {
              map<int,string>::iterator it_u = useTable.find(opernd);

              if(it_u != useTable.end()) {
                string symbol = it_u->second;

                map<string, int>::iterator it_s = symbolTable.find(symbol);
                if(it_s != symbolTable.end()) {
                  int memAddres = it_s->second;

                  ostringstream insts;
                  insts << instruction;
                  string istr_str = insts.str();

                  ostringstream mAdrs;
                  mAdrs << memAddres;
                  string mAdrs_str = mAdrs.str();

                  int len_inst = istr_str.length();
                  int len_adrs = mAdrs_str.length();

                  if(len_inst - len_adrs > 0) {
                    instruction_str = istr_str.replace(len_inst-len_adrs, len_inst, mAdrs_str);
                  }
                  else { instruction_str = mAdrs_str; }
                }
                else {
                  ostringstream insts;
                  insts << instruction;
                  instruction_str = insts.str();
                  error_code = 1;
                  err_symbol = symbol;
                }
                usedUseTable[it_u->second] = true;
              }
              else {
                // treating as immediate
                ostringstream insts;
                insts << instruction;
                instruction_str = insts.str();
                error_code = 2;
              }
            }
            else if (instType_A == instType) {
              ostringstream insts;

              if(opernd > machineSize){
                insts << opCode;
                error_code = 3;
              }
              else {
                insts << instruction;
              }

              instruction_str = insts.str();
            }
          }
        }

        printInstMemoryAddrs(instruction_str, error_code, err_symbol);
        mmCounter++;
     }
  }
}

void _passOne(){
    fp = fopen(::filename, "r");

     while(feof(fp) == 0) {
       readDefList();
       readUseList();
       readInstList();

       if(!::syntaxErrOcurred && !::eof) {
         ::mdulsBsAddrsList.push_back(::baseAddress);
         ::baseAddress = ::baseAddress + ::instrsnCount;
         ::totalInstCount = ::totalInstCount + ::instrsnCount;
         modulesCount++;

         map<string,int>::iterator it_d = tempDefList.begin();
         for (it_d=tempDefList.begin(); it_d!=tempDefList.end(); ++it_d){
             string symbol = it_d->first;
             int addrs = it_d->second;

             if(addrs > ::instrsnCount-1) {
               symbolTable[symbol] = 0;
               char *err_symbol = new char[symbol.length()+1];
               std::strcpy (err_symbol, symbol.c_str());

               __warningmsgs(0, modulesCount,err_symbol,addrs,instrsnCount-1);
                free(err_symbol);
             }
          }
          tempDefList.clear();
       }
       else {
         if(::syntaxErrOcurred){
           symbolTable.clear();
           symbolErrMsgs.clear();
           mdulsBsAddrsList.clear();
         }
         break;
       }
      }

      if(!::syntaxErrOcurred){
        cout << "Symbol Table" << endl;

        map<string,int>::iterator it = symbolTable.begin();
        for (it=symbolTable.begin(); it!=symbolTable.end(); ++it){
            cout << it->first << "=" << it->second;

            map<string,string>::iterator it_er = symbolErrMsgs.find(it->first);
            if(it_er != symbolErrMsgs.end()) {
                cout << " " << it_er->second;
            }
            cout << endl;
          }
      }

    fclose(fp);
}  // end_passOne()


void _passTwo(){
     if(!::syntaxErrOcurred){
       __cleanup();

       fp = fopen(::filename, "r");

       cout << "Memory Map" << endl;

       while(feof(fp) == 0) {
         readDefList();
         readUseList();
         readInstList();

         produceMemoryMap();
         modulesCount++;

         map<string,bool>::iterator it = usedUseTable.begin();
         for (it=usedUseTable.begin(); it!=usedUseTable.end(); ++it) {
           string symbol = it->first;
           bool usedUp = it->second;
           if(!usedUp){
             char *err_symbol = new char[symbol.length()+1];
             std::strcpy(err_symbol, symbol.c_str());

             __warningmsgs(1, modulesCount, err_symbol, -1, -1);
             free(err_symbol);
            }
          }

          map<int,string>::iterator it_u = useTable.begin();
          for (it_u=useTable.begin(); it_u!=useTable.end(); ++it_u) {
              symbolModule pair = usedSymbolTable[it_u->second];
              usedSymbolTable[it_u->second] = make_pair(pair.first, true);
          }

         useTable.clear();
         usedUseTable.clear();
         instructionTable.clear();
       }

       cout << endl;
       // At last...
       // Print with warning all the symbols unused
        map<string,symbolModule>::iterator it_s = usedSymbolTable.begin();
        for (it_s=usedSymbolTable.begin(); it_s!=usedSymbolTable.end(); ++it_s) {
          string symbol = it_s->first;
          symbolModule pair = it_s->second;
          int moduleNo = pair.first;
          bool usedUp = pair.second;
          if(!usedUp){
            char *err_symbol = new char[symbol.length()+1];
            std::strcpy(err_symbol, symbol.c_str());

            __warningmsgs(2, moduleNo, err_symbol, -1, -1);
            free(err_symbol);
           }
         }

       fclose(fp);
     }
}  // end_passTwo()


int main(int argc, char *argv[]){
    ::filename = argv[1];

    _passOne();
     cout << endl;
    _passTwo();
    return 0;
    }
