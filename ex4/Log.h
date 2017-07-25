/*
 *	Log.h
 *	This file is the header for the log class. The log class is responsible for the documenting
 *	in the log file "/.filesystem.log" in our FileSystem.
 *
 *  Created on: May 27, 2015
 *	Author: Lauren Cohen and Tomer Stark
 */

#ifndef LOG_H_
#define LOG_H_

#include <iostream>
#include <fstream>
#include <string.h>

std::ofstream out; /* Our output log file.*/

	/**
	 *	Initiallizing the log file in the root directory.
	 *	@param root The root of the FileSystem.
	 */
	void initLog(char* root);

	/**
	*	Writes the time and calling method in the log file.
	*	@param time The time th calling function entered to the method.
	*	@param funcName The Name of the calling method.
	*/
	void logWrite(long time, std::string funcName);

	/**
	*	Writes the given block from the cache to the log file in the FileSystem.
	*	@param addr The path of the file.
	*	@param section The Block section in the file.
	*	@param used The number of times the user read the block.
	*/
	void logIoctl(char* addr, int section, int used);

	/**
	*	closing the log file in the root directory.
	*/
	void logClose();

#endif /* LOG_H_ */
