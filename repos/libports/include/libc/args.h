/*
 * \brief  Populate arguments and environment from config
 * \author Christian Prochaska
 * \date   2020-08-14
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC__ARGS_H_
#define _INCLUDE__LIBC__ARGS_H_

/* Genode includes */
#include <libc/component.h>
#include <base/node.h>

/* libc includes */
#include <stdlib.h> /* 'malloc' */

static void populate_args_and_env(Libc::Env &env, int &argc, char **&argv, char **&envp)
{
	using namespace Genode;

	auto with_raw_attr = [] (Node const &node, auto const attr_name, auto const &fn)
	{
		bool found = false;
		node.for_each_attribute([&] (Node::Attribute const &attr) {
			if (!found && attr.name == attr_name) {
				fn(attr.value.start, attr.value.num_bytes);
				found = true;
			}
		});
	};

	auto has_quoted_content = [] (Node const &node)
	{
		bool result = false;
		node.for_each_quoted_line([&] (auto const &) { result = true; });
		return result;
	};

	auto with_legacy_arg = [] (Node const &node, auto const &fn)
	{
		if (node.has_type("arg") && node.has_attribute("value"))
			fn(node);
	};

	auto with_arg = [] (Node const &node, auto const &fn)
	{
		if (node.has_type("arg") && !node.has_attribute("value"))
			fn(node);
	};

	auto with_legacy_env = [] (Node const &node, auto const &fn)
	{
		if (node.has_type("env") && node.has_attribute("key") && node.has_attribute("value"))
			fn(node);
	};

	auto with_env = [] (Node const &node, auto const &fn)
	{
		if (node.has_type("env") && node.has_attribute("name") && !node.has_attribute("key"))
			fn(node);
	};

	env.with_config([&] (Node const &node) {

		int envc = 0;

		/* count the number of arguments and environment variables */
		node.for_each_sub_node([&] (Node const &node) {
			with_arg(node, [&] (Node const &arg) {
				if (arg.has_attribute("name")) argc++;
				if (has_quoted_content(arg))   argc++;
			});
			with_legacy_arg(node, [&] (Node const &arg) {
				if (arg.has_attribute("name")) argc++;
				argc++;
			});
			with_env       (node, [&] (Node const &) { envc++; });
			with_legacy_env(node, [&] (Node const &) { envc++; });
		});

		if (argc == 0 && envc == 0) {
			/*
			 * If argc is zero then argv is still a NULL-terminated array.
			 */
			static char const *args[] = { nullptr, nullptr };
			argc = 0;
			argv = (char**)&args;
			envp = &argv[1];
			return; /* from lambda */
		}

		/* arguments and environment are arranged System V style (but don't count on it) */
		argv = (char**)malloc((argc + envc + 2) * sizeof(char*));
		envp = &argv[argc+1];

		/* read the arguments */
		int arg_i = 0;
		int env_i = 0;
		node.for_each_sub_node([&] (Node const &node) {

			with_arg(node, [&] (Node const &node) {

				/* generate tag argument from name attribute */
				with_raw_attr(node, "name", [&] (char const *start, size_t length) {
					size_t const size = length + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					Genode::copy_cstring(argv[arg_i], start, size);
					++arg_i;
				});

				if (has_quoted_content(node)) {

					size_t const size = num_printed_bytes(Node::Quoted_content(node))
					                  + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					bool const ok = Byte_range_ptr(argv[arg_i], size - 1)
						.as_output([&] (Output &out) {
							print(out, Node::Quoted_content(node)); })
						.ok();

					if (!ok) warning("libc arg buffer exceeded");

					argv[arg_i][size - 1] = 0; /* terminate cstring */
					++arg_i;
				}
			});

			with_legacy_arg(node, [&] (Node const &node) {

				with_raw_attr(node, "name", [&] (char const *start, size_t length) {
					size_t const size = length + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					Genode::copy_cstring(argv[arg_i], start, size);
					++arg_i;
				});

				with_raw_attr(node, "value", [&] (char const *start, size_t length) {

					size_t const size = length + 1; /* for null termination */

					argv[arg_i] = (char *)malloc(size);

					Genode::copy_cstring(argv[arg_i], start, size);
				});
				++arg_i;
			});

			/*
			 * An environment variable has the form <key>=<value>, followed
			 * by a terminating zero.
			 */
			with_env(node, [&] (Node const &node) {

				Node::Quoted_content const content(node);

				with_raw_attr(node, "name", [&] (char const *name, size_t name_len) {

					size_t const size = name_len + 1 + num_printed_bytes(content) + 1;

					envp[env_i] = (char*)malloc(size);

					bool const ok = Byte_range_ptr(envp[env_i], size - 1)
						.as_output([&] (Output &out) {
							print(out, Cstring(name, name_len), "=", content); })
						.ok();

					if (!ok) warning("libc env buffer exceeded");

					envp[env_i][size - 1] = 0; /* terminate cstring */
				});
				++env_i;
			});

			with_legacy_env(node, [&] (Node const &node) {

				auto check_attr = [] (Node const &node, auto key) {
					if (!node.has_attribute(key))
						Genode::warning("<env> node lacks '", key, "' attribute"); };

				check_attr(node, "key");
				check_attr(node, "value");

				size_t var_size = 1 + 1;
				with_raw_attr(node, "key",   [&] (char const *, size_t l) { var_size += l; });
				with_raw_attr(node, "value", [&] (char const *, size_t l) { var_size += l; });

				envp[env_i] = (char*)malloc(var_size);

				size_t pos = 0;

				/* append characters to env variable with zero termination */
				auto append = [&] (char const *s, size_t len) {

					if (pos + len >= var_size) {
						/* this should never happen */
						warning("truncated environment variable: ", node);
						return;
					}

					copy_cstring(envp[env_i] + pos, s, len + 1);
					pos += len;
				};

				with_raw_attr(node, "key", [&] (char const *start, size_t length) {
					append(start, length); });

				append("=", 1);

				with_raw_attr(node, "value", [&] (char const *start, size_t length) {
					append(start, length); });

				++env_i;
			});
		});

		/* argv and envp are both NULL terminated */
		argv[arg_i] = NULL;
		envp[env_i] = NULL;
	});
}

#endif /* _INCLUDE__LIBC__ARGS_H_ */
