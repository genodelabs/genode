/*
 * \brief  Registry of running children
 * \author Norman Feske
 * \date   2013-10-05
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CHILD_REGISTRY_H_
#define _CHILD_REGISTRY_H_

/* Genode includes */
#include <util/list.h>

/* local includes */
#include <child.h>

class Child_registry : public List<Child>
{
	private:

		/**
		 * Return true if a child with the specified name already exists
		 */
		bool _child_name_exists(const char *label)
		{
			for (Child *child = first() ; child; child = child->next())
				if (child->name() == label)
					return true;
			return false;
		}

	public:

		enum { CHILD_NAME_MAX_LEN = 64 };

		/**
		 * Produce new unique child name
		 */
		void unique_child_name(const char *prefix, char *dst, int dst_len)
		{
			char buf[CHILD_NAME_MAX_LEN];
			char suffix[8];
			suffix[0] = 0;

			for (int cnt = 1; true; cnt++) {

				/* build program name composed of prefix and numeric suffix */
				snprintf(buf, sizeof(buf), "%s%s", prefix, suffix);

				/* if such a program name does not exist yet, we are happy */
				if (!_child_name_exists(buf)) {
					strncpy(dst, buf, dst_len);
					return;
				}

				/* increase number of suffix */
				snprintf(suffix, sizeof(suffix), ".%d", cnt + 1);
			}
		}

		/**
		 * Call functor 'fn' for each child
		 *
		 * The functor receives the child name as 'char const *'.
		 */
		template <typename FN>
		void for_each_child_name(FN const &fn) const
		{
			for (Child const *child = first() ; child; child = child->next())
				fn(child->name());
		}
};

#endif /* _CHILD_REGISTRY_H_ */
