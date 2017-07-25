EX: 3


FILES:
	README      - this file.

	blockchain.cpp - of an asynchronous simplified block chain management library, that will
	store hashed pieces of data in a chainlike structure.
	Makefile    - make file for compilation of this project.


REMARKS:

we implamented the libary as a class of block chain, and made a class for the blocks in the chain. every block has
all the needed data (data, length, id and so...) and it also had a pointer to it's father in the chain.
also, we made a vector to the blocks that will be added later, and a daemon thread that it's job is to add those
Blocks to the chain. moreover, we used mutexes for every time a methid wanted to change one of our data
structures (the chain, and the vector of pending blocks). and for each data structure we made a mutex of it's own.
in the attach now, we used a cv to block all other attaching methods
at the close method, we made a thread that will work in the background (same like the daemon) and
will finish the closing there.

ANSWERS:

Answer to question 1:
The method add_block adds is the method that enable multy-pointing, by using the daemon thread. a main parameter
for the number of sons to every father is the hash function. by changing it,
the number of sons to each block we change as well.


Answer to question 2:
A way to get rid of the job of prune is to do that every time we want to add a Block from the pending list to the
chain, we change it's father to be the last block in the chain. 

Answer to question 3:
In our libary, we reallized that we have to make sure that the pending list of Block also have to be locked,
because several threads are trying to change it every time (mostly adding and closing).
we used a mutex to make sure that the critical section of this vector will not be damaged.