/*
 * \brief  Trigger segmentation fault
 * \author Norman Feske
 * \date   2012-11-01
 */

#include <base/component.h>
#include <base/log.h>

void Component::construct(Genode::Env &)
{
	Genode::log("going to produce a segmentation fault...");

	*((int *)0x44) = 0x55;
}
