

#ifndef BLOCK_DRIVER_INCLUDED
#define BLOCK_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : block_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the BLOCK storage system.
//
//  Author         : Patrick McDaniel
//

// Include files
#include <stdint.h>

// Defines
#define BLOCK_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define BLOCK_MAX_PATH_LENGTH 128 // Maximum length of filename length

//
// Interface functions
int32_t checkFileSize(int16_t fd, int32_t count);
//check file size of fd

int32_t readCurrentFrame(int16_t fd, void* buf, int32_t count);
// reads count bytes from the file fd into the buf

int16_t checkFileHandle(int16_t fd);
// checks for calid file handle

//int32_t checkSize(int16_t fd , int32_t count);
//increase frame size if count is greate than max frame size

int32_t increaseFileSize(int16_t fd , int32_t count);
//increases filesize if fd is greater than block size

int32_t checkCurrentFrame(int16_t fd , int32_t count);
//used to check current frame in fd file 

int16_t addNewFrame(int16_t fd);
// add new frames to file handle

int16_t setNextFrame(int16_t fd, int32_t count);
//sets next frame, if fill then add new frame

int32_t writeCurrentframe(int16_t fd, void* buf, int32_t count);
// Writes count bytes to fd file from the buffer

int computeframechecksum(void* frame, uint32_t* checksum);
//This functions calculates the checksum value using generatems5 function
int32_t block_poweron(void);
// Startup up the BLOCK interface, initialize filesystem

int32_t block_poweroff(void);
// Shut down the BLOCK interface, close all files

int16_t block_open(char* path);
// This function opens the file and returns a file handle

int16_t block_close(int16_t fd);
// This function closes the file

int32_t block_read(int16_t fd, char* buf, int32_t count);
// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t block_write(int16_t fd, char* buf, int32_t count);
// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t block_seek(int16_t fd, uint32_t loc);
// Seek to specific point in the file

#endif
