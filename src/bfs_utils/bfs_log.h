#ifndef BFS_LOG_INCLUDED
#define BFS_LOG_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_log.h
//  Description   : This is the logging service for the BFS utility
//                  library.  It provides access enable log events,
//                  whose levels are registered by the calling programs.
//
//   Note: The log process works on a bit-vector of levels, and all
//         functions operate on bit masks of levels (lvl).  Log entries are
//         given a level which is checked at run-time.  If the log level is
//         enabled, then the entry it written to the log, and not otherwise.
//
//  Author   : Patrick McDaniel
//  Created  : Wed 10 Mar 2021 01:59:24 PM EST
//

// Include files

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "bfs_common.h"

//
// Library Constants

#define LOG_SERVICE_NAME "bfs.log"

// Default log levels
#define LOG_ERROR_LEVEL 1
#define LOG_ERROR_LEVEL_DESC "ERROR"
#define LOG_WARNING_LEVEL 2
#define LOG_WARNING_LEVEL_DESC "WARNING"
#define LOG_INFO_LEVEL 4
#define LOG_INFO_LEVEL_DESC "INFO"
#define LOG_OUTPUT_LEVEL 8
#define LOG_OUTPUT_LEVEL_DESC "OUTPUT"
#define MAX_RESERVE_LEVEL LOG_INFO_LEVEL
#define MAX_LOG_LEVEL 32
#define DEFAULT_LOG_LEVEL LOG_ERROR_LEVEL | LOG_WARNING_LEVEL | LOG_OUTPUT_LEVEL
#define MAX_LOG_MESSAGE_SIZE 1024
#define BFS_LOG_STDOUT 1
#define BFS_LOG_STDERR 2

//
// Interface
#ifdef __cplusplus
extern "C" {
#endif

//
// Basic logging interfaces

int initializeLogging(void);
// Intialize the logging system, set to defaults

unsigned long registerLogLevel(const char *descriptor, int enable);
// Register a new log level

void enableLogLevels(unsigned long lvl);
// Turn on different log levels

void disableLogLevels(unsigned long lvl);
// Turn off different log levels

int levelEnabled(unsigned long lvl);
// Are any of the log levels turned on?

void setEchoDescriptor(int eh);
// Set a file handle to echo content to

int initializeLogWithFilename(const char *logname);
// Create a log with a given filename

int initializeLogWithFilehandle(int out);
// Create a log with a fixed file handle

int freeLogRegistrations(void);
// Cleanup all of the registrations

int bufToString(const char *buf, uint32_t blen, char *str, uint32_t slen);
// Description  : Convert the buffer into a readable hex string

//
// Logging functions

int64_t logMessage(unsigned long lvl, const char *fmt, ...);
// Log a "printf"-style message

int64_t vlogMessage(unsigned long lvl, const char *fmt, va_list args);
// Log call the vararg list version

int64_t logBufferMessage(unsigned long lvl, const char *label, const char *buf,
						 uint32_t len);
// Show a buffer with a specified lable (data in hex)

//
// Assert functions

#define BFS_ASSERT0(expr, x) logAssert(expr, __FILE__, __LINE__, x);
#define BFS_ASSERT1(expr, x, y) logAssert(expr, __FILE__, __LINE__, x, y);
#define BFS_ASSERT2(expr, x, y, z) logAssert(expr, __FILE__, __LINE__, x, y, z);

int64_t logAssert(int expr, const char *file, int line, const char *fmt, ...);
// Log a "printf"-style message where ASSERT fails

#ifdef __cplusplus
}
#endif

#endif
