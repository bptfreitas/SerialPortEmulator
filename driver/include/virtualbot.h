#ifndef __VIRTUALBOT_H__

#define __VIRTUALBOT_H__

#include <linux/module.h>

#define VIRTUALBOT_DRIVER_NAME "emulatedport_tty"

#define VIRTUALBOT_TTY_NAME "ttyEmulatedPort"

#define VIRTUALBOT_TTY_MAJOR 200

#define VB_COMM_DRIVER_NAME "exogenous_tty"

#define VB_COMM_TTY_NAME "ttyExogenous"

#define VB_COMM_TTY_MAJOR (VIRTUALBOT_TTY_MAJOR + 1)

// Set this to 1 for extra debugging messages
#define VIRTUALBOT_DEBUG 1

// Maximum numbers of Virtualbot devices
#define VIRTUALBOT_MAX_TTY_MINORS 4

// Maximum number of characters for a signal
#define VIRTUALBOT_MAX_SIGNAL_LEN 262

/*
	Maximum number of signals that can be stored

	Since the driver won't release its allocated memory when the tty port 
	is closed it is important to have a safeguard against unlimited kernel
	memory allocation to prevent system colapse
*/
#define VIRTUALBOT_TOTAL_SIGNALS 10


/**
 * This buffer is needed to stop the ACK sent by the receiving device 
*/
#define IGNORE_CHAR_CBUFFER_SIZE 512

struct virtualbot_dev {
	// struct scull_qset *data;  /* Pointer to first quantum set */
	//int quantum;              /* the current quantum size */
	// int qset;                 /* the current array size */
	// unsigned long size;       /* amount of data stored here */
	// struct semaphore sem;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};

#endif