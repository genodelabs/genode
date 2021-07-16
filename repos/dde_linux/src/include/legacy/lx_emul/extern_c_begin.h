/*
 * \brief  Include before including Linux headers in C++
 * \author Christian Helmuth
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifdef __cplusplus

#define extern_c_begin

extern "C" {

/* some warnings should only be switched of for Linux headers */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wsign-compare"

/* deal with C++ keywords used for identifiers etc. */
#define private   private_
#define class     class_
#define new       new_
#define delete    delete_
#define namespace namespace_
#define virtual   virtual_

#endif /* __cplusplus */
