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

namespace Genode {

	/**
	 * Generic parts of the platform-specific 'Native_capability'
	 *
	 * \param POLICY  policy class that provides the type used as capability
	 *                destination and functions for checking the validity of
	 *                the platform-specific destination type and for
	 *                invalid destinations.
	 *
	 * The struct passed as 'POLICY' argument must have the following
	 * interface:
	 *
	 * ! typedef Dst;
	 * ! static bool valid(Dst dst);
	 * ! static Dst invalid();
	 *
	 * The 'Dst' type is the platform-specific destination type (e.g., the ID
	 * of the destination thread targeted by the capability). The 'valid'
	 * function returns true if the specified destination is valid. The
	 * 'invalid' function produces an invalid destination.
	 */
	template <typename POLICY>
	class Native_capability_tpl
	{
		private:

			typedef typename POLICY::Dst Dst;

			Dst  _tid;
			long _local_name;

		protected:

			/**
			 * Constructor for a local capability
			 *
			 * A local capability just encapsulates a pointer to some
			 * local object. This constructor is only used by a factory
			 * method for local-capabilities in the generic Capability
			 * class.
			 *
			 * \param ptr address of the local object
			 */
			Native_capability_tpl(void* ptr)
			: _tid(POLICY::invalid()), _local_name((long)ptr) { }

		public:

			/**
			 * Constructor for an invalid capability
			 */
			Native_capability_tpl() : _tid(POLICY::invalid()), _local_name(0) { }

			/**
			 * Publicly available constructor
			 *
			 * \param tid         kernel-specific thread id
			 * \param local_name  ID used as key to lookup the 'Rpc_object'
			 *                    that corresponds to the capability.
			 */
			Native_capability_tpl(Dst tid, long local_name)
			: _tid(tid), _local_name(local_name) { }

			/**
			 * Return true when the capability is valid
			 */
			bool valid() const { return POLICY::valid(_tid); }

			/**
			 * Return ID used to lookup the 'Rpc_object' by its capability
			 */
			long local_name() const { return _local_name; }

			/**
			 * Return pointer to object referenced by a local-capability
			 */
			void* local() const { return (void*)_local_name; }

			/**
			 * Copy this capability to another PD
			 */
			void copy_to(void* dst) { POLICY::copy(dst, this); }


			/*****************************************
			 ** Only used by platform-specific code **
			 *****************************************/

			/**
			 * Return the kernel-specific capability destination
			 */
			Dst dst() const { return _tid; }
	};
}

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_H_ */

