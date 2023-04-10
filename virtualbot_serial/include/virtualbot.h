#ifndef __VIRTUALBOT_H__

#define __VIRTUALBOT_H__

#include <linux/module.h>

#define DEVICE_NAME "virtualbot"

#define VIRTUALBOT_DRIVER_NAME "virtualbot_tty"

#define VIRTUALBOT_TTY_NAME "ttyVB"

#define VIRTUALBOT_TTY_MAJOR 200

// Set this to 1 for extra debugging messages
#define VIRTUALBOT_DEBUG 1

// Maximum numbers of Virtualbot devices
#define VIRTUALBOT_MAX_TTY_MINORS 4

// Maximum number of characters for a signal
#define VIRTUALBOT_MAX_SIGNAL_LEN 10

struct virtualbot_dev {
	// struct scull_qset *data;  /* Pointer to first quantum set */
	//int quantum;              /* the current quantum size */
	// int qset;                 /* the current array size */
	// unsigned long size;       /* amount of data stored here */
	// struct semaphore sem;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};

#endif