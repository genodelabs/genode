/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \author Stefan Kalkowski
 * \date   2011-11-03
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <board.h>
#include <cpu.h>

using namespace Genode;

unsigned Cpu::executing_id() { return Mpidr::Aff_0::get(Mpidr::read()); }


unsigned Cpu::primary_id() { return Board::PRIMARY_MPIDR_AFF_0; }
