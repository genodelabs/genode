/*
 * \brief  USB: DWC-OTG RaspberryPI Interrupt
 * \author Stefan Kalkowski
 * \date   2019-04-16
 */

#include <base/attached_rom_dataspace.h>
#include <drivers/defs/rpi.h>
#include <util/string.h>

unsigned dwc_irq(Genode::Env &env)
{
	using namespace Genode;

	static Attached_rom_dataspace rom(env, "platform_info");
	static unsigned offset = 0;
	try {
		String<32> kernel_name =
			rom.xml().sub_node("kernel").attribute_value("name", String<32>());
		if (kernel_name == "hw") offset += Rpi::GPU_IRQ_BASE;
	} catch (...) { }

	return offset + 9;
}
