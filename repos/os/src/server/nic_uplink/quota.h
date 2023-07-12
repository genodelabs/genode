/*
 * \brief  Session quota
 * \author Martin Stein
 * \date   2023-07-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _QUOTA_H_
#define _QUOTA_H_

namespace Net {

	class Quota;
}

struct Net::Quota
{
	Genode::size_t ram { 0 };
	Genode::size_t cap { 0 };
};

#endif /* _QUOTA_H_ */
