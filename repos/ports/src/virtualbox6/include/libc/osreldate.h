/*
 * Override libc's osreldate.h because the libc's param.h as included by the
 * original osreldate.h provides a definition of PAGE_SIZE, which collides with
 * VirtualBox' iprt/param.h.
 */
