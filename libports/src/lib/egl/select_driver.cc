/*
 * \brief  Probe GPU device to select the proper Gallium3D driver
 * \author Norman Feske
 * \date   2010-09-23
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <pci_session/connection.h>
#include <pci_device/client.h>

/* local includes */
#include "select_driver.h"


class Gpu_detector
{
	private:

		const char *_driver_name;

	protected:

		/**
		 * Constructor
		 *
		 * \param driver_name  name of Gallium3D driver
		 */
		Gpu_detector(const char *driver_name) : _driver_name(driver_name) { }

	public:

		/**
		 * Return true if specified device and vendor IDs match the GPU
		 */
		virtual bool detect(unsigned short vendor_id, unsigned short device_id) = 0;

		/**
		 * Return name of Gallium3D driver
		 */
		const char *driver_name() { return _driver_name; }
};


class I915_gpu_detector : public Gpu_detector
{
	public:

		I915_gpu_detector() : Gpu_detector("gallium-i915.lib.so") { }


		/****************************
		 ** GPU detector interface **
		 ****************************/

		bool detect(unsigned short vendor_id, unsigned short device_id)
		{
			if (vendor_id != 0x8086) return false;

			/*
			 * Supported PCI device IDs according to 'gallium/drivers/i915/i915_reg.h'
			 * and 'gallium/drivers/i915/i915_screen.c'.
			 */
			unsigned short supported_device_ids[] = {
				0x2582, /* I915_G   */
				0x2592, /* I915_GM  */
				0x2772, /* I945_G   */
				0x27A2, /* I945_GM  */
				0x27AE, /* I945_GME */
				0x29C2, /* G33_G    */
				0x29B2, /* Q35_G    */
				0x29D2, /* Q33_G    */
				0
			};

			for (unsigned i = 0; supported_device_ids[i]; i++)
				if (device_id == supported_device_ids[i])
					return true;

			return false;
		}
};


const char *probe_gpu_and_select_driver()
{
	const char *result = 0;
	try {
		I915_gpu_detector i915_detector;
		Pci::Connection pci;

		/*
		 * Iterate through the available PCI devices and present each to the
		 * GPU detector(s).
		 */
		Pci::Device_capability cap = pci.first_device();
		while (cap.valid()) {

			Pci::Device_capability next_cap = pci.next_device(cap);

			unsigned short vendor_id = Pci::Device_client(cap).vendor_id(),
			               device_id = Pci::Device_client(cap).device_id();

			if (i915_detector.detect(vendor_id, device_id))
				result = i915_detector.driver_name();

			pci.release_device(cap);

			cap = next_cap;
		}
	}
	/* catch exception if no PCI service is available */
	catch (Genode::Parent::Service_denied) { }

	return result;
}
