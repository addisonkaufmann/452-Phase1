/* ------------------------------------------------------------------------
	 phase1.c

	 University of Arizona
	 Computer Science 452
	 Fall 2015

	 ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
int isInKernelMode();
int isInterruptEnabled();
int enableInterrupts();
int enterKernelMode();
int enterUserMode();
unsigned int getNextPid();
int isProcessTableFull();
void initProcessTable();
void initReadyLists();



/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// the process table
procStruct ProcTable[MAXPROC];

// Process lists
// static procPtr ReadyList;  

static procPtr ReadyLists[SENTINELPRIORITY]; //linked list (queue) for each priority

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
	 Name - startup
	 Purpose - Initializes process lists and clock interrupt vector.
						 Start up sentinel process and the test process.
	 Parameters - argc and argv passed in by USLOSS
	 Returns - nothing
	 Side Effects - lots, starts the whole thing
	 ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
		int result; /* value returned by call to fork1() */

		/* initialize the process table */
		if (DEBUG && debugflag)
				USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
		initProcessTable();


		// Initialize the Ready list, etc.
		if (DEBUG && debugflag)
				USLOSS_Console("startup(): initializing the Ready list\n");
		initReadyLists();

		// Initialize the clock interrupt handler -- ignoring for now

		// startup a sentinel process
		if (DEBUG && debugflag)
				USLOSS_Console("startup(): calling fork1() for sentinel\n");
		result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
										SENTINELPRIORITY);
		if (result < 0) {
				if (DEBUG && debugflag) {
						USLOSS_Console("startup(): fork1 of sentinel returned error, ");
						USLOSS_Console("halting...\n");
				}
				USLOSS_Halt(1);
		}
	
		// start the test process
		if (DEBUG && debugflag)
				USLOSS_Console("startup(): calling fork1() for start1\n");
		result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
		if (result < 0) {
				USLOSS_Console("startup(): fork1 for start1 returned an error, ");
				USLOSS_Console("halting...\n");
				USLOSS_Halt(1);
		}

		USLOSS_Console("startup(): Should not see this message! ");
		USLOSS_Console("Returned from fork1 call that created start1\n");

		return;
} /* startup */


/* ------------------------------------------------------------------------
	 Name - finish
	 Purpose - Required by USLOSS
	 Parameters - none
	 Returns - nothing
	 Side Effects - none
	 ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
		if (DEBUG && debugflag)
				USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
	 Name - fork1
	 Purpose - Gets a new process from the process table and initializes
						 information of the process.  Updates information in the
						 parent process to reflect this child process creation.
	 Parameters - the process procedure address, the size of the stack and
								the priority to be assigned to the child process.
	 Returns - the process id of the created child or -1 if no child could
						 be created or if priority is not between max and min priority.
	 Side Effects - ReadyList is changed, ProcTable is changed, Current
									process information changed
	 ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
					int stacksize, int priority)
{
		int procSlot = -1;

		if (DEBUG && debugflag)
				USLOSS_Console("fork1(): creating process %s\n", name);

		// test if in kernel mode; halt if in user mode 
		if ( !isInKernelMode() ) {
			USLOSS_Console("fork1(): USLOSS in user mode. Halting...\n");
			USLOSS_Halt(1);
		}

		if (name == NULL || startFunc == NULL) {
			fprintf(stderr, "fork1(): Name and/or start function cannot be null.\n");
			return -1;
		}

		// test if trying to apply sentinel priority to non-sentinel process
		if (strcmp(name, "sentinel") != 0 && priority == SENTINELPRIORITY) {
			fprintf(stderr, "fork1(): Cannot assign sentinel prority to process other than the sentinel. Halting...\n");
			USLOSS_Halt(1);
		}

		// check for priority out of range
		if (priority > SENTINELPRIORITY || priority < MAXPRIORITY) {
			fprintf(stderr, "fork1(): Priority out of range.\n");
			return -1;
		}

		// Return if stack size is too small
		if ( stacksize < USLOSS_MIN_STACK ){
			USLOSS_Console("fork1(): Requested Stack size too small.\n");
			return -2;
		}

		// Is there room in the process table? What is the next PID?
		if (isProcessTableFull()){
			USLOSS_Console("fork1(): Process Table is full.\n");
			return -1;
		}

		int pid = getNextPid();


		// fill-in entry in process table */
		if ( strlen(name) >= (MAXNAME - 1) ) {
				USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
				USLOSS_Halt(1);
		}

		strcpy(ProcTable[procSlot].name, name);
		ProcTable[procSlot].startFunc = startFunc;
		if ( arg == NULL )
				ProcTable[procSlot].startArg[0] = '\0';
		else if ( strlen(arg) >= (MAXARG - 1) ) {
				USLOSS_Console("fork1(): argument too long.  Halting...\n");
				USLOSS_Halt(1);
		}
		else
				strcpy(ProcTable[procSlot].startArg, arg);

		// Initialize context for this process, but use launch function pointer for
		// the initial value of the process's program counter (PC)

		USLOSS_ContextInit(&(ProcTable[procSlot].state),
											 ProcTable[procSlot].stack,
											 ProcTable[procSlot].stackSize,
											 NULL,
											 launch);

		// for future phase(s)
		p1_fork(ProcTable[procSlot].pid);

		// More stuff to do here...

		return pid;
} /* fork1 */

/* ------------------------------------------------------------------------
	 Name - launch
	 Purpose - Dummy function to enable interrupts and launch a given process
						 upon startup.
	 Parameters - none
	 Returns - nothing
	 Side Effects - enable interrupts
	 ------------------------------------------------------------------------ */
void launch()
{
		int result;

		if (DEBUG && debugflag)
				USLOSS_Console("launch(): started\n");

		// Enable interrupts
		result = enableInterrupts();
		if (result == -1) {
			fprintf(stderr, "launch(): failed to enable interrupts.\n");
		}

		// Call the function passed to fork1, and capture its return value
		result = Current->startFunc(Current->startArg);

		if (DEBUG && debugflag)
				USLOSS_Console("Process %d returned to launch\n", Current->pid);

		quit(result);

} /* launch */


/* ------------------------------------------------------------------------
	 Name - join
	 Purpose - Wait for a child process (if one has been forked) to quit.  If 
						 one has already quit, don't wait.
	 Parameters - a pointer to an int where the termination code of the 
								quitting process is to be stored.
	 Returns - the process id of the quitting child joined on.
						 -1 if the process was zapped in the join
						 -2 if the process has no children
	 Side Effects - If no child process has quit before join is called, the 
									parent is removed from the ready list and blocked.
	 ------------------------------------------------------------------------ */
int join(int *status)
{
		return -1;  // -1 is not correct! Here to prevent warning.
} /* join */


/* ------------------------------------------------------------------------
	 Name - quit
	 Purpose - Stops the child process and notifies the parent of the death by
						 putting child quit info on the parents child completion code
						 list.
	 Parameters - the code to return to the grieving parent
	 Returns - nothing
	 Side Effects - changes the parent of pid child completion status list.
	 ------------------------------------------------------------------------ */
void quit(int status)
{
		p1_quit(Current->pid);
} /* quit */


/* ------------------------------------------------------------------------
	 Name - dispatcher
	 Purpose - dispatches ready processes.  The process with the highest
						 priority (the first on the ready list) is scheduled to
						 run.  The old process is swapped out and the new process
						 swapped in.
	 Parameters - none
	 Returns - nothing
	 Side Effects - the context of the machine is changed
	 ----------------------------------------------------------------------- */
void dispatcher(void)
{
		procPtr nextProcess = NULL;

		p1_switch(Current->pid, nextProcess->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
	 Name - sentinel
	 Purpose - The purpose of the sentinel routine is two-fold.  One
						 responsibility is to keep the system going when all other
						 processes are blocked.  The other is to detect and report
						 simple deadlock states.
	 Parameters - none
	 Returns - nothing
	 Side Effects -  if system is in deadlock, print appropriate error
									 and halt.
	 ----------------------------------------------------------------------- */

int sentinel (char *dummy)
{
		if (DEBUG && debugflag)
				USLOSS_Console("sentinel(): called\n");
		while (1)
		{
				checkDeadlock();
				USLOSS_WaitInt();
		}
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
		// turn the interrupts OFF iff we are in kernel mode
		// if not in kernel mode, print an error message and
		// halt USLOSS
	if (isInKernelMode()) {
		unsigned int psr = USLOSS_PsrGet();
		unsigned int op = 0xfffffffd;
		int result = USLOSS_PsrSet(psr & op);
		if (result == USLOSS_ERR_INVALID_PSR) {
			fprintf(stderr, "Failed to set PSR to kernel mode.");
			USLOSS_Halt(0);
		}
	}
	else {
		fprintf(stderr, "Failed to disable interrupts as not in kernel mode.");
		USLOSS_Halt(0);
	}

	// TODO: May need more than just switching the bit?

} /* disableInterrupts */

/*
 * Returns 1 if in kernel mode, else 0.
 */
int isInKernelMode() {
	unsigned int psr = USLOSS_PsrGet();
	unsigned int op = 0x1;
	return psr & op;
}

int enterKernelMode() {
	unsigned int psr = USLOSS_PsrGet();
	unsigned int op = 0x1;
	int result = USLOSS_PsrSet(psr | op);
	if (result == USLOSS_ERR_INVALID_PSR) {
		return -1;
	}
	else {
		return 0;
	}

	// TODO: May need more than just switching the bit?
}

int enterUserMode() {
	unsigned int psr = USLOSS_PsrGet();
	unsigned int op = 0xfffffffe;
	int result = USLOSS_PsrSet(psr & op);
	if (result == USLOSS_ERR_INVALID_PSR) {
		return -1;
	}
	else {
		return 0;
	}
	// TODO: May need more than just switching the bit?
}

/*
 * Returns 1 if interrupts are enabled, else 0.
 */
int isInterruptEnabled() {
	unsigned int psr = USLOSS_PsrGet();
	unsigned int op = 0x2;
	return (psr & op) >> 1;
}

int enableInterrupts() {
	unsigned int psr = USLOSS_PsrGet();
	unsigned int op = 0x2;
	int result = USLOSS_PsrSet(psr & op);
	if (result == USLOSS_ERR_INVALID_PSR) {
		return -1;
	}
	else {
		return 0;
	}

	// TODO: May need more than just switching the bit?
}

/*
	Checks if process table is full
*/

int isProcessTableFull(){
	for (int i = 0; i < MAXPROC; i++){
		if (ProcTable[i].status == EMPTY){
			return 0;
		}
	}
	return 1;
}

/*
	Scans for available pid
*/
unsigned int getNextPid(){
	while (ProcTable[nextPid % MAXPROC].status != EMPTY){
		nextPid++;
	}
	return nextPid;
}

void initProcessTable(){
	for (int i = 0; i < MAXPROC; i++){
		ProcTable[i].status = EMPTY;
	}
}

void initReadyLists(){
	//do something maybe
}

