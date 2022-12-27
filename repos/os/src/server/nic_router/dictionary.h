/*
 * \brief  Local convenience wrapper for the Genode dictionary
 * \author Martin Stein
 * \date   2022-09-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DICTIONARY_H_
#define _DICTIONARY_H_

/* Genode includes */
#include <base/allocator.h>
#include <util/dictionary.h>

namespace Net { template <typename, typename> class Dictionary; }


template <typename OBJECT_T,
          typename NAME_T>

class Net::Dictionary : public Genode::Dictionary<OBJECT_T, NAME_T>
{
	private:

		using Dict = Genode::Dictionary<OBJECT_T, NAME_T>;

	public:

		template <typename FUNCTION_T>
		void for_each(FUNCTION_T const &function) const
		{
			Dict::for_each(
				[&] (OBJECT_T const &obj)
				{
					function(*const_cast<OBJECT_T *>(&obj));
				}
			);
		}

		void destroy_each(Genode::Deallocator &dealloc)
		{
			auto destroy_element { [&] (OBJECT_T &obj) {
				destroy(dealloc, &obj);
			} };
			while (this->with_any_element(destroy_element)) { }
		}
};

#endif /* _DICTIONARY_H_ */
