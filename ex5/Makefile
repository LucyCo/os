all: srftp clftp

srftp: srftp.o
	g++ -Wall -lpthread srftp.o -o srftp
	
srftp.o: srftp.cpp
	g++ -Wall -c srftp.cpp -o srftp.o

clftp: clftp.o
	g++ -Wall clftp.o -o clftp
	
clftp.o: clftp.cpp
	g++ -Wall -c clftp.cpp -o clftp.o

clean: 
	rm srftp*.o srftp clftp*.o clftp

tar:
	tar cvf ex5.tar srftp.cpp clftp.cpp Makefile README performance.jpg
