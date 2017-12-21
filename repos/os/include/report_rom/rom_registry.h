/*
 * \brief  Interfaces for the registry of ROM modules
 * \author Norman Feske
 * \date   2014-01-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REPORT_ROM__ROM_REGISTRY_H_
#define _INCLUDE__REPORT_ROM__ROM_REGISTRY_H_

#include <report_rom/rom_module.h>

namespace Rom {
	struct Registry_for_reader;
	struct Registry_for_writer;
}


struct Rom::Registry_for_reader : Interface
{
	/**
	 * Exception type
	 */
	class Lookup_failed { };

	/**
	 * Lookup ROM module for given ROM session label
	 *
	 * \throw Lookup_failed
	 */
	virtual Readable_module &lookup(Reader &reader,
	                                Module::Name const &rom_label) = 0;

	virtual void release(Reader &reader, Readable_module &module) = 0;
};


struct Rom::Registry_for_writer : Interface
{
	virtual Module &lookup(Writer &writer, Module::Name const &name) = 0;

	virtual void release(Writer &writer, Module &module) = 0;
};

#endif /* _INCLUDE__REPORT_ROM__ROM_REGISTRY_H_ */
