/*
 * CachingBlock.cpp
 *
 *  Created on: 19 May 2015
 *	Author: Lauren Cohen and Tomer Stark
 */

#include <string.h>
#include <climits>
#include <math.h>
#include <iostream>
#include "CachingBlock.h"

using namespace std;

// --------------------------------------------------------------------------------------
// This file contains the implementation of the class CachingBlock.
// --------------------------------------------------------------------------------------

// ------------------ The Block class ------------------------


// ------------------ Constructors ------------------------
Block::Block() {
	_path = NULL;
	_data = NULL;
	_sectionInBlock = 0;
	_numOfUses = 1;
}

Block::Block(char* path, char* data, int section = 0) {
	_path = new char [strlen(path)];
	strcpy(_path, path);
	_data = new char [strlen(data)];
	strcpy(_data, data);
	_sectionInBlock = section;
	_numOfUses = 1;
}

// ------------------ destructor ------------------------
Block::~Block() {
	delete[] _path;
	delete[] _data;
}

// ------------------ The CachingBlock class ------------------------

// ------------------ Constructor ------------------------
CachingBlock::CachingBlock(int numberOfBlocks, int blockSize) {
	capacity = 0;
	_cache = new Block*[numberOfBlocks]();
	for (int i = 0; i < numberOfBlocks; ++i) {
		_cache[i] = new Block();
	}
	_numberOfBlocks = numberOfBlocks;
	_blockSize = blockSize;
}

// ------------------ destructor ------------------------
CachingBlock::~CachingBlock() {
	for (int i = 0; i < _numberOfBlocks; ++i) {
		delete _cache[i];
	}
	delete[] _cache;
}

// ------------------ Access methods ------------------------
int CachingBlock::isExists(const char* path, int sectionInBlock) {
	for (int i = 0; i < _numberOfBlocks; i++) {
		if (path != NULL && _cache[i]->_path != NULL &&
			memcmp(path, _cache[i]->_path, strlen(_cache[i]->_path) * sizeof(char)) == 0 &&
			sectionInBlock == _cache[i]->_sectionInBlock) {
			// Case The Block exists in the cache array.
			return i;
		}
	}
	// Case The Block dosen't exists in the cache array.
	return -1;
}

int CachingBlock::getLeastUsed() {
	int min = INT_MAX;
	int index = -1;
	for (int i = 0; i < _numberOfBlocks; i++) {
		if (_cache[i]->_numOfUses < min) {
			min = _cache[i]->_numOfUses;
			index = i;
		}
	}
	return index;
}

// ------------------ Modify method ------------------------
void CachingBlock::add(char* path, int sectionInBlock, char* data) {
	if (capacity < _numberOfBlocks) {
		// Automatic insertion to the cache array.
		addBlock(capacity, path, sectionInBlock, data);
		capacity++;
	}
	else {
		// insertion using the LRU algorithem.
		int index = getLeastUsed();
		addBlock(index, path, sectionInBlock, data);
	}
}

void CachingBlock::addBlock(int index, char* path, int sectionInBlock, char* data) {
	Block* blk = new Block(path, data, sectionInBlock);
	delete _cache[index];
	_cache[index] = blk;
}

int CachingBlock::increase(int index) {
	_cache[index]->_numOfUses = _cache[index]->_numOfUses + 1;
	return strlen(_cache[index]->_data);
}

void CachingBlock::rename(const char* path, const char* newPath) {
	for (int i = 0; i < _numberOfBlocks; i++) {
		if (_cache[i]->_path != NULL &&
			memcmp(path, _cache[i]->_path, strlen(_cache[i]->_path) * sizeof(char)) == 0) {
			// rename the i Block in the cache array.
			memcpy(&(_cache[i]->_path[0]), &newPath[0], strlen(newPath) * sizeof(char));
		}
		else {
			for (int j = 0; j < (int) strlen(_cache[i]->_path); j++) {
				if (_cache[i]->_path[j] == '/') {
					string strPath(path);
					string strBlockPath(_cache[i]->_path);
					if (strPath.compare(strBlockPath.substr(0, j)) == 0) {
						string strNewPath(newPath);
						strNewPath = strNewPath.substr(0, j + 1) + strBlockPath.substr(j, strBlockPath.length());
//						delete _cache[i]->_path;
//						_cache[i]->_path = new char [strNewPath.length()];
						memcpy(_cache[i]->_path, (strNewPath).c_str(), strNewPath.length() * sizeof(char));
					}
				}
			}
		}
	}
}




