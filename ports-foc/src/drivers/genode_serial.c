/*
 * \brief  Serial and console driver to access Genode's terminal service
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2011-09-16
 *
 *  Based on l4ser.c from TU-Dresden's L4Linux.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#if defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>

#include <genode/terminal.h>

enum { MAX_PORTS = 10 };

static struct genode_uart_port {
	struct uart_port port;
	l4_cap_idx_t     irq_cap;
	int              initialized;
	unsigned         idx;
} genode_serial_port[MAX_PORTS];


static void genode_serial_stop_tx(struct uart_port *port) { }


static void genode_serial_stop_rx(struct uart_port *port) { }


static void genode_serial_enable_ms(struct uart_port *port) { }


static void genode_serial_rx_chars(struct uart_port *port) {
	struct genode_uart_port *l4port   = (struct genode_uart_port *)port;
	struct tty_port         *tty_port = &port->state->port;
	unsigned long            flags;
	unsigned int             flg;
	char                     buf[64];
	unsigned                 cnt, i;

	while (1)  {
		local_irq_save(flags);
		cnt = genode_terminal_readchar(l4port->idx, buf, sizeof(buf));
		local_irq_restore(flags);

		if (!cnt)
			break;

		for (i = 0; i < cnt; i++) {
			port->icount.rx++;
			flg = TTY_NORMAL;
			if (uart_handle_sysrq_char(port, buf[i]))
				continue;

			tty_insert_flip_char(tty_port, buf[i], flg);
		}

		if (cnt < sizeof(buf))
			break;
	}
	tty_flip_buffer_push(tty_port);
	return;
}

static void genode_serial_tx_chars(struct uart_port *port) {
	struct genode_uart_port *l4port = (struct genode_uart_port *)port;
	struct circ_buf         *xmit   = &port->state->xmit;
	unsigned long            flags;
	unsigned                 c;

	if (port->x_char) {
		local_irq_save(flags);
		genode_terminal_writechar(l4port->idx, &port->x_char, sizeof(char));
		local_irq_restore(flags);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	while (!uart_circ_empty(xmit)) {
		c = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		local_irq_save(flags);
		genode_terminal_writechar(l4port->idx, &xmit->buf[xmit->tail], c);
		local_irq_restore(flags);
		xmit->tail = (xmit->tail + c) & (UART_XMIT_SIZE - 1);
		port->icount.tx += c;
	}
}


static void genode_serial_start_tx(struct uart_port *port) {
	genode_serial_tx_chars(port); }


static irqreturn_t genode_serial_int(int irq, void *dev_id) {
	struct uart_port *sport = dev_id;
	genode_serial_rx_chars(sport);
	return IRQ_HANDLED;
}


static unsigned int genode_serial_tx_empty(struct uart_port *port) {
	return TIOCSER_TEMT; }


static unsigned int genode_serial_get_mctrl(struct uart_port *port) {
	return 0; }


static void genode_serial_set_mctrl(struct uart_port *port, unsigned int mctrl) { }


static void genode_serial_break_ctl(struct uart_port *port, int break_state) { }


static int genode_serial_startup(struct uart_port *port) {
	if (port->irq) {
		int retval = request_irq(port->irq, genode_serial_int, 0, "Genode uart", port);
		if (retval)
			return retval;
		genode_serial_rx_chars(port);
	}
	return 0;
}


static void genode_serial_shutdown(struct uart_port *port) {
	if (port->irq)
		free_irq(port->irq, port);
}


static void genode_serial_set_termios(struct uart_port *port, struct ktermios *termios,
                              struct ktermios *old) { }


static const char *genode_serial_type(struct uart_port *port) {
	return port->type == PORT_SA1100 ? "L4" : NULL; }


static int genode_serial_request_port(struct uart_port *port) {
	return 0; }


static void genode_serial_release_port(struct uart_port *port) { }


static void genode_serial_config_port(struct uart_port *port, int flags) {
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_SA1100;
}


static int genode_serial_verify_port(struct uart_port *port, struct serial_struct *ser) {
	return 0; }


static struct uart_ops genode_serial_pops = {
	.tx_empty     = genode_serial_tx_empty,
	.set_mctrl    = genode_serial_set_mctrl,
	.get_mctrl    = genode_serial_get_mctrl,
	.stop_tx      = genode_serial_stop_tx,
	.start_tx     = genode_serial_start_tx,
	.stop_rx      = genode_serial_stop_rx,
	.enable_ms    = genode_serial_enable_ms,
	.break_ctl    = genode_serial_break_ctl,
	.startup      = genode_serial_startup,
	.shutdown     = genode_serial_shutdown,
	.set_termios  = genode_serial_set_termios,
	.type         = genode_serial_type,
	.release_port = genode_serial_release_port,
	.request_port = genode_serial_request_port,
	.config_port  = genode_serial_config_port,
	.verify_port  = genode_serial_verify_port,
};


static int __init genode_serial_init_port(int num) {
	int irq;
	unsigned long flags;

	if (genode_serial_port[num].initialized)
		return 0;
	genode_serial_port[num].initialized = 1;

	local_irq_save(flags);
	genode_serial_port[num].irq_cap = genode_terminal_irq(num);
	local_irq_restore(flags);

	if((irq = l4x_register_irq(genode_serial_port[num].irq_cap)) < 0) {
		return -EIO;
	}

	genode_serial_port[num].port.uartclk   = 3686400;
	genode_serial_port[num].port.ops       = &genode_serial_pops;
	genode_serial_port[num].port.fifosize  = 8;
	genode_serial_port[num].port.line      = num;
	genode_serial_port[num].port.iotype    = UPIO_MEM;
	genode_serial_port[num].port.membase   = (void *)1;
	genode_serial_port[num].port.mapbase   = 1;
	genode_serial_port[num].port.flags     = UPF_BOOT_AUTOCONF;
	genode_serial_port[num].port.irq       = irq;
	genode_serial_port[num].idx            = num;
	return 0;
}


static int __init
genode_console_setup(struct console *co, char *options) {
	struct uart_port *up;

	if (co->index >= 1 + MAX_PORTS)
		co->index = 0;
	up = &genode_serial_port[co->index].port;
	if (!up)
		return -ENODEV;

	return uart_set_options(up, co, 115200, 'n', 8, 'n');
}


/*
 * Interrupts are disabled on entering
 */
static void
genode_console_write(struct console *co, const char *s, unsigned int count) {
	unsigned long flags;
	local_irq_save(flags);
	genode_terminal_writechar(genode_serial_port[co->index].idx, s, count);
	local_irq_restore(flags);
}


static struct uart_driver genode_reg;
static struct console genode_console = {
	.name		= "ttyS",
	.write		= genode_console_write,
	.device		= uart_console_device,
	.setup		= genode_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &genode_reg,
};


static int __init genode_rs_console_init(void)
{
	unsigned long flags;
	local_irq_save(flags);
	if (!genode_terminal_count()) {
		local_irq_restore(flags);
		return -ENODEV;
	}
	local_irq_restore(flags);

	if (genode_serial_init_port(0))
		return -ENODEV;

	register_console(&genode_console);
	return 0;
}
console_initcall(genode_rs_console_init);


static struct uart_driver genode_reg = {
	.owner       = THIS_MODULE,
	.driver_name = "ttyS",
	.dev_name    = "ttyS",
	.major       = 204,
	.minor       = 5,
	.nr          = 0,
	.cons        = &genode_console,
	.state       = 0,
	.tty_driver  = 0,
};


static int __init genode_serial_init(void)
{
	unsigned i;
	unsigned long flags;
	local_irq_save(flags);
	genode_reg.nr = (genode_terminal_count() > MAX_PORTS)
	                ? MAX_PORTS : genode_terminal_count();
	local_irq_restore(flags);

	if (genode_reg.nr == 0)
		return 0;

	if (uart_register_driver(&genode_reg))
		return -ENODEV;

	for (i = 0; i < genode_reg.nr; i++) {
		if (genode_serial_init_port(i))
			return -ENODEV;
		uart_add_one_port(&genode_reg, &genode_serial_port[i].port);
	}
	return 0;
}


static void __exit genode_serial_exit(void)
{
	unsigned i;
	unsigned long flags;

	for (i = 0; i < genode_reg.nr; i++) {
		uart_remove_one_port(&genode_reg, &genode_serial_port[i].port);
		local_irq_save(flags);
		genode_terminal_stop(i);
		local_irq_restore(flags);
	}
	if (genode_reg.nr)
		uart_unregister_driver(&genode_reg);

}


module_init(genode_serial_init);
module_exit(genode_serial_exit);

MODULE_AUTHOR("Stefan Kalkowski <stefan.kalkowski@genode-labs.com");
MODULE_DESCRIPTION("Genode serial driver");
MODULE_LICENSE("GPL");
