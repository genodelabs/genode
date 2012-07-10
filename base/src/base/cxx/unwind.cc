/*
 * \brief  Wrapper for symbols required by libgcc_eh's exception handling
 * \author Sebastian Sumpf
 * \date   2011-07-20
 *
 * The actual wrapper function is prefixed with '__cxx', the wrapper always
 * calls a function with prefix '_cxx' (note the missing '_'). In 'cxx.mk'
 * we remove the leading '__cxx' from the wrapper which than becomes the symbol
 * of the wrapped function, in turn the wrapped function is prefixed with '_cxx'.
 * This whole procedure became necessary since the wrapped symbols are marked
 * 'GLOBAL', 'HIDDEN' in libgcc_eh.a and thus ligcc_eh had to be linked to *all*
 * binaries. In shared libaries this became unfeasible since libgcc_eh uses
 * global data which might not be initialized during cross-library interaction.
 * The clean way to go would be to link libgcc_s.so to DSOs and dynamic
 * binaries, unfortunally ligcc_s requires libc6 in the current Genode tool
 * chain.
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

extern "C" {

	/* Unwind function found in all binaries */
	void  _cxx__Unwind_Resume(void *exc)  __attribute__((weak));
	void __cxx__Unwind_Resume(void *exc) {
		_cxx__Unwind_Resume(exc); }

	void  _cxx__Unwind_DeleteException(void *exc)  __attribute__((weak));
	void __cxx__Unwind_DeleteException(void *exc) {
		_cxx__Unwind_DeleteException(exc); }

	/* Special ARM-EABI personality functions */
#ifdef __ARM_EABI__

	int  _cxx___aeabi_unwind_cpp_pr0(int state, void *block, void *context) __attribute__((weak));
	int __cxx___aeabi_unwind_cpp_pr0(int state, void *block, void *context) {
		return _cxx___aeabi_unwind_cpp_pr0(state, block, context); }

	int  _cxx___aeabi_unwind_cpp_pr1(int state, void *block, void *context) __attribute__((weak));
	int __cxx___aeabi_unwind_cpp_pr1(int state, void *block, void *context) {
		return _cxx___aeabi_unwind_cpp_pr1(state, block, context); }

	/* Unwind functions found in some binaries */
	void  _cxx__Unwind_Complete(void *exc)  __attribute__((weak));
	void __cxx__Unwind_Complete(void *exc) {
		_cxx__Unwind_Complete(exc); }
#endif
}
