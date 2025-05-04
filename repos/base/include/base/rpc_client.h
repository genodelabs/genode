/*
 * \brief  Support for performing RPC calls
 * \author Norman Feske
 * \date   2011-04-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RPC_CLIENT_H_
#define _INCLUDE__BASE__RPC_CLIENT_H_

#include <base/ipc.h>
#include <base/sleep.h>
#include <base/trace/events.h>

namespace Genode {

	template <typename> struct Rpc_client;

	/*
	 * Number of capabilities received by RPC_FUNCTION as out parameters
	 */
	template <typename T> struct Cap_para_out                  { enum { Value = 0 }; };
	template <typename T> struct Cap_para_out<Capability<T> *> { enum { Value = 1 }; };
	template <typename T> struct Cap_para_out<Capability<T> &> { enum { Value = 1 }; };
	template <> struct Cap_para_out<Native_capability *>       { enum { Value = 1 }; };
	template <> struct Cap_para_out<Native_capability &>       { enum { Value = 1 }; };

	/*
	 * Presence of capability received as return value from RPC_FUNCTION
	 */
	template <typename T> struct Cap_return                  { enum { Value = 0 }; };
	template <typename T> struct Cap_return<Capability<T> >  { enum { Value = 1 }; };
	template <typename T> struct Cap_return<Capability<T> *> { enum { Value = 1 }; };
	template <typename T> struct Cap_return<Capability<T> &> { enum { Value = 1 }; };
	template <> struct Cap_return<Native_capability>         { enum { Value = 1 }; };
	template <> struct Cap_return<Native_capability *>       { enum { Value = 1 }; };
	template <> struct Cap_return<Native_capability &>       { enum { Value = 1 }; };

	/*
	 * Presence of capability received as 'Attempt' return value from RPC_FUNCTION
	 */
	template <typename T, typename E>
	struct Cap_return<Attempt<T, E> > { enum { Value = Cap_return<T>::Value }; };

	template <typename ARGS>
	struct Rpc_caps_out {
		static constexpr unsigned
			Value = Cap_para_out<typename ARGS::Head>::Value
			      + Rpc_caps_out<typename ARGS::Tail>::Value; };

	template <>
	struct Rpc_caps_out<Meta::Empty> { static constexpr unsigned Value = 0; };

	template <typename RPC_FUNCTION>
	struct Rpc_function_caps_out {
		static constexpr unsigned
			Value = Rpc_caps_out<typename RPC_FUNCTION::Server_args>::Value +
			        Cap_return  <typename RPC_FUNCTION::Ret_type>::Value; };


	/***************************************************
	 ** Implementation of 'Capability:call' functions **
	 ***************************************************/

	template <typename RPC_INTERFACE>
	template <typename ATL>
	void Capability<RPC_INTERFACE>::
	_marshal_args(Msgbuf_base &msg, ATL &args) const
	{
		if (Trait::Rpc_direction<typename ATL::Head>::Type::IN)
			msg.insert(args.get());

		_marshal_args(msg, args._2);
	}


	template <typename RPC_INTERFACE>
	template <typename T>
	void Capability<RPC_INTERFACE>::
	_unmarshal_result(Ipc_unmarshaller &unmarshaller, T &arg,
	                  Meta::Overload_selector<Rpc_arg_out>) const
	{
		unmarshaller.extract(arg);
	}


	template <typename RPC_INTERFACE>
	template <typename T>
	void Capability<RPC_INTERFACE>::
	_unmarshal_result(Ipc_unmarshaller &unmarshaller, T &arg,
	                  Meta::Overload_selector<Rpc_arg_inout>) const
	{
		_unmarshal_result(unmarshaller, arg, Meta::Overload_selector<Rpc_arg_out>());
	}


	template <typename RPC_INTERFACE>
	template <typename ATL>
	void Capability<RPC_INTERFACE>::
	_unmarshal_results(Ipc_unmarshaller &unmarshaller, ATL &args) const
	{
		/*
		 * Unmarshal current argument. The overload of
		 * '_unmarshal_result' is selected depending on the RPC
		 * direction.
		 */
		using Rpc_dir = typename Trait::Rpc_direction<typename ATL::Head>::Type;
		_unmarshal_result(unmarshaller, args.get(), Meta::Overload_selector<Rpc_dir>());

		/* unmarshal remaining arguments */
		_unmarshal_results(unmarshaller, args._2);
	}


	template <typename RPC_INTERFACE>
	template <typename IF>
	typename IF::Ret_type Capability<RPC_INTERFACE>::
	_call(typename IF::Client_args &args) const
	{
		/**
		 * Message buffer for RPC message
		 *
		 * The message buffer gets automatically dimensioned according to the
		 * specified 'IF' RPC function.
		 */
		enum { PROTOCOL_OVERHEAD = 4*sizeof(long),
		       CALL_MSG_SIZE     = Rpc_function_msg_size<IF, RPC_CALL>::Value,
		       REPLY_MSG_SIZE    = Rpc_function_msg_size<IF, RPC_REPLY>::Value,
		       RECEIVE_CAPS      = Rpc_function_caps_out<IF>::Value };

		Msgbuf<CALL_MSG_SIZE  + PROTOCOL_OVERHEAD>  call_buf;
		Msgbuf<REPLY_MSG_SIZE + PROTOCOL_OVERHEAD> reply_buf;

		/* determine opcode of RPC function */
		using Rpc_functions = typename RPC_INTERFACE::Rpc_functions;
		Rpc_opcode opcode(static_cast<int>(Meta::Index_of<Rpc_functions, IF>::Value));

		/* marshal opcode and RPC input arguments */
		call_buf.insert(opcode);
		_marshal_args(call_buf, args);

		{
			Trace::Rpc_call trace_event(IF::name(), call_buf);
		}

		/* perform RPC, unmarshal return value */
		Rpc_exception_code const exception_code =
			ipc_call(*this, call_buf, reply_buf, RECEIVE_CAPS);

		if (exception_code.value == Rpc_exception_code::INVALID_OBJECT) {
			error("attempt of IPC call to an invalid object");
			sleep_forever();
		}

		Ipc_unmarshaller unmarshaller(reply_buf);

		{
			Trace::Rpc_returned trace_event(IF::name(), reply_buf);
		}

		/* unmarshal RPC output arguments */
		_unmarshal_results(unmarshaller, args);

		/* reflect callee-side exception at the caller */
		_check_for_exceptions(exception_code,
		                      Meta::Overload_selector<typename IF::Exceptions>());

		/* the return value does only exist if no exception was thrown */
		Meta::Overload_selector<typename IF::Ret_type> ret_overloader;
		return unmarshaller.extract(ret_overloader);
	}
}


/**
 * RPC client
 *
 * This class template is the base class of the client-side implementation
 * of the specified 'RPC_INTERFACE'. Usually, it inherits the pure virtual
 * functions declared in 'RPC_INTERFACE' and has the built-in facility to
 * perform RPC calls to this particular interface. Hence, the client-side
 * implementation of each pure virtual interface function comes down to a
 * simple wrapper in the line of 'return call<Rpc_function>(arguments...)'.
 */
template <typename RPC_INTERFACE>
class Genode::Rpc_client : public RPC_INTERFACE
{
	private:

		Capability<RPC_INTERFACE> _cap;

	public:

		using Rpc_interface = RPC_INTERFACE;

		Rpc_client(Capability<RPC_INTERFACE> const &cap) : _cap(cap) { }

		template <typename IF, typename... ARGS>
		typename IF::Ret_type call(ARGS &&...args)
		{
			return _cap.template call<IF>(args...);
		}

		template <typename IF, typename... ARGS>
		typename IF::Ret_type call(ARGS &&...args) const
		{
			return _cap.template call<IF>(args...);
		}

		Capability<RPC_INTERFACE> rpc_cap() const { return _cap; }
};

#endif /* _INCLUDE__BASE__RPC_CLIENT_H_ */
