/*
 * \brief  Assertion macro
 * \author Martin Stein
 * \author Stefan Kalkowski
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__ASSERT_H_
#define _SRC__LIB__HW__ASSERT_H_

/* Genode includes */
#include <base/log.h>

#ifdef GENODE_RELEASE

/**
 * Suppress assertions in release version
 */
#define assert(expression)
#else

/**
 * Make an assertion
 *
 * \param expression  statement that must be true
 *
 * Use this macro as if it could always be empty as well.
 * I.e. it should not be used with expressions that are relevant
 * to the protection against another, untrusted PD or expressions
 * that contain mandatory function calls! A good rule of thumb
 * is to use it only for the protection of a component against
 * a PD-local interface misuse that can't be avoided due to language
 * constraints (e.g. inaccuracy of integer ranges).
 */
#define assert(expression) \
	do { \
		if (!(expression)) { \
			Genode::error("Assertion failed: "#expression""); \
			Genode::error("  File: ", __FILE__, ":", __LINE__); \
			Genode::error("  Function: ", __PRETTY_FUNCTION__); \
			while (1) ; \
		} \
	} while (0) ;

#endif /* GENODE_RELEASE */


#endif /* _SRC__LIB__HW__ASSERT_H_ */
