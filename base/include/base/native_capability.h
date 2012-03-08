/*
 * \brief  Native capability template.
 * \author Stefan Kalkowski
 * \date   2011-03-07
 *
 * This file is a generic variant of the Native_capability, which is
 * suitable many platforms such as Fiasco, Pistachio, OKL4, Linux, Codezero,
 * and some more.
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_H_

#include <util/string.h>

namespace Genode {

	template <typename ID, typename T>
	class Native_capability_tpl
	{
		private:

			ID   _tid;
			long _local_name;

		protected:

			/**
			 * Constructor for a local capability.
			 *
			 * A local capability just encapsulates a pointer to some
			 * local object. This constructor is only used by a factory
			 * method for local-capabilities in the generic Capability
			 * class.
			 *
			 * \param ptr address of the local object.
			 */
			Native_capability_tpl(void* ptr)
			: _tid(T::invalid()), _local_name((long)ptr) { }

		public:

			/**
			 * Constructor for an invalid capability.
			 */
			Native_capability_tpl() : _tid(T::invalid()), _local_name(0) { }

			/**
			 * Publicly available constructor.
			 *
			 * \param tid        kernel-specific thread id
			 * \param local_name global, unique id of the cap.
			 */
			Native_capability_tpl(ID tid, long local_name)
			: _tid(tid), _local_name(local_name) {}

			/**
			 * \return true when the capability is a valid one, otherwise false.
			 */
			bool valid() const { return T::valid(_tid); }

			/**
			 * \return the globally unique id.
			 */
			long local_name() const { return _local_name; }

			/**
			 * \return true if this is a local-capability, otherwise false.
			 */
			void* local() const { return (void*)_local_name; }

			/**
			 * Copy this capability to another pd.
			 */
			void copy_to(void* dst) {
				memcpy(dst, this, sizeof(Native_capability_tpl)); }


			/*****************************************
			 ** Only used by platform-specific code **
			 *****************************************/

			/**
			 * \return the kernel-specific thread specifier.
			 */
			ID tid() const { return _tid; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */

