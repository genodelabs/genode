/*
 * \brief  Hook functions for bootstrapping a libc-using Genode component
 * \author Norman Feske
 * \date   2016-12-23
 *
 * This interface is implemented by components that use both Genode's API and
 * the libc. For such components, the libc provides the 'Component::construct'
 * function that takes the precautions needed for letting the application use
 * blocking I/O via POSIX functions like 'read' or 'select'. The libc's
 * 'Component::construct' function finally passes control to the application by
 * calling the application-provided 'Libc::Component::construct' function.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LIBC__COMPONENT_H_
#define _INCLUDE__LIBC__COMPONENT_H_

#include <util/meta.h>
#include <vfs/file_system.h>
#include <base/env.h>
#include <base/stdint.h>

namespace Libc { class Env; }


/**
 * Interface to be provided by the component implementation
 */
class Libc::Env : public Genode::Env
{
	private:

		virtual Genode::Xml_node _config_xml() const = 0;

	public:

		/**
		 * Component configuration
		 */
		template <typename FUNC>
		void config(FUNC const &func) const {
			func(_config_xml()); }

		/**
		 * Virtual File System configured for this component
		 */
		virtual Vfs::File_system &vfs() = 0;

		/**
		 * Libc configuration for this component
		 */
		virtual Genode::Xml_node libc_config() = 0;
};


namespace Libc { namespace Component {

	/**
	 * Return stack size of the component's initial entrypoint
	 */
	Genode::size_t stack_size();

	/**
	 * Construct component
	 *
	 * \param env  extended interface to the component's execution environment
	 */
	void construct(Libc::Env &env);
} }


namespace Libc {

	class Application_code : Genode::Interface
	{
		protected:

			/*
			 * Helper to distinguish void from non-void return type
			 */
			template <typename FUNC, typename RET>
			void _execute(RET &ret_out, FUNC const &func) { ret_out = func(); }

			template <typename FUNC>
			void _execute(Genode::Meta::Empty &, FUNC const &func) { func(); }

		public:

			virtual void execute() = 0;
	};

	void execute_in_application_context(Application_code &);

	/**
	 * Execute 'FUNC' lambda in the libc application context
	 *
	 * In order to invoke the libc's I/O functions (in particular 'select',
	 * 'read', 'write', or other functions like 'socket' that indirectly call
	 * them), the libc-calling application code must be executed under the
	 * supervision of the libc runtime. This is not the case for signal
	 * handlers or RPC functions that are executed in the context of the
	 * 'Genode::Entrypoint'. The 'with_libc' function allows such handlers to
	 * interact with the libc by deliberately subjecting the specified function
	 * ('func') to the libc runtime.
	 */
	template <typename FUNC>
	auto with_libc(FUNC const &func)
	-> typename Genode::Trait::Call_return
	       <typename Genode::Trait::Functor
	            <decltype(&FUNC::operator())>::Return_type>::Type
	{
		using Functor     = Genode::Trait::Functor<decltype(&FUNC::operator())>;
		using Return_type = typename Genode::Trait::Call_return<typename Functor::Return_type>::Type;

		/*
		 * Implementation of the 'Application_code' interface that executes
		 * the code in the 'func' lambda.
		 */
		struct Application_code_func : Application_code
		{
			FUNC const &func;
			Return_type retval { };
			void execute() override { _execute(retval, func); }
			Application_code_func(FUNC const &func) : func(func) { }
		} application_code_func { func };

		execute_in_application_context(application_code_func);

		return application_code_func.retval;
	}
}

#endif /* _INCLUDE__LIBC__COMPONENT_H_ */
