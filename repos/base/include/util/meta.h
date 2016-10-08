/*
 * \brief  Utilities for template-based meta programming
 * \author Norman Feske
 * \date   2011-02-28
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__META_H_
#define _INCLUDE__UTIL__META_H_

namespace Genode {

	namespace Trait {

		/***************************************
		 ** Reference and non-reference types **
		 ***************************************/

		template <typename T> struct Reference     { typedef T& Type; };
		template <typename T> struct Reference<T*> { typedef T* Type; };
		template <typename T> struct Reference<T&> { typedef T& Type; };

		template <typename T> struct Non_reference     { typedef T Type; };
		template <typename T> struct Non_reference<T*> { typedef T Type; };
		template <typename T> struct Non_reference<T&> { typedef T Type; };

		template <typename T> struct Non_const          { typedef T Type; };
		template <typename T> struct Non_const<T const> { typedef T Type; };

		/**
		 * Determine plain-old-data type corresponding to type 'T'
		 */
		template <typename T> struct Pod {
			typedef typename Non_const<typename Non_reference<T>::Type>::Type Type; };

	} /* namespace Trait */

	namespace Meta {

		/***********************************
		 ** Variadic template type access **
		 ***********************************/

		template <unsigned long N, typename HEAD, typename... TAIL>
		struct Variadic_type_tuple
		{
			using Type = typename Variadic_type_tuple<N-1, TAIL...>::Type;
		};

		template <typename HEAD, typename... TAIL>
		struct Variadic_type_tuple<0, HEAD, TAIL...>
		{
			using Type = HEAD;
		};


		/***************
		 ** Type list **
		 ***************/

		/**
		 * Type representing an omitted template argument
		 */
		struct Void { };

		/**
		 * Marker for end of type list
		 */
		struct Empty { };

		/**
		 * Basic building block for creating type lists
		 */
		template <typename HEAD, typename TAIL>
		struct Type_tuple
		{
			typedef HEAD Head;
			typedef TAIL Tail;
		};

		/**
		 * Type list with variable number of types
		 */
		template <typename...>
		struct Type_list;

		template <>
		struct Type_list<> { typedef Empty Head; };

		template <typename T>
		struct Type_list<T> : Type_tuple<T, Empty> { };

		template <typename HEAD, typename... TAIL>
		struct Type_list<HEAD, TAIL...> : Type_tuple<HEAD, Type_list<TAIL...> > { };

		/**
		 * Macro for wrapping the 'Type_list' template
		 *
		 * This macro allows for specifying a type list as macro argument. If we supplied
		 * the 'Type_list' template with the types as arguments, the preprocessor would
		 * take the comma between the type-list arguments as separator for the macro
		 * arguments.
		 */
#define GENODE_TYPE_LIST(...) ::Genode::Meta::Type_list<__VA_ARGS__>

		/**
		 * Calculate the length of typelist 'TL'
		 */
		template <typename TL>
		struct Length { enum { Value = Length<typename TL::Tail>::Value + 1 }; };

		template <> struct Length<Empty> { enum { Value = 0 }; };


		/**
		 * Return index of type 'T' within typelist 'TL'
		 */
		template <typename TL, typename T, unsigned I = 0>
		struct Index_of {
			enum { Value = Index_of<typename TL::Tail, T, I + 1>::Value }; };

		template <typename TL, unsigned I>
		struct Index_of<TL, typename TL::Head, I> { enum { Value = I }; };


		/**
		 * Append type list 'APPENDIX' to type list 'TL'
		 */
		template <typename TL, typename APPENDIX>
		class Append
		{
			/* pass appendix towards the end of the typelist */
			typedef typename Append<typename TL::Tail, APPENDIX>::Type _Tail;

			public:

				/* keep head, replace tail */
				typedef Type_tuple<typename TL::Head, _Tail> Type;
		};


		/* replace end of type list ('Empty' type) with appendix */
		template <typename APPENDIX>
		struct Append<Empty, APPENDIX> { typedef APPENDIX Type; };


		/**
		 * Return type at index 'I' of type list 'TL'
		 */
		template <typename TL, unsigned I>
		struct Type_at {
			typedef typename Type_at<typename TL::Tail, I - 1>::Type Type; };

		/* end recursion if we reached the type */
		template <typename TL>
		struct Type_at<TL, 0U> { typedef typename TL::Head Type; };

		/* end recursion at the end of type list */
		template <unsigned I> struct Type_at<Empty, I> { typedef void Type; };

		/* resolve ambiguous specializations */
		template <> struct Type_at<Empty, 0> { typedef void Type; };


		/**
		 * Statically check if all elements of type list 'CTL' are contained in
		 * type list 'TL'
		 */
		template <typename TL, typename CTL>
		struct Contains {
			enum { Check = Index_of<TL, typename CTL::Head>::Value
			             + Contains<TL, typename CTL::Tail>::Check }; };

		template <typename TL> struct Contains<TL, Empty> { enum { Check = 0 }; };


		/**
		 * Tuple holding references
		 */
		template <typename HEAD, typename TAIL>
		struct Ref_tuple : public Type_tuple<HEAD, TAIL>
		{
			typename Trait::Reference<HEAD>::Type _1;
			typename Trait::Reference<TAIL>::Type _2;

			Ref_tuple(typename Trait::Reference<HEAD>::Type v1,
			          typename Trait::Reference<TAIL>::Type v2) : _1(v1), _2(v2) { }

			typename Trait::Reference<HEAD>::Type get() { return _1; }
		};

		/**
		 * Specialization of 'Ref_tuple' used if the 'HEAD' is a pointer type
		 *
		 * The differentiation between pointer and non-pointer types is
		 * necessary to obtain a reference to the pointed-to object via the
		 * 'get' function when marshalling or unmarshalling a pointer.
		 */
		template <typename HEAD, typename TAIL>
		struct Ref_tuple<HEAD *, TAIL> : public Type_tuple<HEAD *, TAIL>
		{
			HEAD                                 *_1;
			typename Trait::Reference<TAIL>::Type _2;

			Ref_tuple(HEAD *v1, typename Trait::Reference<TAIL>::Type v2)
			: _1(v1), _2(v2) { }

			typename Trait::Reference<HEAD>::Type get() { return *_1; }
		};

		template <typename T1, typename T2, typename T3>
		struct Ref_tuple_3 : public Ref_tuple<T1, Ref_tuple<T2, T3> >
		{
			Ref_tuple<T2, T3> _t2;
			Ref_tuple_3(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3)
			: Ref_tuple<T1, Ref_tuple<T2, T3> >(v1, _t2), _t2(v2, v3) { }
		};

		template <typename T1, typename T2, typename T3, typename T4>
		struct Ref_tuple_4 : public Ref_tuple<T1, Ref_tuple_3<T2, T3, T4> >
		{
			Ref_tuple_3<T2, T3, T4> _t2;
			Ref_tuple_4(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3,
			            typename Trait::Reference<T4>::Type v4)
			: Ref_tuple<T1, Ref_tuple_3<T2, T3, T4> >(v1, _t2), _t2(v2, v3, v4) { }
		};

		template <typename T1, typename T2, typename T3, typename T4, typename T5>
		struct Ref_tuple_5 : public Ref_tuple<T1, Ref_tuple_4<T2, T3, T4, T5> >
		{
			Ref_tuple_4<T2, T3, T4, T5> _t2;
			Ref_tuple_5(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3,
			            typename Trait::Reference<T4>::Type v4,
			            typename Trait::Reference<T5>::Type v5)
			: Ref_tuple<T1, Ref_tuple_4<T2, T3, T4, T5> >(v1, _t2), _t2(v2, v3, v4, v5) { }
		};

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
		struct Ref_tuple_6 : public Ref_tuple<T1, Ref_tuple_5<T2, T3, T4, T5, T6> >
		{
			Ref_tuple_5<T2, T3, T4, T5, T6> _t2;
			Ref_tuple_6(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3,
			            typename Trait::Reference<T4>::Type v4,
			            typename Trait::Reference<T5>::Type v5,
			            typename Trait::Reference<T6>::Type v6)
			: Ref_tuple<T1, Ref_tuple_5<T2, T3, T4, T5, T6> >(v1, _t2), _t2(v2, v3, v4, v5, v6) { }
		};

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
		struct Ref_tuple_7 : public Ref_tuple<T1, Ref_tuple_6<T2, T3, T4, T5, T6, T7> >
		{
			Ref_tuple_6<T2, T3, T4, T5, T6, T7> _t2;
			Ref_tuple_7(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3,
			            typename Trait::Reference<T4>::Type v4,
			            typename Trait::Reference<T5>::Type v5,
			            typename Trait::Reference<T6>::Type v6,
			            typename Trait::Reference<T7>::Type v7)
			: Ref_tuple<T1, Ref_tuple_6<T2, T3, T4, T5, T6, T7> >(v1, _t2), _t2(v2, v3, v4, v5, v6, v7) { }
		};

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7, typename T8>
		struct Ref_tuple_8 : public Ref_tuple<T1, Ref_tuple_7<T2, T3, T4, T5, T6, T7, T8> >
		{
			Ref_tuple_7<T2, T3, T4, T5, T6, T7, T8> _t2;
			Ref_tuple_8(typename Trait::Reference<T1>::Type v1,
			            typename Trait::Reference<T2>::Type v2,
			            typename Trait::Reference<T3>::Type v3,
			            typename Trait::Reference<T4>::Type v4,
			            typename Trait::Reference<T5>::Type v5,
			            typename Trait::Reference<T6>::Type v6,
			            typename Trait::Reference<T7>::Type v7,
			            typename Trait::Reference<T8>::Type v8)
			: Ref_tuple<T1, Ref_tuple_7<T2, T3, T4, T5, T6, T7, T8> >(v1, _t2), _t2(v2, v3, v4, v5, v6, v7, v8) { }
		};

		/**
		 * Tuple holding raw (plain old) data
		 *
		 * Has to be an aggregate type to allow non-default-constructible RPC
		 * arguments. But aggregate types must not derive from a base class in
		 * C++11. So we do not derive from Meta::Type_tuple although it is an
		 * empty class.
		 */
		template <typename HEAD, typename TAIL>
		struct Pod_tuple
		{
			typedef typename Trait::Pod<HEAD>::Type Stored_head;
			Stored_head _1;
			TAIL _2;

			typedef HEAD Head;
			typedef TAIL Tail;

			/**
			 * Accessor for requesting the data reference to '_1'
			 */
			Stored_head &get() { return _1; }
		};

		/**
		 * Specialization of 'Pod_tuple' for pointer types
		 *
		 * For pointer types, the corresponding data structure must be able to
		 * host a copy of the pointed-to object. However, the accessor for data
		 * must be consistent with the input (pointer) type. Hence, the 'get'
		 * function returns a pointer to the stored copy.
		 */
		template <typename HEAD, typename TAIL>
		struct Pod_tuple<HEAD *, TAIL>
		{
			typedef typename Trait::Non_reference<HEAD>::Type Stored_head;
			Stored_head _1;
			TAIL _2;

			typedef HEAD* Head;
			typedef TAIL Tail;

			HEAD *get() { return &_1; }
		};


		/*************************************************************************
		 ** Support for representing function arguments in a normalized fashion **
		 *************************************************************************/

		/**
		 * Return recursive type for holding the specified reference argument types
		 *
		 * Depending on the number of supplied template arguments, a differently
		 * dimensioned type is returned. This template is called with the variable
		 * argument list used by the 'GENODE_RPC' macro and effectifely translates the
		 * argument list to a recursive type that can be processed with template meta
		 * programming. The result of the translation is returned as 'Ref_args::Type'.
		 */
		template <typename T1 = Void, typename T2 = Void, typename T3 = Void,
		          typename T4 = Void, typename T5 = Void, typename T6 = Void,
		          typename T7 = Void, typename T8 = Void>
		struct Ref_args;

		template <>
		struct Ref_args<Void> {
			typedef Empty Type; };

		template <typename T1>
		struct Ref_args<T1, Void> {
			typedef Ref_tuple<T1, Empty> Type; };

		template <typename T1, typename T2>
		struct Ref_args<T1, T2, Void> {
			typedef Ref_tuple_3<T1, T2, Empty> Type; };

		template <typename T1, typename T2, typename T3>
		struct Ref_args<T1, T2, T3, Void> {
			typedef Ref_tuple_4<T1, T2, T3, Empty> Type; };

		template <typename T1, typename T2, typename T3, typename T4>
		struct Ref_args<T1, T2, T3, T4, Void> {
			typedef Ref_tuple_5<T1, T2, T3, T4, Empty> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5>
		struct Ref_args<T1, T2, T3, T4, T5, Void> {
			typedef Ref_tuple_6<T1, T2, T3, T4, T5, Empty> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
		struct Ref_args<T1, T2, T3, T4, T5, T6, Void> {
			typedef Ref_tuple_7<T1, T2, T3, T4, T5, T6, Empty> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
		struct Ref_args<T1, T2, T3, T4, T5, T6, T7, Void> {
			typedef Ref_tuple_8<T1, T2, T3, T4, T5, T6, T7, Empty> Type; };


		/**
		 * Return recursive type for storing the specified data types
		 *
		 * The 'Pod_args' template works analogously to the 'Ref_args' template, except
		 * for returning a type for storing values, not references.
		 */
		template <typename T1 = Void, typename T2 = Void, typename T3 = Void,
		          typename T4 = Void, typename T5 = Void, typename T6 = Void,
		          typename T7 = Void, typename T8 = Void>
		struct Pod_args;

		template <>
		struct Pod_args<Void> { typedef Empty Type; };

		template <typename T1>
		struct Pod_args<T1, Void> { typedef Pod_tuple<T1, Empty> Type; };

		template <typename T1, typename T2>
		struct Pod_args<T1, T2, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, Void>::Type> Type; };

		template <typename T1, typename T2, typename T3>
		struct Pod_args<T1, T2, T3, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, T3, Void>::Type> Type; };

		template <typename T1, typename T2, typename T3, typename T4>
		struct Pod_args<T1, T2, T3, T4, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, T3, T4, Void>::Type> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5>
		struct Pod_args<T1, T2, T3, T4, T5, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, T3, T4, T5, Void>::Type> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
		struct Pod_args<T1, T2, T3, T4, T5, T6, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, T3, T4, T5, T6, Void>::Type> Type; };

		template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6, typename T7>
		struct Pod_args<T1, T2, T3, T4, T5, T6, T7, Void> { typedef Pod_tuple<T1, typename Pod_args<T2, T3, T4, T5, T6, T7, Void>::Type> Type; };


		/**
		 * Helper for calling member functions via a uniform interface
		 *
		 * Member functions differ in their types and numbers of arguments as
		 * well as their return types or their lack of a return type. This
		 * makes them difficult to call from generic template code. The
		 * 'call_member' function template remedies this issue by providing a
		 * wrapper function with a unified signature. For each case, the
		 * compiler generates a new overload of the 'call_member' function. For
		 * each number of function arguments, there exists a pair of overloads,
		 * one used if a return type is present, the other used for functions
		 * with no return value.
		 *
		 * \param RET_TYPE  return type of member function, or 'Meta::Empty'
		 *                  if the function has no return type
		 * \param SERVER    class that hosts the member function to call
		 * \param ARGS      recursively defined 'Pod_args' type composed of
		 *                  the function-argument types expected by the member
		 *                  function
		 * \param ret       reference for storing the return value of the
		 *                  member function
		 * \param server    reference to the object to be used for the call
		 * \param args      function arguments
		 * \param func      pointer-to-member function to invoke
		 */

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &,
		                               RET_TYPE (SERVER::*func)())
		{ return (server.*func)(); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &,
		                               RET_TYPE (SERVER::*func)() const)
		{ return (server.*func)(); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &,
		                               void (SERVER::*func)())
		{ (server.*func)(); return Empty(); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &,
		                               void (SERVER::*func)() const)
		{ (server.*func)(); return Empty(); }

		/* 1 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type))
		{ return (server.*func)(args.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type))
		{ (server.*func)(args.get()); return Empty(); }

		/* 2 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type))
		{ return (server.*func)(args.get(), args._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type))
		{ (server.*func)(args.get(), args._2.get()); return Empty(); }

		/* 3 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type,
		                                                        typename Type_at<ARGS, 2>::Type))
		{ return (server.*func)(args.get(), args._2.get(), args._2._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type,
		                                                    typename Type_at<ARGS, 2>::Type))
		{ (server.*func)(args.get(), args._2.get(), args._2._2.get()); return Empty(); }

		/* 4 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type,
		                                                        typename Type_at<ARGS, 2>::Type,
		                                                        typename Type_at<ARGS, 3>::Type))
		{ return (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type,
		                                                    typename Type_at<ARGS, 2>::Type,
		                                                    typename Type_at<ARGS, 3>::Type))
		{ (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get()); return Empty(); }

		/* 5 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type,
		                                                        typename Type_at<ARGS, 2>::Type,
		                                                        typename Type_at<ARGS, 3>::Type,
		                                                        typename Type_at<ARGS, 4>::Type))
		{ return (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(), args._2._2._2._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type,
		                                                    typename Type_at<ARGS, 2>::Type,
		                                                    typename Type_at<ARGS, 3>::Type,
		                                                    typename Type_at<ARGS, 4>::Type))
		{ (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(), args._2._2._2._2.get()); return Empty(); }

		/* 6 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type,
		                                                        typename Type_at<ARGS, 2>::Type,
		                                                        typename Type_at<ARGS, 3>::Type,
		                                                        typename Type_at<ARGS, 4>::Type,
		                                                        typename Type_at<ARGS, 5>::Type))
		{ return (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(),
		                       args._2._2._2._2.get(), args._2._2._2._2._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type,
		                                                    typename Type_at<ARGS, 2>::Type,
		                                                    typename Type_at<ARGS, 3>::Type,
		                                                    typename Type_at<ARGS, 4>::Type,
		                                                    typename Type_at<ARGS, 5>::Type))
		{ (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(),
		                 args._2._2._2._2.get(), args._2._2._2._2._2.get()); return Empty(); }

		/* 7 */
		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               RET_TYPE (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                        typename Type_at<ARGS, 1>::Type,
		                                                        typename Type_at<ARGS, 2>::Type,
		                                                        typename Type_at<ARGS, 3>::Type,
		                                                        typename Type_at<ARGS, 4>::Type,
		                                                        typename Type_at<ARGS, 5>::Type,
		                                                        typename Type_at<ARGS, 6>::Type))
		{ return (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(),
		                       args._2._2._2._2.get(), args._2._2._2._2._2.get(), args._2._2._2._2._2._2.get()); }

		template <typename RET_TYPE, typename SERVER, typename ARGS>
		static inline RET_TYPE call_member(SERVER &server, ARGS &args,
		                               void (SERVER::*func)(typename Type_at<ARGS, 0>::Type,
		                                                    typename Type_at<ARGS, 1>::Type,
		                                                    typename Type_at<ARGS, 2>::Type,
		                                                    typename Type_at<ARGS, 3>::Type,
		                                                    typename Type_at<ARGS, 4>::Type,
		                                                    typename Type_at<ARGS, 5>::Type,
		                                                    typename Type_at<ARGS, 6>::Type))
		{ (server.*func)(args.get(), args._2.get(), args._2._2.get(), args._2._2._2.get(),
		                 args._2._2._2._2.get(), args._2._2._2._2._2.get(), args._2._2._2._2._2._2.get()); return Empty(); }


		/********************
		 ** Misc utilities **
		 ********************/

		/**
		 * Round unsigned long value to next machine-word-aligned value
		 */
		template <unsigned long SIZE>
		struct Round_to_machine_word {
			enum { Value = (SIZE + sizeof(long) - 1) & ~(sizeof(long) - 1) }; };

		/**
		 * Utility for partial specialization of member function templates
		 *
		 * By passing an artificial 'Overload_selector' argument to a function
		 * template, we can use overloading to partially specify such a
		 * function template. The selection of the overload to use is directed
		 * by one or two types specified as template arguments of
		 * 'Overload_selector'.
		 */
		template <typename T1, typename T2 = T1>
		struct Overload_selector
		{
			/*
			 * Make class unique for different template arguments. The types
			 * are never used.
			 */
			typedef T1 _T1;
			typedef T2 _T2;

			/* prevent zero initialization of objects */
			Overload_selector() { }
		};

		/**
		 * Convert boolean value to type
		 */
		template <bool VALUE> struct Bool_to_type { enum { V = VALUE }; };

	} /* namespace Meta */

	namespace Trait {

		template<typename T> struct Functor;

		template<typename RET, typename T, typename... ARGS>
		struct Functor<RET (T::*)(ARGS...) const>
		{
			static constexpr unsigned long argument_count = sizeof...(ARGS);

			using Return_type = RET;

			template <unsigned long N>
			struct Argument
			{
				static_assert(N < argument_count, "Invalid index");

				using Type =
					typename Meta::Variadic_type_tuple<N, ARGS...>::Type;
			};
		};
	} /* namespace Trait */
}

#endif /* _INCLUDE__UTIL__META_H_ */
