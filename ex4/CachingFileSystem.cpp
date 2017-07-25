/*
 * CachingFileSystem.cpp
 *
 *  Created on: 15 April 2015
 *  Author: Netanel Zakay, HUJI, 67808  (Operating Systems 2014-2015).
 */

#define FUSE_USE_VERSION 26

#include "CachingBlock.cpp"
#include "Log.cpp"
#include <iostream>
#include <limits.h>
#include <time.h>
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAIN_ERROR "usage: CachingFileSystem rootdir mountdir numberOfBlocks blockSize"
#define ALLOC_ERROR "Allocation Error!"
#define LOG_FILE_NAME_LENGTH 16
#define LOG_FILE_NAME "/.filesystem.log"

using namespace std;

#define private_data ((state *) fuse_get_context()->private_data)

typedef struct state {
	char *rootdir; /* The Root path of the File System. */
	CachingBlock* cb; /* Our cache DB. */

}state;

struct fuse_operations caching_oper;

/* Returns the current time using time.h. */
long getTime() {
	time_t timev;
	return time(&timev);
}

/* Checks if the given number is negative or not. */
int checkRetstat(int retstat) {
	return (retstat < 0) ? -errno : retstat;
}

/* gets a path and return it's full address. */
string getFullPath(const char* path) {
	string root(private_data->rootdir);
	string strPath(path);
	return root + strPath;
}

/*	Checks if the given path input is legal.  returns the error errno that fits if the path is 
 *	illigel. otherwise, it returns 0.
 */
int checkPath(const char* path) {
	if (path == NULL || memcmp(path, LOG_FILE_NAME, LOG_FILE_NAME_LENGTH * sizeof(char)) == 0) {
		// Case the path is NULL of it is the path of the Log file.
		return -ENOENT;
	}
	if (strlen(getFullPath(path).c_str()) > PATH_MAX) {
		return -ENAMETOOLONG;
	}
	return 0;
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int caching_getattr(const char *path, struct stat *statbuf){
	logWrite(getTime(), "getattr");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	string fp = getFullPath(path).c_str();
	int res = lstat(fp.c_str(), statbuf);
	return checkRetstat(res);
}

/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int caching_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi) {
	logWrite(getTime(), "fgetattr");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	int retstat = 0;
	if (!strcmp(path, "/")) {
		return caching_getattr(path, statbuf);
	}
	retstat = fstat(fi->fh, statbuf);
	if (retstat < 0) {
		// error in fstat.
		retstat = -errno;
	}
	return retstat;
}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int caching_access(const char *path, int mask) {
	logWrite(getTime(), "access");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	int retstat = access(getFullPath(path).c_str(), mask);
	return checkRetstat(retstat);
}


/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.

 * pay attention that the max allowed path is PATH_MAX (in limits.h).
 * if the path is longer, return error.

 * Changed in version 2.2
 */
int caching_open(const char *path, struct fuse_file_info *fi){
	logWrite(getTime(), "open");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	if ((fi->flags & 3) != O_RDONLY) {
		// the given command is not for read only - cannot write!
		return -EACCES;
	}
	int retstat = 0;
	int fd = open(getFullPath(path).c_str(), fi->flags);
	fi->fh = fd;
	if (fd < 0) {
		// error in the openning of the file.
		retstat = -errno;
	}
	return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.
 *
 * Changed in version 2.2
 */
int caching_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi){
	logWrite(getTime(), "read");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	char temp_path[strlen(path)];
	strcpy(temp_path, path);
	// Calculation for how much to read from the file.
	int off = offset % private_data->cb->_blockSize > 0 ? 1:0;
	int remain = size % private_data->cb->_blockSize > 0 ? 1:0;
	int num = (size / private_data->cb->_blockSize) + off + remain;
	int start = (offset / private_data->cb->_blockSize) + 1;
	int sum = 0;
	// Creating a temporary buffer for each Block reading.
	char* temp = (char*) calloc(private_data->cb->_blockSize, sizeof(char));
	for (int i = start; i < start + num; i++) {
		int index = private_data->cb->isExists(path, i);
		if (index >= 0) {
			// Case the Block is in the cache.
			sum += private_data->cb->increase(index);
			strcpy(&buf[offset], private_data->cb->_cache[index]->_data);
		}
		else {
			// the Block is not in the cache.
			int res = pread(fi->fh, temp, private_data->cb->_blockSize, offset);
			if (res < 0) {
				// Error in the reading.
				free(temp);
				return -errno;
			}
			if (res > 0) {
				strcpy(&buf[offset], &temp[0]);
				sum += res;
				private_data->cb->add(temp_path, i, temp);
			}
			if (res < private_data->cb->_blockSize) {
				// finished with the reading.
				i = start + num;
			}
		}
		offset += private_data->cb->_blockSize;

	}
	free(temp);
	return checkRetstat(sum);
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write+lush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int caching_flush(const char *path, struct fuse_file_info *fi)
{
	logWrite(getTime(), "flush");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int caching_release(const char *path, struct fuse_file_info *fi){
	logWrite(getTime(), "release");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	return checkRetstat(close(fi->fh));
}

/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int caching_opendir(const char *path, struct fuse_file_info *fi){
	logWrite(getTime(), "opendir");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	string fp = getFullPath(path).c_str();
	DIR *dir;
	int retstat = 0;
	dir = opendir(fp.c_str());
	if (dir == NULL) {
		return -errno;
	}
	fi->fh = (intptr_t) dir;
	return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * Introduced in version 2.3
 */
int caching_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
		struct fuse_file_info *fi){
	logWrite(getTime(), "readdir");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	int retstat = 0;
	DIR* dir;
	struct dirent *ent;
	dir = (DIR *) (uintptr_t) fi->fh;
	ent = readdir(dir);
	if (ent == 0) {
		return -errno;
	}
	 do {
		if (filler(buf, ent->d_name, NULL, 0) != 0) {
		    return -ENOMEM;
		}
	}
	while ((ent = readdir(dir)) != NULL);
	return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int caching_releasedir(const char *path, struct fuse_file_info *fi) {
	logWrite(getTime(), "releasedir");
	int error = checkPath(path);
	if (error != 0) {
		return error;
	}
	return checkRetstat(closedir((DIR *) (uintptr_t) fi->fh));
}

/** Rename a file */
int caching_rename(const char *path, const char *newpath) {
	logWrite(getTime(), "rename");
	int error = checkPath(path);
	int errorNew = checkPath(newpath);
	if (error != 0 || errorNew != 0) {
		return error > errorNew ? errorNew : error;
	}
	string fp = getFullPath(path);
	string fnp = getFullPath(newpath);
	int retstat = rename(fp.c_str(), fnp.c_str());
	if (retstat >= 0) {
		private_data->cb->rename(path, newpath);
	}
	return checkRetstat(retstat);
}

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *caching_init(struct fuse_conn_info *conn){
	logWrite(getTime(), "init");
	return private_data;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void caching_destroy(void *userdata){
	logWrite(getTime(), "destroy");
	delete private_data->cb;
}

/**
 * Ioctl from the FUSE sepc:
 * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
 * 64bit environment.  The size and direction of data is
 * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
 * data will be NULL, for _IOC_WRITE data is out area, for
 * _IOC_READ in area and if both are set in/out area.  In all
 * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
 *
 * However, in our case, this function only needs to print cache table to the log file .
 *
 * Introduced in version 2.8
 */
int caching_ioctl (const char *, int cmd, void *arg,
		struct fuse_file_info *, unsigned int flags, void *data){
	logWrite(getTime(), "ioctl");
	for (int i= 0; i < private_data->cb->capacity; i++) {
		logIoctl(private_data->cb->_cache[i]->_path,
				private_data->cb->_cache[i]->_sectionInBlock,
				private_data->cb->_cache[i]->_numOfUses);
	}
	return 0;
}

// Initialise the operations. 
// You are not supposed to change this function.
void init_caching_oper()
{
	caching_oper.getattr = caching_getattr;
	caching_oper.access = caching_access;
	caching_oper.open = caching_open;
	caching_oper.read = caching_read;
	caching_oper.flush = caching_flush;
	caching_oper.release = caching_release;
	caching_oper.opendir = caching_opendir;
	caching_oper.readdir = caching_readdir;
	caching_oper.releasedir = caching_releasedir;
	caching_oper.rename = caching_rename;
	caching_oper.init = caching_init;
	caching_oper.destroy = caching_destroy;
	caching_oper.ioctl = caching_ioctl;
	caching_oper.fgetattr = caching_fgetattr;


	caching_oper.readlink = NULL;
	caching_oper.getdir = NULL;
	caching_oper.mknod = NULL;
	caching_oper.mkdir = NULL;
	caching_oper.unlink = NULL;
	caching_oper.rmdir = NULL;
	caching_oper.symlink = NULL;
	caching_oper.link = NULL;
	caching_oper.chmod = NULL;
	caching_oper.chown = NULL;
	caching_oper.truncate = NULL;
	caching_oper.utime = NULL;
	caching_oper.write = NULL;
	caching_oper.statfs = NULL;
	caching_oper.fsync = NULL;
	caching_oper.setxattr = NULL;
	caching_oper.getxattr = NULL;
	caching_oper.listxattr = NULL;
	caching_oper.removexattr = NULL;
	caching_oper.fsyncdir = NULL;
	caching_oper.create = NULL;
	caching_oper.ftruncate = NULL;
}

static bool isNumber(const string& s)
{
	bool hitDecimal = 0;
	for(int i = 0; i < (int) s.length(); i++) {
		if(s[i] == '.' && !hitDecimal) {
			// 2 '.' in string mean invalid
			hitDecimal = 1; // first hit here, we forgive and skip
		}
		else if(!isdigit(s[i])) {
			return 0 ; // not ., not
		}
	}
	return 1;
}

//basic main. You need to complete it.
int main(int argc, char* argv[]){
	state * _file_data;

	//checks the errors.
	if (argc != 5 || !isNumber(argv[argc - 2]) || !isNumber(argv[argc - 1])) {
		cout << MAIN_ERROR << endl;
		exit(1);
	}

	if (strlen(argv[1]) > PATH_MAX || strlen(argv[2]) > PATH_MAX)
	{
		cout << MAIN_ERROR << endl;
		exit(1);
	}

	DIR* dirRoot = opendir(argv[1]);
	DIR* dirMount = opendir(argv[2]);
	if (!dirRoot || !dirMount)
	{
		cout << MAIN_ERROR << endl;
		exit(1);
	}
	closedir(dirRoot);
	closedir(dirMount);
	// All is clear.

	int num = atoi(argv[argc - 2]);
	int size = atoi(argv[argc - 1]);
	_file_data =(state*) calloc(sizeof(struct state), sizeof(char));
	_file_data->cb = new CachingBlock(num, size);
	if (_file_data == NULL || _file_data->cb == NULL) {
		cout << ALLOC_ERROR << endl;
		exit(1);
	}

	_file_data->rootdir = realpath(argv[1], NULL);
	initLog(_file_data->rootdir);
	init_caching_oper();

	argv[1] = argv[2];
	for (int i = 2; i < (argc - 1); i++){
		argv[i] = NULL;
	}
   	argv[2] = (char*) "-s";
	argc = 3;
	int fuse_stat = fuse_main(argc, argv, &caching_oper, _file_data);
	logClose();
	free(_file_data->rootdir);
	free(_file_data);
	return fuse_stat;
}
