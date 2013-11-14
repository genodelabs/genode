/*
 * \brief  Placement new operator
 * \author Martin Stein
 * \date   2013-11-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLACEMENT_NEW_H_
#define _PLACEMENT_NEW_H_

/**
 * Placement new operator
 */
inline void *operator new(Genode::size_t, void *at) { return at; }

#endif /* _PLACEMENT_NEW_H_ */
