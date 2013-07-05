/*
 * \brief  Guard to save a utcb and restore it during Guard desctruction
 * \author Alexander Boettcher
 * \date   2013-07-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SEOUL_UTCB_GUARD_H_
#define _SEOUL_UTCB_GUARD_H_

#include <base/printf.h>
#include <nova/syscalls.h>

class Utcb_guard {

	private:
		Genode::Native_utcb &_backup_utcb;

	public:

		Utcb_guard(Genode::Native_utcb &backup_utcb)
		: _backup_utcb(backup_utcb)
		{
			using namespace Genode;
	
			Nova::Utcb *utcb =
				reinterpret_cast<Nova::Utcb *>(Thread_base::myself()->utcb());

			unsigned header_len = (char *)utcb->msg - (char *)utcb;
			unsigned len = header_len + utcb->msg_words() * sizeof(Nova::mword_t);
			Genode::memcpy(&_backup_utcb, utcb, len);

			if (utcb->msg_items())
				PWRN("Error: msg items on UTCB are not saved and restored !!!");
		}

		~Utcb_guard()
		{
			using namespace Genode;

			Nova::Utcb *utcb = reinterpret_cast<Nova::Utcb *>(&_backup_utcb);

			unsigned header_len = (char *)utcb->msg - (char *)utcb;
			unsigned len = header_len + utcb->msg_words() * sizeof(Nova::mword_t);
			Genode::memcpy(Thread_base::myself()->utcb(), utcb, len);
		}
};

#endif /* _SEOUL_UTCB_GUARD_H_ */
