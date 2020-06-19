/*
 * \brief  Testing CPU performance
 * \author Stefan Kalkowski
 * \date   2018-10-22
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

extern "C" void          bogomips(unsigned long);
extern "C" unsigned long bogomips_instr_count(void);
