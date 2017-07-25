#include "blockchain.h"
#include "hash.h"
#include <iostream>
#include <vector>
#include <pthread.h>
#include <climits>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUCCESS 0
#define FALSE 0
#define TRUE 1
#define INIT_BOOL false
#define ALREADYINCHAIN 1
#define FAIL -1
#define NOTEXIST -2

using namespace std;

static unsigned int next_block_num; /* Define the lowest id of the next block to be add. */

/* This class represent a Block in the BlockChain. */
class Block
{
public:
	int _block_num; /* The id of the Block. */
	Block * _father; /* a pointer to the father's Block in the BlockChain. */
	char * _data;
	char * _hash_sig;
	int _nonce;
	int _level; /* The Block's level in the BlockChain. */
	size_t _length;
	bool _isLeaf; /* A boolean that indecates if the Block has any sons in the BlockChain. */

	/* Default constructor. will be used only for the genesis of the BlockChain. */
	Block()
	{
		this->_data = new char();
		this->_father = NULL;
		this->_block_num = 0;
		this->_hash_sig = NULL;
		this->_length = 0;
		this->_level = 0;
		this->_nonce = 0;
		this->_isLeaf = true;
	}

	/* constructor for Block. will be used for any Block that wish to enter BlockChain. */
	Block(char * data, Block * lowest_child, int block_num, int level, size_t length, int nonce)
	{
		_data = new char [length]; // allocate for string and ending \0
		strcpy(_data, data);
		_father = lowest_child;
		_block_num = block_num;
		_level = level;
		_length = length;
		_nonce = nonce;
		_isLeaf = true;
		_hash_sig =  NULL;
	}

	/* Destructor for Block. */
	~Block() {
		if (_hash_sig != NULL)
		{
			delete _hash_sig;
		}
		delete _data;
	}


};

/* This class represent a BlockChain. there can be only one at the time. */
class BlockChain
{
public:
	/* Default constructor for BlockChain. */
	BlockChain()
	{
		try{
			_genesis = new Block();
			_blocks.push_back(_genesis);
		}catch(bad_alloc& bad)
		{
			throw(bad);
		}
	}

	/* Destructor for BlockChain. */
	~BlockChain()
	{
		for (int i = 0; i < (int)_blocks.size(); i++) {
			delete _blocks[i];
		}
	}
	Block * _genesis; /* the root of the BlockChain. */
	vector<Block*> _blocks; /* A vector that contains all the Blocks in the BlockChain. */
};

BlockChain* newChain;
vector<Block*> pendingList;/* A vector that has all the Blocks that wish to enter the BlockChain.*/
pthread_t daemon;
pthread_t onClose;
pthread_cond_t cv;
pthread_mutex_t blockMutex; /* A mutex on the BlockChain* newChain. */
pthread_mutex_t pendingMutex;  /* A mutex on the vector pendingList. */
int numBlocks; /* The number of Blocks that was added to the BlockChain. */
bool inClose; /* A boolean that indecates if the BlockChain is being closed. */
bool isInitialized = INIT_BOOL;  /* A boolean that indecates if a BlockChain is initiallize. */
bool daemonInitialized = INIT_BOOL; /* A boolean that indecates if there exists a deamon thread. */
bool inAttachNow; /* A boolean that indecates if the BlockChain is in the method attach_now. */

/* get the lowest Block in the BlockChain (a Chian with the biggest level). if there exist more
	than one, it chooses them randomly. */
Block* getRandomLowestChild()
{
	vector<Block*>::iterator it;
	vector<Block*> candidates;
	int maxLevel = 0;
	for (it = newChain->_blocks.begin(); it != newChain->_blocks.end(); ++it)
	{
		if ((*it)->_level > maxLevel && candidates.size() > 0)
		{
			candidates.clear();
			candidates.push_back(*it);
			maxLevel = (*it)->_level;
		}
		else if ((*it)->_level == maxLevel)
		{
			candidates.push_back(*it);
		}
	}
	int chosen = (rand() % (int)(candidates.size()));
	return candidates[chosen];
}

/* Checks if the Block with the id of the given int exists in the BlockChain. */
bool blockNumExists(int block_num)
{
	vector<Block*>::iterator it;
	for (it = newChain->_blocks.begin(); it != newChain->_blocks.end(); it++)
	{
		if ((*it)->_block_num == block_num)
		{
			// Block exists in the BlockChain.
			return true;
		}
	}
	// Block dosen't exists in the BlockChain.
	return false;
}

/* calculate the nonce and the hash of the giving Block, using libhash.a. */
char* getHash(vector<Block*>::iterator it)
{
	(*it)->_nonce = generate_nonce((*it)->_block_num, (*it)->_father->_block_num);
	(*it)->_hash_sig = generate_hash((*it)->_data, (*it)->_length, (*it)->_nonce);
	return (*it)->_hash_sig;
}


/* Finds a new father to the given block in the BlockChain. */
void getNewFather(vector<Block*>::iterator it)
{
	(*it)->_father = getRandomLowestChild();
	(*it)->_level = (*it)->_father->_level + 1;
}

/* Checks if the Block with the id of the given int exists in the pendingList vector. */
vector<Block*>::iterator InPendingList(int block_num)
{
	vector<Block*>::iterator it;
	int i = 0;
	for (it = pendingList.begin(); it != pendingList.end(); it++)
	{
		if ((*it)->_block_num == block_num)
		{
			return it;
		}
		i++;
	}
	return it;
}

/* Adds the given Block to the BlockChain. Returns it's hash.*/
char* finalAdd(vector<Block*>::iterator it) {
	char* hash = getHash(it);
	newChain->_blocks.push_back((*it));
	(*it)->_father->_isLeaf = false;
	pendingList.erase(it);
	numBlocks++;
	return hash;
}

/* Adds the given Block to the BlockChain, from pendingList. */
void addToChain(vector<Block*>::iterator it)
{
	if (InPendingList((*it)->_block_num) != pendingList.end()) {
		if (!blockNumExists((*it)->_father->_block_num)) {
			getNewFather(it);
		}
		finalAdd(it);
	}
}

/* Checks if needs to be blocked and wait by cv. if so, the thread is being blocked. */
void cond_wait() {
	pthread_cond_wait (&cv, &blockMutex);
}

/* This method used as a daemon thread. It add to Block from the
pendingList vector to the BlockChain. exits in case of error. */
void *daemonAdd(void*)
{
	try {
		if (isInitialized && !inClose)
		{
				while (pendingList.size() > 0 && !inClose)
				{
						pthread_mutex_lock(&pendingMutex); // getting a lock for pendingList.
						pthread_mutex_lock(&blockMutex); // getting a lock for BlockChain.
						if (inAttachNow)
						{
							cond_wait();
						}
						addToChain(pendingList.begin());
						pthread_mutex_unlock(&blockMutex); // unlock for pendingList.
						pthread_mutex_unlock(&pendingMutex); // unlock for BlockChain.
				}
		}
	} catch(int error) {
		daemonInitialized = INIT_BOOL;
		exit(1);
	}
	daemonInitialized = INIT_BOOL;
	return NULL;
}

/*
* The function initiates the blockchain, and creates a root block for additional data storage.
* This function should be called prior to any other functions as a necessary
* precondition for their success.
* RETURN VALUE: On success 0, otherwise -1.
*/
int init_blockchain()
{
	if (!isInitialized) {
		isInitialized = true;
		try{
			newChain = new BlockChain();
		} catch(bad_alloc& bad)
		{
			return FAIL;
		}
		init_hash_generator();
		next_block_num = 1;
		numBlocks = 0;

		inClose = INIT_BOOL;
		inAttachNow = INIT_BOOL;
		// Initiallize 2 mutexes - one in BlockChain and the other is on pendingList.
		if (pthread_mutex_init(&blockMutex, NULL) != 0 ||
			pthread_mutex_init(&pendingMutex, NULL) != 0)
		{
				return FAIL;
		}
		return SUCCESS;
	}
	return FAIL;
}

/* Findes the next number */
void getNextBlockNum() {
	int size = pendingList.size() + newChain->_blocks.size() + 1;
	int blockNums[size];

	for (int i = 0; i < size; ++i) {
		blockNums[i] = 0;
	}
	for (int i = 0; i < (int)pendingList.size(); i++)
	{
		if (pendingList[i]->_block_num <= size - 1)
		{
			blockNums[pendingList[i]->_block_num - 1] = 1;
		}
	}
	for (int i = 0; i < (int)newChain->_blocks.size(); i++)
	{
		if (newChain->_blocks[i]->_block_num <= (size - 1))
		{
			blockNums[newChain->_blocks[i]->_block_num - 1] = 1;
		}
	}
	int j = 0;
	while(j != size)
	{
		if(blockNums[j] == 0)
		{
			next_block_num = j + 1;
			return;
		}
		j++;
	}
	next_block_num = size;
}

/*
* The function adds the hashed data to the block chain, once this call returns the data may be freed.
* Since this is a non-blocking package your implementated method should return as soon as possible,
* Even before the block was added to the chain.
* Further more the father block should be determined before this function returns,
* The father block should be the current longest chain.
*
* RETURN VALUE: On success, the function returns the lowest available blocks num (> 0),
*               which is assigned to an individual piece of data.
*               On failure, -1 will be returned.
*/
int add_block(char * data , size_t length)
{
	if (!isInitialized || next_block_num == INT_MAX)
	{
		return FAIL;
	}
	try{
		getNextBlockNum();
		pthread_mutex_lock(&pendingMutex); // getting a lock for pendingList.
		Block* father = getRandomLowestChild(); // Gets a father for the new Block.
		pendingList.push_back(new Block(data, father, next_block_num, father->_level + 1,length, 0));
		pthread_mutex_unlock(&pendingMutex); // unlock for pendingList.
		if (!daemonInitialized){
			// Initiallize a deamon.
			pthread_join(daemon, NULL);
			if (pthread_create(&daemon, NULL, &daemonAdd, NULL) != 0) {
				return FAIL;
			}
			daemonInitialized = true;
		}
	}
	catch(bad_alloc& bad)
	{
		return FAIL;
	}
	return next_block_num;
}

/* Adds the given Block to the BlockChain. being called from to_longest. */
void addToLongest(vector<Block*>::iterator it)
{
	if (InPendingList((*it)->_block_num) != pendingList.end()) {
		if((!blockNumExists((*it)->_father->_block_num)) ||
			((*it)->_father->_level != getRandomLowestChild()->_level))
		{
			getNewFather(it);
		}
		finalAdd(it);
	}
}

/*
* Add this block_num to the attach-time longest chain,
* i.e. the real longest chain while attaching the bock.
* As opposed to the original add_block that adds the block to
* the longest chain during the time that add_block was called.
*
* RETURN VALUE: If block_num doesn't exist, return -2;
*               In case of other errors, return -1; In case of success return 0.
*
*/
int to_longest(int block_num)
{
	if (!isInitialized || !(block_num >= 0))
	{
		return FAIL;
	}
	vector<Block*>::iterator it = InPendingList(block_num);
	if (it == pendingList.end())
	{
		if (blockNumExists(block_num))
		{ // Block is already in the BlockChain.
			return ALREADYINCHAIN;
		}
		return NOTEXIST;
	}
	pthread_mutex_lock(&pendingMutex); // getting a lock for pendingList.
	pthread_mutex_lock(&blockMutex); // getting a lock for blockMutex.
	if (inAttachNow)
	{
		cond_wait();
	}
	addToLongest(it);
	pthread_mutex_unlock(&blockMutex); // unlock for blockMutex.
	pthread_mutex_unlock(&pendingMutex); // unlock for pendingList.
	return SUCCESS;
}

/*
* Block all data storage until block_num is added to chain.
* The block_num is the assigned value that was previously returned by add_block.
*
* RETURN VALUE: If block_num doesn't exist, return -2;
*               In case of other errors, return -1; In case of success return 0.
*
*/
int attach_now(int block_num)
{
	if (!isInitialized || !(block_num >= 0))
	{
		return FAIL;
	}
	if (pthread_cond_init(&cv, NULL) != 0) {
		// Blocks all attach threads.
		return FAIL;
	}
	inAttachNow = true;
	vector<Block*>::iterator it = InPendingList(block_num);
	if (it == pendingList.end())
	{
		if (blockNumExists(block_num))
		{
			// Block is already in the BlockChain.
			pthread_cond_broadcast(&cv);
			return 1;
		}
		pthread_cond_broadcast(&cv);
		return NOTEXIST;
	}
	addToChain(it);
	pthread_cond_broadcast(&cv); // Unblocks all attach threads.
	inAttachNow = INIT_BOOL;
	pthread_cond_destroy(&cv);
	return SUCCESS;
}

/*
* Without blocking check whether block_num was added to the chain.
*     	The block_num is the assigned value that was previously returned by add_block.
*
* RETURN VALUE: 0 if true and 1 if false.
* 		If the block_num doesn't exist, return -2; In case of other errors, return -1.
*
*/
int was_added(int block_num)
{
	if (!isInitialized || !(block_num >= 0))
	{
		return FAIL;
	}
	vector<Block*>::iterator it = InPendingList(block_num);
	if (blockNumExists(block_num))
	{
		return TRUE;
	}
	else if (it == pendingList.end())
	{
			return NOTEXIST;
	}
	return FALSE;
}

/*
* Without blocking return how many blocks were attached to the chain since last init_blockchain.
* In case of error, return -1.
*
*/
int chain_size()
{
	return numBlocks;
}

/* returns a leaf in the BlockChain. if the BlockChain has only one longest chain,
it will not return it's leaf. used by prune_chain. */
vector<Block*>::iterator getLeaf()
{
	vector<Block*>::iterator it;
	Block* lowest = getRandomLowestChild();
	for (it = newChain->_blocks.begin(); it != newChain->_blocks.end(); it++)
	{
		if(((*it)->_isLeaf) && ((*it)->_block_num != lowest->_block_num))
		{
			return it;
		}
	}
	// Case for no leafs but the one in the longest chain.
	return newChain->_blocks.begin();
}

/* Checks if the given Block has one son. if not return false. */
bool HasOnlyOneSon(int num_block)
{
	int count = 0;
	vector<Block*>::iterator it;
	for (it = newChain->_blocks.begin(); it != newChain->_blocks.end(); it++)
	{
		if((*it)->_block_num != newChain->_genesis->_block_num &&
			(*it)->_father->_block_num == num_block)
		{
			count++;
		}
	}
	return count == 1;
}

/*
* Search throughout the tree for sub-chains that are not the longest chain,
* detach them from the tree, free the blocks, and reuse the bloc_nums.
*
*/
int prune_chain()
{
	pthread_mutex_lock(&blockMutex);  // getting a lock for blockMutex.
	if (!isInitialized)
	{
		return FAIL;
	}
	vector<Block*>::iterator it = getLeaf();
	while(((*it)->_block_num) != (newChain->_genesis->_block_num))
	{
		if ((*it)->_father->_block_num != newChain->_genesis->_block_num)
		{
			if(HasOnlyOneSon((*it)->_father->_block_num))
			{
				(*it)->_father->_isLeaf = true;
			}
		}
		newChain->_blocks.erase(it);
		it = getLeaf(); // Finds a new lead to cut from the BlockChain.
	}
	getNextBlockNum();
	pthread_mutex_unlock(&blockMutex); // unlock for blockMutex.
	return SUCCESS;
}

/* Add all the Blocks in the pendingList vector to the BlockChain and prints it's hash. */
void hashAdd() {
	vector<Block*>::iterator it;
	for (it = pendingList.begin(); it != pendingList.end(); it++)
	{
		if (blockNumExists((*it)->_father->_block_num))
		{
			getNewFather(it);
		}
		cout << getHash(it) << endl;
		newChain->_blocks.push_back(*it);
		(*it)->_father->_isLeaf = false;
		numBlocks++;
	}
}

/* This method is used as a thread and close the BlockChain. used by close_chain.*/
void *closeBlockChain(void*)
{
	pthread_mutex_lock(&pendingMutex); // getting a lock for pendingMutex.
	pthread_mutex_lock(&blockMutex); // getting a lock for blockMutex.
	hashAdd();
	pendingList.clear();
	numBlocks = 0;
	close_hash_generator();
	delete newChain;
	daemonInitialized = INIT_BOOL;
	pthread_join(daemon, NULL);
	pthread_mutex_unlock(&blockMutex); // unlock for blockMutex.
	pthread_mutex_unlock(&pendingMutex); // unlock for pendingMutex.
	pthread_mutex_destroy(&blockMutex);
	pthread_mutex_destroy(&pendingMutex);
	inClose = INIT_BOOL;
	isInitialized = INIT_BOOL;
	return NULL;
}

/*
* Close the recent blockchain and reset the system, so that it is possible to call init_blockchain
* again. All pending block_num should be added to the blockchain.
* Any attempt to attach new block or to initialize the system while it is
* being shut should cause an error.
* In case of error, the function should cause the process to exit.
*
*/
void close_chain()
{
	if (isInitialized)
	{
		// Initiallize a thread for closing the BlockChain.
		if (pthread_create(&onClose, NULL, &closeBlockChain, NULL) != 0) {
			return;
		}
		inClose = true;
	}
}

/*
* The function waits for close_chain to finish.
* If closing was successful, it returns 1.
* If end_chain was not called it should return -2.
* In case of other error, it should return -1.
*
*/
int return_on_close()
{
	if (isInitialized)
	{
		try {
			if(inClose)
			{
				pthread_join(onClose, NULL);
			}
			else
			{
				return NOTEXIST;
			}
		}
		catch(int e) {
			return FAIL;
		}
	}
	return SUCCESS;
}
