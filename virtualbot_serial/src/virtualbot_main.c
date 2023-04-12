/*
 * VirtualBot TTY driver
 *
 * Copyright (C) 2023 Bruno Policarpo (bruno.freitas@cefet-rj.br)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2 of the License.
 *
 * This driver simulates the behaviour of a MAS agent embedded device communicating via a serial port
 * It was based on Tiny TTY driver from https://github.com/martinezjavier/ldd3/blob/master/tty/tiny_tty.c
 * It does not rely on any backing hardware, but creates a timer that emulates data being received
 * from some kind of hardware.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include <linux/string.h>

#include <virtualbot.h>

#define DRIVER_VERSION "v0.0"
#define DRIVER_AUTHOR "Bruno Policarpo <bruno.freitas@cefet-rj.br>"
#define DRIVER_DESC "VirtualBot TTY Driver"

/* Module information */
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define DELAY_TIME		(HZ * 2)	/* 2 seconds per character */
#define TINY_DATA_CHARACTER	't'

struct MAS_signal {

	char data[ VIRTUALBOT_MAX_SIGNAL_LEN + 2 ];

    struct list_head MAS_signals_list;

};

static int MAS_signals_count = 0;

struct tiny_serial {
	struct tty_struct	*tty;		/* pointer to the tty for this device */
	int			open_count;	/* number of times this port has been opened */
	struct mutex	mutex;		/* locks this structure */
	struct timer_list	timer;

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;

	wait_queue_head_t	wait;
	
	struct async_icount	icount;

};


/* for MAS agent */
static struct list_head *MAS_signals_list_head[ VIRTUALBOT_MAX_TTY_MINORS ];

static struct tiny_serial *tiny_table[ VIRTUALBOT_MAX_TTY_MINORS ];	/* initially all NULL */

static struct tty_port tiny_tty_port[ VIRTUALBOT_MAX_TTY_MINORS ];

static void tiny_timer(struct timer_list *t)
{
	struct tiny_serial *tiny = from_timer(tiny, t, timer);
	struct tty_struct *tty;
	struct tty_port *port;
	int i;
	char data[1] = {TINY_DATA_CHARACTER};
	int data_size = 1;

	if (!tiny)
		return;

	tty = tiny->tty;
	port = tty->port;

	/* send the data to the tty layer for users to read.  This doesn't
	 * actually push the data through unless tty->low_latency is set */
	for (i = 0; i < data_size; ++i) {
		if (!tty_buffer_request_room(port, 1))
			tty_flip_buffer_push(port);
		tty_insert_flip_char(port, data[i], TTY_NORMAL);
	}
	tty_flip_buffer_push(port);

	/* resubmit the timer again */
	tiny->timer.expires = jiffies + DELAY_TIME;
	add_timer(&tiny->timer);
}

static int tiny_open(struct tty_struct *tty, struct file *file)
{
	struct tiny_serial *tiny;
	int index;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	index = tty->index;
	tiny = tiny_table[index];

	pr_debug("virtualbot: open port %d", index);

	if (tiny == NULL) {
		/* first time accessing this device, let's create it */
		tiny = kmalloc(sizeof(*tiny), GFP_KERNEL);
		if (!tiny)
			return -ENOMEM;

		mutex_init(&tiny->mutex);
		tiny->open_count = 0;

		tiny_table[index] = tiny;
	}

	mutex_lock(&tiny->mutex);

	/* save our structure within the tty structure */
	tty->driver_data = tiny;
	tiny->tty = tty;

	++tiny->open_count;
	if (tiny->open_count == 1) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */

		/* create our timer and submit it */
		timer_setup(&tiny->timer, tiny_timer, 0);

		/* 
			MAS structure initialization. 
			It will be initialized the first time the driver is openned
			and stay alive on the driver until it is unregistered with 
			modprobe -r
		*/
		if ( MAS_signals_list_head[ tty->index ] == NULL ){

			MAS_signals_list_head[ tty->index ] = kmalloc( sizeof( struct list_head ), GFP_KERNEL );

			INIT_LIST_HEAD( MAS_signals_list_head[ tty->index ] );
		}

		tiny->timer.expires = jiffies + DELAY_TIME;

		add_timer(&tiny->timer);
	}

	mutex_unlock(&tiny->mutex);

	pr_debug("virtualbot: finished open port %d", index);

	return 0;
}

static void do_close(struct tiny_serial *tiny)
{
	pr_debug("virtualbot: do_close");

	mutex_lock(&tiny->mutex);

	if (!tiny->open_count) {
		/* port was never opened */
		goto exit;
	}

	--tiny->open_count;
	if (tiny->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */

		/* shut down our timer */
		del_timer(&tiny->timer);
	}
exit:
	pr_debug("virtualbot: do_close finished");
	mutex_unlock(&tiny->mutex);
}

static void tiny_close(struct tty_struct *tty, struct file *file)
{
	struct tiny_serial *tiny = tty->driver_data;

	pr_debug("virtualbot: close port %d", tty->index);

	if (tiny){
		do_close(tiny);
		pr_debug("virtualbot: do_close port %d finished", tty->index);
	}
}

static int tiny_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	struct tiny_serial *tiny = tty->driver_data;
	int i, buffer_len, index = tty->index;
	int retval = -EINVAL;

	struct MAS_signal *new_MAS_signal;	

	if (!tiny)
		return -ENODEV;

	mutex_lock(&tiny->mutex);

	if (!tiny->open_count)
		/* port was not opened */
		goto exit;

	// discarding the carriage return and new line the tty core is sending
	if ( count == 2 && buffer[0] == '\r' && buffer[1]=='\n' ){
		retval = 2;
		goto exit;
	}

	/* fake sending the data out a hardware port by
	 * writing it to the kernel debug log.
	 */

	// TODO: race condition here!
	if (MAS_signals_count < VIRTUALBOT_TOTAL_SIGNALS ){

		buffer_len = ( count < VIRTUALBOT_MAX_SIGNAL_LEN) ? 
			count : VIRTUALBOT_MAX_SIGNAL_LEN;

		if ( count > VIRTUALBOT_MAX_SIGNAL_LEN ){
			pr_warn("virtualbot: signal length (%d) is greater than maximum (%d) - discarding", 
				count,
				VIRTUALBOT_MAX_SIGNAL_LEN );

			for ( i = 0; i < buffer_len; i++ ){
				pr_warn("%02x ", buffer[i]);
			}
			retval = 0;
			goto exit;
		}			

		pr_debug("virtualbot: %s - writing %d length of data", __func__, count);		

		new_MAS_signal = kmalloc(sizeof(struct MAS_signal), GFP_KERNEL);

		for ( i = 0; i < buffer_len; i++ ){
			pr_info("%02x ", buffer[i]);
		}

		strcpy( new_MAS_signal->data, 
			buffer
			);

		list_add( &new_MAS_signal->MAS_signals_list , MAS_signals_list_head[ index ] ) ;

		retval = count;
		pr_info("\n");
	} else {
		// Maximum number of signals stored achieved - warn then exit
		pr_warn("virtualbot: maximum number of stored signals achieved");
		retval = 0;
	}

exit:
	mutex_unlock(&tiny->mutex);
	return retval;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
static int tiny_write_room(struct tty_struct *tty)
#else
static unsigned int tiny_write_room(struct tty_struct *tty)
#endif
{
	struct tiny_serial *tiny = tty->driver_data;
	int room = -EINVAL;

	if (!tiny)
		return -ENODEV;

	mutex_lock(&tiny->mutex);

	if (!tiny->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	room = 255;

exit:
	mutex_unlock(&tiny->mutex);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void tiny_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
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

/* Our fake UART values */
#define MCR_DTR		0x01
#define MCR_RTS		0x02
#define MCR_LOOP	0x04
#define MSR_CTS		0x08
#define MSR_CD		0x10
#define MSR_RI		0x20
#define MSR_DSR		0x40

static int tiny_tiocmget(struct tty_struct *tty)
{
	struct tiny_serial *tiny = tty->driver_data;

	unsigned int result = 0;
	unsigned int msr = tiny->msr;
	unsigned int mcr = tiny->mcr;

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
		((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
		((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
		((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
		((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
		((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
		((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */

	return result;
}

static int tiny_tiocmset(struct tty_struct *tty, unsigned int set,
			 unsigned int clear)
{
	struct tiny_serial *tiny = tty->driver_data;
	unsigned int mcr = tiny->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	tiny->mcr = mcr;
	return 0;
}

static int tiny_proc_show(struct seq_file *m, void *v)
{
	struct tiny_serial *tiny;
	int i;

	seq_printf(m, "tinyserinfo:1.0 driver:%s\n", DRIVER_VERSION);
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {
		tiny = tiny_table[i];
		if (tiny == NULL)
			continue;

		seq_printf(m, "%d\n", i);
	}

	return 0;
}

#define tiny_ioctl tiny_ioctl_tiocgserial

static int tiny_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCGSERIAL) {
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));

		tmp.type		= tiny->serial.type;
		tmp.line		= tiny->serial.line;
		tmp.port		= tiny->serial.port;
		tmp.irq			= tiny->serial.irq;
		tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size	= tiny->serial.xmit_fifo_size;
		tmp.baud_base		= tiny->serial.baud_base;
		tmp.close_delay		= 5*HZ;
		tmp.closing_wait	= 30*HZ;
		tmp.custom_divisor	= tiny->serial.custom_divisor;
		tmp.hub6		= tiny->serial.hub6;
		tmp.io_type		= tiny->serial.io_type;

		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

#define tiny_ioctl tiny_ioctl_tiocmiwait
static int tiny_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCMIWAIT) {
		DECLARE_WAITQUEUE(wait, current);
		struct async_icount cnow;
		struct async_icount cprev;

		cprev = tiny->icount;
		while (1) {
			add_wait_queue(&tiny->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&tiny->wait, &wait);

			/* see if a signal woke us up */
			if (signal_pending(current))
				return -ERESTARTSYS;

			cnow = tiny->icount;
			if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
			    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
				return -EIO; /* no change => error */
			if (((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
			    ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
			    ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
			    ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts))) {
				return 0;
			}
			cprev = cnow;
		}

	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

#define tiny_ioctl tiny_ioctl_tiocgicount
static int tiny_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct tiny_serial *tiny = tty->driver_data;

	if (cmd == TIOCGICOUNT) {
		struct async_icount cnow = tiny->icount;
		struct serial_icounter_struct icount;

		icount.cts	= cnow.cts;
		icount.dsr	= cnow.dsr;
		icount.rng	= cnow.rng;
		icount.dcd	= cnow.dcd;
		icount.rx	= cnow.rx;
		icount.tx	= cnow.tx;
		icount.frame	= cnow.frame;
		icount.overrun	= cnow.overrun;
		icount.parity	= cnow.parity;
		icount.brk	= cnow.brk;
		icount.buf_overrun = cnow.buf_overrun;

		if (copy_to_user((void __user *)arg, &icount, sizeof(icount)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
#undef tiny_ioctl

/* the real tiny_ioctl function.  The above is done to get the small functions in the book */
static int tiny_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	switch (cmd) {
	case TIOCGSERIAL:
		return tiny_ioctl_tiocgserial(tty, cmd, arg);
	case TIOCMIWAIT:
		return tiny_ioctl_tiocmiwait(tty, cmd, arg);
	case TIOCGICOUNT:
		return tiny_ioctl_tiocgicount(tty, cmd, arg);
	}

	return -ENOIOCTLCMD;
}


static const struct tty_operations serial_ops = {
	.open = tiny_open,
	.close = tiny_close,
	.write = tiny_write,
	.write_room = tiny_write_room,
	.set_termios = tiny_set_termios,
	.proc_show = tiny_proc_show,
	.tiocmget = tiny_tiocmget,
	.tiocmset = tiny_tiocmset,
	.ioctl = tiny_ioctl,
};

static struct tty_driver *tiny_tty_driver;

static int __init tiny_init(void)
{
	int retval;
	unsigned i;

	/* allocate the tty driver */
	//tiny_tty_driver = alloc_tty_driver(TINY_TTY_MINORS);

	tiny_tty_driver = tty_alloc_driver( VIRTUALBOT_MAX_TTY_MINORS, \
		TTY_DRIVER_REAL_RAW );

	if (!tiny_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	tiny_tty_driver->owner = THIS_MODULE;
	tiny_tty_driver->driver_name = VIRTUALBOT_DRIVER_NAME;
	tiny_tty_driver->name = "ttyVB";
	tiny_tty_driver->major = VIRTUALBOT_TTY_MAJOR,
	tiny_tty_driver->minor_start = 0,
	tiny_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	tiny_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	tiny_tty_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	tiny_tty_driver->init_termios = tty_std_termios;
	tiny_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;

	//tiny_tty_driver->init_termios = tty_std_termios;
	//tiny_tty_driver->init_termios.c_iflag = 0;
	//tiny_tty_driver->init_termios.c_oflag = 0;
	//tiny_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	//tiny_tty_driver->init_termios.c_lflag = 0;
	//tiny_tty_driver->init_termios.c_ispeed = 38400;
	//tiny_tty_driver->init_termios.c_ospeed = 38400;

	tty_set_operations(tiny_tty_driver, &serial_ops);

	pr_debug("virtualbot: set operations");

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tty_port_init(tiny_tty_port + i);

		pr_debug("virtualbot: port %i initiliazed", i);

		tty_port_register_device(tiny_tty_port + i, tiny_tty_driver, i, NULL);

		pr_debug("virtualbot: port %i linked", i);

		// Starting the MAS signal list with a NULL pointer
		MAS_signals_list_head[ i ] = NULL;
	}	

	/* register the tty driver */
	retval = tty_register_driver(tiny_tty_driver);

	if (retval) {
		pr_err("virtualbot: failed to register tiny tty driver");

		tty_driver_kref_put(tiny_tty_driver);

		return retval;
	}
	pr_debug("virtualbot: driver registered");
	
	pr_info("virtualbot: driver initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );
	return retval;
}

static void __exit tiny_exit(void)
{
	struct tiny_serial *tiny;
	int i;

	struct list_head *pos, *n;

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {
		tty_unregister_device(tiny_tty_driver, i);
		
		pr_debug("virtualbot: device %d unregistered" , i);

		tty_port_destroy(tiny_tty_port + i);

		pr_debug("virtualbot: port %i destroyed", i);
	}

	tty_unregister_driver(tiny_tty_driver);

	tty_driver_kref_put(tiny_tty_driver);

	pr_debug("virtualbot: driver unregistered");

	/* shut down all of the timers and free the memory */
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tiny = tiny_table[i];

		pr_debug("virtualbot: freeing VB %i", i);

		if (tiny) {
			/* close the port */
			while (tiny->open_count)
				do_close(tiny);

			/* shut down our timer and free the memory */
			del_timer(&tiny->timer);
			kfree(tiny);
			tiny_table[i] = NULL;
		}

		if ( MAS_signals_list_head[ i ] != NULL ) {

			// Delete MAS signals list 
			list_for_each_safe(pos, n, MAS_signals_list_head[ i ] ){

				struct MAS_signal *signal = NULL;

				signal = list_entry(pos, struct MAS_signal, MAS_signals_list);
						
				pr_debug("virtualbot: delete %s", signal->data );

				list_del( pos );
			}

			kfree( MAS_signals_list_head[ i ] );
		}
	}
}

module_init(tiny_init);
module_exit(tiny_exit);