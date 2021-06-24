#ifndef _FFICONFIG_H_
#define _FFICONFIG_H_

#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name) .hidden name
#else
#define FFI_HIDDEN __attribute__ ((visibility ("hidden")))
#endif

#define EH_FRAME_FLAGS "a"

#define STDC_HEADERS 1

#define HAVE_MEMCPY 1

#endif /* _FFICONFIG_H_ */
