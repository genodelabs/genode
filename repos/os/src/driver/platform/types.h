/*
 * \brief  Common types used by the platform driver
 * \author Norman Feske
 * \date   2021-11-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TYPES_H_
#define _TYPES_H_

#include <util/avl_tree.h>

namespace Driver {

	using namespace Genode;

	/**
	 * Utility for switching clocks/resets/powers on/off depending on the
	 * number of users
	 */
	template <typename DEV>
	class Switch : Noncopyable
	{
		private:

			unsigned _count = 0;

			DEV &_dev;

			void (DEV::*_activate)   ();
			void (DEV::*_deactivate) ();

		public:

			Switch(DEV &dev, void (DEV::*activate) (), void (DEV::*deactivate) ())
			:
				_dev(dev), _activate(activate), _deactivate(deactivate)
			{ }

			void use()
			{
				if (_count == 0)
					(_dev.*_activate)();

				_count++;
			}

			void unuse()
			{
				if (_count == 0)
					return;

				_count--;

				if (_count == 0)
					(_dev.*_deactivate)();
			}
	};
}

#endif /* _TYPES_H_ */
