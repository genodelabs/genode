/*
 * \brief  USB modem driver terminal service
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2020-12-02
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <terminal.h>
#include <lx_emul.h>

#include <legacy/lx_emul/extern_c_begin.h>
#include <linux/usb.h>
#include <legacy/lx_emul/extern_c_end.h>

using namespace Terminal;


Session_component::Session_component(Genode::Env &env,
                  Genode::size_t io_buffer_size,
                  usb_class_driver *class_driver)
:
	_io_buffer(env.ram(), env.rm(), io_buffer_size),
	_io_buffer_size(io_buffer_size),
	_class_driver(class_driver)
{
	if (class_driver == nullptr) {
		Genode::error("No class driver for terminal");
		throw Genode::Service_denied();
	}

	Lx::scheduler().schedule();
}

void Session_component::_run_wdm_device(void *args)
{
	Session_component *session = static_cast<Session_component *>(args);

	usb_class_driver *driver = session->_class_driver;

	int err = -1;
	struct file file { };

	if ((err = driver->fops->open(nullptr, &file))) {
		Genode::error("Could not open WDM device: ", err);
		return;
	}

	session->_wdm_device = file.private_data;
	Lx::scheduler().current()->block_and_schedule();
	//XXX: close
}


void Session_component::_run_wdm_write(void *args)
{
	Lx::scheduler().current()->block_and_schedule();

	Session_component *session = static_cast<Session_component *>(args);
	usb_class_driver  *driver = session->_class_driver;

	struct file file { .private_data = session->_wdm_device };

	while (1) {
		ssize_t length = driver->fops->write(&file, session->buffer(),
		                                     session->_data_avail, nullptr);
		if (length < 0) {
			Genode::error("WDM write error: ", length);
		}

		session->_schedule_read();
		Lx::scheduler().current()->block_and_schedule();
	}
}


void Session_component::_run_wdm_read(void *args)
{
	Lx::scheduler().current()->block_and_schedule();
	Session_component *session = static_cast<Session_component *>(args);

	usb_class_driver *driver = session->_class_driver;

	struct file file { .private_data = session->_wdm_device };

	while (1) {
		ssize_t length = driver->fops->read(&file, session->buffer(), 0x1000, nullptr);
		if (length > 0) {
			session->_data_avail = length;
			session->signal_data_avail();
		}
		Lx::scheduler().current()->block_and_schedule();
	}
}
