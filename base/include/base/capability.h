/*
 * \brief  Capability
 * \author Norman Feske
 * \date   2011-05-22
 *
 * A typed capability is a capability tied to one specifiec RPC interface
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__CAPABILITY_H_
#define _INCLUDE__BASE__CAPABILITY_H_

#include <util/string.h>
#include <base/rpc.h>
#include <base/native_types.h>

namespace Genode {

	/**
	 * Forward declaration needed for internal interfaces of 'Capability'
	 */
	class Ipc_client;


	/**
	 * Capability that is not associated with a specific RPC interface
	 */
	typedef Native_capability Untyped_capability;


	/**
	 * Capability referring to a specific RPC interface
	 *
	 * \param RPC_INTERFACE  class containing the RPC interface declaration
	 */
	template <typename RPC_INTERFACE>
	class Capability : public Untyped_capability
	{
		private:

			/**
			 * Insert RPC arguments into the message buffer
			 */
			template <typename ATL>
			void _marshal_args(Ipc_client &ipc_client, ATL &args) const;

			void _marshal_args(Ipc_client &, Meta::Empty &) const { }

			/**
			 * Unmarshal single RPC argument from the message buffer
			 */
			template <typename T>
			void _unmarshal_result(Ipc_client &ipc_client, T &arg,
			                       Meta::Overload_selector<Rpc_arg_out>) const;

			template <typename T>
			void _unmarshal_result(Ipc_client &ipc_client, T &arg,
			                       Meta::Overload_selector<Rpc_arg_inout>) const;

			template <typename T>
			void _unmarshal_result(Ipc_client &, T &arg,
			                       Meta::Overload_selector<Rpc_arg_in>) const { }

			/**
			 * Read RPC results from the message buffer
			 */
			template <typename ATL>
			void _unmarshal_results(Ipc_client &ipc_client, ATL &args) const;

			void _unmarshal_results(Ipc_client &, Meta::Empty &) const { }

			/**
			 * Check RPC return code for the occurrence of exceptions
			 *
			 * A server-side exception is indicated by a non-zero exception
			 * code. Each exception code corresponds to an entry in the
			 * exception type list specified in the RPC function declaration.
			 * The '_check_for_exception' function template throws the
			 * exception type belonging to the received exception code.
			 */
			template <typename EXC_TL>
			void _check_for_exceptions(Rpc_exception_code const exc_code,
			                           Meta::Overload_selector<EXC_TL>) const
			{
				enum { EXCEPTION_CODE = RPC_EXCEPTION_BASE - Meta::Length<EXC_TL>::Value };

				if (exc_code == EXCEPTION_CODE)
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
			void _call(typename IF::Client_args &args,
			           typename IF::Ret_type    &ret) const;

			/**
			 * Shortcut for querying argument types used in 'call' functions
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

			/**
			 * Private constructor, should be used by the local-capability
			 * factory method only.
			 *
			 * \param ptr pointer to the local object this capability represents.
			 */
			Capability(void *ptr) : Untyped_capability(ptr) {}

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

			/**
			 * Factory method to construct a local-capability.
			 *
			 * Local-capabilities can be used protection-domain internally
			 * only. They simply incorporate a pointer to some process-local
			 * object.
			 *
			 * \param ptr pointer to the corresponding local object.
			 * \return a capability that represents the local object.
			 */
			static Capability<RPC_INTERFACE> local_cap(RPC_INTERFACE* ptr) {
				return Capability<RPC_INTERFACE>((void*)ptr); }

			/**
			 * Dereference a local-capability.
			 *
			 * \param c the local-capability.
			 * \return pointer to the corresponding local object.
			 */
			static RPC_INTERFACE* deref(Capability<RPC_INTERFACE> c) {
				return reinterpret_cast<RPC_INTERFACE*>(c.local()); }

			/*
			 * Suppress warning about uninitialized 'ret' variable in 'call'
			 * functions on compilers that support the #pragma. If this is
			 * not the case, the pragma can be masked by supplying the
			 * 'SUPPRESS_GCC_PRAGMA_WUNINITIALIZED' define to the compiler.
			 */
			#ifndef SUPPRESS_GCC_PRAGMA_WUNINITIALIZED
			#pragma GCC diagnostic ignored "-Wuninitialized" call();
			#endif

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call() const
			{
				Meta::Empty e;
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(e, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, v2, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
			     typename Arg<IF, 2>::Type v3) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, v2, v3, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
			     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, v2, v3, v4, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
			     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4,
			     typename Arg<IF, 4>::Type v5) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, v2, v3, v4, v5, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}

			template <typename IF>
			typename Trait::Call_return<typename IF::Ret_type>::Type
			call(typename Arg<IF, 0>::Type v1, typename Arg<IF, 1>::Type v2,
			     typename Arg<IF, 2>::Type v3, typename Arg<IF, 3>::Type v4,
			     typename Arg<IF, 4>::Type v5, typename Arg<IF, 5>::Type v6) const
			{
				Meta::Empty e;
				typename IF::Client_args args(v1, v2, v3, v4, v5, v6, e);
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
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
				typename Trait::Call_return<typename IF::Ret_type>::Type ret;
				_call<IF>(args, ret);
				return ret;
			}
	};


	/**
	 * Convert an untyped capability to a typed capability
	 */
	template <typename RPC_INTERFACE>
	Capability<RPC_INTERFACE>
	reinterpret_cap_cast(Untyped_capability const &untyped_cap)
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
	Capability<TO_RPC_INTERFACE>
	static_cap_cast(Capability<FROM_RPC_INTERFACE> cap)
	{
		/* check interface compatibility */
		(void)static_cast<TO_RPC_INTERFACE *>((FROM_RPC_INTERFACE *)0);

		return reinterpret_cap_cast<TO_RPC_INTERFACE>(cap);
	}
}

#endif /* _INCLUDE__BASE__CAPABILITY_H_ */
