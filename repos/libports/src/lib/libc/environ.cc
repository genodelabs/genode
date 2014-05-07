/*
 * \brief  Pointer to environment
 * \author Norman Feske
 * \date   2011-08-23
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/*
 * Because we have no UNIX environment, we set this pointer to NULL. However,
 * an external library or a libc plugin may initialize 'environ' to point to a
 * meaningful environment.
 */
char **environ = (char **)0;

