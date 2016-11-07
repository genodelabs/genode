/*
 * \brief  Native capability template.
 * \author Stefan Kalkowski
 * \date   2011-03-07
 *
 * This file is a generic variant of the Native_capability, which is
 * suitable many platforms such as Fiasco, Pistachio, OKL4, Linux,
 * and some more.
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__NATIVE_CAPABILITY_TPL_H_
#define _INCLUDE__BASE__NATIVE_CAPABILITY_TPL_H_

namespace Genode { template <typename> class Native_capability_tpl; } 


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
 * method returns true if the specified destination is valid. The
 * 'invalid' method produces an invalid destination.
 */
template <typename POLICY>
class Genode::Native_capability_tpl
{
	public:

		typedef typename POLICY::Dst Dst;

		/**
		 * Compound object used to copy raw capability members
		 *
		 * This type is a utility solely used to communicate the
		 * information about the parent capability from the parent to the
		 * new process.
		 */
		struct Raw { Dst dst; long local_name; };

	private:

		Dst  _dst;
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
		: _dst(POLICY::invalid()), _local_name((long)ptr) { }

	public:

		/**
		 * Constructor for an invalid capability
		 */
		Native_capability_tpl() : _dst(POLICY::invalid()), _local_name(0) { }

		/**
		 * Publicly available constructor
		 *
		 * \param tid         kernel-specific thread id
		 * \param local_name  ID used as key to lookup the 'Rpc_object'
		 *                    that corresponds to the capability.
		 */
		Native_capability_tpl(Dst tid, long local_name)
		: _dst(tid), _local_name(local_name) { }

		/**
		 * Return true when the capability is valid
		 */
		bool valid() const { return POLICY::valid(_dst); }

		/**
		 * Return ID used to lookup the 'Rpc_object' by its capability
		 */
		long local_name() const { return _local_name; }

		/**
		 * Return pointer to object referenced by a local-capability
		 */
		void* local() const { return (void*)_local_name; }

		/**
		 * Return capability destination
		 */
		Dst dst() const { return _dst; }

		/**
		 * Return raw data representation of the capability
		 */
		Raw raw() const { return { _dst, _local_name }; }
};

#endif /* _INCLUDE__BASE__NATIVE_CAPABILITY_TPL_H_ */
