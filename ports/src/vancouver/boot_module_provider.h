/*
 * \brief  Back end used for obtaining multi-boot modules
 * \author Norman Feske
 * \date   2011-11-20
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _BOOT_MODULE_PROVIDER_H_
#define _BOOT_MODULE_PROVIDER_H_

/* Genode includes */
#include <util/string.h>
#include <util/misc_math.h>
#include <util/xml_node.h>
#include <base/env.h>
#include <base/printf.h>
#include <rom_session/connection.h>


class Boot_module_provider
{
	private:

		Genode::Xml_node _multiboot_node;

		enum { MODULE_NAME_MAX_LEN = 48 };

	public:

		/**
		 * Exception class
		 */
		class Destination_buffer_too_small { };
		class Module_loading_failed { };

		/**
		 * Constructor
		 *
		 * \param multiboot_node  XML node containing the list of boot modules
		 *                        as sub nodes
		 */
		Boot_module_provider(Genode::Xml_node multiboot_node)
		:
			_multiboot_node(multiboot_node)
		{ }


		/************************************
		 ** Boot_module_provider interface **
		 ************************************/

		/**
		 * Copy module data to specified buffer
		 *
		 * \return module size in bytes, or 0 if module does not exist
		 * \throw  Destination_buffer_too_small
		 */
		Genode::size_t data(int module_index,
		                    void *dst, Genode::size_t dst_len) const
		{
			using namespace Genode;

			try {
				Xml_node mod_node = _multiboot_node.sub_node(module_index);

				if (mod_node.has_type("rom")) {

					/*
					 * Determine ROM file name, which is specified as 'name'
					 * attribute of the 'rom' node.
					 */
					char name[MODULE_NAME_MAX_LEN];
					mod_node.attribute("name").value(name, sizeof(name));

					/*
					 * Open ROM session
					 */
					Rom_connection rom(name);
					Dataspace_capability ds = rom.dataspace();
					size_t const src_len = Dataspace_client(ds).size();

					if (src_len > dst_len)
						throw Destination_buffer_too_small();

					void * const src = env()->rm_session()->attach(ds);

					/*
					 * Copy content to destination buffer
					 */
					memcpy(dst, src, src_len);

					/*
					 * Detach ROM dataspace from local address space. The ROM
					 * session will be closed automatically when we leave the
					 * current scope and the 'rom' object gets destructed.
					 */
					env()->rm_session()->detach(src);

					return src_len;
				}

				PWRN("XML node %d in multiboot node has unexpected type",
				     module_index);

				throw Module_loading_failed();
			}
			catch (Xml_node::Nonexistent_sub_node) { }
			catch (Xml_node::Nonexistent_attribute) { }
			catch (...) {
				throw Module_loading_failed();
			}

			/*
			 * We should get here only if there are XML parsing errors
			 */
			return 0;
		}

		/**
		 * Copy command line to specified buffer
		 *
		 * \return length of command line in bytes, or 0 if module does not exist
		 */
		Genode::size_t cmdline(int module_index,
		                       char *dst, Genode::size_t dst_len) const
		{
			using namespace Genode;

			try {
				Xml_node mod_node = _multiboot_node.sub_node(module_index);

				if (mod_node.has_type("rom")) {

					size_t cmd_len = 0;

					char name[MODULE_NAME_MAX_LEN];
					mod_node.attribute("name").value(name, sizeof(name));

					size_t const name_len = Genode::strlen(name);

					/*
					 * Check if destination buffer can hold the name including
					 * the zero termination.
					 */
					if (name_len + 1 >= dst_len)
						return 0;

					/* copy name to command line */
					strncpy(&dst[cmd_len], name, name_len + 1);
					cmd_len += name_len;

					/* check if name fills entire destination buffer */
					if (cmd_len + 1 == dst_len) {
						dst[cmd_len++] = 0;
						return cmd_len;
					}

					try {
						Xml_node::Attribute cmdline_attr = mod_node.attribute("cmdline");

						/* add single space between name and arguments */
						dst[cmd_len++] = ' ';
						if (cmd_len + 1 == dst_len) {
							dst[cmd_len++] = 0;
							return cmd_len;
						}

						/* copy 'cmdline' attribute to destination buffer */
						cmdline_attr.value(&dst[cmd_len], dst_len - cmd_len);

						/*
						 * The string returned by the 'value' function is
						 * zero-terminated. Count and return the total number
						 * of command-line characters.
						 */
						return Genode::strlen(dst);

					} catch (Xml_node::Nonexistent_attribute) { }

					return cmd_len;
				}

				PWRN("XML node %d in multiboot node has unexpected type",
				     module_index);

				return 0;
			}
			catch (Xml_node::Nonexistent_sub_node) { }

			/*
			 * We should get here only if there are XML parsing errors
			 */
			return 0;
		}
};

#endif /* _BOOT_MODULE_PROVIDER_H_ */
