#include <base/printf.h>

#include <VBox/settings.h>

#include "ConsoleImpl.h"
#include "MachineImpl.h"
#include "MouseImpl.h"
#include "DisplayImpl.h"
#include "GuestImpl.h"

#include "dummy/macros.h"

#include "console.h"
#include "fb.h"

static const bool debug = false;

void    Console::uninit()                                                       DUMMY()
HRESULT Console::resume(Reason_T aReason)                                       DUMMY(E_FAIL)
HRESULT Console::pause(Reason_T aReason)                                        DUMMY(E_FAIL)
void    Console::enableVMMStatistics(BOOL aEnable)                              DUMMY()
void    Console::changeClipboardMode(ClipboardMode_T aClipboardMode)            DUMMY()
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
HRESULT Console::PowerButton()                                                  DUMMY(E_FAIL)
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

void    Console::onKeyboardLedsChange(bool, bool, bool)                         TRACE()
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

void Console::onUSBDeviceStateChange(IUSBDevice *aDevice, bool aAttached,
                                     IVirtualBoxErrorInfo *aError)              TRACE()

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

	Genode::env()->parent()->exit(0);
}

void fireRuntimeErrorEvent(IEventSource* aSource, BOOL a_fatal,
                           CBSTR a_id, CBSTR a_message)
{
	PERR("%s : %u %s %s", __func__, a_fatal,
	     Utf8Str(a_id).c_str(), Utf8Str(a_message).c_str());

	TRACE();
}

void Console::onAdditionsStateChange()
{
	dynamic_cast<GenodeConsole *>(this)->update_video_mode();
}

void GenodeConsole::update_video_mode()
{
	Display  *d    = getDisplay();
	Guest    *g    = getGuest();
	Genodefb *fb   = dynamic_cast<Genodefb *>(d->getFramebuffer());
	LONG64 ignored = 0;

	AdditionsFacilityType_T is_graphics;
	g->GetFacilityStatus(AdditionsFacilityType_Graphics, &ignored, &is_graphics);

	if (fb && is_graphics)
		d->SetVideoModeHint(0 /*=display*/,
		                    true /*=enabled*/, false /*=changeOrigin*/,
		                    0 /*=originX*/, 0 /*=originY*/,
		                    fb->w(), fb->h(),
		                    /* Windows 8 only accepts 32-bpp modes */
		                    32);
}

void GenodeConsole::handle_input(unsigned)
{
	static LONG64 mt_events [64];
	unsigned      mt_number = 0;

	/* read out input capabilities of guest */
	bool guest_abs = false, guest_rel = false, guest_multi = false;
	_vbox_mouse->COMGETTER(AbsoluteSupported)(&guest_abs);
	_vbox_mouse->COMGETTER(RelativeSupported)(&guest_rel);
	_vbox_mouse->COMGETTER(MultiTouchSupported)(&guest_multi);

	for (int i = 0, num_ev = _input.flush(); i < num_ev; ++i) {
		Input::Event &ev = _ev_buf[i];

		bool const is_press   = ev.type() == Input::Event::PRESS;
		bool const is_release = ev.type() == Input::Event::RELEASE;
		bool const is_key     = is_press || is_release;
		bool const is_motion  = ev.type() == Input::Event::MOTION;
		bool const is_wheel   = ev.type() == Input::Event::WHEEL;
		bool const is_touch   = ev.type() == Input::Event::TOUCH;

		if (is_key) {
			Scan_code scan_code(ev.keycode());

			unsigned char const release_bit =
				(ev.type() == Input::Event::RELEASE) ? 0x80 : 0;

			if (scan_code.is_normal())
				_vbox_keyboard->PutScancode(scan_code.code() | release_bit);

			if (scan_code.is_ext()) {
				_vbox_keyboard->PutScancode(0xe0);
				_vbox_keyboard->PutScancode(scan_code.ext() | release_bit);
			}
		}

		/*
		 * Track press/release status of keys and buttons. Currently,
		 * only the mouse-button states are actually used.
		 */
		if (is_press)
			_key_status[ev.keycode()] = true;

		if (is_release)
			_key_status[ev.keycode()] = false;

		bool const is_mouse_button_event =
			is_key && _is_mouse_button(ev.keycode());

		bool const is_mouse_event = is_mouse_button_event || is_motion;

		if (is_mouse_event) {
			unsigned const buttons = (_key_status[Input::BTN_LEFT]   ? MouseButtonState_LeftButton : 0)
					               | (_key_status[Input::BTN_RIGHT]  ? MouseButtonState_RightButton : 0)
					               | (_key_status[Input::BTN_MIDDLE] ? MouseButtonState_MiddleButton : 0);
			if (ev.is_absolute_motion()) {

				_last_received_motion_event_was_absolute = true;

				/* transform absolute to relative if guest is so odd */
				if (!guest_abs && guest_rel) {
					int const boundary = 20;
					int rx = ev.ax() - _ax;
					int ry = ev.ay() - _ay;
					rx = Genode::min(boundary, Genode::max(-boundary, rx));
					ry = Genode::min(boundary, Genode::max(-boundary, ry));
					_vbox_mouse->PutMouseEvent(rx, ry, 0, 0, buttons);
				} else
					_vbox_mouse->PutMouseEventAbsolute(ev.ax(), ev.ay(), 0,
							                           0, buttons);

				_ax = ev.ax();
				_ay = ev.ay();

			} else if (ev.is_relative_motion()) {

				_last_received_motion_event_was_absolute = false;

				/* prefer relative motion event */
				if (guest_rel)
					_vbox_mouse->PutMouseEvent(ev.rx(), ev.ry(), 0, 0, buttons);
				else if (guest_abs) {
					_ax += ev.rx();
					_ay += ev.ry();
					_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
				}
			}
			/* only the buttons changed */
			else {

				if (_last_received_motion_event_was_absolute) {
					/* prefer absolute button event */
					if (guest_abs)
						_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
					else if (guest_rel)
						_vbox_mouse->PutMouseEvent(0, 0, 0, 0, buttons);
				} else {
					/* prefer relative button event */
					if (guest_rel)
						_vbox_mouse->PutMouseEvent(0, 0, 0, 0, buttons);
					else if (guest_abs)
						_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, 0, 0, buttons);
				}

			}
		}

		if (is_wheel) {
			if (_last_received_motion_event_was_absolute)
				_vbox_mouse->PutMouseEventAbsolute(_ax, _ay, -ev.ry(), -ev.rx(), 0);
			else
				_vbox_mouse->PutMouseEvent(0, 0, -ev.ry(), -ev.rx(), 0);
		}

		if (is_touch) {
			/* if multitouch queue is full - send it */
			if (mt_number >= sizeof(mt_events) / sizeof(mt_events[0])) {
				_vbox_mouse->PutEventMultiTouch(mt_number, mt_number,
						                        mt_events, RTTimeMilliTS());
				mt_number = 0;
			}

			int x    = ev.ax();
			int y    = ev.ay();
			int slot = ev.code();

			/* Mouse::putEventMultiTouch drops values of 0 */
			if (x <= 0) x = 1;
			if (y <= 0) y = 1;

			enum MultiTouch {
				None = 0x0,
				InContact = 0x01,
				InRange = 0x02
			};

			int status = MultiTouch::InContact | MultiTouch::InRange;
			if (ev.is_touch_release())
				status = MultiTouch::None;

			uint16_t const s = RT_MAKE_U16(slot, status);
			mt_events[mt_number++] = RT_MAKE_U64_FROM_U16(x, y, s, 0);
		}

	}

	/* if there are elements - send it */
	if (mt_number)
		_vbox_mouse->PutEventMultiTouch(mt_number, mt_number, mt_events,
				                        RTTimeMilliTS());
}

void GenodeConsole::handle_mode_change(unsigned)
{
	Display  *d  = getDisplay();
	Genodefb *fb = dynamic_cast<Genodefb *>(d->getFramebuffer());

	fb->update_mode();
	update_video_mode();
}

void GenodeConsole::event_loop(IKeyboard * gKeyboard, IMouse * gMouse)
{
	_vbox_keyboard = gKeyboard;
	_vbox_mouse = gMouse;

	/* register the mode change signal dispatcher at the framebuffer */
	Display  *d  = getDisplay();
	Genodefb *fb = dynamic_cast<Genodefb *>(d->getFramebuffer());
	fb->mode_sigh(_mode_change_signal_dispatcher);

	for (;;) {

		Genode::Signal sig = _receiver.wait_for_signal();
		Genode::Signal_dispatcher_base *dispatcher =
			dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());

		if (dispatcher)
			dispatcher->dispatch(sig.num());
	}

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
