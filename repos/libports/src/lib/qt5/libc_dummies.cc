/*
 * \brief  quiet libc dummy functions to reduce the log noise
 * \author Christian Prochaska
 * \date   2014-04-16
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" void _sigprocmask() { }
extern "C" void sigprocmask() { }
