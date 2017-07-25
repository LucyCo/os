#include "uthreads.h"
#include <vector>
#include <iostream>
#include <errno.h>
#include <algorithm>
#include <signal.h>
#include <sys/time.h>
#include <setjmp.h>

#define FAIL -1
#define MILLION 1000000
#define SET_TIMER_FAIL "The timer period is too large."
#define BLOCK_FAIL "The how argument is invalid."
#define MAX_THREADS "Maximum threads."
#define NEG_QUAN "Quantom is negative."
#define ID_NOT_EXIST "The given id doesn't exist."
#define BLOCK_MAIN "Main cannot be blocked."
#define BAD_ALLOC "Bad_alloc caught."

using namespace std;

struct itimerval tv;

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
		: "=g" (ret)
		: "0" (addr));
	return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
	address_t ret;
	asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
		: "=g" (ret)
		: "0" (addr));
	return ret;
}

#endif

/**
 * Prints to the screen an informed message of the given error type.
 */
void printError(const char* msg, bool typeError)
{
	(typeError)? cout << "system error: " << msg << endl :
				cout << "thread library error: " << msg << endl;
}

/**
 * class Thread: constructs a new thread and initializes it.
 */
class Thread
{
	public:
		unsigned int _id; //the thread's id
		int _quantom; //the current quantom amount of the thread
		Priority _priority; //the priority of the thread
		void(*_func)(void); //pointer to the thread's function
		sigjmp_buf* _env; //environment of the thread
		char* _stack; //memory stack of the thread

		/**
		 * Constructor: constructs a new thread and initializes it.
		 */
		Thread(unsigned int id, int quantom, Priority priority, void(*func)(void)):
			_id(id), _quantom(quantom), _priority(priority), _func(func) {
			try { //try initializing the variables of the class-some may cause bad alloc error
				address_t sp, pc;
				if (id == 0) {
					pc = (address_t)0;
				}
				else {
					_stack = new char [STACK_SIZE];
					pc = (address_t)_func;
					sp = (address_t)_stack + STACK_SIZE - sizeof(address_t);
				}
				_env =(sigjmp_buf*) new sigjmp_buf();
				int ret_val = sigsetjmp((*_env), 1);
				if (ret_val == 1) {
					return;
				}

				((*_env)->__jmpbuf)[JB_SP] = translate_address(sp);
				((*_env)->__jmpbuf)[JB_PC] = translate_address(pc);
				sigemptyset(&((*_env)->__saved_mask));

			}
			catch (bad_alloc& ba)
			{ //catch bad alloc if allocation failed and print a system error.
				printError(BAD_ALLOC, false);
			}

		}

		~Thread() {
			if (_id != 0)
			{
				delete[] _stack;
			}
			delete _env;
		}
};


Thread* running; //pointer to current running thread
vector<Thread*> readyList; //the ready list
vector<Thread*> blockedList; //the blocked list
static unsigned int nextId;
int delFuncQuan = 0; //deleted thread's quantom counter

sigset_t signals;

/**
 * Blocks or un-blocks all signals - call with "true" when the event starts running and
 * with "false" when it ends.
 */
void signalHandler(bool block)
{
	if (block)
	{
		if (sigprocmask (SIG_BLOCK, &signals, NULL) == FAIL)
		{
			printError(BLOCK_FAIL, true);
			exit(1);
		}
	}
	else
	{
		if (sigprocmask (SIG_UNBLOCK, &signals, NULL) == FAIL)
		{
			printError(BLOCK_FAIL, true);
			exit(1);
		}
	}
}

/**
 * Checks weather the given ID is in the blocked list - returns an iterator for the object if found,
 * else returns the last object
 */
vector<Thread*>::iterator inBlockedList(int tid)
{
	vector<Thread*>::iterator it;
	for (it = blockedList.begin(); it != blockedList.end(); it++)
	{
		if ((*it)->_id == (unsigned int)tid)
		{
			return it;
		}
	}
	return it;
}

/**
 * Checks weather the given ID is in the ready list - returns an iterator for the object if found,
 * else returns the last object
 */
vector<Thread*>::iterator inReadyList(int tid)
{
	vector<Thread*>::iterator it;
	for (it = readyList.begin(); it != readyList.end(); it++)
	{
		if ((*it)->_id == (unsigned int)tid)
		{
			return it;
		}
	}
	return it;
}

/**
 * Checks weather the given ID is in the ready list - returns false if nor, else otherwise
 */
bool isInReadyList(int tid)
{
		return inReadyList(tid) != readyList.end();
}

/**
 * Checks weather the given ID is in the blocked list - returns false if nor, else otherwise
 */
bool isInBlockedList(int tid)
{
	return inBlockedList(tid) != blockedList.end();
}

/**
 * Adds the given thread to the ready list, the function pushes the thread to the back of the list and
 * swaps the threads if the new thread's priority is higher than the other threads' stops, when it is smaller
 * or equal. this way the new thread is placed in a position according to it's priority.
 */
static void addThreadReady(Thread* thr){
	readyList.push_back(thr);
	if(readyList.size() > 1)
	{
		int i = 0;
		Thread* temp;
		while(readyList[readyList.size() - i - 1]->_priority <
					readyList[readyList.size() - i - 2]->_priority)
		{
			temp = readyList[readyList.size() - i - 2];
			readyList[readyList.size() - i - 2] = readyList[readyList.size() - i - 1];
			readyList[readyList.size() - i - 1] = temp;
			if((readyList.size() - i - 2) == 0)
			{
				break;
			}
			++i;
		}
	}
}

void timer_handler(int);

/**
 * Resets the time count for this process when either, the quantom for a thread ends, or threads are switched due to
 * termination or blocking of the running thread.
 */
void resetTimer(int quantum_usecs)
{
	signal(SIGVTALRM, timer_handler);
	tv.it_value.tv_sec = quantum_usecs / MILLION;  /* first time interval, seconds part */
	tv.it_value.tv_usec = quantum_usecs % MILLION; /* first time interval, microseconds part */
	tv.it_interval.tv_sec = quantum_usecs / MILLION;  /* following time intervals, seconds part */
	tv.it_interval.tv_usec = quantum_usecs % MILLION; /* following time intervals, microseconds part */
	if (setitimer(ITIMER_VIRTUAL, &tv, NULL) == FAIL)
	{ //if the set timer fails, print out a system call error and exit with the value 1
		printError(SET_TIMER_FAIL, true);
		exit(1);
	}
}

/**
 * switches between one thread to the other, setting the new thread in running state.
 */
void switchThreads(Thread* newThread, Thread* oldThread)
{
	newThread->_quantom++; //whenever a thread starts running, set quan to plus 1
	if (oldThread != NULL)
	{
		int ret_val = sigsetjmp(*(oldThread->_env), 1);
		if (ret_val == 1) {
			return;
		}
	}
	resetTimer(tv.it_interval.tv_usec);
	signalHandler(false); //unblock signals
	siglongjmp(*(newThread->_env), 1);

}

/**
 * timer handler sends the current thread and a new thread to be switched, unless the ready list is empty,
 * then the current running thread will continue to run.
 */
void timer_handler(int sig)
{
	if (readyList.size() == 0)
	{
		running->_quantom++;
		return;
	}
	else
	{
		Thread* temp = running;
		running = readyList[0];
		readyList.erase(readyList.begin());
		addThreadReady(temp);
		switchThreads(running, temp);
	}
}

/**
 * Initializes the thread library, according to the given quantom seconds. if the quantom seconds'
 * value is higher than a million, then the value will be read as seconds.
 */
int uthread_init(int quantum_usecs) {
	if (quantum_usecs <= 0)
	{
		printError(NEG_QUAN, false);
		return FAIL;
	}
	running = new Thread (0, 1, ORANGE, 0); //sets main as a new thread with orange priority
	nextId = 1;
	sigemptyset(&signals);
	sigaddset (&signals, SIGVTALRM); //add sigvtalrm to signals set
	resetTimer(quantum_usecs);
	return 0;
}

/**
 * finds the next available ID number, even if the ID used to be associated with a thread that was
 * terminated it will still return as an empty id and will be reallocated to a new thread.
 */
void findNextId()
{
	while (isInReadyList(nextId) || isInBlockedList(nextId) || running->_id == nextId)
	{
		nextId++;
	}
}
/**
 * Creates a new thread with the given function pointer and the priority variable.
 */
int uthread_spawn(void(*f)(void), Priority pr) {
	signalHandler(true); //block all signals
	if (nextId >= MAX_THREAD_NUM) { //print error if the number of threads is over 100
		printError(MAX_THREADS, false);
		return FAIL;
	}
	Thread* thr = new Thread(nextId, 0, pr, f); //create a new thread with the given priority
	addThreadReady(thr); //add it to the current ready list
	findNextId(); //set the nextID again
	signalHandler(false); //unblock all signals
	return thr->_id; //reutrn the ID
}

/*
 * This method switch between the running method and the next method in the ready list.
 * if the given bool is true, we delete the current running thread. otherwise, we saves him
 * in the blocked list.
 */
void switchRunning(bool toDelete)
{
	Thread* temp = running;
	running = readyList[0];
	readyList.erase(readyList.begin());
	if (toDelete)
	{ // Case for deleting the current running thread.
		delete temp;
		switchThreads(running, NULL);
	}
	else
	{ // Case for saving the current running thread.
		blockedList.push_back(temp);
		switchThreads(running, temp);
	}
}

/*
 * This method delete a Thread with the given id. we can assume the id is
 * in ready list or blocked list.
 */
void removeFromList(int tid)
{
	vector<Thread*>::iterator it;
	Thread* temp;
	if (isInReadyList(tid))
	{ // Case for given Thread is in the ready list.
		it = inReadyList(tid);
		temp = *it;
		readyList.erase(it);
	}
	else
	{ // Case for given Thread is in the blocked list.
		it = inBlockedList(tid);
		temp = *it;
		blockedList.erase(it);
	}
	delFuncQuan = delFuncQuan + temp->_quantom;
	delete temp;
}

/*
 * This method delete a Thread with the given id. we can assume the id is
 * not 0 (the id of the main). also, we can assume that there is at least one thread in the
 * ready list (that is because the main thread can't be terminated or blocked and the given id
 * is not the main's id).
 * If the given id is not exist the method returns -1.
 */
int removeThread(unsigned int tid)
{
	if (isInReadyList(tid) || isInBlockedList(tid) || running->_id == tid)
	{// The given Thread exists.
		if (tid < nextId)
		{
			nextId = tid;
		}
		if (running->_id == (unsigned int)tid)
		{ // Case for given Thread is the running Thread.
			if (tid < nextId)
			{
				nextId = tid;
			}
			delFuncQuan = delFuncQuan + running->_quantom;
			switchRunning(true);//switch between the running thread and the next thread on the list.
		}
		else
		{ // Case for given Thread is in the blocked list or The Ready list.
			removeFromList(tid);
		}
		signalHandler(false);
		return 0;
	}
	signalHandler(false);
	printError(ID_NOT_EXIST, false);
	return FAIL; //tid not exists.
}

/*
 * Deletes all the Threads.
 */
void deleteLists()
{
	for (int i = 0; i < (int)readyList.size(); i++)
	{
		delete readyList[i];
	}
	for (int i = 0; i < (int)blockedList.size(); i++)
	{
		delete blockedList[i];
	}
	delete running;
}

/*
 * Terminate a thread.
 * If the given id of is the id of the main (0) the program exit.
 * If the given id is not exist the method returns -1.
 */
int uthread_terminate(int tid){
	signalHandler(true);
	if (tid == 0)
	{
		deleteLists();
		exit(0);
	}
	return removeThread((unsigned int)tid);
}

/*
 * This method moves a Thread with the given id to the blocked list.
 * If the Thread with the given id is in the blocked list, the method does nothing.
 * If the given id is not exist or is the id of the main (0) the methods return -1.
 */
int block(int tid)
{
	if (isInReadyList(tid) || isInBlockedList(tid) || running->_id == (unsigned int)tid)
	{ // The given Thread exists.
		vector<Thread*>::iterator it;
		if ((it = inReadyList(tid)) != readyList.end())
		{ // Case for given Thread is in the ready list.
			blockedList.push_back(*it);
			readyList.erase(it);
		}
		else if(running->_id == (unsigned int)tid)
		{ // Case for given Thread is the running Thread.
			switchRunning(false);
		}
		signalHandler(false);
		return 0;
	}
	signalHandler(false);
	printError(ID_NOT_EXIST, false);
	return FAIL; //tid not exists.
}

/*
 * Suspend a thread.
 * If the Thread with the given id is in the blocked list, the method does nothing.
 * If the given id is not exist or is the id of the main (0) the method returns -1.
 */
int uthread_suspend(int tid){
	signalHandler(true);
	if (tid == 0)
	{
		printError(BLOCK_MAIN, false);
		return FAIL;
	}
	return block(tid);
}

/*
 * Resume a thread.
 * If the Thread with the given id is the running Thread or in
 * the ready list, the method does nothing.
 * If the given id is not exist the method returns -1.
 */
int uthread_resume(int tid)
{
	signalHandler(true);
	vector<Thread*>::iterator it;
	if (isInReadyList(tid) || isInBlockedList(tid) || running->_id == (unsigned int)tid)
	{ // The given Thread exists.
		if ((it = inBlockedList(tid)) != blockedList.end())
		{ // Case for given Thread is in the blocked list.
			addThreadReady(*it);
			blockedList.erase(it);
		}
		signalHandler(false);
		return 0;
	}
	signalHandler(false);
	printError(ID_NOT_EXIST, false);
	return FAIL; //tid not exists.
}

/* Get the id of the calling thread */
int uthread_get_tid(){
	return running->_id;
}

/* Get the total number of library quantums */
int uthread_get_total_quantums(){
	signalHandler(true);
	int sum = 0;
	for (int i = 0; i < (int)readyList.size(); i++){
		sum += readyList[i]->_quantom;
	}
	for (int i = 0; i < (int)blockedList.size(); i++){
		sum += blockedList[i]->_quantom;
	}
	signalHandler(false);
	return sum + delFuncQuan + running->_quantom;
}

/*
 * Get the number of thread quantums.
 * If the given id is not exist the method returns -1.
 */
int uthread_get_quantums(int tid){
	signalHandler(true);
	vector<Thread*>::iterator it = inBlockedList(tid);
	if (it != blockedList.end())
	{ // Case for given Thread is in the blocked list.
		signalHandler(false);
		return (*it)->_quantom;
	}
	it = inReadyList(tid);
	if (it != readyList.end())
	{ // Case for given Thread is in the ready list.
		signalHandler(false);
		return (*it)->_quantom;
	}
	if (running->_id == (unsigned int)tid)
	{ // Case for given Thread is the running Thread.
		signalHandler(false);
		return running->_quantom;
	}
	printError(ID_NOT_EXIST, false);
	return FAIL; //tid not exists.
}
