/*
 * \brief  Framework for component internal modularization
 * \author Martin Stein
 * \date   2023-02-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__MODULE_H_
#define _TRESOR__MODULE_H_

/* base includes */
#include <util/string.h>
#include <base/log.h>

/* tresor includes */
#include <tresor/verbosity.h>
#include <tresor/construct_in_buf.h>

namespace Tresor {

	using namespace Genode;

	using Module_id = uint64_t;
	using Module_request_id = uint64_t;

	enum {
		INVALID_MODULE_ID = ~0UL,
		INVALID_MODULE_REQUEST_ID = ~0UL,
	};

	enum Module_id_enum : Module_id
	{
		CRYPTO               = 0,
		CLIENT_DATA          = 1,
		TRUST_ANCHOR         = 2,
		COMMAND_POOL         = 3,
		BLOCK_IO             = 4,
		CACHE                = 5,
		META_TREE            = 6,
		FREE_TREE            = 7,
		VIRTUAL_BLOCK_DEVICE = 8,
		SUPERBLOCK_CONTROL   = 9,
		BLOCK_ALLOCATOR      = 10,
		VBD_INITIALIZER      = 11,
		FT_INITIALIZER       = 12,
		SB_INITIALIZER       = 13,
		REQUEST_POOL         = 14,
		SB_CHECK             = 15,
		VBD_CHECK            = 16,
		FT_CHECK             = 17,
		FT_RESIZING          = 18,
		MAX_MODULE_ID        = 18,
	};

	char const *module_name(Module_id module_id);

	class Module_request;
	class Module;
	class Module_composition;
}


class Tresor::Module_request : public Interface
{
	private:

		Module_id         _src_module_id  { INVALID_MODULE_ID };
		Module_request_id _src_request_id { INVALID_MODULE_REQUEST_ID };
		Module_id         _dst_module_id  { INVALID_MODULE_ID };
		Module_request_id _dst_request_id { INVALID_MODULE_REQUEST_ID };

	public:

		Module_request() { }

		Module_request(Module_id         src_module_id,
		               Module_request_id src_request_id,
		               Module_id         dst_module_id);

		void dst_request_id(Module_request_id id) { _dst_request_id = id; }

		String<32> src_request_id_str() const;

		String<32> dst_request_id_str() const;

		virtual void print(Output &) const = 0;

		virtual ~Module_request() { }


		/***************
		 ** Accessors **
		 ***************/

		Module_id         src_module_id() const { return _src_module_id; }
		Module_request_id src_request_id() const { return _src_request_id; }
		Module_id         dst_module_id() const { return _dst_module_id; }
		Module_request_id dst_request_id() const { return _dst_request_id; }
};


class Tresor::Module : public Interface
{
	private:

		virtual bool _peek_completed_request(uint8_t *,
		                                     size_t   )
		{
			return false;
		}

		virtual void _drop_completed_request(Module_request &)
		{
			class Exception_1 { };
			throw Exception_1 { };
		}

		virtual bool _peek_generated_request(uint8_t *,
		                                     size_t   )
		{
			return false;
		}

		virtual void _drop_generated_request(Module_request &)
		{
			class Exception_1 { };
			throw Exception_1 { };
		}

	public:

		enum Handle_request_result { REQUEST_HANDLED, REQUEST_NOT_HANDLED };

		typedef Handle_request_result (
			*Handle_request_function)(Module_request &req);

		virtual bool ready_to_submit_request() { return false; };

		virtual void submit_request(Module_request &)
		{
			class Exception_1 { };
			throw Exception_1 { };
		}

		virtual void execute(bool &) { }

		template <typename FUNC>
		void for_each_generated_request(FUNC && handle_request)
		{
			uint8_t buf[4000];
			while (_peek_generated_request(buf, sizeof(buf))) {

				Module_request &req = *(Module_request *)buf;
				switch (handle_request(req)) {
				case Module::REQUEST_HANDLED:

					_drop_generated_request(req);
					break;

				case Module::REQUEST_NOT_HANDLED:

					return;
				}
			}
		}

		virtual void generated_request_complete(Module_request &)
		{
			class Exception_1 { };
			throw Exception_1 { };
		}

		template <typename FUNC>
		void for_each_completed_request(FUNC && handle_request)
		{
			uint8_t buf[4000];
			while (_peek_completed_request(buf, sizeof(buf))) {

				Module_request &req = *(Module_request *)buf;
				handle_request(req);
				_drop_completed_request(req);
			}
		}

		virtual ~Module() { }
};


class Tresor::Module_composition
{
	private:

		Module *_module_ptrs[MAX_MODULE_ID + 1] { };

	public:

		void add_module(Module_id  module_id,
		                Module        &module)
		{
			if (module_id > MAX_MODULE_ID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] != nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = &module;
		}

		void remove_module(Module_id  module_id)
		{
			if (module_id > MAX_MODULE_ID) {
				class Exception_1 { };
				throw Exception_1 { };
			}
			if (_module_ptrs[module_id] == nullptr) {
				class Exception_2 { };
				throw Exception_2 { };
			}
			_module_ptrs[module_id] = nullptr;
		}

		void execute_modules(bool &progress)
		{
			for (Module_id id { 0 }; id <= MAX_MODULE_ID; id++) {

				if (_module_ptrs[id] == nullptr)
					continue;

				Module *module_ptr { _module_ptrs[id] };
				module_ptr->execute(progress);
				module_ptr->for_each_generated_request([&] (Module_request &req) {
					if (req.dst_module_id() > MAX_MODULE_ID) {
						class Exception_1 { };
						throw Exception_1 { };
					}
					if (_module_ptrs[req.dst_module_id()] == nullptr) {
						class Exception_2 { };
						throw Exception_2 { };
					}
					Module &dst_module { *_module_ptrs[req.dst_module_id()] };
					if (!dst_module.ready_to_submit_request()) {

						if (VERBOSE_MODULE_COMMUNICATION)
							log(
								module_name(id), " ", req.src_request_id_str(),
								" --", req, "-| ",
								module_name(req.dst_module_id()));

						return Module::REQUEST_NOT_HANDLED;
					}
					dst_module.submit_request(req);

					if (VERBOSE_MODULE_COMMUNICATION)
						log(
							module_name(id), " ", req.src_request_id_str(),
							" --", req, "--> ",
							module_name(req.dst_module_id()), " ",
							req.dst_request_id_str());

					progress = true;
					return Module::REQUEST_HANDLED;
				});
				module_ptr->for_each_completed_request([&] (Module_request &req) {
					if (req.src_module_id() > MAX_MODULE_ID) {
						class Exception_3 { };
						throw Exception_3 { };
					}
					if (VERBOSE_MODULE_COMMUNICATION)
						log(
							module_name(req.src_module_id()), " ",
							req.src_request_id_str(), " <--", req,
							"-- ", module_name(id), " ",
							req.dst_request_id_str());

					Module &src_module { *_module_ptrs[req.src_module_id()] };
					src_module.generated_request_complete(req);
					progress = true;
				});
			}
		}
};

#endif /* _TRESOR__MODULE_H_ */
