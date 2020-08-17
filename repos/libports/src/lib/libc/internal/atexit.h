/*
 * \brief  POSIX atexit handling
 * \author Norman Feske
 * \date   2020-08-17
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__ATEXIT_H_
#define _LIBC__INTERNAL__ATEXIT_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <util/list.h>
#include <base/mutex.h>

/* libc-internal includes */
#include <internal/init.h>
#include <internal/types.h>

namespace Libc { struct Atexit; }


struct Libc::Atexit : Noncopyable
{
	private:

		using Allocator = Genode::Allocator;

		Allocator &_alloc;

		struct Handler : List<Handler>::Element
		{
			enum class Type { STD, CXA };

			Type type;

			void (*std_func)(void);
			void (*cxa_func)(void *);

			void *fn_arg;   /* argument for CXA callback */
			void *dso;      /* shared object handle */

			void execute()
			{
				switch (type) {
				case Type::CXA: cxa_func(fn_arg); break;
				case Type::STD: std_func();       break;
				}
			}
		};

		Mutex _mutex { };

		List<Handler> _handlers { };

	public:

		Atexit(Allocator &alloc) : _alloc(alloc) { }

		void register_cxa_handler(void (*func)(void*), void *arg, void *dso)
		{
			Mutex::Guard guard(_mutex);

			_handlers.insert(
				new (_alloc) Handler { .type     = Handler::Type::CXA,
				                       .std_func = nullptr,
				                       .cxa_func = func,
				                       .fn_arg   = arg,
				                       .dso      = dso });
		}

		void register_std_handler(void (*func)())
		{
			Mutex::Guard guard(_mutex);

			_handlers.insert(
				new (_alloc) Handler { .type     = Handler::Type::STD,
				                       .std_func = func,
				                       .cxa_func = nullptr,
				                       .fn_arg   = nullptr,
				                       .dso      = nullptr });
		}

		/*
		 * Execute all exit handler for shared object 'dso'.
		 *
		 * If 'dso' is nullptr, all remaining handlers are executed.
		 */
		void execute_handlers(void *dso)
		{
			auto try_execute_one_matching_handler = [&] ()
			{
				Handler *handler_ptr = nullptr;

				/* search matching handler */
				{
					Mutex::Guard guard(_mutex);

					handler_ptr = _handlers.first();
					while (handler_ptr && dso && (handler_ptr->dso != dso)) {
						handler_ptr = handler_ptr->next();
					}
				}

				if (!handler_ptr)
					return false;

				handler_ptr->execute();
				_handlers.remove(handler_ptr);
				destroy(_alloc, handler_ptr);
				return true;
			};

			while (try_execute_one_matching_handler());
		}
};


namespace Libc { void execute_atexit_handlers_in_application_context(); }

#endif /* _LIBC__INTERNAL__ATEXIT_H_ */
