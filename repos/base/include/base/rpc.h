/*
 * \brief  Support for defining and working with RPC interfaces
 * \author Norman Feske
 * \date   2011-03-28
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__RPC_H_
#define _INCLUDE__BASE__RPC_H_

#include <util/meta.h>


/**
 * Macro for declaring an RPC function
 *
 * \param rpc_name   type name representing the RPC function
 * \param ret_type   RPC return type
 * \param func_name  RPC function name
 * \param exc_types  type list of exceptions that may be thrown by the
 *                   function
 * \param ...        variable number of RPC function arguments
 *
 * Each RPC function is represented by a struct that contains the meta data
 * about the function arguments, the return type, and the exception types.
 * Furthermore, it contains an adapter function called 'serve', which is used
 * on the server side to invoke the server-side implementation of the RPC
 * function. It takes an a 'Pod_tuple' argument structure and calls the
 * server-side function with individual arguments using the 'call_member'
 * mechanism provided by 'meta.h'.
 */
#define GENODE_RPC_THROW(rpc_name, ret_type, func_name, exc_types, ...) \
	struct rpc_name { \
		typedef ::Genode::Meta::Ref_args<__VA_ARGS__>::Type  Client_args; \
		typedef ::Genode::Meta::Pod_args<__VA_ARGS__>::Type  Server_args; \
		typedef ::Genode::Trait::Exc_list<exc_types>::Type   Exceptions; \
		typedef ::Genode::Trait::Call_return<ret_type>::Type Ret_type; \
		\
		template <typename SERVER, typename RET> \
		static RET serve(SERVER &server, Server_args &args) { \
			return ::Genode::Meta::call_member<RET, SERVER, Server_args> \
				(server, args, &SERVER::func_name); } \
		\
		static const char* name() { return #func_name; } \
	}

/**
 * Shortcut for 'GENODE_RPC_THROW' for an RPC that throws no exceptions
 */
#define GENODE_RPC(rpc_name, ret_type, func_name, ...) \
	GENODE_RPC_THROW(rpc_name, ret_type, func_name, GENODE_TYPE_LIST(), __VA_ARGS__)

/**
 * Macro for declaring a RPC interface
 *
 * \param ...  list of RPC functions as declared via 'GENODE_RPC'
 *
 * An RPC interface is represented as type list of RPC functions. The RPC
 * opcode for each function is implicitly defined by its position within
 * this type list.
 */
#define GENODE_RPC_INTERFACE(...) \
	typedef GENODE_TYPE_LIST(__VA_ARGS__) Rpc_functions

/**
 * Macro for declaring a RPC interface derived from another RPC interface
 *
 * \param base  class hosting the RPC interface to be inherited
 * \param ...   list of the locally declared RPC functions
 *
 * RPC interface inheritance is simply the concatenation of the type list
 * of RPC functions declared for the base interface and the locally declared
 * RPC functions. By appending the local RPC functions, the RPC opcodes of
 * the inherited RPC functions are preserved.
 */
#define GENODE_RPC_INTERFACE_INHERIT(base, ...) \
	typedef ::Genode::Meta::Append<base::Rpc_functions, \
	                               GENODE_TYPE_LIST(__VA_ARGS__) >::Type \
		Rpc_functions; \
	typedef base Rpc_inherited_interface;


namespace Genode {

	struct Rpc_arg_in    { enum { IN = true,  OUT = false }; };
	struct Rpc_arg_out   { enum { IN = false, OUT = true  }; };
	struct Rpc_arg_inout { enum { IN = true,  OUT = true  }; };

	namespace Trait {

		/*****************************************
		 ** Type meta data used for marshalling **
		 *****************************************/

		template <typename T> struct Rpc_direction;


		template <typename T> struct Rpc_direction            { typedef Rpc_arg_in    Type; };
		template <typename T> struct Rpc_direction<T const *> { typedef Rpc_arg_in    Type; };
		template <typename T> struct Rpc_direction<T const &> { typedef Rpc_arg_in    Type; };
		template <typename T> struct Rpc_direction<T*>        { typedef Rpc_arg_inout Type; };
		template <typename T> struct Rpc_direction<T&>        { typedef Rpc_arg_inout Type; };

		/**
		 * Representation of function return type
		 *
		 * For RPC functions with no return value, we use a pseudo return value
		 * of type 'Empty' instead. This way, we can process all functions
		 * regardless of the presence of a return type with the same meta
		 * program.
		 */
		template <typename T> struct Call_return       { typedef T Type; };
		template <>           struct Call_return<void> { typedef Meta::Empty Type; };

		/**
		 * Representation of the list of exception types
		 *
		 * This template maps the special case of a 'Type_list' with no arguments
		 * to the 'Empty' type.
		 */
		template <typename T> struct Exc_list { typedef T Type; };
		template <> struct Exc_list<Meta::Type_list<> > { typedef Meta::Empty Type; };
	}


	/*******************************************************
	 ** Automated computation of RPC message-buffer sizes **
	 *******************************************************/

	/**
	 * Determine transfer size of an RPC argument
	 *
	 * For data arguments, the transfer size is the size of the data type. For
	 * pointer arguments, the transfer size is the size of the pointed-to
	 * object.
	 */
	template <typename T>
	struct Rpc_transfer_size {
		enum { Value = Meta::Round_to_machine_word<sizeof(T)>::Value }; };

	template <typename T>
	struct Rpc_transfer_size<T *> {
		enum { Value = Meta::Round_to_machine_word<sizeof(T)>::Value }; };


	/**
	 * Type used for transmitting the opcode of a RPC function (used for RPC call)
	 */
	struct Rpc_opcode
	{
		long value = 0;

		explicit Rpc_opcode(int value) : value(value) { }
	};


	/**
	 * Type used for transmitting exception information (used for RPC reply)
	 */
	struct Rpc_exception_code
	{
		long value;

		enum {
			SUCCESS = 0,

			/**
			 * Server-side object does not exist
			 *
			 * This exception code is not meant to be reflected from the server
			 * to the client. On kernels with capability support, the condition
			 * can never occur. On kernels without capability protection, the
			 * code is merely used for diagnostic purposes at the server side.
			 */
			INVALID_OBJECT = -1,

			/**
			 * Special exception code used to respond to illegal opcodes
			 */
			INVALID_OPCODE = -2,

			/**
			 * Opcode base used for passing exception information
			 */
			EXCEPTION_BASE = -1000
		};

		explicit Rpc_exception_code(int value) : value(value) { }
	};


	/**
	 * Return the accumulated size of RPC arguments
	 *
	 * \param ARGS  typelist with RPC arguments
	 * \param IN    true to account for RPC-input arguments
	 * \param OUT   true to account for RPC-output arguments
	 */
	template <typename ARGS, bool IN, bool OUT>
	struct Rpc_args_size {
		typedef typename ARGS::Head This;
		enum { This_size = Rpc_transfer_size<This>::Value };
		enum { Value = (IN  && Trait::Rpc_direction<This>::Type::IN  ? This_size : 0)
		             + (OUT && Trait::Rpc_direction<This>::Type::OUT ? This_size : 0)
		             + Rpc_args_size<typename ARGS::Tail, IN, OUT>::Value }; };
	
	template <bool IN, bool OUT>
	struct Rpc_args_size<Meta::Empty, IN, OUT> { enum { Value = 0 }; };


	/**
	 * Return the size of the return value
	 *
	 * The return type of an RPC function can be either a real type or
	 * 'Meta::Empty' if the function has no return value. In the latter case,
	 * 'Retval_size' returns 0 instead of the non-zero size of 'Meta::Empty'.
	 */
	template <typename RET>
	struct Rpc_retval_size { enum { Value = sizeof(RET) }; };

	template <>
	struct Rpc_retval_size<Meta::Empty> { enum { Value = 0 }; };


	/**
	 * Calculate the payload size of a RPC message
	 *
	 * Setting either IN or OUT to true, the call size or respectively the
	 * reply size is calculated. Protocol-related message parts (such as RPC
	 * opcode or exception status) is not accounted for.
	 */
	template <typename RPC_FUNCTION, bool IN, bool OUT>
	struct Rpc_msg_payload_size {
		typedef typename RPC_FUNCTION::Server_args Args;
		enum { Value = Rpc_args_size<Args, IN, OUT>::Value }; };


	/**
	 * RPC message type
	 *
	 * An RPC message can be either a 'RPC_CALL' (from client to server) or a
	 * 'RPC_REPLY' (from server to client). The message payload for each type
	 * depends on the RPC function arguments as well as protocol-specific
	 * message parts. For example, a 'RPC_CALL' requires the transmission of
	 * the RPC opcode to select the server-side RPC function. In contrast, a
	 * 'RPC_REPLY' message carries along the exception state returned from the
	 * server-side RPC implementation. The 'Rpc_msg_type' is used as template
	 * argument to specialize the calculation of message sizes for each of both
	 * cases.
	 */
	enum Rpc_msg_type { RPC_CALL, RPC_REPLY };


	/**
	 * Calculate size of RPC message
	 *
	 * The send and receive cases are handled by respective template
	 * specializations for the 'MSG_TYPE' template argument.
	 */
	template <typename RPC_FUNCTION, Rpc_msg_type MSG_TYPE>
	struct Rpc_function_msg_size;

	template <typename RPC_FUNCTION>
	struct Rpc_function_msg_size<RPC_FUNCTION, RPC_CALL> {
		enum { Value = Rpc_msg_payload_size<RPC_FUNCTION, true, false>::Value
		             + sizeof(Rpc_opcode) }; };

	template <typename RPC_FUNCTION>
	struct Rpc_function_msg_size<RPC_FUNCTION, RPC_REPLY> {
		enum { Value = Rpc_msg_payload_size<RPC_FUNCTION, false, true>::Value
		             + Rpc_retval_size<typename RPC_FUNCTION::Ret_type>::Value
		             + sizeof(Rpc_exception_code) }; };


	/**
	 * Calculate size of message buffer needed for a list of RPC functions
	 *
	 * \param RPC_FUNCTIONS  type list of RPC functions
	 *
	 * The returned 'Value' is the maximum of all function's message sizes.
	 */
	template <typename RPC_FUNCTIONS, Rpc_msg_type MSG_TYPE>
	struct Rpc_function_list_msg_size {
		enum {
			This_size = Rpc_function_msg_size<typename RPC_FUNCTIONS::Head, MSG_TYPE>::Value,
			Tail_size = Rpc_function_list_msg_size<typename RPC_FUNCTIONS::Tail, MSG_TYPE>::Value,
			Value     = (This_size > Tail_size) ? This_size : Tail_size }; };

	template <Rpc_msg_type MSG_TYPE>
	struct Rpc_function_list_msg_size<Meta::Empty, MSG_TYPE> { enum { Value = 0 }; };


	/**
	 * Calculate size of message buffer needed for an RPC interface
	 *
	 * \param RPC_IF  class that hosts the RPC interface declaration
	 *
	 * This is a convenience wrapper for 'Rpc_function_list_msg_size'.
	 */
	template <typename RPC_IF, Rpc_msg_type MSG_TYPE>
	struct Rpc_interface_msg_size {
		typedef typename RPC_IF::Rpc_functions Rpc_functions;
		enum { Value = Rpc_function_list_msg_size<Rpc_functions, MSG_TYPE>::Value }; };


	/**
	 * Determine if a RPC interface is inherited
	 */
	template <typename INTERFACE>
	struct Rpc_interface_is_inherited
	{
		typedef char yes[1];
		typedef char  no[2];

		template <typename IF>
		static yes &test(typename IF::Rpc_inherited_interface *);

		template <typename>
		static no &test(...);

		enum { VALUE = sizeof(test<INTERFACE>(0)) == sizeof(yes) };
	};
}

#endif /* _INCLUDE__BASE__RPC_H_ */
