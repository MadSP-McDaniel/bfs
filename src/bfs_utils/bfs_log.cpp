////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_log.c
//  Description   : This is the logging service for the BFS utility
//                  library.  It provides access enable log events,
//                  whose levels are registered by the calling programs.
//
//  Author   : Patrick McDaniel
//  Created  : Wed 10 Mar 2021 01:59:24 PM EST
//

// System include files
#include <assert.h>
#include <cstdint>
#include <errno.h>

/* For the enclave side of enclave-based applications */
#ifdef __BFS_ENCLAVE_MODE

#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_trts.h"

/* For debug code with no use of enclaves */
#else

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdlib.h>
#include <string.h>
#include <string>

// Project Include Files
#include "bfs_log.h"
#include "bfs_util.h"

//
// Global data

bool loggingInitialzed = false;	  // Flag indicating if system setup
unsigned long logLevel;			  // This is the current log level
char *descriptors[MAX_LOG_LEVEL]; // This is the array of log level descriptors
const char *logFilename;		  // The log filename, with path
int32_t fileHandle = -1; // This is the filehandle that we are using to write.
int32_t echoHandle = -1; // This is descriptor to echo the content with
int errored = 0;		 // Is the log permanently errored?

// Functional prototypes
int openLog(void);
int closeLog(void);

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initializeLogging
// Description  : Intialize the logging system, set to defaults
//
// Inputs       : lvl - the log level to enable
// Outputs      : none

int initializeLogging(void) {

	// Only initialze as needed
	if (loggingInitialzed) {
		return (0);
	}

	// Setup library global variables
	fileHandle = STDOUT_FILENO;
	echoHandle = -1;
	errored = 0;
	memset(descriptors, 0x0, sizeof(descriptors));
	logFilename = LOG_SERVICE_NAME;
	loggingInitialzed = true;

	// Register the default levels
	registerLogLevel(LOG_ERROR_LEVEL_DESC, 1);
	registerLogLevel(LOG_WARNING_LEVEL_DESC, 1);
	registerLogLevel(LOG_INFO_LEVEL_DESC, 0);
	registerLogLevel(LOG_OUTPUT_LEVEL_DESC, 1);
	logLevel = DEFAULT_LOG_LEVEL;

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : enableLogLevels
// Description  : Turn on different log levels
//
// Inputs       : lvl - the log level to enable
// Outputs      : none

void enableLogLevels(unsigned long lvl) {

	// Only initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Set and return, no return code
	logLevel |= lvl;
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : disableLogLevels
// Description  : Turn off different log levels
//
// Inputs       : lvl - the log level to disable
// Outputs      : none

void disableLogLevels(unsigned long lvl) {

	// Only initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Set and return, no return code
	logLevel &= (lvl ^ 0xffffffff);
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : levelEnabled
// Description  : Are any of the log levels turned on?
//
// Inputs       : lvl - the log level(s) to check
// Outputs      : !0 if any levels on, 0 otherwise

int levelEnabled(unsigned long lvl) {

	// Only initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Is this level on?
	return ((logLevel & lvl) != 0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : setEchoDescriptor
// Description  : Set a file handle to echo content to
//
// Inputs       : eh - the new file handle
// Outputs      : !0 if any levels on, 0 otherwise

void setEchoDescriptor(int32_t eh) {

	// initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Set handle and return
	echoHandle = eh;
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initializeLogWithFilename
// Description  : Create a log with a given filename
//
// Inputs       : logname - the log filename
// Outputs      : none

int initializeLogWithFilename(const char *logname) {

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Setup output type, Return successfully
	logFilename = (logname == NULL) ? LOG_SERVICE_NAME : logname;
	fileHandle = -1;
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initializeLogWithFilehandle
// Description  : Create a log with a fixed file handle
//
// Inputs       : out - the file handle to use
// Outputs      : 0 if successfully, -1 if failure

int initializeLogWithFilehandle(int out) {

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Setup output type, Return successfully
	fileHandle = out;
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : registerLogLevel
// Description  : Register a new log level
//
// Inputs       : descriptor - a description of the new level
//                enable - flag indicating whether the level should be enabled
// Outputs      : the new log level, -1 if none available

unsigned long registerLogLevel(const char *descriptor, int enable) {

	// Local variables
	unsigned long i, lvl;

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Look for an open descriptor table
	for (i = 0; i < MAX_LOG_LEVEL; i++) {
		if (descriptors[i] == NULL) {

			// Compute level, enable (if necessary), save descriptor and return
			lvl = 1 << i;
			if (enable) {
				enableLogLevels(lvl);
			}

#ifdef __BFS_ENCLAVE_MODE
			descriptors[i] = bfs_strdup(descriptor);
#else
			descriptors[i] = strdup(descriptor);
#endif
			return (lvl);
		}
	}

	// Return no room left to create a new log level
#ifdef __BFS_ENCLAVE_MODE
	char _msg[MAX_LOG_MESSAGE_SIZE] = {0};
	snprintf(_msg, MAX_LOG_MESSAGE_SIZE, "Too many log levels [%s]\n",
			 logFilename);
	int64_t ocall_ret = 0;
	// ocall_printf(&ocall_ret, 2, MAX_LOG_MESSAGE_SIZE, _msg);
	ocall_printf(&ocall_ret, 2, (uint32_t)strlen(logFilename), _msg);
#else
	fprintf(stderr, "Too many log levels [%s]\n", logFilename);
#endif
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : freeLogRegistrations
// Description  : Cleanup all of the registrations
//
// Inputs       : none
// Outputs      : 0 if succesfull

int freeLogRegistrations(void) {

	// Local variables
	int i;

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Look for an descriptor table, free it
	for (i = 0; i < MAX_LOG_LEVEL; i++) {
		if (descriptors[i] != NULL) {
			free(descriptors[i]);
			descriptors[i] = NULL;
		}
	}

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bufToString
// Description  : Convert the buffer into a readable hex string
//
// Inputs       : buf - the buffer to make a string out of
//                blen - the length of the buffer
//                str - the place to put the string
//                slen - the length of the output string
// Outputs      : 0 if successful, -1 if failure

int bufToString(const char *buf, uint32_t blen, char *str, uint32_t slen) {

	// Variables and startup
	int i;
	char sbuf[25];
	str[0] = 0x0; // Null terminate the string

	// Now walk the bytes (up to a max 128 bytes)
	for (i = 0; ((i < (int)blen) && (i < 128) && (strlen(str) + 5 < slen));
		 i++) {
		snprintf(sbuf, 25, "0x%02x%s", (unsigned char)buf[i],
				 (i + 1 < (int)blen) ? " " : "");
		strncat(str, sbuf, slen - strlen(str));
	}

	// Return successfully
	return (0);
}

//
// Logging functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : logMessage
// Description  : Log a "printf"-style message
//
// Inputs       : lvl - the levels to log on, format (etc)
// Outputs      : 0 if successful, -1 if failure

int64_t logMessage(unsigned long lvl, const char *fmt, ...) {
	// QB: need to adapt this method to allow enclaves to use, since they cant
	// write directly to file descriptors

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Call the the va list version of the logging message
	va_list args;
	va_start(args, fmt);
	int64_t ret = vlogMessage(lvl, fmt, args);
	va_end(args);

	// Return the log return
	return (ret);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sLog::vlogMessage
// Description  : Log a "printf"-style message (va list prepared)
//
// Inputs       : lvl - the levels to disable,
//                fmt - format (etc)
//                args - the list of arguments for log message
// Outputs      : 0 if successful, -1 if failure

int64_t vlogMessage(unsigned long lvl, const char *fmt, va_list args) {

	// Local variables
	char msg[MAX_LOG_MESSAGE_SIZE + 256], tbuf[MAX_LOG_MESSAGE_SIZE + 256];
	uint64_t first = 1;
	int64_t ret, i;
	long writelen;

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Bail out if not read, open file if necessary
	if (!levelEnabled(lvl)) {
		return (0);
	}

	// Check to see if we have a valid filehandle for the log
	if (fileHandle == -1) {
		openLog();
	}

	// Check for error state before moving forward
	if (errored) {
		return (errored);
	}

	// Add header with descriptor names
#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	char ctm[100] = {0};

	if ((ocall_getTime(&ocall_ret, 100, ctm) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;

	ctm[strlen(ctm) - 1] = 0x0; // null out the newline char

	strncpy(tbuf, ctm, strlen(ctm)); // dont copy the null terminator
	strncpy(tbuf + strlen(ctm), " [ENCLAVE]", 11); // copy null term
#else
	time_t tm;
	time(&tm);
	char *ctm = ctime((const time_t *)&tm);

	ctm[strlen(ctm) - 1] = 0x0; // null out the newline char

	strncpy(tbuf, ctm, strlen(ctm) + 1); // copy null term
#endif
	strncat(tbuf, " [", MAX_LOG_MESSAGE_SIZE - 1);
	for (i = 0; i < MAX_LOG_LEVEL; i++) {
		if (levelEnabled((1 << i) & lvl)) {

			// Comma separate the levels if necessary
			if (!first) {
				strncat(tbuf, ",", MAX_LOG_MESSAGE_SIZE - 1);
			} else {
				first = 0;
			}

			// Add the level descriptor
			if (descriptors[i] == NULL) {
				strncat(tbuf, "*BAD LEVEL*", MAX_LOG_MESSAGE_SIZE - 1);
			} else {
				strncat(tbuf, descriptors[i], MAX_LOG_MESSAGE_SIZE - 1);
			}
		}
	}
	strncat(tbuf, "] ", MAX_LOG_MESSAGE_SIZE - 1);

	// Setup the "printf" like message, get write len
	vsnprintf(msg, MAX_LOG_MESSAGE_SIZE, fmt, args);
	strncat(tbuf, msg, MAX_LOG_MESSAGE_SIZE);
	writelen = strlen(tbuf);

	// Check if we need to CR/LF the line
	if (tbuf[writelen - 1] != '\n') {
		strncat(tbuf, "\n", MAX_LOG_MESSAGE_SIZE - 1);
		writelen = strlen(tbuf);
	}

	// Echo, then Write the entry to the log and return
	if (echoHandle != -1) {
#ifdef __BFS_ENCLAVE_MODE
		// ocall_printf((int64_t *)&ret, echoHandle, MAX_LOG_MESSAGE_SIZE + 256,
		// tbuf);
		ocall_printf((int64_t *)&ret, echoHandle, (int)writelen, tbuf);
#else
		ret = write(echoHandle, tbuf, (int)writelen);
#endif
	}

#ifdef __BFS_ENCLAVE_MODE
	ocall_printf((int64_t *)&ret, fileHandle, (uint32_t)strlen(tbuf), tbuf);
	if (ret != writelen) {
		char _msg[MAX_LOG_MESSAGE_SIZE] = {0};
		snprintf(_msg, MAX_LOG_MESSAGE_SIZE + 256,
				 "Error writing to log: [%s] (%ld)\n", logFilename, ret);
		ocall_printf((int64_t *)&ret, 2, (uint32_t)strlen(_msg), _msg);
	}
#else
	if ((ret = write(fileHandle, tbuf, strlen(tbuf))) != writelen) {
		fprintf(stderr, "Error writing to log: %s [%s] (%lu)\n", tbuf,
				logFilename, ret);
	}
#endif

	return (ret);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : logAssert
// Description  : Log a "printf"-style message where ASSERT fails
//
// Inputs       : lvl - the levels to log on, format (etc)
// Outputs      : 0 if successful, -1 if failure

int64_t logAssert(int expr, const char *file, int line, const char *fmt, ...) {

	// Check the assertion
	if (expr) {
		return (0);
	}

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Now log the assert failure
	logMessage(LOG_ERROR_LEVEL, "LOG_ASSERT_FAILED: %s @ line %d", file, line);
	va_list args;
	va_start(args, fmt);
	int64_t ret = vlogMessage(LOG_ERROR_LEVEL, fmt, args);
	va_end(args);
	assert(0);

	// Return the log return (UNREACHABLE)
	return (ret);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : logBufferMessage
// Description  : Show a buffer with a specified lable (data in hex)
//
// Inputs       : lvl - the log level of the message
//                label - the label to place on the buffer
//                buf - the buffer to show
//                len - the length of the buffer
// Outputs      : 0 if successful, -1 if failure

int64_t logBufferMessage(unsigned long lvl, const char *label, const char *buf,
						 uint32_t len) {

	// Local variables
	char printbuf[128];

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Bail out if level not enabled
	if (!levelEnabled(lvl)) {
		return (0);
	}

	// Convert the buffer and show it
	bufToString(buf, len, printbuf, 128);
	return (logMessage(lvl, "%s : %s", label, printbuf));
}

//
// Private Interfaces

////////////////////////////////////////////////////////////////////////////////
//
// Function     : openLog
// Description  : Open the log for processing
//
// Inputs       : none
// Outputs      : 0 if successful, -1 otherwise

int openLog(void) {

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// If no filename, then use STDERR
	if (logFilename == NULL) {
		fileHandle = 2;
	} else {

		// Open the log for writing (append if existing)
#ifdef __BFS_ENCLAVE_MODE
		// need a known-size buffer to make the ocall safely
		char *logFilename_copy = (char *)calloc(MAX_FILE_NAME_LEN, 1);
		assert(strlen(logFilename) <= MAX_FILE_NAME_LEN - 1);
		memcpy(logFilename_copy, logFilename, strlen(logFilename));
		logFilename_copy[strlen(logFilename)] = '\0';
		ocall_openLog((int32_t *)&fileHandle, logFilename_copy,
					  MAX_FILE_NAME_LEN);
		if (fileHandle == -1) {
			errored = 1;
			int64_t ret = 0;
			char _msg[MAX_LOG_MESSAGE_SIZE] = {0};
			snprintf(_msg, MAX_LOG_MESSAGE_SIZE, "Error opening log: %s\n",
					 logFilename);
			// ocall_printf((int64_t *)&ret, 2, MAX_LOG_MESSAGE_SIZE, _msg);
			ocall_printf((int64_t *)&ret, 2, (uint32_t)strlen(_msg), _msg);
		}
#else
		if ((fileHandle = open(logFilename, O_APPEND | O_CREAT | O_WRONLY,
							   S_IRUSR | S_IWUSR)) == -1) {
			// Error out
			errored = 1;
			fprintf(stderr, "Error opening log: %s (%s)\n", logFilename,
					strerror(errno));
		}
#endif
	}

	// Return successfully
	return (errored);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : closeLog
// Description  : Close the log
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int closeLog(void) {

	// Local variables
	int i;

	// Initialze as needed
	if (!loggingInitialzed) {
		initializeLogging();
	}

	// Cleanup the structures
	for (i = 0; i < MAX_LOG_LEVEL; i++) {
		if (descriptors[i] != NULL)
			free(descriptors[i]);
	}

	// Close and return successfully
	if (fileHandle != -1) {
#ifdef __BFS_ENCLAVE_MODE
		int64_t ret = 0;
		ocall_closeLog((int64_t *)&ret, fileHandle);
#else
		close(fileHandle);
#endif
		fileHandle = -1;
	}
	return (0);
}
