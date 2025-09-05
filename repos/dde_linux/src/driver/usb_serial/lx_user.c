/*
 * \brief  Post kernel activity
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2025-09-02
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/sched/task.h>
#include <linux/tty.h>

#include <lx_emul/usb_client.h>
#include <lx_user/init.h>
#include <lx_user/io.h>

#include "lx_emul.h"


void read_fn(struct genode_terminal_read_ctx *, struct genode_const_buffer buffer)
{
	lx_emul_usb_serial_write(buffer);
}


static void process_output_bytes(struct genode_terminal *terminal)
{
	genode_terminal_read(terminal, read_fn, NULL);
}


static void process_input_bytes(struct genode_terminal *terminal)
{
	char buf[1000];

	struct genode_buffer read_buffer = { .start     = buf,
	                                     .num_bytes = sizeof(buf) };

	unsigned long num_bytes = lx_emul_usb_serial_read(read_buffer);

	struct genode_const_buffer write_buffer = { .start     = buf,
	                                            .num_bytes = num_bytes };

	unsigned long written = genode_terminal_write(terminal, write_buffer);

	if (written != num_bytes)
		printk("truncated terminal write - %ld of %ld bytes written", written, num_bytes);
}


static int user_task_function(void *arg)
{
	tty_init();
	n_tty_init();

	lx_emul_usb_serial_wait_for_device();

	struct genode_terminal_args args = { .label = "ttyUSB0" };

	struct genode_terminal *terminal = genode_terminal_create(&args);

	for (;;) {
		process_output_bytes(terminal);
		process_input_bytes(terminal);

		/* block until lx_emul_task_unblock */
		lx_emul_task_schedule(true);
	}

	return 0;
}


static struct task_struct *user_task;

void lx_user_init(void)
{
	lx_emul_usb_client_init();

	int pid = kernel_thread(user_task_function, NULL, "user_task", CLONE_FS | CLONE_FILES);

	user_task = find_task_by_pid_ns(pid, NULL);
}


void lx_user_handle_io(void)
{
	lx_emul_usb_client_ticker();

	if (user_task) lx_emul_task_unblock(user_task);
}
