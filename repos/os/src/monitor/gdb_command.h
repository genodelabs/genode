/*
 * \brief  Interfaces for providing GDB commands
 * \author Norman Feske
 * \date   2023-05-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GDB_COMMAND_H_
#define _GDB_COMMAND_H_

#include <base/registry.h>
#include <monitor/string.h>
#include <types.h>

namespace Monitor { namespace Gdb {

	struct State;
	struct Command;
	struct Command_with_separator;
	struct Command_without_separator;

	using Commands = Registry<Command>;
} }


struct Monitor::Gdb::Command : private Commands::Element, Interface
{
	using Name = String<32>;

	Name const name;

	Command(Commands &commands, Name const &name)
	:
		Commands::Element(commands, *this), name(name)
	{ }

	void _match_name(Const_byte_range_ptr const &bytes, auto const &match_remainder_fn) const
	{
		with_skipped_prefix(bytes, name, match_remainder_fn);
	}

	struct With_args_fn : Interface
	{
		virtual void call(Const_byte_range_ptr const &args) const = 0;
	};

	virtual void _with_args(Const_byte_range_ptr const &, With_args_fn const &) const = 0;

	void with_args(Const_byte_range_ptr const &command_bytes, auto const &fn) const
	{
		using Fn = typeof(fn);
		struct Impl : With_args_fn
		{
			Fn const &_fn;
			Impl(Fn const &fn) : _fn(fn) { }
			void call(Const_byte_range_ptr const &args) const override { _fn(args); }
		};
		_with_args(command_bytes, Impl(fn));
	}

	virtual void execute(State &, Const_byte_range_ptr const &args, Output &) const = 0;

	/**
	 * Argument-separating character
	 */
	struct Sep { char value; };

	/**
	 * Call 'fn' for each semicolon-separated argument
	 */
	static void for_each_argument(Const_byte_range_ptr const &args, Sep sep, auto const &fn)
	{
		char const *start     = args.start;
		size_t      num_bytes = args.num_bytes;

		for (; num_bytes > 0;) {

			auto field_len = [] (char sep, Const_byte_range_ptr const &arg)
			{
				size_t n = 0;
				for ( ; (n < arg.num_bytes) && (arg.start[n] != sep); n++);
				return n;
			};

			size_t const arg_len = field_len(sep.value, { start, num_bytes });

			fn(Const_byte_range_ptr(start, arg_len));

			auto skip = [&] (size_t n)
			{
				if (num_bytes >= n) {
					start     += n;
					num_bytes -= n;
				}
			};

			skip(arg_len);  /* argument */
			skip(1);        /* separating semicolon */
		}
	}

	/**
	 * Call 'fn' with the Nth semicolon-separated argument
	 */
	static void with_argument(Const_byte_range_ptr const &args, Sep const sep,
	                          unsigned const n, auto const &fn)
	{
		unsigned i = 0;
		for_each_argument(args, sep, [&] (Const_byte_range_ptr const &arg) {
			if (n == i++)
				fn(arg); });
	}

	/**
	 * Call 'fn' with pointer to 'arg' as null-terminated string
	 *
	 * Note that the argument length is limited by the bounds of the locally
	 * allocated buffer, which is dimensioned for parsing number arguments.
	 */
	static void with_null_terminated(Const_byte_range_ptr const &arg, auto const &fn)
	{
		char null_terminated[20] { };
		memcpy(null_terminated, arg.start,
		       min(sizeof(null_terminated) - 1, arg.num_bytes));
		fn(const_cast<char const *>(null_terminated));
	}

	/**
	 * Return Nth comma-separated hexadecimal number from args
	 */
	template <typename T>
	static T comma_separated_hex_value(Const_byte_range_ptr const &args,
	                                   unsigned const n, T const default_value)
	{
		T result { default_value };
		with_argument(args, Sep {','}, n, [&] (Const_byte_range_ptr const &arg) {
			with_null_terminated(arg, [&] (char const *str) {
				ascii_to_unsigned<T>(str, result, 16); }); });
		return result;
	}
};


struct Monitor::Gdb::Command_with_separator : Command
{
	using Command::Command;

	void _match_separator(Const_byte_range_ptr const &bytes, auto const &match_remainder_fn) const
	{
		if (bytes.num_bytes == 0)
			return;

		char const c = bytes.start[0];

		if ((c == ',') || (c == ';') || (c || ':'))
			with_skipped_bytes(bytes, 1, match_remainder_fn);
	}

	void _with_args(Const_byte_range_ptr const &bytes,
	                With_args_fn         const &fn) const override
	{
		_match_name(bytes, [&] (Const_byte_range_ptr const &bytes) {
			_match_separator(bytes, [&] (Const_byte_range_ptr const &args) {
				fn.call(args); }); });
	}
};


struct Monitor::Gdb::Command_without_separator : Command
{
	using Command::Command;

	void _with_args(Const_byte_range_ptr const &bytes,
	                With_args_fn         const &fn) const override
	{
		_match_name(bytes, [&] (Const_byte_range_ptr const &args) {
			fn.call(args); });
	}
};

#endif /* _GDB_COMMAND_H_ */
