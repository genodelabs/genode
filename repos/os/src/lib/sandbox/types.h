/*
 * \brief  Common types used within init
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__TYPES_H_
#define _LIB__SANDBOX__TYPES_H_

#include <util/string.h>
#include <util/list.h>
#include <session/session.h>

namespace Sandbox {

	class Child;

	using namespace Genode;
	using Genode::size_t;
	using Genode::strlen;

	struct Prio_levels { long value; };

	typedef List<List_element<Child> > Child_list;

	template <typename T>
	struct Resource_info
	{
		T quota, used, avail;

		static Resource_info from_pd(Pd_session const &pd);

		void generate(Xml_generator &xml) const
		{
			typedef String<32> Value;
			xml.attribute("quota", Value(quota));
			xml.attribute("used",  Value(used));
			xml.attribute("avail", Value(avail));
		}

		bool operator != (Resource_info const &other) const
		{
			return quota.value != other.quota.value
			    || used.value  != other.used.value
			    || avail.value != other.avail.value;
		}
	};

	typedef Resource_info<Ram_quota> Ram_info;
	typedef Resource_info<Cap_quota> Cap_info;

	template <>
	inline Ram_info Ram_info::from_pd(Pd_session const &pd)
	{
		return { .quota = pd.ram_quota(),
		         .used  = pd.used_ram(),
		         .avail = pd.avail_ram() };
	}

	template <>
	inline Cap_info Cap_info::from_pd(Pd_session const &pd)
	{
		return { .quota = pd.cap_quota(),
		         .used  = pd.used_caps(),
		         .avail = pd.avail_caps() };
	}

}

#endif /* _LIB__SANDBOX__TYPES_H_ */
