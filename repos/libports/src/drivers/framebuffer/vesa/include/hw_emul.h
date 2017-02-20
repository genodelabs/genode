/*
 * \brief  Hardware emulation interface
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2010-06-01
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _HW_EMUL_H_
#define _HW_EMUL_H_


namespace Genode { struct Env; }

void hw_emul_init(Genode::Env &);

/**
 * Handle port-read access
 *
 * \return true if the port access referred to emulated hardware
 */
template <typename T>
bool hw_emul_handle_port_read(unsigned short port, T *val);

/**
 * Handle port-write access
 *
 * \return true if the port access referred to emulated hardware
 */
template <typename T>
bool hw_emul_handle_port_write(unsigned short port, T val);

#endif /* _HW_EMUL_H_ */
