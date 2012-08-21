/*
 * \brief  Utilities
 * \author Norman Feske
 * \author Christian Prochaska
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/* Genode includes */
#include <util/string.h>


/**
 * Return true if character 'c' occurs in null-terminated string 'str'
 */
static inline bool string_contains(char const *str, char c)
{
	for (; *str; str++)
		if (*str == c)
			return true;
	return false;
}


/**
 * Return true if null-terminated string 'substr' occurs in null-terminated
 * string 'str'
 */
static bool string_contains(char const *str, char const *substr)
{
	using namespace Genode;

	size_t str_len = strlen(str);
	size_t substr_len = strlen(substr);

	if (str_len < substr_len)
		return false;

	for (size_t i = 0; i <= (str_len - substr_len); i++)
		if (strcmp(&str[i], substr, substr_len) == 0)
			return true;

	return false;
}


/**
 * Return true if 'str' is a valid file name
 */
static inline bool valid_filename(char const *str)
{
	if (!str) return false;

	/* must have at least one character */
	if (str[0] == 0) return false;

	/* must not contain '/' or '\' or ':' */
	if (string_contains(str, '/') ||
	    string_contains(str, '\\') ||
	    string_contains(str, ':'))
		return false;

	return true;
}


/**
 * Return true if 'str' is a valid path
 */
static inline bool valid_path(char const *str)
{
	if (!str) return false;

	/* must start with '/' */
	if (str[0] != '/')
		return false;

	/* must not contain '\' or ':' */
	if (string_contains(str, '\\') ||
	    string_contains(str, ':'))
		return false;

	/* must not contain "/../" */
	 if (string_contains(str, "/../")) return false;

	return true;
}


/**
 * Return true if 'str' is "/"
 */
static inline bool is_root(const char *str)
{
	return (Genode::strcmp(str, "/") == 0);
}

#endif /* _UTIL_H_ */
