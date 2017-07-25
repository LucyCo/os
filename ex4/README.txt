EX: 4

FILES:
README - this file.
CashingFileSystem.cpp - includes a main method to check the given arguments and transfer
control to fuse main while using override functions from the fuse API.
CashingBlock.h - header for the cashing block and block classes.
CashingBlock.cpp - implementation of the Block and CashingBlock classes. these classes
represent the heap cache for the fuse we implemented.
Log.h - header for the log writer.
Log.cpp - implementation for the log writer. opens a hidden unreachable log file and
documents the CashingFileSystem function usage during runtime.
makefile - make file for compilation of this project.

REMARKS:

ANSWERS:
1. Since the cache is saved in the RAM, each reach for the cache can be much faster than
reaching for the disk if the block we looked for is entirely saved in the cache (if not,
we’ll have to get the data from the disk). Also, the cache updating system has to be
mentioned properly, there are some cases that can cause going back and forth, never saving
the data in a smart way that will help minimise the average time retrieving each call.

2. The answer is in the given explanation: The array is easy to maintain (but has to be
given the cache size upfront) when pushing each block, but in order to remove a certain
block according to LFU we need to go through the entire array which will cost O(n).
The list LFU is harder to maintain as we need to update the order with each use but
removing a block is O(1).

3. In a hardware cache the search must be performed in hardware lead to more conflicts and
reduced utilization. (because costs dictate a sharp reduction in associativity).Therefore,
using more sophisticated algorithms like LRU which will be much harder to manage. But the
buffer cache is maintained by the operating system, so this is not needed and using LRU is
manageable and results in more efficient performance.

4. LRU is better than LFU: a file that was recently imported in a block and the next few
readings are mostly from this file, but another block has to be imported and there’s
another block that was used many times before. This new file will be thrown from cache and
re-imported when the file was was used many times before is will not be used again for a
while.
LFU is better than LRU: A file was used many times before, but one large read fills the
block and it had to be thrown even though it is likely to be used again after this recent
large read.
LRU and LFU are not effective: when the entire program is linear and don’t reuse blocks.

5. The ideal block size would be 4096 since the file system reads this amount from a given
file. different block size will result in incomplete blocks or files being cut to few
blocks and losing efficiency.