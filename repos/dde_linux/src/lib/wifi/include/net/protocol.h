/*
 * net/core/sock.c needs <net/inet_sock.h> but it does not
 * include this header directly. <net/protocol.h> is normally
 * provided by lx_emul.h and is included by sock.c.
 */
#include <net/inet_sock.h>
