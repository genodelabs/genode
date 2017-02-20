/*
 * \brief  Echo interface
 * \author Norman Feske
 * \date   2010-01-19
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__ECHO_H_
#define _CORE__INCLUDE__ECHO_H_

/* NOVA includes */
#include <nova/syscalls.h>

class Echo
{
	private:

		int         _ec_sel;  /* execution context */
		int         _pt_sel;  /* portal */
		Nova::Utcb *_utcb;

	public:

		enum {
			ECHO_UTCB_ADDR  = 0xbff00000,
		};

		/**
		 * Constructor
		 *
		 * \param utcb_addr  designated UTCB location for echo EC
		 */
		Echo(Genode::addr_t utcb_addr);

		/**
		 * UTCB of echo execution context
		 */
		Nova::Utcb *utcb() { return _utcb; }

		/**
		 * Capability selector for portal to echo
		 */
		int pt_sel() { return _pt_sel; }
};


/**
 * Get single 'Echo' instance
 */
Echo *echo();

#endif /* _CORE__INCLUDE__ECHO_H_ */
