/*
 * \brief  Pager support for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-01-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/ipc_pager.h>
#include <base/printf.h>
#include <base/thread.h>

namespace Fiasco {
#include <l4/sys/ipc.h>
}

using namespace Genode;
using namespace Fiasco;

void Ipc_pager::_parse(unsigned long label) {
	_badge = label & ~0x3;
	_parse_msg_type();
	if (_type == PAGEFAULT || _type == EXCEPTION)
		_parse_pagefault();
	if (_type == PAUSE || _type == EXCEPTION)
		memcpy(&_regs, l4_utcb_exc(), sizeof(l4_exc_regs_t));
}


void Ipc_pager::_parse_pagefault()
{
	if (_tag.is_exception()) {
		_pf_addr = l4_utcb_exc_pfa(l4_utcb_exc());
		_pf_ip   = l4_utcb_exc_pc(l4_utcb_exc());
	} else {
		_pf_addr = l4_utcb_mr()->mr[0];
		_pf_ip   = l4_utcb_mr()->mr[1];
	}
}


void Ipc_pager::_parse_msg_type()
{
	if (_tag.is_exception() && !l4_utcb_exc_is_pf(l4_utcb_exc())) {
		_parse_exception();
		return;
	}

	if (_tag.is_page_fault())
		_type = PAGEFAULT;
	else {
		_type = WAKE_UP;
		_pf_ip = l4_utcb_mr()->mr[1];
	}
}


void Ipc_pager::wait_for_fault()
{
	l4_umword_t label;

	do {
		_tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
		int err = l4_ipc_error(_tag, l4_utcb());
		if (!err) {
			_parse(label);
			return;
		}
		PERR("Ipc error %d in pagefault from %lx", err, label & ~0x3);
	} while (true);
}


void Ipc_pager::reply_and_wait_for_fault()
{
	l4_umword_t label;
	l4_msgtag_t snd_tag = l4_msgtag(0, 0, 1, 0);

	l4_umword_t grant   = _reply_mapping.grant() ? L4_MAP_ITEM_GRANT : 0;
	l4_utcb_mr()->mr[0] = _reply_mapping.dst_addr() | L4_ITEM_MAP | grant;

	/*
	 * XXX Does L4_FPAGE_BUFFERABLE imply L4_FPAGE_UNCACHEABLE?
	 */
	if (_reply_mapping.write_combined())
		l4_utcb_mr()->mr[0] |= L4_FPAGE_BUFFERABLE << 4;

	l4_utcb_mr()->mr[1] = _reply_mapping.fpage().raw;

	_tag = l4_ipc_send_and_wait(_last, l4_utcb(), snd_tag,
	                            &label, L4_IPC_SEND_TIMEOUT_0);
	int err = l4_ipc_error(_tag, l4_utcb());
	if (err) {
		PERR("Ipc error %d in pagefault from %lx", err, label & ~0x3);
		wait_for_fault();
	} else
		_parse(label);
}


void Ipc_pager::acknowledge_wakeup()
{
	l4_cap_idx_t dst = Fiasco::Capability::valid(_last) ? _last : L4_SYSF_REPLY;

	/* answer wakeup call from one of core's region-manager sessions */
	l4_ipc_send(dst, l4_utcb(), l4_msgtag(0, 0, 0, 0), L4_IPC_SEND_TIMEOUT_0);
}


Ipc_pager::Ipc_pager()
: Native_capability((Cap_index*)Fiasco::l4_utcb_tcr()->user[Fiasco::UTCB_TCR_BADGE]),
  _badge(0) { }

