/**
 * \brief  Dummy functions not supported by Genode
 * \author Sebastian Sumpf
 * \date   2021-06-24
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <ffi.h>

/* win32 specific functions for x86_32 */
void ffi_closure_STDCALL (ffi_closure *c)  { }
void ffi_closure_REGISTER (ffi_closure *c) { }
void ffi_closure_THISCALL (ffi_closure *c) { }
void ffi_closure_FASTCALL (ffi_closure *c) { }
