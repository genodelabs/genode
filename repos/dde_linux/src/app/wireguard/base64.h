/***************************************************
 ** FIXME                                         **
 ** Slightly adapted copy from wireguard-tools.   **
 ** Replace with contrib or Genode implementation **
 ***************************************************/

#ifndef _BASE64_H_
#define _BASE64_H_

/* base includes */
#include <base/stdint.h>

enum {
	WG_KEY_LEN = 32,
	WG_KEY_LEN_BASE64 = ((((WG_KEY_LEN) + 2) / 3) * 4 + 1),
};

int decode_base64(const char src[4]);

bool key_from_base64(Genode::uint8_t key[WG_KEY_LEN], const char *base64);

#endif /* _BASE64_H_ */
