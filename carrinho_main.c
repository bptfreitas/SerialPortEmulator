

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/fs.h>

#include <linux/slab.h> // For kmalloc/kfree

MODULE_LICENSE("GPL v2");

#define BUF_LEN 10
#define SUCCESS 0
#define DEVICE_NAME "virtualbot"


// Simulador do Arduino
// crw-rw---- root dialout 166 0 -


#define VIRTUALBOT_MAJOR 166

/*  
 *  Prototypes - this would normally go in a .h file
 */
int virtualbot_init(void);
void virtualbot_exit(void);

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

/* 
 * Global variables are declared as static, so are global within the file. 
 */

static int Major;		/* Major number assigned to our device driver */
static int Device_Open = 0;	/* Is device open?  
				 * Used to prevent multiple access to device */
static char msg[BUF_LEN];	/* The msg the device will give when asked */
static char *msg_Ptr;


#define JAVINO_READ_TEST_STRING "fffe02OK"


static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

int __init virtualbot_init(void){

    Major = __register_chrdev(VIRTUALBOT_MAJOR, 0, 1, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", VIRTUALBOT_MAJOR);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", VIRTUALBOT_MAJOR);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, VIRTUALBOT_MAJOR);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS; 
};


void __exit virtualbot_exit(void){
	/* 
	 * Unregister the device 
	 */
	__unregister_chrdev(VIRTUALBOT_MAJOR, 0, 1, DEVICE_NAME);
	
	//printk(KERN_ALERT "Error in unregister_chrdev: %d\n", ret);


}

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	static int counter = 0;

	if (Device_Open)
		return -EBUSY;

	Device_Open++;
	sprintf(msg, "I already told you %d times Hello world!\n", counter++);
	msg_Ptr = msg;
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;		/* We're now ready for our next caller */

	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{

    char javino_test[] = JAVINO_READ_TEST_STRING;

    int nbytes_read = sizeof(char) * strlen(javino_test);

    buffer = (char*)kmalloc( nbytes_read + 1 , GFP_KERNEL);

    buffer[ nbytes_read ] = '\n';

#ifdef NOT_USED
	/*
	 * Number of bytes actually written to the buffer 
	 */
	int bytes_read = sizeof(javino_test);

	/*
	 * If we're at the end of the message, 
	 * return 0 signifying end of file 
	 */
	if (*msg_Ptr == 0)
		return 0;

	/* 
	 * Actually put the data into the buffer 
	 */
	while (length && *msg_Ptr) {

		/* 
		 * The buffer is in the user data segment, not the kernel 
		 * segment so "*" assignment won't work.  We have to use 
		 * put_user which copies data from the kernel data segment to
		 * the user data segment. 
		 */
		put_user(*(msg_Ptr++), buffer++);

		length--;
		bytes_read++;
	}
#endif

	/* 
	 * Most read functions return the number of bytes put into the buffer
	 */
	return nbytes_read;
}

/*  
 * Called when a process writes to dev file: echo "hi" > /dev/hello 
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	printk(KERN_ALERT "Sorry, this operation isn't supported.\n");
	//return -EINVAL;
    return 0;
}


module_init(virtualbot_init);
module_exit(virtualbot_exit);
