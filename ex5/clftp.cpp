/*
 * clftp.cpp
 *
 *  Created on: Jun 7, 2015
 *      Author: Lauren Cohen and Tomer Stark.
 */

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <errno.h>
#include <netdb.h>

#define FAIL -1
#define SIZE_OF_POINTER 8
#define PAGE_SIZE 1024
#define CLIENT_USAGE "Usage: clftp server-port server-hostname file-to-transfer filename-in-server"
#define FILE_TOO_BIG "Transmission failed: too big file"
#define ERROR "Error: function:"
#define ERRNO " errno:"
#define WRITE "write"
#define OPEN "open"
#define READ "read"
#define STAT "stat"
#define WRITE "write"
#define CONNECT "connect"
#define GET_HOST_BY_NAME "gethostbyname"
#define SOCKET "socket"

using namespace std;

/*
 * return true if the given string can be represented as number. otherwise, return false.
 */
bool isNumber(const string & s) {
	bool hitDecimal = 0;
	for(int i = 0; i < (int) s.length(); i++) {
		if(s[i] == '.' && !hitDecimal) {
			// 2 '.' in string mean invalid
			hitDecimal = 1;
			// first hit here, we forgive and skip
		}
		else if(!isdigit(s[i])) {
			return 0 ; // not ., not
		}
	}
	return 1;
}

/*
 * return the size of the file with the given name file. in case of an error return -1.
 */
int GetFileSize(char* fileName) {
	struct stat stat_buf;
	int rc = stat(fileName, &stat_buf);
	return rc == 0 ? stat_buf.st_size : FAIL;
}

/*
 * return true if file with the given name exists. otherwise, return false.
 */
bool FileExists(char* path) {
	struct stat fileStat;
	if (stat(path, &fileStat) == FAIL) {
		return false;
	}
	if (!S_ISREG(fileStat.st_mode)) {
		return false;
	}
	return true;
}

/*
 * This method gets a host name and a port and create a socket to connect to. return this socket.
 */
int call_socket(char* hostname, unsigned short portnum) {
	struct sockaddr_in sa;
	struct hostent *hp;
	int s;
	if ((hp = gethostbyname(hostname)) == NULL) {
		cerr << ERROR << GET_HOST_BY_NAME << ERRNO << errno << endl;
		exit(1);
	}
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = hp->h_addrtype;
	memcpy((char *)&sa.sin_addr,hp->h_addr,hp->h_length);
	sa.sin_port = htons((u_short)portnum);
	if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		// Case for socket failed.
		cerr << ERROR << SOCKET << ERRNO << errno << endl;
		exit(1);
	}
	if (connect(s, (struct sockaddr*)&sa,sizeof(sa)) < 0)
	{
		// Case for connect failed.
		close(s);
		cerr << ERROR << CONNECT << ERRNO << errno << endl;
		exit(1);
	}
	return s;
}

/*
 * send a part of the file with the given file name to the server which start and ending of the file is given and
 * the file is sent using the given socket. returns the size of bytes writen by the method.
 */
int sendFileChunck (int socket, char* fileName, int start, int end) {
	vector<unsigned char> vec;
	struct stat filestat = {0};
	if (stat(fileName, &filestat) != 0) {
		cerr << ERROR << STAT << ERRNO << errno << endl;
		exit(1);
	}
	ifstream ifs(fileName, ios::binary | ios::in);
	if (!ifs) {
		// error opening the file.
		cerr << ERROR << OPEN << ERRNO << errno << endl;
		exit(1);
	}
	vec.resize(end - start);
	int size = vec.size();
	ifs.seekg (streamoff(start)); // moves the pointer to the start char.
	if (!ifs.read((char*)&vec[0], end - start)) {
		cerr << ERROR << READ << ERRNO << errno << endl;
		exit(1);
	}
	ifs.close(); // close the file.
	char* buffer = new char[size];
	// copy the vector to the buffer.
	for (int i = 0; i < size; ++i)
	{
		buffer[i] = vec[i];
	}
	//sends the chunck to the server.
	if(write(socket, buffer, size) == FAIL){
		cerr << ERROR << WRITE << ERRNO << errno << endl;
		exit(1);
	}
	delete[] buffer;
	return size;
}

/*
 * This method sends to the server the file with the given file name with the given
 * size through the given socket.
 */
void sendFile(int socket, char * fileName, int size) {
	// writes the file size to the server.
	if (write(socket, &size, sizeof(int)) == FAIL){
		cerr << ERROR << WRITE << ERRNO << errno << endl;
		exit(1);
	}
	int start = 0; //pointer to the start of the first part of the file.
	int end = PAGE_SIZE; //pointer to the end of the first part of the file.
	while (size > 0) {
		if (size > PAGE_SIZE) {
			// There is at least a PAGE_SIZE to send to the server.
			size -= sendFileChunck(socket, fileName, start, end);
		}
		else {
			// There is ess than PAGE_SIZE to send to the server.
			size -= sendFileChunck(socket, fileName, start, start + size);
		}
		start += PAGE_SIZE;
		end += PAGE_SIZE;
	}
}

/*
 * The main gets a port name, host name a file name and a new file name, and sends
 * the file to the given server (with the given host an port) and tells him the name
 * of the file he should saves the file the client sent.
 * return 0 on success.
 */
int main(int argc, char **argv) {
	if(argc != 5 || !isNumber(argv[1])) {
		// bad number of arguments.
		cout << CLIENT_USAGE  << endl;
		exit(1);
	}
	int port = atoi(argv[1]);
	char* host = argv[2];
	if(1 >= port || 65535 <= port) {
		cout << CLIENT_USAGE << endl;
		exit(1);
	}
	if(!FileExists(argv[3])) {
		//file not found.
		cout << CLIENT_USAGE << endl;
		exit(1);
	}
	int socket = call_socket(host, port);
	int maxFileSize;
	int fileSize = GetFileSize(argv[3]);
	read(socket, &maxFileSize, sizeof(int));
	if ((int)maxFileSize < fileSize) {
		cout << FILE_TOO_BIG << endl;
		return 0;
	}
	int fileNameSize = strlen(argv[4]);
	if (fileNameSize > PATH_MAX) {
		cout << CLIENT_USAGE << endl;
		//error
		exit(1);
	}
	// sends the size of the new file name to the server.
	if (write(socket, &fileNameSize, sizeof(int)) == FAIL) {
		cerr << ERROR << WRITE << ERRNO << errno << endl;
		exit(1);
	}
	// sends the new file's name to the server.
	if (write(socket, argv[4], fileNameSize) == FAIL) {
		cerr << ERROR << WRITE << ERRNO << errno << endl;
		exit(1);
	}
	// sends file size  to the server (divide it to blocks / packets if needed)
	sendFile(socket, argv[3], fileSize);
	return 0;
}
