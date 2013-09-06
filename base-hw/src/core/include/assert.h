/*
 * \brief  Assertion macro
 * \author Martin Stein
 * \date   2012-04-04
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__ASSERT_H_
#define _CORE__INCLUDE__ASSERT_H_

/* Genode includes */
#include <base/printf.h>

enum { CHECK_ASSERTIONS = 0 };

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
		if (CHECK_ASSERTIONS) { \
			if (!(expression)) { \
				PERR("Assertion failed: "#expression""); \
				PERR("  File: %s:%d", __FILE__, __LINE__); \
				PERR("  Function: %s", __PRETTY_FUNCTION__); \
				while (1) ; \
			} \
		} \
	} while (0) ;

#endif /* _CORE__INCLUDE__ASSERT_H_ */

