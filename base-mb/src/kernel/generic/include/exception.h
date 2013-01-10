/*
 * \brief  Handling of concrete set of hardware-exceptions
 * \author Martin stein
 * \date   2010-06-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__CORE__KERNEL__INCLUDE__EXCEPTION_H_
#define _SRC__CORE__KERNEL__INCLUDE__EXCEPTION_H_

#include <base/fixed_stdint.h>

#include "scheduler.h"

using namespace Genode;


/**
 * Exception metadata structure
 */
struct Exception
{
	uint32_t cause;
	uint32_t status;
	uint32_t address;
};


/**
 * Virtual class that qualifies heirs be exception handler
 */
class Exception_handler
{
	protected:

		/**
		 * Enable all hw exceptions and let us be the handler for them
		 */
		void _alloc_exceptions();

		/**
		 * Relieve us of handling any exception
		 *
		 * Dissable all exceptions if we are the current handler.
		 */
		void _free_exceptions();

	public:

		/**
		 * Destructor
		 */
		virtual ~Exception_handler() {}

		/**
		 * Handle occured exception
		 *
		 * \param  type  type of exception - see xmb/include/config.h
		 */
		virtual void handle_exception(uint32_t type, uint32_t status, uint32_t address) = 0;
};


/**
 * C exception handling, after assembler entry
 */
void handle_exception();


/**
 * Clear exception if one is in progress
 */
void _exception_clear();


#endif /* _SRC__CORE__KERNEL__INCLUDE__EXCEPTION_H_ */
