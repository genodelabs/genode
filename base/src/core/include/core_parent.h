/*
 * \brief  Core-specific parent client implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-20
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_PARENT_H_
#define _CORE__INCLUDE__CORE_PARENT_H_

#include <base/printf.h>
#include <parent/parent.h>

namespace Genode {

	/**
	 * In fact, Core has _no_ parent. But most of our libraries could work
	 * seamlessly inside Core too, if it had one. Core_parent fills this gap.
	 */
	class Core_parent : public Parent
	{
		public:

			/**
			 * Constructor
			 */
			Core_parent() { }


			/**********************
			 ** Parent interface **
			 **********************/

			void exit(int);

			void announce(Service_name const &, Root_capability)
			{
				PDBG("implement me, please");
			}

			Session_capability session(Service_name const &, Session_args const &);

			void upgrade(Session_capability, Upgrade_args const &)
			{
				PDBG("implement me, please");
				throw Quota_exceeded();
			}

			void close(Session_capability)
			{
				PDBG("implement me, please");
			}

			Thread_capability main_thread_cap() const
			{
				PDBG("implement me, please");
				return Thread_capability();
			}
	};
}

#endif /* _CORE__INCLUDE__CORE_PARENT_H_ */
