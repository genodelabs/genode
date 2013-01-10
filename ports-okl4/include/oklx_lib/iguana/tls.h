/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Norman Feske
 * \date   2009-04-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__TLS_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__TLS_H_

/**
 * Initialize thread local storage for the actual thread
 *
 * \param tls_buffer buffer for the actual thread
 */
void __tls_init(void *tls_buffer);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__TLS_H_
