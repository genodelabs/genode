/*
 * \brief  Trigger segmentation fault
 * \author Norman Feske
 * \date   2012-11-01
 */

#include <base/log.h>

int main(int argc, char **argv)
{
	Genode::log("going to produce a segmentation fault...");

	*((int *)0x44) = 0x55;
	return 0;
}
