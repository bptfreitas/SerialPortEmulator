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

#include <linux/circ_buf.h>
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


struct virtualbot_serial {
	struct tty_struct	*tty;		/* pointer to the tty for this device */

	unsigned long index;

	int	open_count;	/* number of times this port has been opened */

	struct timer_list timer;

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;

	wait_queue_head_t	wait;
	
	struct async_icount	icount;

	/* Circular buffer to discard the return chars on writes */
	struct circ_buf recv_buffer;
	
};


/**
 *  Struct to represent the VirtualBot Commander
 */
struct vb_comm_serial {

	struct tty_struct	*tty;		/* pointer to the tty for this device */
	int			open_count;	/* number of times this port has been opened */

	/* Used to simulate serial port data rate */
	struct timer_list timer;

	int created; 

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;

	wait_queue_head_t	wait;
	
	struct async_icount	icount;	

};


/** 
 * This mutex locks the global ports of VirtualBot and its Commander
*/
static struct mutex virtualbot_global_port_lock[ VIRTUALBOT_MAX_TTY_MINORS ];

/** 
 * This mutex locks the vb_comm_serial structure
*/
static struct mutex virtualbot_lock[ VIRTUALBOT_MAX_TTY_MINORS ];

static struct virtualbot_serial *virtualbot_table[ VIRTUALBOT_MAX_TTY_MINORS ];	/* initially all NULL */

static struct tty_port virtualbot_tty_port[ 2 * VIRTUALBOT_MAX_TTY_MINORS ];

/** 
 * The VirtualBot Commander structures
*/
static struct mutex vb_comm_lock[ VIRTUALBOT_MAX_TTY_MINORS ];

static struct vb_comm_serial *vb_comm_table[ VIRTUALBOT_MAX_TTY_MINORS ];	/* initially all NULL */

static struct tty_port vb_comm_tty_port[ VIRTUALBOT_MAX_TTY_MINORS ];


static void virtualbot_timer(struct timer_list *t)
{
	unsigned long virtualbot_index;

	struct virtualbot_serial *virtualbot_dev;

	struct tty_struct *virtualbot_tty;

	struct vb_comm_serial *vb_comm_dev;

	virtualbot_dev = container_of(t, struct virtualbot_serial, timer);	

	virtualbot_tty = virtualbot_dev->tty;	

	if (virtualbot_tty != NULL){
		// virtualbot is allocated
		virtualbot_index = virtualbot_dev->index;

		pr_debug("virtualbot: timer index = %lu", 
			virtualbot_index);

		vb_comm_dev = vb_comm_table[ virtualbot_index ];

		if (vb_comm_dev != NULL){
			// vb_comm_dev is ready	
			
		} else {
			pr_warn( "virtualbot: vb_comm device for tty %lu not set!", 
				virtualbot_index );
		}

	} else {
		pr_err("virtualbot: virtualbot_tty not set!");
	}

	/* resubmit the timer again */		
	timer_setup(t, virtualbot_timer, 0);

	t->expires = jiffies + DELAY_TIME;

	add_timer(t);

	//mod_timer(&tiny->timer, jiffies + msecs_to_jiffies(2000));
}

static int virtualbot_open(struct tty_struct *tty, struct file *file)
{
	struct virtualbot_serial *virtualbot;
	int index;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	index = tty->index;
	virtualbot = virtualbot_table[index];

	pr_debug("virtualbot: open port %d", index);

	mutex_lock( &virtualbot_lock[ index ] );

	if (virtualbot == NULL) {
		/* first time accessing this device, let's create it */
		virtualbot = kmalloc(sizeof(*virtualbot), GFP_KERNEL);

		if (!virtualbot)
			return -ENOMEM;

		virtualbot->index = index;
			
		virtualbot->open_count = 0;

		virtualbot_table[index] = virtualbot;

		/**
		 *  Allocating receive buffer , 4 KiB default size
		 * */
		virtualbot_table[index]->recv_buffer.head = 0;
		virtualbot_table[index]->recv_buffer.tail = 0;

		virtualbot_table[index]->recv_buffer.buf = kmalloc( 
			sizeof(char) * 4096,
			GFP_KERNEL );

		// pointer to the tty struct
		virtualbot_table[index]->tty = tty;

		/* create our timer and submit it */
		timer_setup(&virtualbot->timer, virtualbot_timer, 0);		

		virtualbot->timer.expires = jiffies + DELAY_TIME;

		add_timer(&virtualbot->timer);

	} else {
		// Already set		
	}


	/* save our structure within the tty structure */
	tty->driver_data = virtualbot;
	virtualbot->tty = tty;

	++virtualbot->open_count;
	if (virtualbot->open_count == 1 ) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}

	mutex_unlock( &virtualbot_lock[ index ] );	

	pr_debug("virtualbot: open port %d finished", index);

	return 0;
}

static void do_close(struct virtualbot_serial *virtualbot)
{
	int index = virtualbot->tty->index;

	// pr_debug("virtualbot: do_close port %d", index);

	mutex_lock( &virtualbot_lock[ index ] );

	if (!virtualbot->open_count) {
		/* port was never opened */
		goto exit;
	}

	--virtualbot->open_count;
	if (virtualbot->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */


	}

	// Force push the vb_comm buffer
	tty_flip_buffer_push( & vb_comm_tty_port [ index ] );

exit:
	// pr_debug("virtualbot: do_close port %d finished", index);
	mutex_unlock( &( virtualbot_lock[ index ] ) );
}

static void virtualbot_close(struct tty_struct *tty, struct file *file)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	pr_debug("virtualbot: port %d", tty->index);

	if (virtualbot){
		pr_debug("virtualbot: do_close port %d", tty->index);
		do_close(virtualbot);
		pr_debug("virtualbot: do_close port %d finished", tty->index);
	}

	pr_debug("virtualbot: close port %d finished", tty->index);
}

static int virtualbot_write(struct tty_struct *tty,
	const unsigned char *buffer,
	int count)
{	
	int i, index;
	int retval;

	struct vb_comm_serial *vb_comm;
	struct tty_struct *vb_comm_tty;
	struct tty_port *vb_comm_port;
	struct virtualbot_serial *virtualbot;	

	virtualbot = tty->driver_data;

	retval = -EINVAL;

	index = tty->index;

	//struct MAS_signal *new_MAS_signal;	

	if (!virtualbot)
		return -ENODEV;

	mutex_lock(&vb_comm_lock[ index ]);
	
	vb_comm = vb_comm_table[ index ];
	if (vb_comm == NULL){
		pr_err("virtualbot: %s vb_comm not set!", __func__);
		goto exit;
	}

	vb_comm_tty = vb_comm->tty;
	if (vb_comm_tty == NULL ){
		pr_err("virtualbot: %s vb_comm tty not set!", __func__);
		goto exit;
	}

	vb_comm_port = vb_comm_tty->port;
	if (vb_comm_port == NULL){
		pr_err("virtualbot: %s vb_comm port not set!", __func__);
		goto exit;
	}

	if (!virtualbot->open_count){
		/* port was not opened */
		goto exit;
	}

	pr_debug("virtualbot: %s - writing %d length of data", __func__, count);	

	for ( i = 0; i < count; i++ ){
		pr_debug("%02x ", buffer[i]);

		tty_insert_flip_char( vb_comm_port, 
			buffer[i],
			TTY_NORMAL);

	}

	tty_flip_buffer_push(vb_comm_port);

	retval = count;

exit:
	mutex_unlock( &vb_comm_lock[ index ] );
	return retval;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
static int virtualbot_write_room(struct tty_struct *tty)
#else
static unsigned int virtualbot_write_room(struct tty_struct *tty)
#endif
{
	struct virtualbot_serial *virtualbot = tty->driver_data;
	unsigned int room = -EINVAL, 
		index = tty->index;

	pr_debug("virtualbot: %s", __func__);

	if (!virtualbot)
		return -ENODEV;	

	mutex_lock( &virtualbot_lock[ index ] );

	if (!virtualbot->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	// room = 255;

	room = tty_buffer_space_avail( tty->port );

exit:
	mutex_unlock(&virtualbot_lock[ index ]);

	pr_debug("virtualbot: room = %u", room );

	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
//static int virtualbot_write_room(struct tty_struct *tty)
#else
//static unsigned int virtualbot_write_room(struct tty_struct *tty)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6,0, 0)) 
static void virtualbot_set_termios(struct tty_struct *tty, 
	struct ktermios *old_termios)
#else
static void virtualbot_set_termios(struct tty_struct *tty, 
	const struct ktermios *old_termios)
#endif	
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

static int virtualbot_tiocmget(struct tty_struct *tty)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	unsigned int result = 0;
	unsigned int msr = virtualbot->msr;
	unsigned int mcr = virtualbot->mcr;

	result = ((mcr & MCR_DTR)  ? TIOCM_DTR  : 0) |	/* DTR is set */
		((mcr & MCR_RTS)  ? TIOCM_RTS  : 0) |	/* RTS is set */
		((mcr & MCR_LOOP) ? TIOCM_LOOP : 0) |	/* LOOP is set */
		((msr & MSR_CTS)  ? TIOCM_CTS  : 0) |	/* CTS is set */
		((msr & MSR_CD)   ? TIOCM_CAR  : 0) |	/* Carrier detect is set*/
		((msr & MSR_RI)   ? TIOCM_RI   : 0) |	/* Ring Indicator is set */
		((msr & MSR_DSR)  ? TIOCM_DSR  : 0);	/* DSR is set */

	return result;
}

static int virtualbot_tiocmset(struct tty_struct *tty, unsigned int set,
			 unsigned int clear)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;
	unsigned int mcr = virtualbot->mcr;

	if (set & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (set & TIOCM_DTR)
		mcr |= MCR_RTS;

	if (clear & TIOCM_RTS)
		mcr &= ~MCR_RTS;
	if (clear & TIOCM_DTR)
		mcr &= ~MCR_RTS;

	/* set the new MCR value in the device */
	virtualbot->mcr = mcr;
	return 0;
}

static int virtualbot_proc_show(struct seq_file *m, void *v)
{
	struct virtualbot_serial *virtualbot;
	int i;

	seq_printf(m, "virtualbotserinfo:1.0 driver:%s\n", DRIVER_VERSION);
	
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {
		virtualbot = virtualbot_table[i];
		if (virtualbot == NULL)
			continue;

		seq_printf(m, "%d\n", i);
	}

	return 0;
}

#define virtualbot_ioctl virtualbot_ioctl_tiocgserial

static int virtualbot_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	if (cmd == TIOCGSERIAL) {
		struct serial_struct tmp;

		if (!arg)
			return -EFAULT;

		memset(&tmp, 0, sizeof(tmp));

		tmp.type		= virtualbot->serial.type;
		tmp.line		= virtualbot->serial.line;
		tmp.port		= virtualbot->serial.port;
		tmp.irq			= virtualbot->serial.irq;
		tmp.flags		= ASYNC_SKIP_TEST | ASYNC_AUTO_IRQ;
		tmp.xmit_fifo_size	= virtualbot->serial.xmit_fifo_size;
		tmp.baud_base		= virtualbot->serial.baud_base;
		tmp.close_delay		= 5*HZ;
		tmp.closing_wait	= 30*HZ;
		tmp.custom_divisor	= virtualbot->serial.custom_divisor;
		tmp.hub6		= virtualbot->serial.hub6;
		tmp.io_type		= virtualbot->serial.io_type;

		if (copy_to_user((void __user *)arg, &tmp, sizeof(struct serial_struct)))
			return -EFAULT;
		return 0;
	}
	return -ENOIOCTLCMD;
}
#undef virtualbot_ioctl

#define virtualbot_ioctl virtualbot_ioctl_tiocmiwait
static int virtualbot_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	if (cmd == TIOCMIWAIT) {
		DECLARE_WAITQUEUE(wait, current);
		struct async_icount cnow;
		struct async_icount cprev;

		cprev = virtualbot->icount;
		while (1) {
			add_wait_queue(&virtualbot->wait, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			remove_wait_queue(&virtualbot->wait, &wait);

			/* see if a signal woke us up */
			if (signal_pending(current))
				return -ERESTARTSYS;

			cnow = virtualbot->icount;
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
#undef virtualbot_ioctl

#define virtualbot_ioctl virtualbot_ioctl_tiocgicount
static int virtualbot_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	if (cmd == TIOCGICOUNT) {
		struct async_icount cnow = virtualbot->icount;
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
#undef virtualbot_ioctl

/* the real virtualbot_ioctl function.  The above is done to get the small functions in the book */
static int virtualbot_ioctl(struct tty_struct *tty, unsigned int cmd,
		      unsigned long arg)
{
	switch (cmd) {
	case TIOCGSERIAL:
		return virtualbot_ioctl_tiocgserial(tty, cmd, arg);
	case TIOCMIWAIT:
		return virtualbot_ioctl_tiocmiwait(tty, cmd, arg);
	case TIOCGICOUNT:
		return virtualbot_ioctl_tiocgicount(tty, cmd, arg);
	}

	return -ENOIOCTLCMD;
}


static const struct tty_operations virtualbot_serial_ops = {
	.open = virtualbot_open,
	.close = virtualbot_close,
	.write = virtualbot_write,
	.write_room = virtualbot_write_room,
	.set_termios = virtualbot_set_termios,
	.proc_show = virtualbot_proc_show,
	.tiocmget = virtualbot_tiocmget,
	.tiocmset = virtualbot_tiocmset,
	.ioctl = virtualbot_ioctl,
};


static int vb_comm_open(struct tty_struct *tty, struct file *file)
{
	struct vb_comm_serial *vb_comm;
	int index;

	/* initialize the pointer in case something fails */
	tty->driver_data = NULL;

	/* get the serial object associated with this tty pointer */
	index = tty->index;
	vb_comm = vb_comm_table[ index ];

	pr_debug("vb_comm: open port %d", index );

	mutex_lock(&vb_comm_lock[ index ]);

	if (vb_comm == NULL) {
		/* first time accessing this device, let's create it */
		vb_comm = kmalloc(sizeof(*vb_comm), GFP_KERNEL);

		if (!vb_comm)
			return -ENOMEM;

		// mutex_init(&vm_comm->mutex);
		vb_comm->open_count = 0;

		vb_comm_table[index] = vb_comm;

	} else {
		// Port is already open

		//mutex_lock(&virtualbot_global_port_lock[ index ]);
	}	

	/* save our structure within the tty structure */
	tty->driver_data = vb_comm;
	vb_comm->tty = tty;

	++vb_comm->open_count;
	if (vb_comm->open_count == 1 ) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}

	mutex_unlock(&vb_comm_lock[ index ]);

	pr_debug("vb-comm: open port %d finished", index);

	return 0;
}


static void vb_comm_do_close(struct vb_comm_serial *vb_comm)
{
	int index = vb_comm->tty->index;

	mutex_lock( &vb_comm_lock[ index ] );

	if (!vb_comm->open_count) {
		/* port was never opened */
		goto exit;
	}

	--vb_comm->open_count;
	if (vb_comm->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */

		/* shut down our timer */
		// del_timer(&virtualbot->timer);
	}
exit:
	//pr_debug("vb_comm: do_close finished");
	mutex_unlock( &vb_comm_lock[ index ] );
}

static void vb_comm_close(struct tty_struct *tty, struct file *file)
{
	struct vb_comm_serial *vb_comm = tty->driver_data;

	pr_debug("vb_comm: close port %d", tty->index);

	if (vb_comm){
		pr_debug("vb_comm: do_close port %d", tty->index);
		vb_comm_do_close(vb_comm);
		pr_debug("vb_comm: do_close port %d finished", tty->index);
	}

	pr_debug("vb_comm: close port %d finished", tty->index);
}


#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
static int vb_comm_write_room(struct tty_struct *tty)
#else
static unsigned int vb_comm_write_room(struct tty_struct *tty)
#endif
{

	unsigned int room;

	struct tty_port *port;

	pr_debug("vb_comm: %s", __func__ );	
	
	room = -ENODEV;

	port = tty->port;

	if (! port ){
		goto exit;
	}

	room = tty_buffer_space_avail( tty->port );

exit:

	pr_debug("vb_comm: room = %u", room );

	return room;

}

static int vb_comm_write(struct tty_struct *tty,
	const unsigned char *buffer, 
	int count)
{	
	int i, index, retval;

	struct vb_comm_serial *vb_comm;
	struct virtualbot_serial *virtualbot;
	struct tty_struct *virtualbot_tty;
	struct tty_port *virtualbot_port;

	pr_debug("vb_comm: %s", __func__ );	

	index = tty->index;
	retval = -EINVAL;
	vb_comm = tty->driver_data;

	if (!vb_comm){
		return -ENODEV;
	}

	if (!vb_comm->open_count){
		/* port was not opened */
		goto exit;
	}	

	mutex_lock( &virtualbot_lock[ index ] );

	virtualbot = virtualbot_table[ index ];
	if (virtualbot == NULL){
		pr_err("vb_comm: virtualbot table not set!");
		goto exit;
	}

	virtualbot_tty = virtualbot->tty;
	if (virtualbot_tty == NULL){
		pr_err("vb_comm: virtualbot tty not set!");
		goto exit;
	}

	virtualbot_port = virtualbot_tty->port;
	if (virtualbot_port == NULL){
		pr_err("vb_comm: virtualbot port not set!");
		goto exit;
	}

	pr_debug("vb-comm: %s - writing %d length of data", __func__, count);	

	for ( i = 0; i < count; i++ ){
		pr_debug("%02x ", buffer[i]);

		tty_insert_flip_char( virtualbot_port, 
			buffer[i],
			TTY_NORMAL);
	}

	tty_flip_buffer_push( virtualbot_port );

	retval = count;

exit:
	mutex_unlock( &virtualbot_lock[ index ] );

	return retval;
}


static const struct tty_operations vb_comm_serial_ops = {
	.open = vb_comm_open,
	.close = vb_comm_close,
	.write = vb_comm_write,
	.write_room = vb_comm_write_room,
	//.set_termios = virtualbot_set_termios,
	//.proc_show = virtualbot_proc_show,
	//.tiocmget = virtualbot_tiocmget,
	//.tiocmset = virtualbot_tiocmset,
	//.ioctl = virtualbot_ioctl,
};


static struct tty_driver *virtualbot_tty_driver;

static struct tty_driver *vb_comm_tty_driver;

static int __init virtualbot_init(void)
{
	int retval;
	unsigned i;

	/* allocate the tty driver */
	//virtualbot_tty_driver = alloc_tty_driver(virtualbot_TTY_MINORS);

	/*
	 * Initializing the VirtialBot driver
	 * 
	 * This is the main hardware emulation device
	 * 
	*/

	virtualbot_tty_driver = tty_alloc_driver( VIRTUALBOT_MAX_TTY_MINORS, \
		TTY_DRIVER_REAL_RAW );

	if (!virtualbot_tty_driver)
		return -ENOMEM;

	/* initialize the tty driver */
	virtualbot_tty_driver->owner = THIS_MODULE;
	virtualbot_tty_driver->driver_name = VIRTUALBOT_DRIVER_NAME;
	virtualbot_tty_driver->name = VIRTUALBOT_TTY_NAME;
	virtualbot_tty_driver->major = VIRTUALBOT_TTY_MAJOR,
	virtualbot_tty_driver->minor_start = 0,
	virtualbot_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	virtualbot_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	virtualbot_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	virtualbot_tty_driver->init_termios = tty_std_termios;
	virtualbot_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	virtualbot_tty_driver->init_termios.c_iflag = 0;
	virtualbot_tty_driver->init_termios.c_oflag = 0;
	
	// Must be 0 to disable ECHO flag
	virtualbot_tty_driver->init_termios.c_lflag = 0;
	
	//virtualbot_tty_driver->init_termios = tty_std_termios;
	//virtualbot_tty_driver->init_termios.c_cflag = B38400 | CS8 | CREAD;
	//virtualbot_tty_driver->init_termios.c_lflag = 0;
	//virtualbot_tty_driver->init_termios.c_ispeed = 38400;
	//virtualbot_tty_driver->init_termios.c_ospeed = 38400;

	tty_set_operations(virtualbot_tty_driver, 
		&virtualbot_serial_ops);

	pr_debug("virtualbot: set operations");

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tty_port_init( &virtualbot_tty_port[ i ]);
		pr_debug("virtualbot: port %i initiliazed", i);

		tty_port_register_device( &virtualbot_tty_port[ i ], 
			virtualbot_tty_driver, 
			i, 
			NULL);
		pr_debug("virtualbot: port %i linked", i);

		virtualbot_table[ i ] = NULL;
		
		mutex_init( &virtualbot_lock[ i ] );
	}

	/* register the tty driver */
	retval = tty_register_driver(virtualbot_tty_driver);

	if (retval) {
		pr_err("virtualbot: failed to register virtualbot tty driver");

		tty_driver_kref_put(virtualbot_tty_driver);

		return retval;
	}
	
	//pr_info("virtualbot: driver initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

	/*
	 * 
	 * Initializing the VirtualBot Commander
	 * 
	 * This is the main interface between
	 * 
	*/

	vb_comm_tty_driver = tty_alloc_driver( VIRTUALBOT_MAX_TTY_MINORS,
		TTY_DRIVER_REAL_RAW );

	if (!vb_comm_tty_driver){
		retval = -ENOMEM;
	}

	/* initialize the driver */
	vb_comm_tty_driver->owner = THIS_MODULE;
	vb_comm_tty_driver->driver_name = VB_COMM_DRIVER_NAME;
	vb_comm_tty_driver->name = VB_COMM_TTY_NAME;
	vb_comm_tty_driver->major = VB_COMM_TTY_MAJOR,
	vb_comm_tty_driver->minor_start = 0,
	vb_comm_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	vb_comm_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	vb_comm_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	vb_comm_tty_driver->init_termios = tty_std_termios;
	vb_comm_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	vb_comm_tty_driver->init_termios.c_iflag = 0;
	vb_comm_tty_driver->init_termios.c_oflag = 0;	
	
	// Must be 0 to disable ECHO flag
	vb_comm_tty_driver->init_termios.c_lflag = 0;	

	tty_set_operations(vb_comm_tty_driver, 
		&vb_comm_serial_ops);

	pr_debug("vb-comm: set operations");

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		tty_port_init(& vb_comm_tty_port[ i ] );
		pr_debug("vb-comm: port %i initiliazed", i);

		tty_port_register_device( & vb_comm_tty_port[ i ], 
			vb_comm_tty_driver, 
			i, 
			NULL);
		pr_debug("vb-comm: port %i linked", i);

		// Port state is initially NULL
		vb_comm_table[ i ] = NULL;
		
		mutex_init( &vb_comm_lock[ i ] );
	}


	/* register the tty driver */
	retval = tty_register_driver(vb_comm_tty_driver);

	if (retval) {

		pr_err("vb-comm: failed to register vb-comm tty driver");

		tty_driver_kref_put(vb_comm_tty_driver);

		return retval;
	}

	for ( i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++ ){
		mutex_init( &virtualbot_global_port_lock[ i ] );
	}

	pr_info("Serial Port Emulator initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

	return retval;
}

static void __exit virtualbot_exit(void)
{
	struct virtualbot_serial *virtualbot;
	struct vb_comm_serial *vb_comm;
	int i;

	// struct list_head *pos, *n;

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {
		
		tty_unregister_device(virtualbot_tty_driver, i);
		
		pr_debug("virtualbot: device %d unregistered" , i);

		tty_port_destroy(virtualbot_tty_port + i);

		pr_debug("virtualbot: port %i destroyed", i);
	}

	tty_unregister_driver(virtualbot_tty_driver);

	tty_driver_kref_put(virtualbot_tty_driver);

	pr_debug("virtualbot: driver unregistered");

	/* shut down all of the timers and free the memory */
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		virtualbot = virtualbot_table[i];

		pr_debug("virtualbot: freeing VB %i", i);

		if (virtualbot) {
			/* close the port */
			while (virtualbot->open_count)
				do_close(virtualbot);

			/* shut down our timer and free the memory */
			del_timer(&virtualbot->timer);

			/* deallocate receive buffer */
			kfree( virtualbot->recv_buffer.buf );
			
			/* destroy structure mutex */
			mutex_destroy( &virtualbot_lock[ i ] );
			
			kfree(virtualbot);
			virtualbot_table[i] = NULL;
		}

	}

	/**
	 * 
	 * Unregistering The Comm part 
	 * 
	*/

	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; ++i) {

		tty_unregister_device(vb_comm_tty_driver, i);
		
		pr_debug("vb-comm: device %d unregistered" , i);

		tty_port_destroy(vb_comm_tty_port + i);

		pr_debug("vb-comm: port %i destroyed", i);
	}

	tty_unregister_driver(vb_comm_tty_driver);

	tty_driver_kref_put(vb_comm_tty_driver);

	// For future
	for (i = 0; i < VIRTUALBOT_MAX_TTY_MINORS; i++) {

		vb_comm = vb_comm_table[i];

		pr_debug("vb-comm: freeing VB %i", i);

		if (vb_comm) {
			/* close thvirtualbote port */
			while (vb_comm->open_count)
				vb_comm_do_close(vb_comm);
				
			mutex_destroy( &vb_comm_lock[ i ] );

			/* shut down our timer and free the memory */
			//del_timer(&virtualbot->timer);
			kfree(vb_comm);
			vb_comm_table[i] = NULL;
		}
	}

	mutex_destroy( &virtualbot_global_port_lock[ i ] );
}

module_init(virtualbot_init);
module_exit(virtualbot_exit);
