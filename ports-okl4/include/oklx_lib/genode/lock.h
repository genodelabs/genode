/*
 * \brief  Genode C API lock functions needed by OKLinux
 * \author Stefan Kalkowski
 * \date   2009-08-17
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__GENODE__LOCK_H_
#define _INCLUDE__OKLINUX_SUPPORT__GENODE__LOCK_H_

/**
 * Allocate a lock
 *
 * \return   virtual address of the lock
 */
void *genode_alloc_lock(void);

/**
 * Free a lock
 *
 * \param lock_addr  virtual address of the lock
 */
void genode_free_lock(void* lock_addr);

/**
 * Lock the lock
 *
 * \param lock_addr  virtual address of the lock
 */
void genode_lock(void* lock_addr);

/**
 * Unlock the lock
 *
 * \param lock_addr  virtual address of the lock
 */
void genode_unlock(void* lock_addr);

#endif //_INCLUDE__OKLINUX_SUPPORT__GENODE__LOCK_H_
