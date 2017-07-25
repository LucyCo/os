/*
 *	CachingBlock.h
 *	This file is the header for the CachingBlock class. The CachingBlock class is a DB for
 *	all the block the user had read from the file system. the DB has a size which being determened
 *	in initiallizing and the Block that being saved is decided by the LRU cache algorithms.
 *
 *	Created on: May 27, 2015
 *	Author: Lauren Cohen and Tomer Stark
 */

#ifndef CACHINGBLOCK_H_
#define CACHINGBLOCK_H_

/**
 *	This class representing a single block of the cache. Every Block containing a data with
 *	a given size at most, the number the user has read it, the path of the File and the section
 *	of the data Block in the file.
 */
class Block {
public:
	char* _path; /* The path of the Block's  File. */
	char* _data; /* The data of the block. */
	int _sectionInBlock; /* The number of section block in the file. */
	int _numOfUses; /* The number of times the user read the block. */

	/**
	 *	Default constructor of the Block.
	 */
	Block();

	/**
	 *	constructor of the Block.
	 *	@param path The path of the Block's File.
	 *	@param data The data of the block.
	 *	@param section The number of section block in the file.
	 */
	Block(char* path, char* data, int section);

	/**
	 *	destructor of the Block.
	 */
	~Block();
};

/**
 *	This class representing a DB of cache Blocks. The DB is represented by an array of Blocks.
 */
class CachingBlock {
public:

	Block** _cache; /* An array of Block that reoresenting the cache. */
	int _numberOfBlocks; /* The size of the array. */
	int _blockSize; /* The size of each Block in the array. */
	int capacity; /* The number of Blocks in the array. */

	/**
	 *	constructor of the Block.
	 *	@param numberOfBlocks The size of the array.
 	 *	@param blockSize The size of each Block in the array.
 	 */
	CachingBlock(int numberOfBlocks, int blockSize);

	/**
	 *	destructor of the Block.
	 */
	~CachingBlock();

	/**
	*	Checks if a Block with the given path and section number in the file is exists in the
	*	cache array.
	*	@param path The path of the Block's File.
	*	@param sectionInBlock The number of section block in the file.
	*	@return If the Block exists, the method return it's index in the array.
	*	otherwise it returns -1.
	*/
	int isExists(const char* path, int sectionInBlock);

	/**
	*	The method gets a Block parameters and checks if it can be inserted to the Block Cache.
	*	it insert it to the array, and if the array is full, the method uses the LRU algorithem
	*	to determine which Block should be removed.
	*	@param path The path of the Block's File.
	*	@param sectionInBlock The number of section block in the file.
	*	@param data The data of the block.
	*/
	void add(char* path, int sectionInBlock, char* data);

	/**
	*	Increases the number of uses to the Block in the given index in the cache array.
	*	@param index The index of the Block in the Cache array.
	*	@return The length of the data of the Block.
	*/
	int increase(int index);

	/*
	 *	Renames all the Blocks in the array with the given path.
	 *	@param path The path of the Block's File.
	 *	@param newpath The new path of the Block's File.
	 */
	void rename(const char* path, const char* newpath);

private:

	/*
	* Returns the Block in the cache array with the minimum number of uses.
	*/
	int getLeastUsed();

	/**
	*	construct a new Block and adds it to the array in the given index.
	*	@param index The index to insert the new Block.
	*	@param path The path of the Block's File.
	*	@param sectionInBlock The number of section block in the file.
	*	@param data The data of the block.
	*/
	void addBlock(int index, char* path, int sectionInBlock, char* data);

};


#endif /* CACHINGBLOCK_H_ */
