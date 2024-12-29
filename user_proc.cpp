//Mateo John
//11/5/24


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
#include <random>

#define SHM_KEY 1234 // we set the shared memory key

#define PERMS 0644

using namespace std;


typedef struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
} msgbuffer;


//this is the same struct used for the clock as is used for parent.
struct SharedClock {
    int ms;
    int ns;
};

// create out shared memory variables
int shmId;
SharedClock *sharedClock;
int msqid = 0;


void handle_timeout(int sig) {
    cout << "\n60 seconds have passed. Terminating child processes and cleaning...\n";
    //detach from shared memory
    shmdt(sharedClock);
    if (msgctl(msqid, IPC_RMID, nullptr) == -1) {
        std::perror("msgctl to get rid of queue in parent failed");
        exit(1);
    }
    exit(0); // Exit the program after killing children
}

//This handles the control ctrl-C terminate program call and cleans up
 void handle_sigint(int sig) {
    std::cout << "\nCtrl-C detected. Cleaning up...\n";

    //detach from shared memory
    shmdt(sharedClock);

    if (msgctl(msqid, IPC_RMID, nullptr) == -1) {
        std::perror("msgctl to get rid of queue in parent failed");
        exit(1);
    }
    exit(0); // Exit the program after cleaning up
}


int main(int argc, char* argv[]){

    signal(SIGALRM, handle_timeout);  // Handle timeout signal (SIGALRM)
    signal(SIGINT, handle_sigint);    // Handle Ctrl-C (SIGINT)
    
    std::random_device rd; // Non-deterministic seed source
    std::mt19937 generator(rd()); // Mersenne Twister engine
    std::uniform_int_distribution<int> distribution(1, 1000);
    int ourResources[10];

    for(int i = 0; i < 10; i++){
        ourResources[i] = 0;
    }

    // we set defualts
    int termTime;
    int secLimit;
    double nanoLimit;
    int termTimeNano;

    // takes the command line argument
    if(argc > 2){
        secLimit = atoi(argv[1]);
        nanoLimit = stod(argv[2]);
    }else {
        cout << "Invalid program call, please try again by calling the function a \"./user [int]" << endl;
        exit(1);
    }

    //first we handle the message from perant before we work with the clock --------------------------------
    msgbuffer buf;
    msgbuffer rcvbuf;
	buf.mtype = 1;
	int msqid = 0;
	key_t key;

	// get a key for our message queue
	if ((key = ftok("msgq.txt", 1)) == -1) {
		perror("ftok");
		exit(1);
	}

	// create our message queue
	if ((msqid = msgget(key, PERMS)) == -1) {
		perror("msgget in child");
		exit(1);
	}

    srand((unsigned) time(NULL));
	
    
    //Now we finished dealing with messages we can move onto the clock-------------------------------------

    // Get the shared memory segment created by the parent
    shmId = shmget(SHM_KEY, sizeof(SharedClock), 0666);
    if (shmId < 0) {
        std::cerr << "Failed to access shared memory segment.\n";
        exit(1);
    }

    // Attach the shared memory segment to the child's address space
    sharedClock = static_cast<SharedClock*>(shmat(shmId, nullptr, 0));
    if (sharedClock == reinterpret_cast<SharedClock*>(-1)) {
        std::cerr << "Failed to attach shared memory segment.\n";
        exit(1);
    }

    termTime = (secLimit*1000) + sharedClock->ms;
    termTimeNano = (nanoLimit*1000) + sharedClock->ns;

    //calculate the termination time in seconds
    int TermTimeSecs = (termTime-(termTime%1000))/1000;

    cout << "WORKER PID: " << getpid() << " PPID: " << getppid() << " SysClockS: " << sharedClock->ms << " SysclockNano: " << sharedClock->ns <<  " TermTimeS: " << TermTimeSecs << " TermTimeNano: " << termTimeNano    << "-- Just Starting" << endl;

    int secondMark = sharedClock->ms + 1000;//sets the time we should output progress
    int sec = 0;
    termTime += termTimeNano/1000000;// we add the termination time in nanoseconds to the termination time.
    bool terminating = false;
    while(true){
        bool validAction = false;
        
        // receive a message, but only one for us
        if ( msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1) {
            perror("failed to receive message from parent.\n");
            cout << "My pid: " << getpid() << endl;
            exit(1);
        }

        std::uniform_int_distribution<int> distribution(1, 1000);
        std::uniform_int_distribution<int> resources(1, 10);

        // Generate a random number

        //we 
        while(validAction == false && terminating == false){
            int userDecision = distribution(generator);

            if(userDecision <= 995){//we request a resource, so we send a negative value
                // now send a message back to our parent
                int requestedResouce = resources(generator);

                if(ourResources[requestedResouce-1] < 20){
                    validAction = true;
                    ourResources[requestedResouce-1]++;
                    buf.mtype = getppid();
                    //we send back the number indicating what resource we want, and then negative so we indicate we are requesting
                    buf.intData = requestedResouce * -1;
                    strcpy(buf.strData,"oranges");
                    cout << "User: we request a resource, so we send a negative value" << endl;
                    if (msgsnd(msqid,&buf,sizeof(msgbuffer)-sizeof(long),0) == -1) {
                        perror("msgsnd to parent failed\n");
                        exit(1);
                    }
                    cout << "User: message was sent to the parent" << endl;
                }
            } else if(userDecision >= 996 && userDecision <= 996){//we return a resource, so we return a postive value
                // cout << "User: User gets blocked and returns the amount of time that it used. Pid: " << getpid() << endl;
                
                int returnedResouce = resources(generator);

                if(ourResources[returnedResouce-1] > 0){
                    validAction = true;
                    ourResources[returnedResouce-1]--;
                    buf.mtype = getppid();
                
                    buf.intData = returnedResouce;
                    strcpy(buf.strData,"oranges");
                    cout << "User: we return a resource, so we return a postive value." << endl;
                    if (msgsnd(msqid,&buf,sizeof(msgbuffer)-sizeof(long),0) == -1) {
                        perror("msgsnd to parent failed\n");
                        exit(1);
                    }
                }
            } else{// we terminate and all resources are freed, we return a 0
                // cout << "User: finishes early and returns a negative value of how much time was remaining. Pid: " << getpid() << endl;
                validAction = true;
                buf.mtype = getppid();

                // cout << "rcvbuf.intData: " << rcvbuf.intData << ", timePercentage: " << timePercentage << endl;
                buf.intData = 0;

                strcpy(buf.strData,"oranges");
                if(msgsnd(msqid,&buf,sizeof(msgbuffer)-sizeof(long),0) == -1) {
                    perror("msgsnd to parent failed\n");
                    exit(1);
                }
                // cout << "User: We should break out of the user loop now and not expect anymore msg sends. Pid: " << getpid() << endl;
                terminating = true;// we finished so we break out of the loop so we can terminate.
            }
        }

        if(terminating == true){
            break;
        }


        if(sharedClock->ms > secondMark){
            secondMark += 1000;
            sec++;
            cout << "WORKER PID: " << getpid() << " PPID: " << getppid() << " SysClockS: " << (sharedClock->ms) << " SysclockNano: " << sharedClock->ns <<  " TermTimeS: " << TermTimeSecs << " TermTimeNano: " << termTimeNano << "-- " << sec << " seconds have passed since starting" << endl;
        }
        
    }
    
    cout << "WORKER PID: " << getpid() << " PPID: " << getppid() << " SysClockS: " << sharedClock->ms << " SysclockNano: " << sharedClock->ns <<  " TermTimeS: " << TermTimeSecs << " TermTimeNano: " << termTimeNano << "-- Terminating" << endl;
    // detach from shared memory
    shmdt(sharedClock);
    return 0;
}
