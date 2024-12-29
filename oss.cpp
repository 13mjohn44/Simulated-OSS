//Mateo John
//11/22/24

#include <iostream>
#include <sstream>
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <csignal>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <thread>     // For sleep_for
#include <chrono>     // For time units
#include <sys/msg.h>
#include <sys/types.h>
#include <map>
#include <fstream>
#include <vector>
#include "descriptor.h"

#define SHM_KEY 1234 // we set the shared memory key

#define PERMS 0644

using namespace std;




struct msgbuffer {
    long mtype;
    char strData[100];
    int intData;
};

//this is meant to keep tract of resource statistics
struct statistics {
    int grantedImmediadiately = 0;
    int grantedAfterWaiting = 0;
    int termitedSuccesfully = 0;
    int terminatedByDeadLockRes = 0;
    int deadlockAlgorithimRan = 0;
    int deadlockRan = 0;
    vector<int> deletedPerRun;
};

//This is a struct that is used for the process table
struct PCB {
    int occupied; // either true or false
    pid_t pid; // process id of this child
    int startSeconds; // time when it was created
    int startNano; // time when it was created
    double serviceTimeSeconds; // total seconds it has been "scheduled"
    double serviceTimeNano; // total nanoseconds it has been "scheduled"
    int eventWaitSec; // when does its event happen?
    int eventWaitNano; // when does its event happen?
    int processNumber;//what in number is it in terms of launched processes
    int blocked; // This will be set to 0,1 and 2. Since a process does not need to be sent a message after it wakes up from being blocked, we will use 1 to indicate it has just woken up and does not need a 
};

//this is the struct that is used for the clock and it is put into shared memory later
struct SharedClock {
    int ms;
    int ns;
};

const int SCHEDULE_LENGHT = 5000;// this is how long each process is scheduled for.
PCB processTable[19];
vector<pid_t> ReadyQue;
vector<pid_t> BlockedQue;
pid_t CurrentlyScheduled;//whichever process is currently scheduled to run. 1 is lowest priority and 0 is highest
map<pid_t, double> schedulingRatio;// this will contain a pid and its schduling ratio for each pid
map<pid_t, double> schedulingRatioNano;
ResourceDescriptors resourceTables;
int msqid;
int total_seconds = 0;
int shmId;
SharedClock *sharedClock;
statistics stats;
char verbose;


//this is meant to find the index of a PID in the Process table
int FindProcessIndexInPCB(pid_t ProcessPid){
    // cout << "function was called" << endl;
    for (int i = 0; i < 19; i++){
        // cout << processTable[i].pid << "==" << ProcessPid << endl;
        if(processTable[i].pid == ProcessPid){
            // cout << "-----------------" << endl;
            return i;
        }
    }
    cout << "We searched for a pid that didn't exist in FindProcessInPCB and returned -1 instead. This shouldn't happen so make sure its right." << endl;
    return -1;
}

//searches for the matching pid entered and sets that entry to default values
void RemoveEntryFromPT(PCB Table[19], pid_t childpid){
    for (int i = 0; i < 19; i++){
        if(Table[i].pid == childpid){
            Table[i].occupied = 0;
            Table[i].pid = 0;
            Table[i].startSeconds = 0;
            Table[i].startNano = 0;
            Table[i].serviceTimeSeconds = 0; // total seconds it has been "scheduled"
            Table[i].serviceTimeNano = 0; // total nanoseconds it has been "scheduled"
            Table[i].eventWaitSec = 0; // when does its event happen?
            Table[i].eventWaitNano = 0; // when does its event happen?
            Table[i].blocked = 0; // is this process waiting on event?
            break;
        }
    }
}

//this program will resolve deadlock by deleting the processes with the most resources one at a time till the deadlock is resovled.
//Deadlock is resolved when there is any process who's resource request can be granted. That process is then woken up and put back in ready que
int DeadlockResolution(ofstream &outputFile){
    int mostResources = 0;
    pid_t pidMostResources;
    int currentResources;
    map<pid_t, int> resourcesOfPid;
    map<pid_t, bool> pidGranted;//a pid, and then a bool representing if that pids request can be granted
    int index;
    bool cannotGrantRequest = false;
    bool deadlockResolved = false;
    int howManyDeleted = 0;
    vector<int> deletedResources;
    bool firstTime = true;


    while(deadlockResolved == false && BlockedQue.empty() == false){
        for(int i = 0; i < BlockedQue.size(); i++){
            cannotGrantRequest = false;
            index = processTable[FindProcessIndexInPCB(BlockedQue.at(i))].processNumber;
            for(int j = 0; j < 10; j++){
                if(resourceTables.requestedResources[index][j] > resourceTables.availableResources[j] && resourceTables.requestedResources[index][j] != 0){ // This checks if there are any resources requested we do not have, meaning that we cannot approve the request
                    cannotGrantRequest = true;
                }
            }
            if(cannotGrantRequest == false){//this means that no processes request was made that cannot be granted so aprove the request.
                outputFile << "OSS: Approved P" << index << "'s request for previosuly blocked resources." << endl;
                stats.grantedAfterWaiting++;
                for(int i = 0; i < 10; i++){
                    if(resourceTables.requestedResources[index][i] > 0){
                        resourceTables.availableResources[i] -= resourceTables.requestedResources[index][i];// we subtract the requested resource from the avalable resource
                        resourceTables.requestedResources[index][i] = 0;
                    }
                }
                deadlockResolved = true;
                processTable[FindProcessIndexInPCB(BlockedQue.at(i))].blocked = 0;
                ReadyQue.push_back(BlockedQue.at(i));
                BlockedQue.erase(BlockedQue.begin() + i);
            }
        }
        if(deadlockResolved == false){
            if(firstTime == true){
                if(verbose == 'Y' || verbose == 'y'){
                    outputFile << "Processes "; 
                    for(int i = 0; i < 19; i++){
                        if(processTable[i].occupied == 1){
                            outputFile << processTable[i].processNumber << " ";
                        }
                    } 
                    outputFile << "are in deadLock."<< endl;
                    outputFile << "Attempting to resolve..." << endl;
                }else{
                    outputFile << "Deadlock detected" << endl;

                }
                firstTime = false;
            }
            
            stats.deadlockAlgorithimRan++;

            //first we need to tally up all resources for each process
            for(int i = 0; i < BlockedQue.size(); i++){
                index = processTable[FindProcessIndexInPCB(BlockedQue.at(i))].processNumber;
                currentResources = 0;
                for(int j = 0; j < 10; j++){
                    currentResources += resourceTables.allocatedResources[index][j];
                }
                resourcesOfPid[BlockedQue.at(i)] = currentResources;
            }
            //next we figure out which pid has the most resources
            for(int i = 0; i < BlockedQue.size(); i++){
                if(mostResources < resourcesOfPid[BlockedQue.at(i)]){
                    mostResources = resourcesOfPid[BlockedQue.at(i)];
                    pidMostResources = BlockedQue.at(i);
                }
            }
            
            // now kil the process that had the most resources
            deletedResources.push_back(processTable[FindProcessIndexInPCB(pidMostResources)].processNumber);

            howManyDeleted++;
            stats.terminatedByDeadLockRes++;
            if(verbose == 'Y' || verbose == 'y'){
                outputFile << "\tKilling process P" << processTable[FindProcessIndexInPCB(pidMostResources)].processNumber << " (PID "<< pidMostResources << ")" << endl;
                outputFile << "\t\tResources released are as follows: ";
                for(int i = 0; i < 10; i++){
                    if(resourceTables.allocatedResources[processTable[FindProcessIndexInPCB(pidMostResources)].processNumber][i] > 0){
                        outputFile << "R" << i << ":" << resourceTables.allocatedResources[processTable[FindProcessIndexInPCB(pidMostResources)].processNumber][i] << " ";
                    }
                }
                outputFile << endl;
            }

            kill(pidMostResources, SIGKILL);
            // and here we return the resources to now be available. And reset the allocated and requested to 0
            for(int i = 0; i < 10; i++){
                resourceTables.availableResources[i] += resourceTables.allocatedResources[processTable[FindProcessIndexInPCB(pidMostResources)].processNumber][i];
                resourceTables.allocatedResources[processTable[FindProcessIndexInPCB(pidMostResources)].processNumber][i] = 0;
                resourceTables.requestedResources[processTable[FindProcessIndexInPCB(pidMostResources)].processNumber][i] = 0;
            }
            RemoveEntryFromPT(processTable, pidMostResources);
        }

        for(int i = 0; i < BlockedQue.size(); i++){
            if(BlockedQue.at(i) == pidMostResources){
                BlockedQue.erase(BlockedQue.begin() + i);
                break;
            }
        }
    }
    if(howManyDeleted > 0){
        outputFile << "Deadlock was resovled by deleting ";
        for(int i = 0; i < deletedResources.size(); i++){
            outputFile << "P" << deletedResources.at(i) << " ";
        }
        outputFile << "and freeing their resources." << endl;

    }

    stats.deadlockRan++;
    return howManyDeleted;

}


//we print the process table
void PrintProcessTable(PCB Table[19]){
	cout << "Process table: " << endl;
	cout << "Entry Occupied PID StartS StartN ServiceTimeSec ServiceTimeNS eventWaitSec eventWaitNano blocked" << endl;
	for(int i = 0; i < 19; i++){
		cout << setw(5) << i << setw(5) << left << Table[i].occupied << setw(10) << Table[i].pid << setw(10) << Table[i].startSeconds << setw(10) << Table[i].startNano << setw(10) << Table[i].serviceTimeSeconds << setw(10) << Table[i].serviceTimeNano << setw(10) << Table[i].eventWaitSec << setw(10) << Table[i].eventWaitNano << setw(10) << Table[i].blocked << endl;
	}
}

void AddToReadyQue(pid_t newProcess){
    ReadyQue.push_back(newProcess);
}

//This searches for the first entry that is not occupied and completes an entry
void AddEntryToPT(PCB Table[19], pid_t childpid, int secs, int nanoSecs, int processNum ){
    for (int i = 0; i < 19; i++){
        if(Table[i].occupied == 0){
            Table[i].occupied = 1;
            Table[i].pid = childpid;
            Table[i].startSeconds = secs;
            Table[i].startNano = nanoSecs;
            Table[i].processNumber = processNum;
            return;
        }
    }
}



//This is a simply function to turn a int to string
string int_to_string(int number) {
    ostringstream ss;
    ss << number;
    return ss.str();
}

string float_to_string(float number) {
    ostringstream ss;
    ss << number;
    return ss.str();
}

//This handles that the program doesn't run more than 60 seconds
void handle_timeout(int sig) {
    cout << "\n10 seconds have passed. Terminating child processes and cleaning...\n";

    // Kill all child processes
    for (int i = 0; i < 19; i++) {
        if(processTable[i].occupied == 1){
            kill(processTable[i].pid, SIGKILL);  // Send kill signal to each child process
        }
    }
    //detach from shared memory
    shmdt(sharedClock);
    // Remove the shared memory segment
    shmctl(shmId, IPC_RMID, nullptr);

    // get rid of message queue
    if (msgctl(msqid, IPC_RMID, nullptr) == -1) {
        std::perror("msgctl to get rid of queue in parent failed");
        exit(1);
    }
    exit(0); // Exit the program after killing children
}
 
//This handles the control ctrl-C terminate program call and cleans up
 void handle_sigint(int sig) {
    std::cout << "\nCtrl-C detected. Cleaning up...\n";
    cout << "Seconds: " << total_seconds <<  ", sharedClock nano" << sharedClock->ns << endl;

    // Kill all child processes
    for (int i = 0; i < 19; i++) {
        if(processTable[i].occupied == 1){
            kill(processTable[i].pid, SIGKILL);  // Send kill signal to each child process
        }
    }
    //detach from shared memory
    shmdt(sharedClock);
    // Remove the shared memory segment
    shmctl(shmId, IPC_RMID, nullptr);
    // get rid of message queue
    if (msgctl(msqid, IPC_RMID, nullptr) == -1) {
        std::perror("msgctl to get rid of queue in parent failed");
        exit(1);
    }
    exit(0); // Exit the program after cleaning up
}

int main(int argc, char* argv[])
{   
    srand((unsigned) time(NULL));//WE MIGHT NEED TO CHANGE THIS LATER SCINCE THE RANDOM MIGHT NEED TO CHANGE DEPENDING ON THE PROCESS-------------!!!!!!!!!!!!!!!!!!
    signal(SIGALRM, handle_timeout);  // Handle timeout signal (SIGALRM)
    signal(SIGINT, handle_sigint);    // Handle Ctrl-C (SIGINT)

    alarm(10);
    //we creat out use varibales here
    bool lastProcess = false;
    bool TerminateLastProcess = false;
    int opt;
    int proc = 1;
    int simul = 1;
    int timeLimitForChildren = 3;
    int intervalInNsToLaunchChildren = 1000;
    int timeToLaunchNewProcess = -1;
    string logFileName = "text.txt";
    ofstream outputFile;


    double speed = 1;// this controls the speed of the clock. 1 means real time, 2 means double time
    int simulated_time = 0;
    

    //initialize the process table
    for(int i = 0; i < 19; i++){
		processTable[i].occupied = 0;
		processTable[i].pid = 0;
		processTable[i].startSeconds = 0;
		processTable[i].startSeconds = 0;
        processTable[i].serviceTimeSeconds = 0; // total seconds it has been "scheduled"
        processTable[i].serviceTimeNano = 0; // total nanoseconds it has been "scheduled"
        processTable[i].eventWaitSec = 0; // when does its event happen?
        processTable[i].eventWaitNano = 0; // when does its event happen?
        processTable[i].processNumber = -1;
        processTable[i].blocked = 0; // is this process waiting on event?
	}

    // This handles the command line arguments and sets the values for the variables if entered.
    while((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1)  
    {
        switch(opt)  
        {  
            case 'h':
                cout << "This program can be called using the following command:" << endl;
                cout << "oss [-h] [-n proc] [-s simul] [-i intervalInMsToLaunchChildren] [-f logfile]" << endl;
                cout << "This is an example run: " << endl;
                cout << "./oss -n 4 -s 4 -t 2 -i 1000 -f text.txt" << endl;
                cout << "No more than 18 process can run at the same time." << endl;
                cout << "for more information read the README file" << endl;
                exit(1);
            case 'n':
                if(atoi(optarg) > 0 && atoi(optarg) < 19){
                    proc = atoi(optarg);
                } else {
                    cout << "defualt value is used for n option." << endl;
                }
                
                break;
            case 's': 
                if(atoi(optarg) > 0 && atoi(optarg) < 19){
                    simul = atoi(optarg);
                } else {
                    cout << "defualt value is used for s option." << endl;
                }
                
                break;  
            case 't':
                if(atoi(optarg) > 0 && atoi(optarg) < 10000){
                    timeLimitForChildren = atoi(optarg);
                    // cout << "timeLimitForChildren was set to: " << timeLimitForChildren << endl;
                } else {
                    cout << "defualt value is used for t option." << endl;
                }
                
                break;
            case 'i':
                if(atoi(optarg) > 0 && atoi(optarg) < 10000){
                    intervalInNsToLaunchChildren = atoi(optarg);
                    // cout << "intervalInNsToLaunchChildren: " << intervalInNsToLaunchChildren << endl;
                } else {
                    cout << "defualt value is used for i option." << endl;
                }
                
                break;
            case 'f':
                logFileName = optarg;
                // cout << " the file name we got is: " << logFileName << endl;
                
                break;
            default:
                cout << "This program can be called using the following command:" << endl;
                cout << "oss [-h] [-n proc] [-s simul] [-i intervalInMsToLaunchChildren] [-f logfile]" << endl;
                cout << "This is an example run: " << endl;
                cout << "./oss -n 4 -s 4 -t 2 -i 1000 -f text.txt" << endl;
                cout << "No more than 18 process can run at the same time." << endl;
                cout << "for more information read the README file" << endl;
                exit(1);
        }  
    }

    //This is used to detect if a wrong command was enterd and then terminates the program if neccesary
    for(; optind < argc; optind++){      
        cout << "An unkown argument was used. Program terminated. Please type -h for help" << endl;
        exit(1);
    }


    //not really sure this would ever happen at this point but just checks if all command line arguments are valid
    if(proc <= 0 || simul <= 0){
        cout << "You entered an invalid value. Please try again or do not enter that argument if you want to use the default." << endl;
        cout << "Use the -h option for help." <<endl;
        exit(1);
    }

    // Create a shared memory segment
    shmId = shmget(SHM_KEY, sizeof(SharedClock), 0666 | IPC_CREAT);
    if (shmId < 0) {
        std::cerr << "Failed to create shared memory segment.\n";
        exit(1);
    }

    // Attach the shared memory segment to the parent's address space
    sharedClock = static_cast<SharedClock*>(shmat(shmId, nullptr, 0));
    if (sharedClock == reinterpret_cast<SharedClock*>(-1)) {
        std::cerr << "Failed to attach shared memory segment.\n";
        exit(1);
    }

    cout << "Would you like to use the verbose option for file output? Enter \"Y\" for yes and anything else for no:" << endl;
    cin >> verbose;

    msgbuffer rcvbuf;
    msgbuffer msgBuffer;
    
    key_t key;
    system("touch msgq.txt");

    // get a key for our message queue
    if ((key = ftok("msgq.txt", 1)) == -1) {
        std::perror("ftok");
        exit(1);
    }

    // create our message queue
    if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
        std::perror("msgget in parent");
        exit(1);
    }

    

    outputFile.open(logFileName);
    
    if(!outputFile){
        cout << "error opening log file. Exiting." << endl;
        exit(1);
    }

    // pid_t childrenPIDs[proc];

    map<pid_t, int> chldMsgStatus;
    

    // these are used to keep track of the current number of programs running, and how many have been launched in total
    bool NoMoreProcesses = false;
    int running = 0;
    int launched = 0;
    int status;
    int timeToDisplayPT = 500;
    //we set the tick time for ms and ns
    auto last_tick = std::chrono::steady_clock::now();
    auto lastTickNano = std::chrono::steady_clock::now();
    int nanoElapsed = 0;
    int currentMS = 0;
    int TotalMS = 0;
    int timeToLaunchNewProcessSec = -1;
    bool halfsec = false;
    int expectedNextPrint = 20;
    //we loop infinitly incrementing the clock untill we break when all processes have ran.
    while (true) {
        int increment;
        pid_t pid;
        pid_t tempPid;
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastLaunch = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick).count();
        
        if(timeSinceLastLaunch >= 5000){
            cout << "It has been 5 seconds, no more process will be launched." << endl;
            NoMoreProcesses = true;
        }
        
        if(nanoElapsed > 1000000000){
            nanoElapsed = nanoElapsed - 1000000000;
            total_seconds++;
        }
        
        
        if(currentMS < nanoElapsed / 1000000){
            // cout << "This triggered and nanoElapsed is: " << nanoElapsed << endl;
            currentMS = nanoElapsed / 1000000;
            TotalMS++;
        }


        //set the shared memory values
        sharedClock->ns = nanoElapsed;
        sharedClock->ms = TotalMS;

    

        if(sharedClock->ms >= timeToDisplayPT){
            PrintProcessTable(processTable);
            timeToDisplayPT += 500;
            halfsec = true;
        }

    

        // int temporary;
        if(launched < 1){
            // temporary = intervalInNsToLaunchChildren / 1000000;
            sharedClock->ns = intervalInNsToLaunchChildren;
            timeToLaunchNewProcess = intervalInNsToLaunchChildren;
            
        }

        //this checks controlls that we only start the correct number of processes
        if(launched < proc){
            //this checks that we limit the number of processes running simultaneously
            if(running < simul){
                //These two ifs control that we launch at the correct specified interval
                if(timeToLaunchNewProcess == -1){
                    timeToLaunchNewProcess = intervalInNsToLaunchChildren;
                }
                if((sharedClock->ns) >= timeToLaunchNewProcess && timeToLaunchNewProcess != -1 && NoMoreProcesses == false) {
                    timeToLaunchNewProcess = (sharedClock->ns) + intervalInNsToLaunchChildren;
                    // we fork here
                    pid = fork();
                    if (pid == 0) {
                        // This is the child process. We execl here with the child and pass the proper parameters
                        
                        int randomTimeLimit = (rand() % timeLimitForChildren) + 1;

                        float randomNanoLimit = (float)rand()/RAND_MAX;

                        string secondLimit = int_to_string(randomTimeLimit);
                        string nanoLimit = float_to_string(randomNanoLimit);
                        execl("./user_proc", "user_proc", secondLimit.c_str(), nanoLimit.c_str() , NULL); // Launch user process
                        cout << "there was an error with the child exec" << endl;
                        perror("execl failed");
                        exit(1);
                    } else if (pid > 0) {
                        // This is the parent process. We add a an entry to the table and increment the program counters
                        // we set the proper values to send a message to the child we just launched
                        AddEntryToPT(processTable, pid, total_seconds, sharedClock->ns, launched);

                        //we schedule the process in the ready que
                        AddToReadyQue(pid);
                        
                        if(running < 1){
                            CurrentlyScheduled = pid;
                        }

                        //Since it is a new process the timeInSystem/runtime ratio will be 0
                        schedulingRatio[pid] = 0;
                        schedulingRatioNano[pid] = 0;
  
                        running++;
                        launched++;
                    }
                }
            } else if((tempPid = waitpid(-1, &status, WNOHANG)) > 0){
                //we check if a child has terminated, save the pid, and then remove the entry from the table matching that pid
                RemoveEntryFromPT(processTable, tempPid);
                running--;// program terminated so we decrement the running programs
            }
        }else{
            //now that we have launched all running programs we will wait for the last few to finish
            if(running == 1){
                lastProcess = true;
            }
            if((tempPid = waitpid(-1, &status, WNOHANG)) > 0 || running == 0){
                RemoveEntryFromPT(processTable, tempPid);
                running--;
                //we wait till there are no running programs to stop the timer
                if(running == 0){
                    break;
                }
            }
        }
        bool emptyQues;

        if(ReadyQue.empty() == false || BlockedQue.empty() == false){
            emptyQues = false;
        }else{
            emptyQues = true;
        }

        if(BlockedQue.empty() == false && ReadyQue.empty() == true){
            // cout << "we ran our deadlock algorithim ---------------------------------------" << endl;

            //we user our detection algorithim and it returns how many process where deleted, so we can that many processes again
            int howManyToReschedule = DeadlockResolution(outputFile);

            // launched -= howManyToReschedule;
        }

        
        //First we need to check if there are any resources that have been freed and its request can be granted so it can be woken up if so
        if(BlockedQue.empty() == false){
            for(int i = 0; i < BlockedQue.size(); i++){
                bool wakeup = false;
                int currentTableIndex = processTable[FindProcessIndexInPCB(BlockedQue.at(i))].processNumber;
                for(int j = 0; j < 10; j++){
                    if(resourceTables.requestedResources[currentTableIndex][j] <= resourceTables.availableResources[j] && resourceTables.requestedResources[currentTableIndex][j] > 0){
                        wakeup = true;
                        resourceTables.availableResources[j] -= resourceTables.requestedResources[currentTableIndex][j];
                        resourceTables.requestedResources[currentTableIndex][j] = 0;
                    }else{
                        wakeup = false;
                    }
                }
                if(wakeup == true){
                    stats.grantedAfterWaiting++;
                    cout << "we woke up a process." << endl;
                    //we granted its request.
                    for(int j = 0; j < 10; j++){
                        resourceTables.availableResources[j] -= resourceTables.requestedResources[currentTableIndex][j];
                        resourceTables.requestedResources[currentTableIndex][j] = 0;
                    }
                    processTable[FindProcessIndexInPCB(BlockedQue.at(i))].blocked = 0;

                    //we now need to add it back into the ready queue
                    ReadyQue.push_back(BlockedQue.at(i));
                    BlockedQue.erase(BlockedQue.begin() + i);//erase it from the blocked que
                    
                    
                }
            }
        }
        
        //This is our scheduling loop that process the ques and sends and receives messages.
        if(launched > 0 && running > 0 && ReadyQue.empty() == false){
            PrintProcessTable(processTable);
            
            //check if a process has passed more than a second of service time
            for(int i = 0; i < ReadyQue.size(); i++){
                if(processTable[FindProcessIndexInPCB(ReadyQue.at(i))].serviceTimeNano > 1000000000){
                    processTable[FindProcessIndexInPCB(CurrentlyScheduled)].serviceTimeNano = processTable[FindProcessIndexInPCB(CurrentlyScheduled)].serviceTimeNano - 1000000000;
                    processTable[FindProcessIndexInPCB(CurrentlyScheduled)].serviceTimeSeconds++;
                }
            }


            
            //We need to recalculate each schedule to runtime ratio
            if(ReadyQue.empty() == false){
                for(int i = 0; i < ReadyQue.size(); i++){
                    if(processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeSeconds == 0 && processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeNano == 0){
                        cout << "This is the reason why" << endl;
                        schedulingRatio[ReadyQue[i]] = 0;
                        schedulingRatioNano[ReadyQue[i]] = 0;
                    }else {
                        if(TotalMS > 0){
                            schedulingRatio[ReadyQue[i]] = processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeSeconds / (total_seconds - processTable[FindProcessIndexInPCB(ReadyQue[i])].startSeconds);
                            schedulingRatioNano[ReadyQue[i]] = processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeNano / (nanoElapsed - processTable[FindProcessIndexInPCB(ReadyQue[i])].startNano);
                        }else{
                            schedulingRatioNano[ReadyQue[i]] = processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeNano / (sharedClock->ns - processTable[FindProcessIndexInPCB(ReadyQue[i])].startNano);
                        }
                    }
                }
            }
            
            


            //we then need to set the CurrentlyScheduled
            CurrentlyScheduled = ReadyQue.front();
            for(int i = 0; i < ReadyQue.size(); i++){
                if(processTable[FindProcessIndexInPCB(ReadyQue[i])].serviceTimeSeconds == 0){
                    if( schedulingRatioNano[ReadyQue[i]] < schedulingRatioNano[CurrentlyScheduled]){
                        CurrentlyScheduled = ReadyQue[i];
                    }
                }else{
                    if( schedulingRatio[ReadyQue[i]] < schedulingRatio[CurrentlyScheduled]){
                        CurrentlyScheduled = ReadyQue[i];
                    }else if(schedulingRatio[ReadyQue[i]] == schedulingRatio[CurrentlyScheduled] && schedulingRatioNano[ReadyQue[i]] < schedulingRatioNano[CurrentlyScheduled]){
                        CurrentlyScheduled = ReadyQue[i];
                    }
                }
                
            }

            nanoElapsed += 1000 * ReadyQue.size();//we add 1000 ms for each process in the ready que for the scheduling algorithim
            
            //after we have set CurrentlyScheduled, we set our message variables
            msgBuffer.mtype = CurrentlyScheduled;
            msgBuffer.intData = SCHEDULE_LENGHT;
            strcpy(msgBuffer.strData, "Pancakes");
            int locationInPTB;
            
            
            if(verbose == 'Y' || verbose == 'y'){
                outputFile << "OSS messaging P" <<  processTable[FindProcessIndexInPCB(CurrentlyScheduled)].processNumber << " (PID "<< CurrentlyScheduled << ")" << endl;
            }
            //we make sure that we are only 
            if(ReadyQue.empty() == false){
                //Here we sent the message and error out if it fails
                
                if (msgsnd(msqid, &msgBuffer, sizeof(msgbuffer) - sizeof(long), 0) == -1) {
                    std::perror("msgsnd to child 1 failed");
                    exit(1);
                }
                rcvbuf;
                // read a message meant for the parent
                // cout << "OSS waiting for message." << endl;
                if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1) {
                    std::perror("failed to receive message in parent");
                    exit(1);
                }
            }
            

            //here we output the available resources
            outputFile << "Available resources:" << endl;
            outputFile << "r0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
            for(int i = 0; i < 10; i++){
                outputFile << resourceTables.availableResources[i]<< "\t";
            }
            outputFile << endl;

            int recievedValue;
            //after we recieve a message back from our child we need to update the time runningTime of the process and place it into the blocked que if neccesary
            // now we need to reschedule the process
            processTable[FindProcessIndexInPCB(CurrentlyScheduled)].serviceTimeNano += SCHEDULE_LENGHT;
            int processindex = FindProcessIndexInPCB(CurrentlyScheduled);// currently scheduled's index in the processTable

            if(rcvbuf.intData > 0 && rcvbuf.intData < 11){//this means it is returned a positve value and freed a resource
                // we have to subtract or add one so we can use it as an index
                recievedValue = rcvbuf.intData - 1;
            
                outputFile << "OSS: P" << processTable[processindex].processNumber << "(PID:" << CurrentlyScheduled << ") released R" << recievedValue* -1 << " at time " << total_seconds << ":" << nanoElapsed << endl;

                if(resourceTables.availableResources[recievedValue] < 20){// if the requested resource is available then we permit it.
                    resourceTables.allocatedResources[processTable[processindex].processNumber][recievedValue]--; // increment the allocated resource by 1
                    resourceTables.availableResources[recievedValue]++;// increment the required resource
                }else{ // otherwise the resource is not available and we need to put the process in the blocked que
                    cout << "Somehow we returned too many resources and this should never happen so make sure it doesn't!!!!!" << endl;
                }


            } else if(rcvbuf.intData < 0){// a negative value was returned so this means that we need to aprove a resource and add to its allocated resources
                // we have to subtract or add one so we can use it as an index
                recievedValue = rcvbuf.intData + 1;
                outputFile << "OSS: recieved request from P" << processTable[processindex].processNumber << "(PID:" << CurrentlyScheduled << ") for R" << recievedValue* -1 << " at time " << total_seconds << ":" << nanoElapsed << endl;

                if(resourceTables.availableResources[(recievedValue)*-1] > 0){// if the requested resource is available then we permit it.
                    stats.grantedImmediadiately++;
                    outputFile << "OSS: approved request from P" << processTable[processindex].processNumber << "(PID:" << CurrentlyScheduled << ") for R" << recievedValue * -1 << " at time " << total_seconds << ":" << nanoElapsed << endl;

                    // increment the allocated resource by 1
                    resourceTables.allocatedResources[processTable[processindex].processNumber][(recievedValue)*-1]++; 
                    resourceTables.availableResources[(recievedValue)*-1]--;// decrement the required resource
                }else{ // otherwise the resource is not available and we need to put the process in the blocked que
                    outputFile << "OSS: blocked request from P" << processTable[processindex].processNumber << "(PID:" << CurrentlyScheduled << ") for R" << recievedValue* -1 << " at time " << total_seconds << ":" << nanoElapsed << endl;
                    
                    BlockedQue.push_back(CurrentlyScheduled);
                    processTable[processindex].blocked = 1;
                    resourceTables.requestedResources[processTable[processindex].processNumber][recievedValue * -1]++;

                    for(int i = 0; i < ReadyQue.size(); i++){
                        if(ReadyQue.at(i) == CurrentlyScheduled){
                            ReadyQue.erase(ReadyQue.begin() + i);
                            break;
                        }
                    }
                }
        
                //pretty sure nothing needs to be done for this since its still in the ready que and we will recalculate the schedule ratio later
            } else if(rcvbuf.intData == 0){

                outputFile << "OSS: P" << processTable[processindex].processNumber << " terminated. The following resources were released: " << endl;
                outputFile << "\tr0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
                for(int j = 0; j < 10; j++){
                    outputFile << "\t" << resourceTables.allocatedResources[processTable[processindex].processNumber][j];
                }
                outputFile << endl;
                

                stats.termitedSuccesfully++;
                // we have to subtract or add one so we can use it as an index
                if(recievedValue < 0){
                    recievedValue = rcvbuf.intData + 1;
                }else{
                    recievedValue = rcvbuf.intData - 1;
                }
                // it terminated and is done so we remove it from the ready que and release all its resources.
                for(int i = 0; i < 10; i++){
                    resourceTables.availableResources[i] += resourceTables.allocatedResources[processTable[processindex].processNumber][i];
                    resourceTables.allocatedResources[processTable[processindex].processNumber][i] = 0;
                }
                
                //remove it from the ready que
                for(int i = 0; i < ReadyQue.size(); i++){
                    // cout << "ReadyQue[i] == CurrentlyScheduled: " << ReadyQue.at(i) << " == " << CurrentlyScheduled << endl;
                    if(ReadyQue.at(i) == CurrentlyScheduled){
                        ReadyQue.erase(ReadyQue.begin() + i);
                        break;
                    }
                }                
            }

            if((stats.grantedImmediadiately + stats.grantedAfterWaiting) >= expectedNextPrint){
                expectedNextPrint += 20;
                outputFile << "allocatedResources: " << endl;
                outputFile << "\tr0\tr1\tr2\tr3\tr4\tr5\tr6\tr7\tr8\tr9" << endl;
                for(int i = 0; i < 19; i++){
                    outputFile << "P" << i << " ";
                    for(int j = 0; j < 10; j++){
                        outputFile << resourceTables.allocatedResources[i][j] << "\t";
                    }
                    outputFile << endl;
                }
            }

            resourceTables.printResources();
            
        }else{

        }
        nanoElapsed += SCHEDULE_LENGHT;
    }

    // We pring the process table one last time to show it is empty

    outputFile << "Statistics:" << endl;
    outputFile << "The number of processes that were killed by the deadlock resolution algorithim were: " << stats.terminatedByDeadLockRes << endl;
    outputFile << "The number of processes that terminated naturally were: " << stats.termitedSuccesfully << endl;
    outputFile << "The number of times the deadlock function ran was: " << stats.deadlockRan << endl;
    outputFile << "The number of times the deadlock resolution algorithim ran function ran was: " << stats.deadlockAlgorithimRan << endl;
    outputFile << "The average number of process that were deleted by the deadlock resoulution algorithim was: " << stats.terminatedByDeadLockRes/stats.deadlockAlgorithimRan << endl;


    PrintProcessTable(processTable);   


    // get rid of message queue
    if (msgctl(msqid, IPC_RMID, nullptr) == -1) {
        std::perror("msgctl to get rid of queue in parent failed");
        exit(1);
    }

    outputFile.close();

    // detach and destry shared memory
    shmdt(sharedClock);
    shmctl(shmId, IPC_RMID, nullptr);
    cout << "---------All processes completed---------" << endl;


    return 0;
}