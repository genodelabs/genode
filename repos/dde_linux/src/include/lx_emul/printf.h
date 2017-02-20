/*
 * \brief  Debugging utilities
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

void lx_printf(char const *, ...) __attribute__((format(printf, 1, 2)));
void lx_vprintf(char const *, va_list);

#define lx_printfln(args...) \
	do { \
		lx_printf(args); \
		lx_printf("\n"); \
	} while (0);

#define lx_log(doit, msg...)         \
	do {                               \
		if (doit) {                      \
			lx_printf("%s(): ", __func__); \
			lx_printf(msg);                \
			lx_printf("\n");               \
		}                                \
	} while(0)
