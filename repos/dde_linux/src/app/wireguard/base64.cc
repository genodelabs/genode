/***************************************************
 ** FIXME                                         **
 ** Slightly adapted copy from wireguard-tools.   **
 ** Replace with contrib or Genode implementation **
 ***************************************************/

/* base includes */
#include <util/string.h>

/* app/wireguard includes */
#include <base64.h>

using namespace Genode;


int decode_base64(const char src[4])
{
	int val = 0;

	for (unsigned int i = 0; i < 4; ++i)
		val |= (-1
			    + ((((('A' - 1) - src[i]) & (src[i] - ('Z' + 1))) >> 8) & (src[i] - 64))
			    + ((((('a' - 1) - src[i]) & (src[i] - ('z' + 1))) >> 8) & (src[i] - 70))
			    + ((((('0' - 1) - src[i]) & (src[i] - ('9' + 1))) >> 8) & (src[i] + 5))
			    + ((((('+' - 1) - src[i]) & (src[i] - ('+' + 1))) >> 8) & 63)
			    + ((((('/' - 1) - src[i]) & (src[i] - ('/' + 1))) >> 8) & 64)
			) << (18 - 6 * i);
	return val;
}


bool key_from_base64(uint8_t key[WG_KEY_LEN], const char *base64)
{
	unsigned int i;
	volatile uint8_t ret = 0;
	int val;
	char tmp[4];

	if (strlen(base64) != WG_KEY_LEN_BASE64 - 1 || base64[WG_KEY_LEN_BASE64 - 2] != '=')
		return false;

	for (i = 0; i < WG_KEY_LEN / 3; ++i) {
		val = decode_base64(&base64[i * 4]);
		ret |= (uint8_t)((uint32_t)val >> 31);
		key[i * 3 + 0] = (uint8_t)((val >> 16) & 0xff);
		key[i * 3 + 1] = (uint8_t)((val >> 8) & 0xff);
		key[i * 3 + 2] = (uint8_t)(val & 0xff);
	}
	tmp[0] = base64[i * 4 + 0];
	tmp[1] = base64[i * 4 + 1];
	tmp[2] = base64[i * 4 + 2];
	tmp[3] = 'A';
	val = decode_base64(tmp);
	ret |= (uint8_t)(((uint32_t)val >> 31) | (val & 0xff));
	key[i * 3 + 0] = (uint8_t)((val >> 16) & 0xff);
	key[i * 3 + 1] = (uint8_t)((val >> 8) & 0xff);

	return 1 & ((ret - 1) >> 8);
}
