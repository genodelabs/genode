/*
 * \brief Translation table definitions for core
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date 2012-02-22
 */
/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _TRANSLATION_TABLE_H_
#define _TRANSLATION_TABLE_H_

/* core includes */
#include <short_translation_table.h>

constexpr unsigned Genode::Translation::_device_tex() { return 2; }

#endif /* _TRANSLATION_TABLE_H_ */
