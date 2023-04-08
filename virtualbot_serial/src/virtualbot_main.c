

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

#include <linux/tty_driver.h>

#include <linux/semaphore.h>

#include <linux/tty.h>


#include <virtualbot.h>


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

#ifdef VIRTUALBOT_UART
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

#endif

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

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

struct virtualbot_serial {
    struct tty_struct   *tty;   /* pointer to the tty for this device */
    int         open_count; 	/* number of times this port has been opened */
    struct semaphore    sem;    /* locks this structure */
    struct timer_list   *timer;
};


struct virtualbot_serial* virtualbot_table[ VIRTUALBOT_MAX_DEVICES ];

static struct tty_port virtualbot_tty_port[ VIRTUALBOT_MAX_DEVICES ];

static int virtualbot_open(struct tty_struct *tty, struct file *file){

    struct virtualbot_serial *virtualbot;
    //struct timer_list *timer;
    int index;

	index = tty->index;

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: open port %d", index);
#endif		

    /* initialize the pointer in case something fails */
    tty->driver_data = NULL;

    virtualbot = virtualbot_table[index];

    if (virtualbot == NULL) {
        /* first time accessing this device, let's create it */
        virtualbot = kmalloc(sizeof(*virtualbot), GFP_KERNEL);

        if (!virtualbot)
            return -ENOMEM;

        sema_init( &virtualbot->sem, 1 );
        virtualbot->open_count = 0;
        virtualbot->timer = NULL;

        virtualbot_table[ index ] = virtualbot;
    }

    //down(&virtualbot->sem);

 	++virtualbot->open_count;
    if (virtualbot->open_count == 1) {
        /* this is the first time this port is opened */
        /* do any hardware initialization needed here */
	}

    /* save our structure within the tty structure */
    tty->driver_data = virtualbot;
    virtualbot->tty = tty;	

	//up(&virtualbot->sem);

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: open port %d success", index);
#endif		

	return 0;
}


static void virtualbot_close(struct tty_struct *tty, struct file *file)
{

	struct virtualbot_serial *virtualbot = tty->driver_data;

	int index = tty->index;

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: closing port %d", index);
#endif		

    if (virtualbot){

		down(&virtualbot->sem);

		if (!virtualbot->open_count) {
			/* port was never opened */
			up(&virtualbot->sem);
			goto exit;
		}
		
		--(virtualbot->open_count);

		if (virtualbot->open_count <= 0) {
			/* The port is being closed by the last user. */
			/* Do any hardware specific stuff here */



			/* shut down our timer */
			del_timer(virtualbot->timer);
			goto exit;		
		}

	exit:

#ifdef VIRTUALBOT_DEBUG
		printk(KERN_INFO "virtualbot: closed port %d", index);
#endif		
		up(&virtualbot->sem);
		return;
	}

    return;
}

static int virtualbot_write(struct tty_struct *tty, 
	const unsigned char *buffer, 
	int count)
{
    struct virtualbot_serial *virtualbot = tty->driver_data;
	int index = tty->index;
    int i;
    int retval = -EINVAL;

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: write on port %d", index);
#endif	

    if (!virtualbot)
        return -ENODEV;

    down(&virtualbot->sem);

    if (!virtualbot->open_count)
        /* port was not opened */
        goto exit;

    /* fake sending the data out a hardware port by
     * writing it to the kernel debug log.
     */
    printk(KERN_DEBUG "virtualbot: %s - ", __FUNCTION__);
    for (i = 0; i < count; ++i)
        printk("%02x ", buffer[i]);

    printk("\n");
        
exit:
    up(&virtualbot->sem);

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: finished write on port %d", index);
#endif		
    return retval;
}

static unsigned int virtualbot_write_room(struct tty_struct *tty) 
{
    struct virtualbot_serial *virtualbot = tty->driver_data;
    int room = -EINVAL;

    if (!virtualbot)
        return -ENODEV;

    down(&virtualbot->sem);
    
    if (!virtualbot->open_count) {
        /* port was not opened */
        goto exit;
    }

    /* calculate how much room is left in the device */
    room = 255;

exit:
    up(&virtualbot->sem);
    return room;
}

static void virtualbot_set_termios(
	struct tty_struct *tty, 
	struct ktermios *old_termios)
{

#ifdef VIRTUALBOT_DEBUG
	printk(KERN_INFO "virtualbot: set_termios");
#endif			
	unsigned int cflag;

	cflag = tty->termios.c_cflag;

	/* check that they really want us to change something */
	if (old_termios) {
		if ((cflag == old_termios->c_cflag) &&
		    (RELEVANT_IFLAG(tty->termios.c_iflag) ==
		     RELEVANT_IFLAG(old_termios->c_iflag))) {
			pr_debug(" - nothing to change...\n");
			return;
		}
	}

	/* get the byte size */
	switch (cflag & CSIZE) {
	case CS5:
		pr_debug(" - data bits = 5\n");
		break;
	case CS6:
		pr_debug(" - data bits = 6\n");
		break;
	case CS7:
		pr_debug(" - data bits = 7\n");
		break;
	default:
	case CS8:
		pr_debug(" - data bits = 8\n");
		break;
	}

	/* determine the parity */
	if (cflag & PARENB)
		if (cflag & PARODD)
			pr_debug(" - parity = odd\n");
		else
			pr_debug(" - parity = even\n");
	else
		pr_debug(" - parity = none\n");

	/* figure out the stop bits requested */
	if (cflag & CSTOPB)
		pr_debug(" - stop bits = 2\n");
	else
		pr_debug(" - stop bits = 1\n");

	/* figure out the hardware flow control settings */
	if (cflag & CRTSCTS)
		pr_debug(" - RTS/CTS is enabled\n");
	else
		pr_debug(" - RTS/CTS is disabled\n");

	/* determine software flow control */
	/* if we are implementing XON/XOFF, set the start and
	 * stop character in the device */
	if (I_IXOFF(tty) || I_IXON(tty)) {
		unsigned char stop_char  = STOP_CHAR(tty);
		unsigned char start_char = START_CHAR(tty);

		/* if we are implementing INBOUND XON/XOFF */
		if (I_IXOFF(tty))
			pr_debug(" - INBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - INBOUND XON/XOFF is disabled");

		/* if we are implementing OUTBOUND XON/XOFF */
		if (I_IXON(tty))
			pr_debug(" - OUTBOUND XON/XOFF is enabled, "
				"XON = %2x, XOFF = %2x", start_char, stop_char);
		else
			pr_debug(" - OUTBOUND XON/XOFF is disabled");
	}

	/* get the baud rate wanted */
	pr_debug(" - baud rate = %d", tty_get_baud_rate(tty));	

}



struct ktermios tty_std_termios = {
    .c_iflag = ICRNL | IXON,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = B38400 | CS8 | CREAD | HUPCL,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK |
               ECHOCTL | ECHOKE | IEXTEN,
    .c_cc = INIT_C_CC
};

static struct tty_operations virtualbot_serial_ops = {
    .open = virtualbot_open,
    .close = virtualbot_close,
    .write = virtualbot_write,
    .write_room = virtualbot_write_room,
    .set_termios = virtualbot_set_termios,
};

static struct tty_driver *virtualbot_tty_driver;

int __init virtualbot_init(void){

	int rc, i;

	virtualbot_tty_driver = tty_alloc_driver(1,
		TTY_DRIVER_DYNAMIC_ALLOC 
		| TTY_DRIVER_REAL_RAW );

/*
	virtualbot_tty_driver = __tty_alloc_driver(1, 
		THIS_MODULE,
		TTY_DRIVER_DYNAMIC_ALLOC
		);
*/

	if (!virtualbot_tty_driver){

		printk( KERN_ERR "virtualbot: error allocating driver!" );
		return -ENOMEM;	
	}

	virtualbot_tty_driver->owner = THIS_MODULE;
    virtualbot_tty_driver->driver_name = "virtualbot_tty";
    virtualbot_tty_driver->name = "ttyVB";
    //virtualbot_tty_driver->devfs_name = "tts/ttty%d";
    virtualbot_tty_driver->major = 200,
    virtualbot_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
    virtualbot_tty_driver->subtype = SERIAL_TYPE_NORMAL,
    virtualbot_tty_driver->flags = 
		TTY_DRIVER_REAL_RAW
		| TTY_DRIVER_DYNAMIC_DEV
		;
    virtualbot_tty_driver->init_termios = tty_std_termios;
    virtualbot_tty_driver->init_termios.c_cflag = 
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;

    tty_set_operations(virtualbot_tty_driver, &virtualbot_serial_ops);

	/*
	for (i = 0; i < VIRTUALBOT_MAX_DEVICES; i++) {
		tty_port_init( virtualbot_tty_port + i );

		tty_port_link_device( virtualbot_tty_port + i, 
			virtualbot_tty_driver, 
			i);
	}
	*/	

	rc = tty_register_driver(virtualbot_tty_driver);

	if (rc) {
		printk(KERN_ERR "virtualbot: failed to register driver");

		tty_driver_kref_put(virtualbot_tty_driver);

		return rc;
	}	

	printk( KERN_INFO "virtualbot: driver registered" );

	for ( i =0; i < VIRTUALBOT_MAX_DEVICES; i++){

		rc = tty_register_device(virtualbot_tty_driver, i, NULL);

		if (rc){
			return rc;
		}

		virtualbot_table[ i ] = NULL;
	}

	return 0; 
};


void __exit virtualbot_exit(void){
	/* 
	 * Unregister the device 
	 */
	//platform_driver_unregister( &virtualbot_serial_driver );
	int i;

	for ( i =0; i < VIRTUALBOT_MAX_DEVICES; i++){

		tty_unregister_device(virtualbot_tty_driver, i);

		virtualbot_table[ i ] = NULL;
	}    

	tty_unregister_driver(virtualbot_tty_driver);

	printk( KERN_INFO "virtualbot: device unregistered" );
}


module_init(virtualbot_init);
module_exit(virtualbot_exit);
