

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

static struct uart_port *port;

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

#ifdef VIRTUALBOT_CONSOLE

static struct console virtualbot_console = {
	.name = VIRTUALBOT_TTY_NAME,
	.write = virtualbot_console_write,
	/* Helper function from the serial_core layer */
	.device = uart_console_device,
	.setup = virtualbot_console_setup,
	/* Ask for the kernel messages buffered during
	* boot to be printed to the console when activated */
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &serial_txx9_reg,
};

static int __init virtualbot_console_init(void)
{
	register_console(&virtualbot_console);
	return 0;
}

static int __init virtualbot_console_setup(
	struct console *co,
	char *options)
{
	int baud = 9600;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';


	return uart_set_options(port, co, baud, parity, bits, flow);
}

console_initcall(virtualbot_console_init);

#endif

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

 /* Grab any interrupt resources and initialise any low level driver state.
 *	Enable the port for reception. It should not activate RTS nor DTR;
 *	this will be done via a separate call to @set_mctrl().
 *
 *	This method will only be called when the port is initially opened.
 *
 *	Locking: port_sem taken.
 *	Interrupts: globally disabled. */
int virtualbot_startup(struct uart_port *port){

//#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: startup" );
//#endif

	return 0;
}

/*	Disable the @port, disable any break condition that may be in effect,
 *	and free any interrupt resources. It should not disable RTS nor DTR;
 *	this will have already been done via a separate call to @set_mctrl().
 *
 *	Drivers must not access @port->state once this call has completed.
 *
 *	This method will only be called when there are no more users of this
 *	@port.
 *
 *	Locking: port_sem taken.
 *	Interrupts: caller dependent. */
void virtualbot_shutdown(struct uart_port *port){

//#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: shutdown" );
//#endif

}

/* Start transmitting characters.
 *
 *	Locking: @port->lock taken.
 *	Interrupts: locally disabled.
 *	This call must not sleep
 */
void virtualbot_start_tx(struct uart_port *port){

	struct circ_buf *xmit = &port->state->xmit;

//#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: start_tx" );
//#endif	

	while (!uart_circ_empty(xmit)) {
		// foo_uart_putc(port, xmit->buf[xmit->tail]);
		printk(KERN_INFO "virtualbot: start_tx[%c]", xmit->buf[xmit->tail] );
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;
	}	


}

/* Stop transmitting characters. This might be due to the CTS line
 *	becoming inactive or the tty layer indicating we want to stop
 *	transmission due to an %XOFF character.
 *
 *	The driver should stop transmitting characters as soon as possible.
 *
 *	Locking: @port->lock taken.
 *	Interrupts: locally disabled.
 *	This call must not sleep
 */
void virtualbot_stop_tx(struct uart_port *port){

	printk(KERN_INFO "virtualbot: stop_rx" );


}

/* This function tests whether the transmitter fifo and shifter for the
 *	@port is empty. If it is empty, this function should return
 *	%TIOCSER_TEMT, otherwise return 0. If the port does not support this
 *	operation, then it should return %TIOCSER_TEMT.
 *
 *	Locking: none.
 *	Interrupts: caller dependent.
 *	This call must not sleep */
unsigned int virtualbot_tx_empty(struct uart_port *port){

	printk(KERN_INFO "virtualbot: tx_empty" );


	return TIOCSER_TEMT;
}

 /*	Request any memory and IO region resources required by the port. If any
 *	fail, no resources should be registered when this function returns, and
 *	it should return -%EBUSY on failure.
 *
 *	Locking: none.
 *	Interrupts: caller dependent. */
 int virtualbot_request_port(struct uart_port *port){

	printk(KERN_INFO "virtualbot: request_port" );


	return 0;
 }

 /*	Release any memory and IO region resources currently in use by the
 *	@port.
 *
 *	Locking: none.
 *	Interrupts: caller dependent. */
void virtualbot_release_port(struct uart_port *port){

	printk(KERN_INFO "virtualbot: release_port" );
}

static struct uart_ops virtualbot_uart_ops = {

	.startup = virtualbot_startup,
	.shutdown = virtualbot_shutdown,

	.tx_empty = virtualbot_tx_empty,

	.start_tx = virtualbot_start_tx,
	.stop_tx = virtualbot_stop_tx,

	.request_port = virtualbot_request_port,
	.release_port = virtualbot_release_port
};

#if 0
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

#endif

static struct platform_driver virtualbot_serial_driver = {
	//.probe = virtualbot_serial_probe,
	//.remove = virtualbot_serial_remove,
	//.suspend = virtualbot_serial_suspend,
	//.resume = virtualbot_serial_resume,
	.driver = {
		.name = "virtualbot_usart",
		.owner = THIS_MODULE,
	},
};



static struct uart_driver virtualbot_uart = {
	.owner		= THIS_MODULE,
	.driver_name	= "virtualbot",
	.dev_name	= "ttySVB",
	.major		= TTY_MAJOR,
	.minor		= 64,
	.nr		= 1,
	.cons		= NULL,
	.tty_driver = NULL,
};

int __init virtualbot_init(void){

	int rc;

	rc = uart_register_driver( &virtualbot_uart );

	if (rc){
		printk( KERN_ERR "virtualbot: driver failed to register with code %d! ", rc );
		return rc;
	}

	printk( KERN_INFO "virtualbot: initializing driver" );

	port = kmalloc( GFP_KERNEL, sizeof(struct uart_port) );

	// This is all fake initialization ...
	port->iotype = UPIO_MEM;
	port->flags = 0x0; // TODO: inspect all flags possible
	port->ops = &virtualbot_uart_ops;
	port->fifosize = 1; // Increase? No idea ...
	port->line = 1; //??? Depends on platform_driver ... what to write here?
	port->dev = NULL; // Probably can't be null ...
	port->icount.tx = 0;
	port->icount.rx = 0;

	rc = uart_add_one_port(&virtualbot_uart, port);

	if (rc){
		printk( KERN_ERR "virtualbot: error on uart_add_one_port! Code = %d", rc);
		kfree( port );

		uart_unregister_driver( &virtualbot_uart );

	 	return rc;
	}

	// uart_add_one_port(struct uart_driver * drv, struct uart_port * uport)

	//platform_driver_register( &virtualbot_serial_driver );

	printk( KERN_INFO "virtualbot: driver registered" );

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
