/*
 * \brief  Pager support for Fiasco.OC
 * \author Stefan Kalkowski
 * \date   2011-01-11
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>

/* core includes */
#include <ipc_pager.h>

/* base-internal includes */
#include <base/internal/native_utcb.h>
#include <base/internal/cap_map.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

using namespace Genode;
using namespace Foc;


/*
 * There is no preparation needed because the entire physical memory is known
 * to be mapped within core.
 */
void Mapping::prepare_map_operation() const { }


void Ipc_pager::_parse(unsigned long label)
{
	_badge = label & ~0x3;
	_parse_msg_type();

	if (_type == PAGEFAULT || _type == EXCEPTION)
		_parse_pagefault();

	if (_type == PAUSE || _type == EXCEPTION)
		_regs = *l4_utcb_exc();
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


/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
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
		error("Ipc error ", err, " in pagefault from ", Hex(label & ~0x3));
	} while (true);
}


void Ipc_pager::reply_and_wait_for_fault()
{
	l4_umword_t label;
	l4_msgtag_t snd_tag = l4_msgtag(0, 0, 1, 0);

	l4_utcb_mr()->mr[0] = _reply_mapping.dst_addr | L4_ITEM_MAP;

	l4_fpage_cacheability_opt_t
		cacheability = _reply_mapping.cached ? L4_FPAGE_CACHEABLE
		                                     : L4_FPAGE_UNCACHEABLE;

	if (_reply_mapping.write_combined)
		cacheability= L4_FPAGE_BUFFERABLE;

	l4_utcb_mr()->mr[0] |= cacheability << 4;

	auto rights = [] (bool writeable, bool executable)
	{
		if ( writeable &&  executable) return L4_FPAGE_RWX;
		if ( writeable && !executable) return L4_FPAGE_RW;
		if (!writeable && !executable) return L4_FPAGE_RO;
		return L4_FPAGE_RX;
	};

	l4_fpage_t const fpage = l4_fpage(_reply_mapping.src_addr,
	                                  _reply_mapping.size_log2,
	                                  rights(_reply_mapping.writeable,
	                                         _reply_mapping.executable));

	l4_utcb_mr()->mr[1] = fpage.raw;

	_tag = l4_ipc_send_and_wait(_last.kcap, l4_utcb(), snd_tag,
	                            &label, L4_IPC_SEND_TIMEOUT_0);
	int const err = l4_ipc_error(_tag, l4_utcb());
	if (err) {
		error("Ipc error ", err, " in pagefault from ", Hex(label & ~0x3));
		wait_for_fault();
	} else
		_parse(label);
}


void Ipc_pager::acknowledge_wakeup()
{
	l4_cap_idx_t dst = Foc::Capability::valid(_last.kcap)
	                 ? _last.kcap : (l4_cap_idx_t)L4_SYSF_REPLY;

	/* answer wakeup call from one of core's region-manager sessions */
	l4_ipc_send(dst, l4_utcb(), l4_msgtag(0, 0, 0, 0), L4_IPC_SEND_TIMEOUT_0);
}


void Ipc_pager::acknowledge_exception()
{
	*l4_utcb_exc() = _regs;
	l4_cap_idx_t dst = Foc::Capability::valid(_last.kcap)
	                 ? _last.kcap : (l4_cap_idx_t)L4_SYSF_REPLY;
	Foc::l4_msgtag_t const msg_tag =
		l4_ipc_send(dst, l4_utcb(),
		            l4_msgtag(0, L4_UTCB_EXCEPTION_REGS_SIZE, 0, 0),
		            L4_IPC_SEND_TIMEOUT_0);

	Foc::l4_umword_t const err = l4_ipc_error(msg_tag, l4_utcb());
	if (err) {
		warning("failed to acknowledge exception, l4_ipc_err=", err);
	}
}


Ipc_pager::Ipc_pager()
:
	Native_capability((Cap_index*)Foc::l4_utcb_tcr()->user[Foc::UTCB_TCR_BADGE]),
	_badge(0)
{ }

