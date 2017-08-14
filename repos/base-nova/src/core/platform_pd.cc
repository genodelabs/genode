/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <util/flex_iterator.h>

/* core includes */
#include <platform.h>
#include <platform_pd.h>

using namespace Genode;


/***************************
 ** Public object members **
 ***************************/

bool Platform_pd::bind_thread(Platform_thread *thread)
{
	thread->bind_to_pd(this, _thread_cnt == 0);
	_thread_cnt++;
	return true;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	warning(__func__, "not implemented");
}


void Platform_pd::assign_parent(Native_capability parent)
{
	if (!_parent.valid() && parent.valid())
		_parent = parent;
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *label,
                         signed pd_id, bool create)
: _thread_cnt(0), _pd_sel(cap_map()->insert()), _label(label)
{
	if (_pd_sel == Native_thread::INVALID_INDEX) {
		error("platform pd creation failed ");
		return;
	}

	/* create task */
	enum { KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE = 2, UPPER_LIMIT_PAGES = 32 };
	uint8_t res = Nova::create_pd(_pd_sel, platform_specific()->core_pd_sel(),
	                        Nova::Obj_crd(),
	                        KEEP_FREE_PAGES_NOT_AVAILABLE_FOR_UPGRADE,
	                        UPPER_LIMIT_PAGES);

	if (res != Nova::NOVA_OK)
		error("create_pd returned ", res);
}


Platform_pd::~Platform_pd()
{
	if (_pd_sel == Native_thread::INVALID_INDEX)
		return;

	/* Revoke and free cap, pd is gone */
	Nova::revoke(Nova::Obj_crd(_pd_sel, 0));
	cap_map()->remove(_pd_sel, 0, false);
}


void Platform_pd::flush(addr_t remote_virt, size_t size, Core_local_addr)
{
	Nova::Rights const revoke_rwx(true, true, true);

	Flexpage_iterator flex(remote_virt, size, remote_virt, size, 0);
	Flexpage page = flex.page();

	if (pd_sel() == Native_thread::INVALID_INDEX)
		return;

	while (page.valid()) {
		Nova::Mem_crd mem(page.addr >> 12, page.log2_order - 12, revoke_rwx);
		Nova::revoke(mem, true, true, pd_sel());

		page = flex.page();
	}
}
