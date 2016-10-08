/*
 * \brief  Capability
 * \author Norman Feske
 * \date   2011-05-22
 *
 * A typed capability is a capability tied to one specifiec RPC interface
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAPABILITY_H_
#define _INCLUDE__BASE__CAPABILITY_H_

#include <util/string.h>
#include <base/rpc.h>
#include <base/native_capability.h>

namespace Genode {

	/**
	 * Forward declaration needed for internal interfaces of 'Capability'
	 */
	class Ipc_unmarshaller;
	class Msgbuf_base;


	/**
	 * Capability that is not associated with a specific RPC interface
	 */
	typedef Native_capability Untyped_capability;

	template <typename> class Capability;

	template <typename RPC_INTERFACE>
	Capability<RPC_INTERFACE> reinterpret_cap_cast(Untyped_capability const &);

	template <typename TO_RPC_INTERFACE, typename FROM_RPC_INTERFACE>
	Capability<TO_RPC_INTERFACE> static_cap_cast(Capability<FROM_RPC_INTERFACE>);
}


/**
 * Capability referring to a specific RPC interface
 *
 * \param RPC_INTERFACE  class containing the RPC interface declaration
 */
template <typename RPC_INTERFACE>
class Genode::Capability : public Untyped_capability
{
	private:

		/**
		 * Insert RPC arguments into the message buffer
		 */
		template <typename ATL>
		void _marshal_args(Msgbuf_base &, ATL &args) const;

		void _marshal_args(Msgbuf_base &, Meta::Empty &) const { }

		/**
		 * Unmarshal single RPC argument from the message buffer
		 */
		template <typename T>
		void _unmarshal_result(Ipc_unmarshaller &, T &arg,
		                       Meta::Overload_selector<Rpc_arg_out>) const;

		template <typename T>
		void _unmarshal_result(Ipc_unmarshaller &, T &arg,
		                       Meta::Overload_selector<Rpc_arg_inout>) const;

		template <typename T>
		void _unmarshal_result(Ipc_unmarshaller &, T &,
		                       Meta::Overload_selector<Rpc_arg_in>) const { }

		/**
		 * Read RPC results from the message buffer
		 */
		template <typename ATL>
		void _unmarshal_results(Ipc_unmarshaller &, ATL &args) const;

		void _unmarshal_results(Ipc_unmarshaller &, Meta::Empty &) const { }

		/**
		 * Check RPC return code for the occurrence of exceptions
		 *
		 * A server-side exception is indicated by a non-zero exception
		 * code. Each exception code corresponds to an entry in the
		 * exception type list specified in the RPC function declaration.
		 * The '_check_for_exception' method template throws the
		 * exception type belonging to the received exception code.
		 */
		template <typename EXC_TL>
		void _check_for_exceptions(Rpc_exception_code const exc_code,
		                           Meta::Overload_selector<EXC_TL>) const
		{
			enum { EXCEPTION_CODE = Rpc_exception_code::EXCEPTION_BASE
			                      - Meta::Length<EXC_TL>::Value };

			if (exc_code.value == EXCEPTION_CODE)
				throw typename EXC_TL::Head();

			_check_for_exceptions(exc_code, Meta::Overload_selector<typename EXC_TL::Tail>());
		}

		void _check_for_exceptions(Rpc_exception_code const,
		                           Meta::Overload_selector<Meta::Empty>) const
		{ }

		/**
		 * Perform RPC call, arguments passed a as nested 'Ref_tuple' object
		 */
		template <typename IF>
		typename IF::Ret_type _call(typename IF::Client_args &args) const;

		/**
		 * Shortcut for querying argument types used in 'call' methods
		 */
		template <typename IF, unsigned I>
		struct Arg
		{
			typedef typename Meta::Type_at<typename IF::Client_args, I>::Type Type;
		};

		template <typename FROM_RPC_INTERFACE>
		Untyped_capability
		_check_compatibility(Capability<FROM_RPC_INTERFACE> const &cap) const
		{
			FROM_RPC_INTERFACE *from = 0;
			RPC_INTERFACE *to = from;
			(void)to;
			return cap;
		}

	public:

		typedef RPC_INTERFACE Rpc_interface;

		/**
		 * Constructor
		 *
		 * This implicit constructor checks at compile time for the
		 * compatibility of the source and target capability types. The
		 * construction is performed only if the target capability type is
		 * identical to or a base type of the source capability type.
		 */
		template <typename FROM_RPC_INTERFACE>
		Capability(Capability<FROM_RPC_INTERFACE> const &cap)
		: Untyped_capability(_check_compatibility(cap))
		{ }

		/**
		 * Default constructor creates invalid capability
		 */
		Capability() { }

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call() const
		{
			Meta::Empty e;
			return _call<IF>(e);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
		     typename Arg<IF, 2>::Type v3) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, v3, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
		     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, v3, v4, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
		     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4,
		     typename Arg<IF, 4>::Type v5) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, v3, v4, v5, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
		     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4,
		     typename Arg<IF, 4>::Type v5, typename Arg<IF, 5>::Type v6) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, v3, v4, v5, v6, e);
			return _call<IF>(args);
		}

		template <typename IF>
		typename Trait::Call_return<typename IF::Ret_type>::Type
		call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
		     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4,
		     typename Arg<IF, 4>::Type v5, typename Arg<IF, 5>::Type v6,
		     typename Arg<IF, 6>::Type v7) const
		{
			Meta::Empty e;
			typename IF::Client_args args(v1, v2, v3, v4, v5, v6, v7, e);
			return _call<IF>(args);
		}
};


/**
 * Convert an untyped capability to a typed capability
 */
template <typename RPC_INTERFACE>
Genode::Capability<RPC_INTERFACE>
Genode::reinterpret_cap_cast(Untyped_capability const &untyped_cap)
{
	/*
	 * The object layout of untyped and typed capabilities is identical.
	 * Hence we can just use it's copy-constructors.
	 */
	Untyped_capability *ptr = const_cast<Untyped_capability*>(&untyped_cap);
	return *static_cast<Capability<RPC_INTERFACE>*>(ptr);
}


/**
 * Convert capability type from an interface base type to an inherited
 * interface type
 */
template <typename TO_RPC_INTERFACE, typename FROM_RPC_INTERFACE>
Genode::Capability<TO_RPC_INTERFACE>
Genode::static_cap_cast(Capability<FROM_RPC_INTERFACE> cap)
{
	/* check interface compatibility */
	(void)static_cast<TO_RPC_INTERFACE *>((FROM_RPC_INTERFACE *)0);

	return reinterpret_cap_cast<TO_RPC_INTERFACE>(cap);
}

#endif /* _INCLUDE__BASE__CAPABILITY_H_ */
