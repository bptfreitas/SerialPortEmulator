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

struct MAS_signal {

	char data[ VIRTUALBOT_MAX_SIGNAL_LEN + 2 ];

    struct list_head MAS_signals_list;

};

static int MAS_signals_count = 0;


struct virtualbot_serial {
	struct tty_struct	*tty;		/* pointer to the tty for this device */
	int			open_count;	/* number of times this port has been opened */
	struct mutex	mutex;		/* locks this structure */

	struct timer_list	timer;

	int created; 

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;

	wait_queue_head_t	wait;
	
	struct async_icount	icount;

	/* Circular buffer to discard the return chars on writes */
	char cbuffer[ IGNORE_CHAR_CBUFFER_SIZE ];
	int head, tail;
};


/**
 *  Struct to represent the VirtualBot Commander
 */
struct vb_comm_serial {

	struct tty_struct	*tty;		/* pointer to the tty for this device */
	int			open_count;	/* number of times this port has been opened */
	struct mutex	mutex;		/* locks this structure */

	struct timer_list	timer;

	int created; 

	/* for tiocmget and tiocmset functions */
	int			msr;		/* MSR shadow */
	int			mcr;		/* MCR shadow */

	/* for ioctl fun */
	struct serial_struct	serial;

	wait_queue_head_t	wait;
	
	struct async_icount	icount;	


	// The 'ack' char buffer 
	char cbuffer[ IGNORE_CHAR_CBUFFER_SIZE ];
	int head, tail;	

};


/** 
 * This mutex locks the global ports of VirtualBot and its Commander
*/
static struct mutex virtualbot_global_port_lock[ VIRTUALBOT_MAX_TTY_MINORS ];

/** 
 * VirtualBot MAS agent structres
 * */
static struct list_head *MAS_signals_list_head[ VIRTUALBOT_MAX_TTY_MINORS ];

static struct virtualbot_serial *virtualbot_table[ VIRTUALBOT_MAX_TTY_MINORS ];	/* initially all NULL */

static struct tty_port virtualbot_tty_port[ 2 * VIRTUALBOT_MAX_TTY_MINORS ];

/** 
 * The VirtualBot Commander structures
*/
static struct vb_comm_serial *vb_comm_table[ VIRTUALBOT_MAX_TTY_MINORS ];	/* initially all NULL */

static struct tty_port vb_comm_tty_port[ VIRTUALBOT_MAX_TTY_MINORS ];

static void tiny_timer(struct timer_list *t)
{
	struct virtualbot_serial *tiny = from_timer(tiny, t, timer);

	struct tty_struct *tty;
	struct tty_port *port;

	int i;

	char data[1] = {TINY_DATA_CHARACTER};

	int data_size = 1;

	if (!tiny){
		pr_warn("virtualbot: tty not set");
		return;
	}

	tty = tiny->tty;
	port = tty->port;

	if (!port){
		pr_warn("virtualbot: port not set on timer setup");
		return;
	}

	/* send the data to the tty layer for users to read.  This doesn't
	 * actually push the data through unless tty->low_latency is set */
	for (i = 0; i < data_size; i++) {

		pr_debug("virtualbot: flip buffer write");

		if ( !tty_buffer_request_room(port, 1) )

			tty_flip_buffer_push(port);

		tty_insert_flip_char(port, data[i], TTY_NORMAL);
	}

	tty_flip_buffer_push(port);

	/* resubmit the timer again */
	//tiny->timer.expires = jiffies + DELAY_TIME;
	//add_timer(&tiny->timer);

	// timer_setup(&tiny->timer, tiny_timer, 0);

	mod_timer(&tiny->timer, jiffies + msecs_to_jiffies(2000));
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

	if (virtualbot == NULL) {
		/* first time accessing this device, let's create it */
		virtualbot = kmalloc(sizeof(*virtualbot), GFP_KERNEL);
		if (!virtualbot)
			return -ENOMEM;

		//mutex_init(&virtualbot_global_port_lock[ index ]);
		virtualbot->open_count = 0;

		virtualbot_table[index] = virtualbot;

		virtualbot_table[index]->head = 0;
		virtualbot_table[index]->tail = 0;

		mutex_lock( &( virtualbot_global_port_lock[ index ] ) );

		/* 
			MAS structure initialization. 
			It will be initialized the first time the driver is openned
			and stay alive on the driver until it is unregistered with 
			modprobe -r
			
			For this reason, there is a hard limit on the number of signals stored
			to avoid infinite memory allocation inside the kernel
		*/
		if ( MAS_signals_list_head[ tty->index ] == NULL ){

			MAS_signals_list_head[ tty->index ] = kmalloc( sizeof( struct list_head ), GFP_KERNEL );

			INIT_LIST_HEAD( MAS_signals_list_head[ tty->index ] );
		}


		/* create our timer and submit it */
		//timer_setup(&virtualbot->timer, virtualbot_timer, 0);

		//mod_timer(&virtualbot->timer, jiffies + msecs_to_jiffies(2000));		

		//virtualbot->timer.expires = jiffies + DELAY_TIME;

		//add_timer(&virtualbot->timer);

	} else {


		mutex_lock( &virtualbot_global_port_lock[ index ] );
	}	

	/* save our structure within the tty structure */
	tty->driver_data = virtualbot;
	virtualbot->tty = tty;

	++virtualbot->open_count;
	if (virtualbot->open_count == 1 ) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}

	mutex_unlock(&virtualbot_global_port_lock[ index ]);

	pr_debug("virtualbot: open port %d finished", index);

	return 0;
}

static void do_close(struct virtualbot_serial *virtualbot)
{
	int index = virtualbot->tty->index;

	// pr_debug("virtualbot: do_close port %d", index);

	mutex_lock(&virtualbot_global_port_lock[ index ]);

	if (!virtualbot->open_count) {
		/* port was never opened */
		goto exit;
	}

	--virtualbot->open_count;
	if (virtualbot->open_count <= 0) {
		/* The port is being closed by the last user. */
		/* Do any hardware specific stuff here */

		/* shut down our timer */
		// del_timer(&virtualbot->timer);		
	}

	// Force push the vb_comm buffer
	tty_flip_buffer_push( & vb_comm_tty_port [ index ] );

exit:
	// pr_debug("virtualbot: do_close port %d finished", index);
	mutex_unlock(&virtualbot_global_port_lock[ index ]);
}

static void virtualbot_close(struct tty_struct *tty, struct file *file)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;

	pr_debug("virtualbot: close port %d", tty->index);

	if (virtualbot){
		pr_debug("virtualbot: do_close port %d", tty->index);
		do_close(virtualbot);
		pr_debug("virtualbot: do_close port %d finished", tty->index);
	}

	pr_debug("virtualbot: close port %d finished", tty->index);
}

static int virtualbot_write(struct tty_struct *tty,
		      const unsigned char *buffer, int count)
{
	struct virtualbot_serial *virtualbot = tty->driver_data;
	int i, buffer_len, index = tty->index;
	int retval = -EINVAL;

	struct vb_comm_serial *vb_comm;
	struct tty_struct *vb_comm_tty;
	struct tty_port *vb_comm_port;

	struct MAS_signal *new_MAS_signal;	

	if (!virtualbot)
		return -ENODEV;

	mutex_lock(&virtualbot_global_port_lock[ index ]);
	
	vb_comm = vb_comm_table[ index ];
	if (!vb_comm){
		pr_err("virtualbot: vb_comm not set!");
		goto exit;
	}

	vb_comm_tty = vb_comm->tty;
	if (!vb_comm_tty){
		pr_err("virtualbot: vb_comm tty not set!");
		goto exit;
	}

	vb_comm_port = vb_comm_tty->port;
	if (!vb_comm_port){
		pr_err("virtualbot: vb_comm port not set!");
		goto exit;
	}

	if (!virtualbot->open_count)
		/* port was not opened */
		goto exit;


	// Check if the 'ack' char buffer has something 
	if ( CIRC_CNT(vb_comm->head, vb_comm->tail, IGNORE_CHAR_CBUFFER_SIZE) >= 1 ){

		// Consume the 'ack' buffer
		if ( count == 1 && vb_comm->cbuffer[ vb_comm->tail ] == buffer[0] ){
			pr_debug("vb-comm: consuming 'ack' buffer[%d]", vb_comm->tail);

			vb_comm->tail ++ ;
			retval = 1;

			goto exit;
		}
	}


	/* 
		Discarding the carriage return and new line the tty core is sending
		
		No idea how to avoid this yet :(
	*/
	if ( count == 2 && buffer[0] == '\r' && buffer[1]=='\n' ){
		retval = 2;

		pr_debug("virtualbot: %s - sending end of line (\\r\\n)", __func__);

		tty_insert_flip_char( vb_comm_port, 
			'\r',
			TTY_NORMAL);

		tty_insert_flip_char( vb_comm_port, 
			'\n',
			TTY_NORMAL);	

		tty_flip_buffer_push(vb_comm_port);

		goto exit;
	}


	if (count == 1 && buffer[0] == '\n '){
		retval = 1 ;

		pr_debug("virtualbot: %s - sending end of line (\\n)", __func__);

		tty_insert_flip_char( vb_comm_port, 
			'\n',
			TTY_NORMAL);	

		tty_flip_buffer_push(vb_comm_port);

		goto exit;
	}

	/* fake sending the data out a hardware port by
	 * writing it to the kernel debug log.
	 */

	/* 
		TODO: race condition here!
		Hard limit on the number of stored signals on VIRTUALBOT_TOTAL_SIGNALS
	*/
#ifdef NOT_YET
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
		}

		new_MAS_signal = kmalloc(sizeof(struct MAS_signal), GFP_KERNEL);

		strcpy( new_MAS_signal->data, 
			buffer
			);

		list_add( &new_MAS_signal->MAS_signals_list , 
			MAS_signals_list_head[ index ] ) ;

		pr_info("\n");
	} else {
		// Maximum number of signals stored achieved - warn kernel then exit
		pr_warn("virtualbot: maximum number of stored signals achieved");
	}
#endif

	pr_debug("virtualbot: %s - writing %d length of data", __func__, count);	

	for ( i = 0; i < count; i++ ){
		pr_debug("%02x ", buffer[i]);

		tty_insert_flip_char( vb_comm_port, 
			buffer[i],
			TTY_NORMAL);

		// Add the char to the 'ignore list' for the vb-comm device
		virtualbot->cbuffer[ virtualbot->head % IGNORE_CHAR_CBUFFER_SIZE ] = buffer[ i ];
		virtualbot->head ++ ;
	}

	tty_flip_buffer_push(vb_comm_port);

	retval = count;

exit:
	mutex_unlock(&virtualbot_global_port_lock[ index ]);
	return retval;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)) 
static int virtualbot_write_room(struct tty_struct *tty)
#else
static unsigned int virtualbot_write_room(struct tty_struct *tty)
#endif
{
	struct virtualbot_serial *virtualbot = tty->driver_data;
	int room = -EINVAL, index = tty->index;

	pr_debug("virtualbot: %s", __func__);

	if (!virtualbot)
		return -ENODEV;

	mutex_lock(&virtualbot_global_port_lock[ index ]);

	if (!virtualbot->open_count) {
		/* port was not opened */
		goto exit;
	}

	/* calculate how much room is left in the device */
	room = 255;

exit:
	mutex_unlock(&virtualbot_global_port_lock[ index ]);
	return room;
}

#define RELEVANT_IFLAG(iflag) ((iflag) & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

static void virtualbot_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
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

	if (vb_comm == NULL) {
		/* first time accessing this device, let's create it */
		vb_comm = kmalloc(sizeof(*vb_comm), GFP_KERNEL);

		if (!vb_comm)
			return -ENOMEM;

		// mutex_init(&vm_comm->mutex);
		vb_comm->open_count = 0;

		vb_comm_table[index] = vb_comm;

		vb_comm_table[index]->head = 0;
		vb_comm_table[index]->tail = 0;

		mutex_lock(&virtualbot_global_port_lock[ index ]);

	} else {
		// Port is already open

		mutex_lock(&virtualbot_global_port_lock[ index ]);
	}	

	/* save our structure within the tty structure */
	tty->driver_data = vb_comm;
	vb_comm->tty = tty;

	++vb_comm->open_count;
	if (vb_comm->open_count == 1 ) {
		/* this is the first time this port is opened */
		/* do any hardware initialization needed here */
	}

	mutex_unlock(&virtualbot_global_port_lock[ index ]);

	pr_debug("vb-comm: open port %d finished", index);

	return 0;
}


static void vb_comm_do_close(struct vb_comm_serial *vb_comm)
{
	int index = vb_comm->tty->index;

	mutex_lock(&virtualbot_global_port_lock[ index ]);

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
	mutex_unlock(&virtualbot_global_port_lock[ index ]);
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
	pr_debug("vb_comm: %s", __func__ );

	return 255;

}

static int vb_comm_write(struct tty_struct *tty,
	const unsigned char *buffer, 
	int count)
{
	pr_debug("vb_comm: %s", __func__ );

	struct vb_comm_serial *vb_comm = tty->driver_data;
	int i, buffer_len, index = tty->index;
	int retval = -EINVAL;

	struct virtualbot_serial *virtualbot;
	struct tty_struct *virtualbot_tty;
	struct tty_port *virtualbot_port;

	if (!vb_comm){
		return -ENODEV;
	}

	mutex_lock( &virtualbot_global_port_lock[ index ] );

	if (!vb_comm->open_count){
		/* port was not opened */
		goto exit;
	}	

	virtualbot = virtualbot_table[ index ];
	if (!virtualbot){
		pr_err("vb_comm: virtualbot table not set!");
		goto exit;
	}

	virtualbot_tty = virtualbot->tty;
	if (!virtualbot_tty){
		pr_err("vb_comm: virtualbot tty not set!");
		goto exit;
	}

	virtualbot_port = virtualbot_tty->port;
	if (!virtualbot_port){
		pr_err("vb_comm: virtualbot port not set!");
		goto exit;
	}

	// Check if the 'ack' char buffer has something
	if ( CIRC_CNT(virtualbot->head, virtualbot->tail, IGNORE_CHAR_CBUFFER_SIZE) >= 1 ){

		// Consume the 'ack' buffer
		if ( count == 1 && virtualbot->cbuffer[ virtualbot->tail ] == buffer[0] ){						
			pr_debug("vb-comm: consuming 'ack' buffer[%d]", virtualbot->tail);

			virtualbot->tail ++ ;
			retval = 1;

			goto exit;
		}
	}
	
	/* 
		Discarding the carriage return and new line the tty core is sending
		
		No idea how to avoid this yet :(
	*/
	if ( count == 2 && buffer[0] == '\r' && buffer[1]=='\n' ){
		retval = 2;

		pr_debug("vb-comm: %s - sending \\CR \\LF (\\r\\n)", __func__);

		tty_insert_flip_char( virtualbot_port, 
			'\r',
			TTY_NORMAL);

		tty_insert_flip_char( virtualbot_port, 
			'\n',
			TTY_NORMAL);	

		tty_flip_buffer_push( virtualbot_port );

		goto exit;
	}


	if (count == 1 && buffer[0] == '\n'){
		retval = 1 ;

		pr_debug("vb-comm: %s - sending end of line (\\n)", __func__);

		tty_insert_flip_char( virtualbot_port,
			'\n',
			TTY_NORMAL);	

		tty_flip_buffer_push( virtualbot_port );

		goto exit;
	}	


	pr_debug("vb-comm: %s - writing %d length of data", __func__, count);	

	for ( i = 0; i < count; i++ ){
		pr_debug("%02x ", buffer[i]);

		tty_insert_flip_char( virtualbot_port, 
			buffer[i],
			TTY_NORMAL);

		vb_comm->cbuffer[ vb_comm->head ] = buffer[ i ] % IGNORE_CHAR_CBUFFER_SIZE;
		vb_comm->head++;
	}

	tty_flip_buffer_push( virtualbot_port );

	retval = count;

exit:
	mutex_unlock(&virtualbot_global_port_lock[ index ]);
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
	virtualbot_tty_driver->driver_name = "virtualbot_tty";
	virtualbot_tty_driver->name = "ttyVB";
	virtualbot_tty_driver->major = VIRTUALBOT_TTY_MAJOR,
	virtualbot_tty_driver->minor_start = 0,
	virtualbot_tty_driver->type = TTY_DRIVER_TYPE_SERIAL,
	virtualbot_tty_driver->subtype = SERIAL_TYPE_NORMAL,
	virtualbot_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV,
	virtualbot_tty_driver->init_termios = tty_std_termios;
	virtualbot_tty_driver->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	virtualbot_tty_driver->init_termios.c_iflag = 0;
	virtualbot_tty_driver->init_termios.c_oflag = 0;	

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

		// Starting the MAS signal list with a NULL pointer
		MAS_signals_list_head[ i ] = NULL;
		virtualbot_table[ i ] = NULL;
	}

	/* register the tty driver */
	retval = tty_register_driver(virtualbot_tty_driver);

	if (retval) {
		pr_err("virtualbot: failed to register virtualbot tty driver");

		tty_driver_kref_put(virtualbot_tty_driver);

		return retval;
	}
	
	pr_info("virtualbot: driver initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

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

	pr_info("vb-comm: driver initialized (" DRIVER_DESC " " DRIVER_VERSION  ")" );

	return retval;
}

static void __exit virtualbot_exit(void)
{
	struct virtualbot_serial *virtualbot;
	struct vb_comm_serial *vb_comm;
	int i;

	struct list_head *pos, *n;

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
			kfree(virtualbot);
			virtualbot_table[i] = NULL;
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
