/*
 * Log.cpp
 *
 *  Created on: May 25, 2015
 *  Author: Lauren Cohen and Tomer Stark
 */

#include <iostream>
#include <fstream>
#include <string.h>
#include "Log.h"

using namespace std;

#define LOG_FILE_NAME "/.filesystem.log"

// --------------------------------------------------------------------------------------
// This file contains the implementation of the class Log.
// --------------------------------------------------------------------------------------

// ------------------ Initiallizing method ------------------------
void initLog(char* root) {
	string log(LOG_FILE_NAME);
	string address = root + log;
	out.open(address.c_str(), ios::app); // opens the file.
}

// ------------------ Modify methods ------------------------
void logWrite(long time, string funcName) {
	out << time << " " << funcName << endl;
}

void logIoctl(char* addr, int section, int used) {
	char* temp = ++addr;
	out << temp << " " << section << " " << used << endl;
}

// ------------------ Closing method ------------------------
void logClose() {
	out.close();
}
