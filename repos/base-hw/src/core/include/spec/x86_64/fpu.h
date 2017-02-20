/*
 * \brief  x86_64 FPU driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Martin stein
 * \author Reto Buerki
 * \author Stefan Kalkowski
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SPEC__X86_64__FPU_H_
#define _SPEC__X86_64__FPU_H_

/* Genode includes */
#include <base/stdint.h>

namespace Genode { class Fpu; }

class Genode::Fpu
{
	public:

		class Context
		{
			private:

				friend class Fpu;

				/*
				 * FXSAVE area providing storage for x87 FPU, MMX, XMM,
				 * and MXCSR registers.
				 *
				 * For further details see Intel SDM Vol. 2A,
				 * 'FXSAVE instruction'.
				 */
				char _fxsave_area[527];

				/* 16-byte aligned start of FXSAVE area. */
				char * _start = nullptr;

				Fpu * _fpu = nullptr;

				bool _init()
				{
					if (_start) return true;

					_start = ((addr_t)_fxsave_area & 15)
						? (char *)((addr_t)_fxsave_area & ~15) + 16
						: _fxsave_area;
					return false;
				}

			public:

				~Context() { if (_fpu) _fpu->unset(*this); }
		};

	private:

		enum { MXCSR_DEFAULT = 0x1f80 };

		Context * _context = nullptr;

		/**
		 * Reset FPU
		 *
		 * Doesn't check for pending unmasked floating-point
		 * exceptions and explicitly sets the MXCSR to the
		 * default value.
		 */
		void _reset()
		{
			unsigned value = MXCSR_DEFAULT;
			asm volatile ("fninit");
			asm volatile ("ldmxcsr %0" : : "m" (value));
		}

		/**
		 * Load x87 FPU context
		 */
		void _load()
		{
			if (!_context->_init()) _reset();
			else asm volatile ("fxrstor %0" : : "m" (*_context->_start));
		}

		/**
		 * Save x87 FPU context
		 */
		void _save() {
			asm volatile ("fxsave %0" : "=m" (*_context->_start)); }

	public:

		/**
		 * Disable FPU by setting the TS flag in CR0.
		 */
		void disable();

		/**
		 * Enable FPU by clearing the TS flag in CR0.
		 */
		void enable() { asm volatile ("clts"); }

		/**
		 * Initialize all FPU-related CR flags
		 *
		 * Initialize FPU with SSE extensions by setting required CR0 and CR4
		 * bits to configure the FPU environment according to Intel SDM Vol.
		 * 3A, sections 9.2 and 9.6.
		 */
		void init();

		/**
		 * Returns True if the FPU is enabled.
		 */
		bool enabled();

		/**
		 * Switch to new context
		 *
		 * \param context  next FPU context
		 */
		void switch_to(Context &context) {
			if (_context != &context) disable(); }

		/**
		 * Return whether to retry an FPU instruction after this call
		 */
		bool fault(Context &context);

		/**
		 * Unset FPU context
		 */
		void unset(Context &context) {
			if (_context == &context) _context = nullptr; }
};

#endif /* _SPEC__X86_64__FPU_H_ */
