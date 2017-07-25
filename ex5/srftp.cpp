/*
 * srftp.cpp
 *
 *  Created on: Jun 7, 2015
 *      Author: Lauren Cohen and Tomer Stark
 */

#include <limits.h>
#include <pthread.h>
#include <errno.h>
#include <netdb.h>
#include <fstream>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define FAIL -1
#define END_OF_FILE '\0'
#define PENDING_CON 5
#define PAGE_SIZE 1024
#define SERVER_USAGE "Usage: srftp server-port max-file-size"
#define ERROR "Error: function:"
#define ERRNO " errno:"
#define WRITE "write"
#define OPEN "open"
#define READ "read"
#define GET_HOST_NAME "gethostname"
#define GET_HOST_BY_NAME "gethostbyname"
#define SOCKET "socket"
#define WRITE "write"
#define BIND "bind"
#define LISTEN "listen"
#define ACCEPT "accept"
#define PTHREAD_CREATE "pthread_create"
using namespace std;

pthread_t client;

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
			return 0 ; // not
		}
	}
	return 1;
}

/*
 * establish a new connection to the server using the given port number.
 * return the new socket to this port.
 */
int establish(unsigned short portnum)
{
	char myname[HOST_NAME_MAX + 1];
	int s;
	struct sockaddr_in sa;
	struct hostent * hp;
	memset(&sa, 0, sizeof(struct sockaddr_in));
	if (gethostname(myname, HOST_NAME_MAX) == FAIL) { //failed to return host name.
		cerr << ERROR << GET_HOST_NAME << ERRNO << errno << endl;
		exit(1);
	}
	hp = gethostbyname(myname);
	if (hp == NULL) { //failed to return address to hostent.
		cerr << ERROR << GET_HOST_BY_NAME << ERRNO << errno << endl;
		exit(1);
	}
	sa.sin_family = hp->h_addrtype;
	memcpy(&sa.sin_addr,hp->h_addr,hp->h_length); // this is our host address .
	sa.sin_port = htons(portnum); // this is ourport number.
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) // create socket.
	{
		cerr << ERROR << SOCKET << ERRNO << errno << endl;
		exit(1);
	}
	// assigns the address to the socket.
	if (bind(s , (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) < 0) {
		close(s);
		cerr << ERROR << BIND << ERRNO << errno << endl;
		exit(1);
	}
	// max number of queued connects.
	if (listen(s, PENDING_CON) < 0) {
		cerr << ERROR << LISTEN << ERRNO << errno << endl;
		exit(1);
	}
	return(s);
}

/*
 * receive a part of a file from the client.
 * returns this part of a file.
 */
char* getChunck(int socket, int length) {
	int bcount = 0; // The number of bytes we read.
	int br = 0; // The number of bytes we read on a single loop.
	char* buf = new char[length+1];
	while (bcount < length) { /* loop until full buffer */
		// Reads the massege from the client
		br = read(socket, buf, length - bcount);
		if (br > 0){
			bcount += br;
			buf += br;
		}
		else if (br == FAIL) {
			// Error at reading.
			cerr << ERROR << READ << ERRNO << errno << endl;
			pthread_exit(NULL);
		}
	}
	buf -= bcount;
	buf[length] = END_OF_FILE;
	return buf;
}

/*
 * receive a file from the client and a file name, and saves the file in the name of the file name.
 * uses the given socket in oreder to read all the messages from the client.
 */
void* getFile(void* args) {
	int socket = *((int*)args);
	int fileNameSize = 0; // the length of the new file name.
	if (read(socket, &fileNameSize, sizeof(int)) == FAIL) {
		// Error at reading.
		cerr << ERROR << READ << ERRNO << errno << endl;
		pthread_exit(NULL);
	}
	char * fileName = getChunck(socket, fileNameSize); // gets the name of the file to save in the data.
	ofstream fileo(fileName,ios::binary); // opens the output file.
	int size = 0;
	// reads the size of the file to save.
	if (read(socket, &size, sizeof(int)) == FAIL) {
		cerr << ERROR << READ << ERRNO << errno << endl;
		pthread_exit(NULL);
	}
	while (size > 0) {
		int length = size > PAGE_SIZE ? PAGE_SIZE : size;
		char* text = getChunck(socket, length); // reads the next part o the file.
		fileo.write(text, length); // saves the next part of the file.
		size -= PAGE_SIZE;
		delete[] text;
	}
	delete[] fileName;
	fileo.close();
	pthread_exit(NULL);
}

/*
 * This method wait till a client wants to connect. when it happens, it create a thread to it
 * which responsable for the transfering of data between this client and the server.
 */
int get_connection(int s, int maxSize) {
	int socket; /*socket of connection*/
	while(true) {
		if((socket = accept(s, NULL, NULL)) < 0) {
			cerr << ERROR << ACCEPT << ERRNO << errno << endl;
			pthread_exit(NULL);
		}
		// A connection from a client has been made.
		if (write(socket, &maxSize, sizeof(int)) == FAIL) {
			cerr << ERROR << WRITE << ERRNO << errno << endl;
			pthread_exit(NULL);
		}
		// create a thread to this client with the specific socket.
		int res =  pthread_create(&client, NULL, getFile, (void *)&socket);
		if (res != 0) {
			cerr << ERROR << PTHREAD_CREATE << ERRNO << errno << endl;
			pthread_exit(NULL);
		}
	}
	return socket;
}

/*
 * create a connection with the given port and waits for client to connect him and transfer data.
 * a client will not transfer a data bigger than the given size.
 * return 0 on success.
 */
int main(int argc, char *argv[]) {
	if(argc != 3) {
		// bad number of arguments.
		cout << SERVER_USAGE << endl;
		exit(1);
	}
	if(!isNumber(argv[1]) || !isNumber(argv[2]) ) {
		// port number or max file size are not numbers.
		cout << SERVER_USAGE << endl;
		exit(1);
	}
	int port = atoi(argv[1]);
	int size = atoi(argv[2]);
	if(1 >= port || 65535 <= port) {
		cout << SERVER_USAGE << endl;
		exit(1);
	}
	if(size < 0) {
		cout << SERVER_USAGE << endl;
		exit(1);
	}
	int socket = establish(port); // establish the connection to the port.
	get_connection(socket, size);
	return 0;
}
