/*
 * \brief  Board driver for core
 * \author Martin Stein
 * \date   2015-02-16
 */

/*
 * Copyright (C) 2012-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;


void Board::prepare_kernel()
{
	/**
	 * FIXME We enable this bit although base-hw doesn't support
	 *       SMP because it fastens RAM access significantly.
	 */
	Cpu::Actlr::access_t actlr = Cpu::Actlr::read();
	Cpu::Actlr::Smp::set(actlr, 1);
	Cpu::Actlr::write(actlr);
}
