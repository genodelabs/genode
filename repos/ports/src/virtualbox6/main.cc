/*
 * \brief  Port of VirtualBox to Genode
 * \author Norman Feske
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>
#include <base/registry.h>
#include <libc/component.h>
#include <libc/args.h>

/* Virtualbox includes */
#include <nsXPCOM.h>
#include <nsCOMPtr.h>
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <VBox/com/listeners.h>

/* Virtualbox includes of generic Main frontend */
#include "ConsoleImpl.h"
#include "MachineImpl.h"
#include "MouseImpl.h"
#include "SessionImpl.h"
#include "VirtualBoxImpl.h"

/* Genode port specific includes */
#include <attempt.h>
#include <init.h>
#include <fb.h>
#include <input_adapter.h>
#include <mouse_shape.h>

using namespace Genode;


struct Event_handler : Interface
{
	virtual void handle_vbox_event(VBoxEventType_T, IEvent &) = 0;
};


struct Event_listener
{
	Event_handler *_handler_ptr = nullptr;

	virtual ~Event_listener() { }

	HRESULT init(Event_handler &handler)
	{
		_handler_ptr = &handler;
		return S_OK;
	}

	void uninit() { }

	STDMETHOD(HandleEvent)(VBoxEventType_T ev_type, IEvent *ev)
	{
		if (_handler_ptr)
			_handler_ptr->handle_vbox_event(ev_type, *ev);

		return S_OK;
	}
};

using Event_listener_impl = ListenerImpl<Event_listener, Event_handler &>;

VBOX_LISTENER_DECLARE(Event_listener_impl)


struct Main : Event_handler
{
	Env &_env;

	Attached_rom_dataspace _config { _env, "config" };

	struct Vbox_file_path
	{
		using Path = String<128>;

		Path const _path;

		com::Utf8Str const utf8 { _path.string() };

		Vbox_file_path(Xml_node config)
		:
			_path(config.attribute_value("vbox_file", Path()))
		{
			if (!_path.valid()) {
				error("missing 'vbox_file' attribute in config");
				throw Fatal();
			}
		}
	} _vbox_file_path { _config.xml() };

	/*
	 * Create VirtualBox object
	 *
	 * We cannot create the object via 'ComObjPtr<VirtualBox>::createObject'
	 * because 'FinalConstruction' uses a temporary 'ComObjPtr<VirtualBox>'
	 * (implicitly constructed as argument for the 'ClientWatcher' constructor.
	 * Upon the destruction of the temporary, the 'VirtualBox' refcnt becomes
	 * zero, which prompts 'VirtualBox::Release' to destruct the object.
	 *
	 * To sidestep this suicidal behavior, we manually perform the steps of
	 * 'createObject' but calling 'AddRef' before 'FinalConstruct'.
	 */
	struct Virtualbox_instance : ComObjPtr<VirtualBox>
	{
		VirtualBox _instance;

		Virtualbox_instance()
		{
			_instance.AddRef();

			attempt([&] () { return _instance.FinalConstruct(); },
			        "construction of VirtualBox object failed");

			ComObjPtr<VirtualBox>::operator = (&_instance);
		}
	} _virtualbox { };

	struct Session_instance : ComObjPtr<::Session>
	{
		Session_instance()
		{
			attempt([&] () { return createObject(); },
			        "construction of VirtualBox session object failed");
		}
	} _session { };

	struct Monitor_count { PRUint32 value; };

	struct Machine_instance : ComObjPtr<Machine>
	{
		Machine_instance(Virtualbox_instance  &virtualbox,
		                 Session_instance     &session,
		                 Vbox_file_path const &vbox_file_path)
		{
			attempt([&] () { return createObject(); },
			        "failed to create Machine object");

			HRESULT const rc =  (*this)->initFromSettings(virtualbox,
			                                              vbox_file_path.utf8,
			                                              nullptr);
			if (FAILED(rc)) {
				Genode::error("failed to init machine from settings");
				/*
				 * Use keeper to retrieve current error message
				 */
				ErrorInfoKeeper eik;
				Bstr const &text = eik.getText();
				Genode::log(Utf8Str(text.raw()).c_str());
				throw Fatal();
			}

			/*
			 * Add the machine to the VirtualBox::allMachines list
			 *
			 * Unfortunately, the 'i_registerMachine' function performs a
			 * 'i_saveSettings' should the 'VirtualBox' object not be in the
			 * 'InInit' state. However, the object is already in 'Ready' state.
			 * So, 'i_saveSettings' attempts to write a 'VirtualBox.xml' file
			 */
			{
				AutoWriteLock alock(virtualbox.m_p COMMA_LOCKVAL_SRC_POS);

				attempt([&] () { return (*this)->i_prepareRegister(); },
				        "could not enter registered state for machine");
			}

			attempt([&] () { return (*this)->LockMachine(session, LockType_VM); },
			        "failed to lock machine");
		}

		Monitor_count monitor_count()
		{
			ComPtr<IGraphicsAdapter> adapter;

			attempt([&] () { return (*this)->COMGETTER(GraphicsAdapter)(adapter.asOutParam()); },
			        "attempt to access virtual graphics adapter failed");

			Monitor_count result { 0 };

			attempt([&] () { return adapter->COMGETTER(MonitorCount)(&result.value); },
			        "unable to determine the number of virtual monitors");

			return result;
		}

	} _machine { _virtualbox, _session, _vbox_file_path };

	struct Console_interface : ComPtr<IConsole>
	{
		Console_interface(Session_instance &session)
		{
			attempt([&] () { return session->COMGETTER(Console)(this->asOutParam()); },
			        "unable to request console for session");
		}
	} _iconsole { _session };

	struct Display_interface : ComPtr<IDisplay>
	{
		Display_interface(Console_interface &iconsole)
		{
			attempt([&] () { return iconsole->COMGETTER(Display)(this->asOutParam()); },
			        "unable to request display from console interface");
		}
	} _idisplay { _iconsole };

	Signal_handler<Main> _capslock_handler { _env.ep(), *this, &Main::_handle_capslock };

	void _handle_capslock() { Libc::with_libc([&] { _sync_capslock(); }); }

	void _sync_capslock();

	struct Capslock
	{
		enum class Mode { RAW, ROM } const mode;

		bool _host  { false };
		bool _guest { false };

		Constructible<Attached_rom_dataspace> _rom;

		Capslock(Env &env, Xml_node config, Signal_context_capability sigh)
		:
			mode(config.attribute_value("capslock", String<4>()) == "rom"
			   ? Mode::ROM : Mode::RAW)
		{
			if (mode == Mode::ROM) {
				_rom.construct(env, "capslock");
				_rom->sigh(sigh);
			}
		}

		void destruct_rom() { _rom.destruct(); }

		void update_guest(bool enabled) { _guest = enabled; }

		bool update_from_rom()
		{
			if (mode != Mode::ROM)
				return false;

			_rom->update();

			bool const rom = _rom->xml().attribute_value("enabled", _guest);

			bool trigger = false;

			/*
			 * Trigger CapsLock change whenever the ROM state changes. This
			 * helps with guests that do not use the keyboard led to indicate
			 * the CapsLock state.
			 */
			if (rom != _host || rom != _guest)
				trigger = true;

			/* remember last seen host capslock state */
			_host = rom;

			return trigger;
		}
	} _capslock { _env, _config.xml(), _capslock_handler };

	Mouse_shape _mouse_shape { _env };

	Signal_handler<Main> _exit_handler { _env.ep(), *this, &Main::_handle_exit };

	Registry<Registered<Gui::Connection>> _gui_connections { };

	Signal_handler<Main> _input_handler { _env.ep(), *this, &Main::_handle_input };

	void _handle_input();

	Signal_handler<Main> _fb_mode_handler { _env.ep(), *this, &Main::_handle_fb_mode };

	void _handle_fb_mode();

	Input_adapter _input_adapter { _iconsole };

	bool const _genode_gui_attached = ( _attach_genode_gui(), true );

	void _attach_genode_gui()
	{
		Monitor_count const num_monitors = _machine.monitor_count();

		for (unsigned i = 0; i < num_monitors.value; i++) {

			String<3> label { i };

			Gui::Connection &gui = *new Registered<Gui::Connection>(_gui_connections, _env, label.string());

			gui.input.sigh(_input_handler);
			gui.mode_sigh(_fb_mode_handler);

			Genodefb *fb = new Genodefb(_env, gui, _idisplay);

			Bstr fb_id { };

			attempt([&] () { return _idisplay->AttachFramebuffer(i, fb, fb_id.asOutParam()); },
			        "unable to attach framebuffer to virtual monitor ", i);
		}
	}

	bool const _machine_powered_up = ( _power_up_machine(), true );

	void _power_up_machine()
	{
		ComPtr <IProgress> progress;

		attempt([&] () { return _iconsole->PowerUp(progress.asOutParam()); },
		        "powering up via console interface failed");

		/* wait until VM is up */
		MachineState_T state = MachineState_Null;
		do {
			if (state != MachineState_Null)
				RTThreadSleep(1000);

			attempt([&] () { return _machine->COMGETTER(State)(&state); },
			        "failed to obtain machine state");

		} while (state == MachineState_Starting);

		if (state != MachineState_Running) {
			error("machine could not enter running state");

			/* retrieve and print error information */
			IVirtualBoxErrorInfo *info;
			progress->COMGETTER(ErrorInfo)(&info);

			PRUnichar *text = (PRUnichar *)malloc(4096);
			info->GetText((PRUnichar **)&text);
			Genode::log("Error: ", Utf8Str(text).c_str());

			throw Fatal();
		}
	}

	void _power_down_machine()
	{
		_capslock.destruct_rom();

		_gui_connections.for_each([&] (Gui::Connection &gui) {
			delete &gui;
		});

		/* signal exit to main entrypoint */
		_exit_handler.local_submit();
	}

	void _handle_exit() { _env.parent().exit(0); }

	void handle_vbox_event(VBoxEventType_T, IEvent &) override;

	bool const _vbox_event_handler_installed = ( _install_vbox_event_handler(), true );

	void _install_vbox_event_handler()
	{
		ComObjPtr<Event_listener_impl> listener;

		listener.createObject();
		listener->init(new Event_listener(), *this);

		ComPtr<IEventSource> ievent_source;
		_iconsole->COMGETTER(EventSource)(ievent_source.asOutParam());

		com::SafeArray<VBoxEventType_T> event_types;
		event_types.push_back(VBoxEventType_OnMouseCapabilityChanged);
		event_types.push_back(VBoxEventType_OnMousePointerShapeChanged);
		event_types.push_back(VBoxEventType_OnKeyboardLedsChanged);
		event_types.push_back(VBoxEventType_OnStateChanged);
		event_types.push_back(VBoxEventType_OnAdditionsStateChanged);

		ievent_source->RegisterListener(listener, ComSafeArrayAsInParam(event_types), true);
	}

	Main(Genode::Env &env) : _env(env)
	{
		/*
		 * Explicitly, adapt to current framebuffer/window size after
		 * initialization finished. This ensures the use of the correct
		 * framebuffer dimensions in scenarios without a window manager.
		 */
		_handle_fb_mode();
	}
};


/* must be called in Libc::with_libc() context */
void Main::_sync_capslock()
{
	if (_capslock.update_from_rom()) {
		_input_adapter.handle_input_event(Input::Event { Input::Press   { Input::KEY_CAPSLOCK } });
		_input_adapter.handle_input_event(Input::Event { Input::Release { Input::KEY_CAPSLOCK } });
	}
}


void Main::_handle_input()
{
	auto handle_one_event = [&] (Input::Event const &ev) {
		/* don't confuse guests and drop CapsLock events in ROM mode  */
		if (_capslock.mode == Capslock::Mode::ROM) {
			if (ev.key_press(Input::KEY_CAPSLOCK)
			 || ev.key_release(Input::KEY_CAPSLOCK))
				return;
		}

		_input_adapter.handle_input_event(ev);
	};

	Libc::with_libc([&] {
		_gui_connections.for_each([&] (Gui::Connection &gui) {
			gui.input.for_each_event([&] (Input::Event const &ev) {
				handle_one_event(ev); }); }); });
}


void Main::_handle_fb_mode()
{
	Libc::with_libc([&] {
		_gui_connections.for_each([&] (Gui::Connection &gui) {
			IFramebuffer *pFramebuffer = NULL;
			HRESULT rc = _idisplay->QueryFramebuffer(0, &pFramebuffer);
			Assert(SUCCEEDED(rc) && pFramebuffer); (void)rc;

			Genodefb *fb = dynamic_cast<Genodefb *>(pFramebuffer);

			fb->update_mode(gui.mode());

			if ((fb->w() <= 1) && (fb->h() <= 1)) {
				/* interpret a size of 0x0 as indication to quit VirtualBox */
				if (_iconsole->PowerButton() != S_OK)
					Genode::error("ACPI shutdown failed");
				return;
			}

			_idisplay->SetVideoModeHint(0 /*=display*/,
			                            true /*=enabled*/, false /*=changeOrigin*/,
			                            0 /*=originX*/, 0 /*=originY*/,
			                            fb->w(), fb->h(),
			                            32, true);
		});
	});
}


void Main::handle_vbox_event(VBoxEventType_T ev_type, IEvent &ev)
{
	switch (ev_type) {
	case VBoxEventType_OnMouseCapabilityChanged:
		{
			ComPtr<IMouseCapabilityChangedEvent> cap_ev = &ev;
			BOOL absolute;
			cap_ev->COMGETTER(SupportsAbsolute)(&absolute);
			_input_adapter.mouse_absolute(!!absolute);
		} break;

	case VBoxEventType_OnMousePointerShapeChanged:
		{
			ComPtr<IMousePointerShapeChangedEvent> shape_ev = &ev;
			BOOL  visible, alpha;
			ULONG xHot, yHot, width, height;
			com::SafeArray <BYTE> shape;

			shape_ev->COMGETTER(Visible)(&visible);
			shape_ev->COMGETTER(Alpha)(&alpha);
			shape_ev->COMGETTER(Xhot)(&xHot);
			shape_ev->COMGETTER(Yhot)(&yHot);
			shape_ev->COMGETTER(Width)(&width);
			shape_ev->COMGETTER(Height)(&height);
			shape_ev->COMGETTER(Shape)(ComSafeArrayAsOutParam(shape));

			_mouse_shape.update(visible, alpha, xHot, yHot, width, height,
			                    ComSafeArrayAsInParam(shape));

		} break;

	case VBoxEventType_OnKeyboardLedsChanged:
		{
			/*
			 * Use CapsLock LED as indicator for guest assumption about the
			 * state and optionally resync to host state. This is required
			 * because the guest may try to switch CapsLock (off) on its own,
			 * e.g. during startup.
			 */

			ComPtr<IKeyboardLedsChangedEvent> led_ev = &ev;
			BOOL capslock;
			led_ev->COMGETTER(CapsLock)(&capslock);
			_capslock.update_guest(!!capslock);
			_sync_capslock();
		} break;

	case VBoxEventType_OnStateChanged:
		{
			ComPtr<IStateChangedEvent> state_change_ev = &ev;
			MachineState_T machineState;
			state_change_ev->COMGETTER(State)(&machineState);

			if (machineState == MachineState_PoweredOff)
				_power_down_machine();

		} break;

	case VBoxEventType_OnAdditionsStateChanged:
		{
			/*
			 * Try to sync initial CapsLock state when starting a guest OS.
			 * Usually this is only a problem when CapsLock is already on
			 * during startup, because the guest will assume it's off or
			 * deliberately clear the CapsLock state during boot.
			 *
			 * Ideally this should only be done once, after the guest is ready
			 * to process the CapsLock key but before it's ready for login. The
			 * OnAdditionsStateChanged event will fire a few times during boot,
			 * but maybe not when we really need it to. Maybe there is a better
			 * event to listen to, once the guest additions are fulling
			 * working, like VBoxEventType_OnGuestSessionRegistered.
			 *
			 * For a list of "VBoxEventType_..." events see
			 * virtualbox6_sdk/sdk/bindings/xpcom/include/VirtualBox_XPCOM.h
			 */
			_sync_capslock();
		} break;

	default: /* ignore other events */ break;
	}
}


/* initial environment for the FreeBSD libc implementation */
extern char **environ;


void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] () {

		/* extract args and environment variables from config */
		int argc    = 0;
		char **argv = nullptr;
		char **envp = nullptr;

		populate_args_and_env(env, argc, argv, envp);

		environ = envp;

		Pthread::init(env);
		Network::init(env);

		/* sidestep 'rtThreadPosixSelectPokeSignal' */
		uint32_t const fFlags = RTR3INIT_FLAGS_UNOBTRUSIVE;

		{
			int const rc = RTR3InitExe(argc, &argv, fFlags);
			if (RT_FAILURE(rc))
				throw -1;
		}

		{
			nsCOMPtr<nsIServiceManager> serviceManager;
			HRESULT const rc = NS_InitXPCOM2(getter_AddRefs(serviceManager), nsnull, nsnull);
			if (NS_FAILED(rc))
			{
				Genode::error("failed to initialize XPCOM, rc=", rc);
				throw -2;
			}
		}

		Sup::init(env);
		Xhci::init(env);
		Services::init(env);

		try {
			static Main main(env);
		}
		catch (...) {
			error("startup of virtual machine failed, giving up.");
		}
	});
}


NS_IMPL_THREADSAFE_ISUPPORTS1_CI(Genodefb, IFramebuffer)
NS_DECL_CLASSINFO(Genodefb)
