

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/fs.h>

#include <linux/types.h>

#include <linux/cdev.h>

#include <linux/slab.h> // For kmalloc/kfree

#include <linux/platform_device.h>
#include <linux/serial_core.h>


#include <virtualbot.h>


struct virtualbot_dev *virtualbot_devices;


MODULE_LICENSE("GPL v2");

#define BUF_LEN 10
#define SUCCESS 0

// Simulador do Arduino
// crw-rw---- root dialout 166 0 -


// #define VIRTUALBOT_MAJOR 166

/*  
 *  Prototypes - this would normally go in a .h file
 */
int virtualbot_init(void);
void virtualbot_exit(void);

//static int virtualbot_open(struct inode *, struct file *);
//static int virtualbot_release(struct inode *, struct file *);
//static ssize_t virtualbot_read(struct file *, char *, size_t, loff_t *);
//static ssize_t virtualbot_write(struct file *, const char *, size_t, loff_t *);

/* 
 * Global variables are declared as static, so are global within the file. 
 */

//static dev_t device;

//  static int Device_Open = 0;	/* Is device open?  
// * Used to prevent multiple access to device */
// static char msg[BUF_LEN];	/* The msg the device will give when asked */
// static char *msg_Ptr;


#define JAVINO_READ_TEST_STRING "fffe02OK"

static struct uart_port *port;

static int virtualbot_serial_probe(struct platform_device *pdev)
{
/*
	struct atmel_uart_port *port;
	port = &atmel_ports[pdev->id];
	port->backup_imr = 0;
	atmel_init_port(port, pdev);
	uart_add_one_port(&atmel_uart, &port->uart);
	platform_set_drvdata(pdev, port);
*/

	port = kmalloc( GFP_KERNEL, sizeof(struct uart_port) );

	platform_set_drvdata(pdev, port);
	
	return 0;
}


static int virtualbot_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	kfree(port);

	//uart_remove_one_port(&atmel_uart, port);

	return 0;
}

static struct uart_driver virtualbot_uart = {
	.owner		= THIS_MODULE,
	.driver_name	= "virtualbot",
	.dev_name	= "ttyVirtualBot",
	.major		= TTY_VIRTUALBOT_MAJOR,
	.minor		= 200,
	.nr		= 1,
	//.cons		= ALTERA_JTAGUART_CONSOLE,
};

/*
static struct platform_driver virtualbot_serial_driver = {
	.probe = virtualbot_serial_probe,
	.remove = virtualbot_serial_remove,
	//.suspend = virtualbot_serial_suspend,
	//.resume = virtualbot_serial_resume,
	.driver = {
		.name = "virtualbot_usart",
		.owner = THIS_MODULE,
	},
};
*/

int __init virtualbot_init(void){

	int rc;

	rc = uart_register_driver(&virtualbot_uart);

	if (rc){
		printk( KERN_ERR "virtualbot: device failed to register!" );
		return rc;
	} else {
		printk( KERN_INFO "virtualbot: device registered" );
	}

	port = kmalloc( GFP_KERNEL, sizeof(struct uart_port) );

	uart_add_one_port(&virtualbot_uart, port);

	// uart_add_one_port(struct uart_driver * drv, struct uart_port * uport)

	//platform_driver_register( &virtualbot_serial_driver );

	return 0; 
};


void __exit virtualbot_exit(void){
	/* 
	 * Unregister the device 
	 */
	//platform_driver_unregister( &virtualbot_serial_driver );

	uart_remove_one_port( &virtualbot_uart, port);

	uart_unregister_driver( &virtualbot_uart );

	kfree( port );

	printk( KERN_INFO "virtualbot: device unregistered" );
}


module_init(virtualbot_init);
module_exit(virtualbot_exit);
