/*
 * \brief  Guard to save a UTCB and restore it during guard destruction
 * \author Alexander Boettcher
 * \date   2013-07-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM__UTCB_GUARD_H_
#define _INCLUDE__VMM__UTCB_GUARD_H_

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* NOVA syscalls */
#include <nova/syscalls.h>


namespace Vmm {
	using namespace Genode;
	class Utcb_guard;
}


class Vmm::Utcb_guard
{
	private:

		Native_utcb &_backup_utcb;

	public:

		Utcb_guard(Native_utcb &backup_utcb) : _backup_utcb(backup_utcb)
		{
			Nova::Utcb *utcb =
				reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb());

			unsigned header_len = (char *)utcb->msg - (char *)utcb;
			unsigned len = header_len + utcb->msg_words() * sizeof(Nova::mword_t);
			Genode::memcpy(&_backup_utcb, utcb, len);

			if (utcb->msg_items())
				PWRN("Error: msg items on UTCB are not saved and restored!");
		}

		~Utcb_guard()
		{
			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(&_backup_utcb);

			unsigned header_len = (char *)utcb->msg - (char *)utcb;
			unsigned len = header_len + utcb->msg_words() * sizeof(Nova::mword_t);
			Genode::memcpy(Thread_base::myself()->utcb(), utcb, len);
		}
};

#endif /* _INCLUDE__VMM__UTCB_GUARD_H_ */
