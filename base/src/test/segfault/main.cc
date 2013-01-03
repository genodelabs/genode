/*
 * \brief  Trigger segmentation fault
 * \author Norman Feske
 * \date   2012-11-01
 */

#include <base/printf.h>

int main(int argc, char **argv)
{
	Genode::printf("going to produce a segmentation fault...\n");

	*((int *)0x44) = 0x55;
	return 0;
}
