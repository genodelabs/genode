/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

/**
 * Return base-name portion of null-terminated path string
 */
static inline char const *basename(char const *path)
{
	char const *start = path;

	for (; *path; path++)
		if (*path == '/')
			start = path + 1;

	return start;
}


/**
 * Return true if specified path is a base name (contains no path delimiters)
 */
static inline bool is_basename(char const *path)
{
	for (; *path; path++)
		if (*path == '/')
			return false;

	return true;
}


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
 * Return true if 'str' is a valid node name
 */
static inline bool valid_name(char const *str)
{
	if (string_contains(str, '/')) return false;

	/* must have at least one character */
	if (str[0] == 0) return false;

	return true;
}

#endif /* _UTIL_H_ */
