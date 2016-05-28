/*
 * \brief   Armv6 translation table for core
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-02-22
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__SPEC__ARM_V6__TRANSLATION_TABLE_H_
#define _CORE__INCLUDE__SPEC__ARM_V6__TRANSLATION_TABLE_H_

/* core includes */
#include <spec/arm/short_translation_table.h>

constexpr unsigned Genode::Translation::_device_tex() { return 0; }

#endif /* _CORE__INCLUDE__SPEC__ARM_V6__TRANSLATION_TABLE_H_ */
