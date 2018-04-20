/*
 * \brief  Port of VirtualBox to Genode
 * \author Norman Feske
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/log.h>
#include <util/xml_node.h>

#include <VBox/settings.h>
#include <SharedClipboard/VBoxClipboard.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include "ConsoleImpl.h"
#include "MachineImpl.h"
#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "GuestImpl.h"

#include "dummy/macros.h"

#include "console.h"
#include "fb.h"
#include "../vmm.h"

static const bool debug = false;
static bool vm_down = false;

static Genode::Attached_rom_dataspace *clipboard_rom = nullptr;
static Genode::Reporter               *clipboard_reporter = nullptr;
static char                           *decoded_clipboard_content = nullptr;

void    Console::uninit()                                                       DUMMY()
HRESULT Console::resume(Reason_T aReason)                                       DUMMY(E_FAIL)
HRESULT Console::pause(Reason_T aReason)                                        DUMMY(E_FAIL)
void    Console::enableVMMStatistics(BOOL aEnable)                              DUMMY()
HRESULT Console::updateMachineState(MachineState_T aMachineState)               DUMMY(E_FAIL)

HRESULT Console::attachToTapInterface(INetworkAdapter *networkAdapter)
{
	ULONG slot = 0;
	HRESULT rc = networkAdapter->COMGETTER(Slot)(&slot);
	AssertComRC(rc);

	maTapFD[slot] = (RTFILE)1;

	TRACE(rc)
}

HRESULT Console::detachFromTapInterface(INetworkAdapter *networkAdapter)
{
	ULONG slot = 0;
	HRESULT rc = networkAdapter->COMGETTER(Slot)(&slot);
	AssertComRC(rc);

	if (maTapFD[slot] != NIL_RTFILE)
		maTapFD[slot] = NIL_RTFILE;

	TRACE(rc)
}

HRESULT Console::teleporterTrg(PUVM, IMachine*, com::Utf8Str*, bool, Progress*,
                               bool*)                                           DUMMY(E_FAIL)
HRESULT Console::saveState(Reason_T aReason, IProgress **aProgress)             DUMMY(E_FAIL)

STDMETHODIMP Console::COMGETTER(Debugger)(IMachineDebugger **aDebugger)                                    DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(USBDevices)(ComSafeArrayOut(IUSBDevice *, aUSBDevices))                    DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(RemoteUSBDevices)(ComSafeArrayOut(IHostUSBDevice *, aRemoteUSBDevices))    DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(VRDEServerInfo)(IVRDEServerInfo **aVRDEServerInfo)                         DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(EmulatedUSB)(IEmulatedUSB **aEmulatedUSB)                                  DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(SharedFolders)(ComSafeArrayOut(ISharedFolder *, aSharedFolders))           DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(AttachedPCIDevices)(ComSafeArrayOut(IPCIDeviceAttachment *, aAttachments)) DUMMY(E_FAIL)
STDMETHODIMP Console::COMGETTER(UseHostClipboard)(BOOL *aUseHostClipboard)                                 DUMMY(E_FAIL)
STDMETHODIMP Console::COMSETTER(UseHostClipboard)(BOOL aUseHostClipboard)                                  DUMMY(E_FAIL)

HRESULT Console::Reset()                                                        DUMMY(E_FAIL)
HRESULT Console::Pause()                                                        DUMMY(E_FAIL)
HRESULT Console::Resume()                                                       DUMMY(E_FAIL)
HRESULT Console::SleepButton()                                                  DUMMY(E_FAIL)
HRESULT Console::GetPowerButtonHandled(bool*)                                   DUMMY(E_FAIL)
HRESULT Console::GetGuestEnteredACPIMode(bool*)                                 DUMMY(E_FAIL)
HRESULT Console::SaveState(IProgress**)                                         DUMMY(E_FAIL)
HRESULT Console::AdoptSavedState(IN_BSTR)                                       DUMMY(E_FAIL)
HRESULT Console::DiscardSavedState(bool)                                        DUMMY(E_FAIL)
HRESULT Console::GetDeviceActivity(DeviceType_T, DeviceActivity_T*)             DUMMY(E_FAIL)
HRESULT Console::AttachUSBDevice(IN_BSTR)                                       DUMMY(E_FAIL)
HRESULT Console::DetachUSBDevice(IN_BSTR, IUSBDevice**)                         DUMMY(E_FAIL)
HRESULT Console::FindUSBDeviceByAddress(IN_BSTR, IUSBDevice**)                  DUMMY(E_FAIL)
HRESULT Console::FindUSBDeviceById(IN_BSTR, IUSBDevice**)                       DUMMY(E_FAIL)
HRESULT Console::CreateSharedFolder(IN_BSTR, IN_BSTR, BOOL, BOOL)               DUMMY(E_FAIL)
HRESULT Console::RemoveSharedFolder(IN_BSTR)                                    DUMMY(E_FAIL)
HRESULT Console::TakeSnapshot(IN_BSTR, IN_BSTR, IProgress**)                    DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshot(IN_BSTR, IProgress**)                           DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshotAndAllChildren(IN_BSTR, IProgress**)             DUMMY(E_FAIL)
HRESULT Console::DeleteSnapshotRange(IN_BSTR, IN_BSTR, IProgress**)             DUMMY(E_FAIL)
HRESULT Console::RestoreSnapshot(ISnapshot*, IProgress**)                       DUMMY(E_FAIL)
HRESULT Console::Teleport(IN_BSTR, ULONG, IN_BSTR, ULONG, IProgress **)         DUMMY(E_FAIL)
HRESULT Console::setDiskEncryptionKeys(const Utf8Str &strCfg)                   DUMMY(E_FAIL)

void    Console::onAdditionsOutdated()                                          DUMMY()

HRESULT Console::onVideoCaptureChange()                                         DUMMY(E_FAIL)
HRESULT Console::onSharedFolderChange(BOOL aGlobal)                             DUMMY(E_FAIL)
HRESULT Console::onUSBControllerChange()                                        DUMMY(E_FAIL)
HRESULT Console::onCPUChange(ULONG aCPU, BOOL aRemove)                          DUMMY(E_FAIL)
HRESULT Console::onClipboardModeChange(ClipboardMode_T aClipboardMode)          DUMMY(E_FAIL)
HRESULT Console::onDragAndDropModeChange(DragAndDropMode_T aDragAndDropMode)    DUMMY(E_FAIL)
HRESULT Console::onCPUExecutionCapChange(ULONG aExecutionCap)                   DUMMY(E_FAIL)
HRESULT Console::onStorageControllerChange()                                    DUMMY(E_FAIL)
HRESULT Console::onMediumChange(IMediumAttachment *aMediumAttachment, BOOL)     DUMMY(E_FAIL)
HRESULT Console::onVRDEServerChange(BOOL aRestart)                              DUMMY(E_FAIL)
HRESULT Console::onUSBDeviceAttach(IUSBDevice *, IVirtualBoxErrorInfo *, ULONG) DUMMY(E_FAIL)
HRESULT Console::onUSBDeviceDetach(IN_BSTR aId, IVirtualBoxErrorInfo *aError)   DUMMY(E_FAIL)

HRESULT Console::onShowWindow(BOOL aCheck, BOOL *aCanShow, LONG64 *aWinId)      DUMMY(E_FAIL)
HRESULT Console::onNetworkAdapterChange(INetworkAdapter *, BOOL changeAdapter)  DUMMY(E_FAIL)
HRESULT Console::onStorageDeviceChange(IMediumAttachment *, BOOL, BOOL)         DUMMY(E_FAIL)
HRESULT Console::onBandwidthGroupChange(IBandwidthGroup *aBandwidthGroup)       DUMMY(E_FAIL)
HRESULT Console::onSerialPortChange(ISerialPort *aSerialPort)                   DUMMY(E_FAIL)
HRESULT Console::onParallelPortChange(IParallelPort *aParallelPort)             DUMMY(E_FAIL)
HRESULT Console::onlineMergeMedium(IMediumAttachment *aMediumAttachment,
                                   ULONG aSourceIdx, ULONG aTargetIdx,
                                   IProgress *aProgress)                        DUMMY(E_FAIL)

void fireStateChangedEvent(IEventSource* aSource,
                           MachineState_T a_state)
{
	if (a_state != MachineState_PoweredOff)
		return;

	vm_down = true;

	genode_env().parent().exit(0);
}

void fireRuntimeErrorEvent(IEventSource* aSource, BOOL a_fatal,
                           CBSTR a_id, CBSTR a_message)
{
	Genode::error(__func__, " : ", a_fatal, " ",
	              Utf8Str(a_id).c_str(), " ",
	              Utf8Str(a_message).c_str());

	TRACE();
}

void Console::onAdditionsStateChange()
{
	dynamic_cast<GenodeConsole *>(this)->update_video_mode();
}

void GenodeConsole::update_video_mode()
{
	Display  *d    = getDisplay();
	Genodefb *fb   = dynamic_cast<Genodefb *>(d->getFramebuffer());

	if (!fb)
		return;

	if ((fb->w() == 0) && (fb->h() == 0)) {
		/* interpret a size of 0x0 as indication to quit VirtualBox */
		if (PowerButton() != S_OK)
			Genode::error("ACPI shutdown failed");
		return;
	}

	d->SetVideoModeHint(0 /*=display*/,
	                    true /*=enabled*/, false /*=changeOrigin*/,
	                    0 /*=originX*/, 0 /*=originY*/,
	                    fb->w(), fb->h(),
	                    /* Windows 8 only accepts 32-bpp modes */
	                    32);
}

void GenodeConsole::handle_input()
{
	/* disable input processing if vm is powered down */
	if (vm_down && (_vbox_mouse || _vbox_keyboard)) {
		_vbox_mouse    = nullptr;
		_vbox_keyboard = nullptr;
		_input.sigh(Genode::Signal_context_capability());
	}

	static LONG64 mt_events [64];
	unsigned      mt_number = 0;

	/* read out input capabilities of guest */
	bool guest_abs = false, guest_rel = false, guest_multi = false;
	if (_vbox_mouse) {
		_vbox_mouse->COMGETTER(AbsoluteSupported)(&guest_abs);
		_vbox_mouse->COMGETTER(RelativeSupported)(&guest_rel);
		_vbox_mouse->COMGETTER(MultiTouchSupported)(&guest_multi);
	}

	_input.for_each_event([&] (Input::Event const &ev) {
		/* if keyboard/mouse not available, consume input events and drop it */
		if (!_vbox_keyboard || !_vbox_mouse)
			return;

		auto keyboard_submit = [&] (Input::Keycode key, bool release) {

			Scan_code scan_code(key);

			unsigned char const release_bit = release ? 0x80 : 0;

			if (scan_code.normal())
				_vbox_keyboard->PutScancode(scan_code.code() | release_bit);

			if (scan_code.ext()) {
				_vbox_keyboard->PutScancode(0xe0);
				_vbox_keyboard->PutScancode(scan_code.ext() | release_bit);
			}
		};

		/* obtain bit mask of currently pressed mouse buttons */
		auto curr_mouse_button_bits = [&] () {
			return (_key_status[Input::BTN_LEFT]   ? MouseButtonState_LeftButton   : 0)
			     | (_key_status[Input::BTN_RIGHT]  ? MouseButtonState_RightButton  : 0)
			     | (_key_status[Input::BTN_MIDDLE] ? MouseButtonState_MiddleButton : 0);
		};

		unsigned const old_mouse_button_bits = curr_mouse_button_bits();

		ev.handle_press([&] (Input::Keycode key, Genode::Codepoint) {
			keyboard_submit(key, false);
			_key_status[key] = true;
		});

		ev.handle_release([&] (Input::Keycode key) {
			keyboard_submit(key, true);
			_key_status[key] = false;
		});

		unsigned const mouse_button_bits = curr_mouse_button_bits();

		if (mouse_button_bits != old_mouse_button_bits) {

			if (_last_received_motion_event_was_absolute) {
				/* prefer absolute button event */
				if (guest_abs)
					_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, mouse_button_bits);
				else if (guest_rel)
					_vbox_mouse->PutMouseEvent(0, 0, 0, 0, mouse_button_bits);
			} else {
				/* prefer relative button event */
				if (guest_rel)
					_vbox_mouse->PutMouseEvent(0, 0, 0, 0, mouse_button_bits);
				else if (guest_abs)
					_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, mouse_button_bits);
			}
		}

		ev.handle_absolute_motion([&] (int x, int y) {

			_last_received_motion_event_was_absolute = true;

			/* transform absolute to relative if guest is so odd */
			if (!guest_abs && guest_rel) {
				int const boundary = 20;
				int rx = x - _ax;
				int ry = y - _ay;
				rx = Genode::min(boundary, Genode::max(-boundary, rx));
				ry = Genode::min(boundary, Genode::max(-boundary, ry));
				_vbox_mouse->PutMouseEvent(rx, ry, 0, 0, mouse_button_bits);
			} else
				_vbox_mouse->PutMouseEventAbsolute(x, y, 0, 0, mouse_button_bits);

			_ax = x;
			_ay = y;
		});

		ev.handle_relative_motion([&] (int x, int y) {

			_last_received_motion_event_was_absolute = false;

			/* prefer relative motion event */
			if (guest_rel)
				_vbox_mouse->PutMouseEvent(x, y, 0, 0, mouse_button_bits);
			else if (guest_abs) {
				_ax += x;
				_ay += y;
				_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, mouse_button_bits);
			}
		});

		ev.handle_wheel([&] (int x, int y) {

			if (_last_received_motion_event_was_absolute)
				_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, -y, -x, 0);
			else
				_vbox_mouse->PutMouseEvent(0, 0, -y, -x, 0);
		});

		ev.handle_touch([&] (Input::Touch_id id, int x, int y) {

			/* if multitouch queue is full - send it */
			if (mt_number >= sizeof(mt_events) / sizeof(mt_events[0])) {
				_vbox_mouse->PutEventMultiTouch(mt_number, mt_number,
				                                mt_events, RTTimeMilliTS());
				mt_number = 0;
			}

			/* Mouse::putEventMultiTouch drops values of 0 */
			if (x <= 0) x = 1;
			if (y <= 0) y = 1;

			enum { IN_CONTACT = 0x01, IN_RANGE = 0x02 };

			uint16_t const s = RT_MAKE_U16(id.value, IN_CONTACT | IN_RANGE);
			mt_events[mt_number++] = RT_MAKE_U64_FROM_U16(x, y, s, 0);
		});

		ev.handle_touch_release([&] (Input::Touch_id id) {
			uint16_t const s = RT_MAKE_U16(id.value, 0);
			mt_events[mt_number++] = RT_MAKE_U64_FROM_U16(0, 0, s, 0);
		});
	});

	/* if there are elements in multi-touch queue - send it */
	if (mt_number)
		_vbox_mouse->PutEventMultiTouch(mt_number, mt_number, mt_events,
		                                RTTimeMilliTS());
}

void GenodeConsole::handle_mode_change()
{
	Display  *d  = getDisplay();
	Genodefb *fb = dynamic_cast<Genodefb *>(d->getFramebuffer());

	fb->update_mode();
	update_video_mode();
}

void GenodeConsole::init_clipboard()
{
	if (!&*machine())
		return;

	ClipboardMode_T mode;
	machine()->COMGETTER(ClipboardMode)(&mode);

	if (mode == ClipboardMode_Bidirectional ||
	    mode == ClipboardMode_HostToGuest) {

		_clipboard_rom = new Genode::Attached_rom_dataspace(genode_env(), "clipboard");
		_clipboard_rom->sigh(_clipboard_signal_dispatcher);

		clipboard_rom = _clipboard_rom;
	}

	if (mode == ClipboardMode_Bidirectional ||
	    mode == ClipboardMode_GuestToHost) {

		_clipboard_reporter = new Genode::Reporter(genode_env(), "clipboard");
		_clipboard_reporter->enabled(true);

		clipboard_reporter = _clipboard_reporter;
	}
}

void GenodeConsole::handle_cb_rom_change()
{
	if (!_clipboard_rom)
		return;

	vboxClipboardSync(nullptr);
}

void GenodeConsole::init_backends(IKeyboard * gKeyboard, IMouse * gMouse)
{
	_vbox_keyboard = gKeyboard;
	_vbox_mouse = gMouse;

	/* register the mode change signal dispatcher at the framebuffer */
	Display  *d  = getDisplay();
	Genodefb *fb = dynamic_cast<Genodefb *>(d->getFramebuffer());
	fb->mode_sigh(_mode_change_signal_dispatcher);

	handle_mode_change();
}

void GenodeConsole::onMouseCapabilityChange(BOOL supportsAbsolute, BOOL supportsRelative,
		                                    BOOL supportsMT, BOOL needsHostCursor)
{
	if (supportsAbsolute) {
		/* let the guest hide the software cursor */
		Mouse *gMouse = getMouse();
		gMouse->PutMouseEventAbsolute(-1, -1, 0, 0, 0);
	}
}




/**********************
 * Clipboard handling *
 **********************/

struct _VBOXCLIPBOARDCONTEXT
{
    VBOXCLIPBOARDCLIENTDATA *pClient;
};

static VBOXCLIPBOARDCONTEXT context;

int vboxClipboardInit (void) { return VINF_SUCCESS; }

void vboxClipboardDestroy (void)
{
	free(decoded_clipboard_content);
	clipboard_rom = nullptr;
}

int vboxClipboardConnect (VBOXCLIPBOARDCLIENTDATA *pClient, bool fHeadless)
{
	if (!pClient || context.pClient != NULL)
		return VERR_NOT_SUPPORTED;

	vboxSvcClipboardLock();

	pClient->pCtx = &context;
	pClient->pCtx->pClient = pClient;

	vboxSvcClipboardUnlock();

	int rc = vboxClipboardSync (pClient);

	return rc;
}

void vboxClipboardDisconnect (VBOXCLIPBOARDCLIENTDATA *pClient)
{
	if (!pClient || !pClient->pCtx)
		return;

	vboxSvcClipboardLock();
	pClient->pCtx->pClient = NULL;
	vboxSvcClipboardUnlock();
}

void vboxClipboardFormatAnnounce (VBOXCLIPBOARDCLIENTDATA *pClient,
                                  uint32_t formats)
{
	if (!pClient)
		return;

	vboxSvcClipboardReportMsg (pClient,
	                           VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA,
	                           formats);
}

int vboxClipboardReadData (VBOXCLIPBOARDCLIENTDATA *pClient, uint32_t format,
                           void *pv, uint32_t const cb, uint32_t *pcbActual)
{
	if (!clipboard_rom || format != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT)
		return VERR_NOT_SUPPORTED;

	if (!pv || !pcbActual || cb == 0)
		return VERR_INVALID_PARAMETER;

	clipboard_rom->update();

	if (!clipboard_rom->valid()) {
		Genode::error("invalid clipboard dataspace");
		return VERR_NOT_SUPPORTED;
	}

	char * data = clipboard_rom->local_addr<char>();

	try {

		Genode::Xml_node node(data);
		if (!node.has_type("clipboard")) {
			Genode::error("invalid clipboard xml syntax");
			return VERR_INVALID_PARAMETER;
		}

		free(decoded_clipboard_content);

		decoded_clipboard_content = (char*)malloc(node.content_size());

		if (!decoded_clipboard_content) {
			Genode::error("could not allocate buffer for decoded clipboard content");
			return 0;
		}

		size_t const len = node.decoded_content(decoded_clipboard_content,
		                                        node.content_size());
		size_t written = 0;

		PRTUTF16 utf16_string = reinterpret_cast<PRTUTF16>(pv);
		int rc = RTStrToUtf16Ex(decoded_clipboard_content, len, &utf16_string, cb, &written);

		if (RT_SUCCESS(rc)) {
			if ((written * 2) + 2 > cb)
				written = (cb - 2) / 2;

			/* +1 stuff required for Windows guests ... linux guest doesn't care */
			*pcbActual = (written + 1) * 2;
			utf16_string[written] = 0;
		} else
			*pcbActual = 0;

	} catch (Genode::Xml_node::Invalid_syntax) {
		Genode::error("invalid clipboard xml syntax");
		return VERR_INVALID_PARAMETER;
	}

	return VINF_SUCCESS;
}

void vboxClipboardWriteData (VBOXCLIPBOARDCLIENTDATA *pClient, void *pv,
                             uint32_t cb, uint32_t format)
{
	if (format != VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT || !pv || !pClient ||
	    !clipboard_reporter)
		return;

	PCRTUTF16 utf16str = reinterpret_cast<PCRTUTF16>(pv);
	char * message = 0;

	int rc = RTUtf16ToUtf8(utf16str, &message);

	if (!RT_SUCCESS(rc) || !message)
		return;

	try {
		Genode::Reporter::Xml_generator xml(*clipboard_reporter, [&] () {
			xml.append_sanitized(message, strlen(message)); });
	} catch (...) {
		Genode::error("could not write clipboard data");
	}

	RTStrFree(message);
}

int vboxClipboardSync (VBOXCLIPBOARDCLIENTDATA *pClient)
{
	if (!pClient)
		pClient = context.pClient;

	if (!pClient)
		return VERR_NOT_SUPPORTED;

	vboxSvcClipboardReportMsg (pClient, VBOX_SHARED_CLIPBOARD_HOST_MSG_FORMATS,
	                           VBOX_SHARED_CLIPBOARD_FMT_UNICODETEXT);

	return VINF_SUCCESS;
}

/**
 * Sticky key handling
 */
static bool host_caps_lock = false;
static bool guest_caps_lock = false;

void Console::onKeyboardLedsChange(bool num_lock, bool caps_lock, bool scroll_lock)
{
	guest_caps_lock = caps_lock;
}

void GenodeConsole::handle_sticky_keys()
{
	/* no keyboard - no sticky key handling */
	if (!_vbox_keyboard || !_caps_lock.constructed())
		return;

	_caps_lock->update();

	if (!_caps_lock->valid())
		return;

	bool const caps_lock = _caps_lock->xml().attribute_value("enabled",
	                                                         guest_caps_lock);
	bool trigger_caps_lock = false;

	/*
	 * If guest didn't respond with led change last time, we have to
	 * trigger caps_lock change - mainly assuming that guest don't use the
	 * led to externalize its internal caps_lock state.
	 */
	if (caps_lock != host_caps_lock && host_caps_lock != guest_caps_lock)
		trigger_caps_lock = true;

	if (caps_lock != guest_caps_lock)
		trigger_caps_lock = true;

	if (trigger_caps_lock) {
		_vbox_keyboard->PutScancode(Input::KEY_CAPSLOCK);
		_vbox_keyboard->PutScancode(Input::KEY_CAPSLOCK | 0x80);
	}
}
