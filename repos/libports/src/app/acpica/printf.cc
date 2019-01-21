/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdarg.h>

extern "C"
void AcpiOsPrintf (const char *fmt, ...)
{ }

extern "C"
void AcpiOsVprintf (const char *fmt, va_list va)
{ }

