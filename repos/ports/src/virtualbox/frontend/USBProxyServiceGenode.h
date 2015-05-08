/*
 * \brief  USBProxyService implementation for Genode
 * \author Christian Prochaska
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef ____H_USBPROXYSERVICEGENODE
#define ____H_USBPROXYSERVICEGENODE

#include <os/attached_rom_dataspace.h>
#include <os/timed_semaphore.h>
#include <timer_session/connection.h>
#include <util/xml_node.h>

#include <USBProxyService.h>

class USBProxyServiceGenode : public USBProxyService
{
	private:

		static constexpr bool debug = false;

		struct Device_list_change_signal_context : Genode::Signal_context { };
		struct Timeout_signal_context            : Genode::Signal_context { };
		struct Wakeup_signal_context             : Genode::Signal_context { };

		Timer::Connection                  _timer;
		Genode::Signal_receiver            _signal_receiver;
		Device_list_change_signal_context  _device_list_change_signal_context;
		Timeout_signal_context             _timeout_signal_context;
		Wakeup_signal_context              _wakeup_signal_context;
		Genode::Signal_context_capability  _wakeup_signal_context_cap;

		Genode::Attached_rom_dataspace    *_usb_devices_ds = 0;

		Genode::Timed_semaphore _wait_sem;

		PUSBDEVICE _create_usb_device(Genode::Xml_node &device_node)
		{
			unsigned int vendor_id = 0;
			unsigned int product_id = 0;

			device_node.attribute("vendor_id").value(&vendor_id);
			device_node.attribute("product_id").value(&product_id);

			if (debug)
				PDBG("vendor_id: %4x, product_id: %4x", vendor_id, product_id);

			char address_buf[10];
			Genode::snprintf(address_buf, sizeof(address_buf),
			                 "%4x:%4x", vendor_id, product_id); 

	        PUSBDEVICE dev = (PUSBDEVICE)RTMemAllocZ(sizeof(USBDEVICE));

			dev->idVendor = vendor_id;
			dev->idProduct = product_id;
			dev->pszAddress = RTStrDup(address_buf);

			dev->pNext = 0;
			dev->pPrev = 0;
			dev->pszManufacturer = 0;
			dev->pszSerialNumber = 0;
			dev->pszProduct = "";
			dev->bcdDevice = 0;
			dev->bcdUSB = 0;
			dev->bDeviceClass = 0x0;
			dev->bDeviceSubClass = 0x0;
			dev->bDeviceProtocol = 0x0;
			dev->bNumConfigurations = 1;
			dev->enmState = USBDEVICESTATE_UNUSED;
			dev->enmSpeed = USBDEVICESPEED_LOW;
			dev->u64SerialHash = 0;
			dev->bBus = 1;
			dev->bPort = 1;
			dev->bDevNum = 3;

			return dev;
		}

	public:

		USBProxyServiceGenode(Host *aHost) : USBProxyService(aHost)
		{
			try {
				_usb_devices_ds = new Genode::Attached_rom_dataspace("usb_devices");

				Genode::Signal_context_capability device_list_change_signal_context_cap =
						_signal_receiver.manage(&_device_list_change_signal_context);
				_usb_devices_ds->sigh(device_list_change_signal_context_cap);

			} catch (...) {
				PWRN("Could not retrieve the \"usb_devices\" ROM file."
				     "USB device pass-through unavailable.");
			}

			Genode::Signal_context_capability timeout_signal_context_cap =
					_signal_receiver.manage(&_timeout_signal_context);
			_timer.sigh(timeout_signal_context_cap);

			_wakeup_signal_context_cap = _signal_receiver.manage(&_wakeup_signal_context);
		}

		~USBProxyServiceGenode()
		{
			delete _usb_devices_ds;
		}

		HRESULT	init()
		{
			if (debug)
				RTLogPrintf("USBProxyServiceGenode::init()\n");

			/*
 	 	 	 * Start the poller thread.
 	 	 	 */
			return (HRESULT)start();
        }

        PUSBDEVICE getDevices()
        {
        	if (debug)
				RTLogPrintf("USBProxyServiceGenode::getDevices()\n");

			PUSBDEVICE first_dev = 0;

			if (!_usb_devices_ds)
				return first_dev;

			_usb_devices_ds->update();

			if (!_usb_devices_ds->is_valid())
				return first_dev;

			try {
				char *content = _usb_devices_ds->local_addr<char>();

				if (debug)
					PDBG("content: %s", content);

				Genode::Xml_node devices_node(_usb_devices_ds->local_addr<char>());

				Genode::Xml_node device_node = devices_node.sub_node("device");

				first_dev = _create_usb_device(device_node);

				PUSBDEVICE prev_dev = first_dev;
				for (;;) {
					device_node = device_node.next("device");
					PUSBDEVICE dev = _create_usb_device(device_node);
					prev_dev->pNext = dev;
					dev->pPrev = prev_dev;
					prev_dev = dev;
				}

			} catch (...) { }

			return first_dev;
        }

		int wait(RTMSINTERVAL aMillies)
		{
			if (debug)
				RTLogPrintf("USBProxyServiceGenode::wait(): aMillies = %u\n",
				            aMillies);

			unsigned long elapsed_ms_start = _timer.elapsed_ms();

			if (aMillies != RT_INDEFINITE_WAIT) 
				_timer.trigger_once(aMillies * 1000);

			for (;;) {

				if (debug)
					PDBG("waiting for signal");

				Genode::Signal signal = _signal_receiver.wait_for_signal();

				Genode::Signal_context *context = signal.context();

				if (dynamic_cast<Timeout_signal_context*>(context)) {

					if (aMillies == RT_INDEFINITE_WAIT) {
						/* received an old signal */
						if (debug)
							PDBG("old timeout signal received");
						continue;
					}

					unsigned long elapsed_ms_now = _timer.elapsed_ms();

					if (elapsed_ms_now - elapsed_ms_start < aMillies) {
						/* received an old signal */
						if (debug)
							PDBG("old timeout signal received");
						continue;
					}

					if (debug)
						PDBG("timeout signal received");

					break;

				} else if (dynamic_cast<Wakeup_signal_context*>(context)) {

					if (debug)
						PDBG("wakeup signal received");

					break;

				} else if (dynamic_cast<Device_list_change_signal_context*>(context)) {

					if (debug)
						PDBG("device list change signal received");

					break;
				}
			}

			return VINF_SUCCESS;
		}

		int interruptWait()
		{
			if (debug)
				RTLogPrintf("USBProxyServiceGenode::interruptWait()\n");

			Genode::Signal_transmitter(_wakeup_signal_context_cap).submit();

			return VINF_SUCCESS;
		}

		int captureDevice(HostUSBDevice *aDevice)
		{
			if (debug)
				RTLogPrintf("USBProxyServiceGenode::captureDevice()\n");

			interruptWait();

			return VINF_SUCCESS;
		}

		bool updateDeviceState(HostUSBDevice *aDevice,
		                       PUSBDEVICE aUSBDevice,
		                       bool *aRunFilters,
		                       SessionMachine **aIgnoreMachine)
		{
			if (debug)
				RTLogPrintf("USBProxyServiceGenode::updateDeviceState()\n");

    		return updateDeviceStateFake(aDevice, aUSBDevice, aRunFilters, aIgnoreMachine);
		}

};

#endif // ____H_USBPROXYSERVICEGENODE
