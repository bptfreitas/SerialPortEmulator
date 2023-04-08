#ifndef __VIRTUALBOT_H__

#define __VIRTUALBOT_H__

#include <linux/module.h>

#define DEVICE_NAME "virtualbot"

#define VIRTUALBOT_TTY_NAME "ttySVB"

#define TTY_VIRTUALBOT_MAJOR 200

// Set this to 1 for extra debugging messages
#define VIRTUALBOT_DEBUG 1

#define VIRTUALBOT_MAX_DEVICES 4



struct virtualbot_dev {
	// struct scull_qset *data;  /* Pointer to first quantum set */
	//int quantum;              /* the current quantum size */
	// int qset;                 /* the current array size */
	// unsigned long size;       /* amount of data stored here */
	// struct semaphore sem;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};

#endif