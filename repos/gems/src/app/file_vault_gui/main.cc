/*
 * \brief  Test for Genode's dialog API
 * \author Norman Feske
 * \date   2023-03-25
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <dialog/runtime.h>
#include <dialog/widgets.h>
#include <file_vault/types.h>

using namespace Dialog;
using namespace File_vault;

using Text = String<32>;

struct Back_button : Widget<Float>
{
	void view(Scope<Float> &s) const
	{
		s.sub_scope<Button>([&] (Scope<Float, Button> &s) {
			if (s.hovered()) s.attribute("hovered", "yes");
			s.attribute("style", "back");
			s.sub_scope<Hbox>();
		});
	}

	void click(Clicked_at const &, auto const &fn) { fn(); }
};

struct Left_align : Sub_scope
{
	static void view_sub_scope(auto &s, auto const &text)
	{
		s.node("float", [&] {
			s.attribute("west", "yes");
			s.named_sub_node("label", "label", [&] {
				s.attribute("text", text); }); });
	}

	static void with_narrowed_at(auto const &, auto const &) { }
};

struct Switch : Widget<Button>
{
	bool &on;

	Switch(bool &on) : on(on) { }

	void view(Scope<Button> &s, Text const &on_text, Text const &off_text) const
	{
		bool const hovered = (s.hovered() && (!s.dragged() || on));
		if (hovered) s.attribute("hovered",  "yes");
		s.sub_scope<Label>(on ? on_text : off_text);
	}

	void click(Clicked_at const &) { on = !on; }
};

struct Prompt : Widget<Button>
{
	struct Action : Interface, Noncopyable { virtual void refresh_prompt() = 0; };

	Text text { };

	void handle_event(Event const &event, Action &action, auto const &appendable)
	{
		event.event.handle_press([&] (Input::Keycode key, Codepoint code) {
			if (key == Input::KEY_BACKSPACE) {
				text = Cstring(text.string(), text.length() - 2);
				action.refresh_prompt();
			} else if (code.valid()) {
				if (appendable(code)) {
					text = { text, code };
					action.refresh_prompt();
				}
			}
		});
	}

	void view(Scope<Button> &s, bool selected, auto const &viewed_text) const
	{
		if (s.hovered())
			s.attribute("hovered", "yes");

		s.sub_scope<Float>([&] (Scope<Button, Float> &s) {
			s.attribute("west",  "yes");
			s.sub_scope<Vbox>([&] (Scope<Button, Float, Vbox> &s) {
				s.sub_scope<Min_ex>(20);
				s.node("float", [&] {
					s.attribute("west", "yes");
					s.sub_scope<Label>(viewed_text(), [&] (Scope<Button, Float, Vbox, Label> &s) {
						if (selected)
							s.sub_node("cursor", [&] {
								s.attribute("name", "cursor");
								s.attribute("at", text.length()); });
					});
				});
			});
		});
	}

	void click(Clicked_at const &, auto const &fn) { fn(); }
};

struct Passphrase_prompt : Prompt
{
	bool show_text { false };

	void view(Scope<Button> &s, bool selected) const
	{
		Prompt::view(s, selected, [&] {

			using Result = String<Text::size()*3>;
			if (show_text)
				return Result(text);

			Result bullets { };
			if (text.valid()) {
				auto fn = [&] (char const *str) { bullets = Cstring(str, strlen(str)); };
				Buffered_output<bullets.size(), decltype(fn)> out(fn);
				for (size_t i = 0; i++ < text.length() - 1;)
					print(out, "\u2022");
			}
			return bullets; });
	}

	void handle_event(Event const &event, Action &action)
	{
		Prompt::handle_event(event, action, [&] (Codepoint code) {
			return code.value >= 0x20 && code.value < 0xf000; });
	}
};

struct Size_prompt : Prompt
{
	void view(Scope<Button> &s, bool selected) const { Prompt::view(s, selected, [&] { return text; }); }

	void handle_event(Event const &event, Action &action)
	{
		Prompt::handle_event(event, action, [&] (Codepoint code) {
			return (code.value >= '0' && code.value <= '9') || code.value == 'K' || code.value == 'M' || code.value == 'G'; });
	}

	Number_of_bytes as_num_bytes() const
	{
		Number_of_bytes ret { };
		ascii_to(text.string(), ret);
		return ret;
	}
};

struct Main : Prompt::Action
{
	enum Dialog_type { NONE, SETUP, WAIT, CONTROLS, UNLOCK };

	struct Unlock_frame : Widget<Frame>
	{
		Main &main;
		Hosted<Frame, Vbox, Hbox, Passphrase_prompt> passphrase { Id("Passphrase") };
		Hosted<Frame, Vbox, Hbox, Switch> show_passphrase { Id("Show Passphrase"), passphrase.show_text };
		Hosted<Frame, Vbox, Action_button> unlock_button { Id("Unlock") };

		Unlock_frame(Main &main) : main(main) { }

		bool passphrase_long_enough() const { return passphrase.text.length() >= MIN_PASSPHRASE_LENGTH + 1; }

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				s.sub_scope<Left_align>(" Passphrase: ");
				s.sub_scope<Hbox>([&] (Scope<Frame, Vbox, Hbox> &s) {
					s.widget(passphrase, true);
					s.widget(show_passphrase, "Hide", "Show");
				});
				if (passphrase_long_enough())
					s.widget(unlock_button);
				else
					s.sub_scope<Left_align>(Text(" Minimum length: ", (size_t)MIN_PASSPHRASE_LENGTH));
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase.propagate(at, [&] { });
			show_passphrase.propagate(at);
			unlock_button.propagate(at, [&] { unlock(); });
		}

		void unlock()
		{
			main.unlock(*this);
			passphrase.text = "";
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (passphrase_long_enough())
						unlock();
					break;

				case Input::KEY_TAB: break;
				default: passphrase.handle_event(event, main); break;
				}
			});
		}
	};

	struct Setup_frame : Widget<Frame>
	{
		enum Prompt_type { PASSPHRASE, CAPACITY, JOURNALING_BUFFER };

		Main &main;
		Prompt_type selected = PASSPHRASE;
		Hosted<Frame, Vbox, Hbox, Passphrase_prompt> passphrase { Id("Passphrase") };
		Hosted<Frame, Vbox, Hbox, Switch> show_passphrase { Id("Show Passphrase"), passphrase.show_text };
		Hosted<Frame, Vbox, Size_prompt> capacity { Id("Capacity") };
		Hosted<Frame, Vbox, Size_prompt> journal_buf { Id("Journaling Buffer") };
		Hosted<Frame, Vbox, Action_button> start_button { Id("Start") };

		Setup_frame(Main &main) : main(main) { }

		bool passphrase_long_enough() const { return passphrase.text.length() >= MIN_PASSPHRASE_LENGTH + 1; }

		bool capacity_sufficient() const { return capacity.as_num_bytes() >= MIN_CAPACITY; }

		bool journal_buf_sufficient() const { return journal_buf.as_num_bytes() >= min_journal_buf(capacity.as_num_bytes()); }

		bool ready_to_setup() const { return passphrase_long_enough() && capacity_sufficient() && journal_buf_sufficient(); }

		Number_of_bytes image_size() const
		{
			return (size_t)(BLOCK_SIZE * tresor_num_blocks(
				NR_OF_SUPERBLOCK_SLOTS, TRESOR_VBD_MAX_LVL + 1, TRESOR_VBD_DEGREE,
				tresor_tree_num_leaves(capacity.as_num_bytes()), TRESOR_FREE_TREE_MAX_LVL + 1,
				TRESOR_FREE_TREE_DEGREE, tresor_tree_num_leaves(journal_buf.as_num_bytes())));
		}

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				s.sub_scope<Left_align>(" Passphrase: ");
				s.sub_scope<Hbox>([&] (Scope<Frame, Vbox, Hbox> &s) {
					s.widget(passphrase, selected == PASSPHRASE);
					s.widget(show_passphrase, "Hide", "Show");
				});
				if (!passphrase_long_enough())
					s.sub_scope<Left_align>(Text(" Minimum length: ", (size_t)MIN_PASSPHRASE_LENGTH, " "));

				s.sub_scope<Left_align>("");
				s.sub_scope<Left_align>(" Capacity: ");
				s.widget(capacity, selected == CAPACITY);
				if (!capacity_sufficient())
					s.sub_scope<Left_align>(Text(" Minimum: ", Number_of_bytes(MIN_CAPACITY), " "));

				s.sub_scope<Left_align>("");
				s.sub_scope<Left_align>(" Journaling buffer: ");
				s.widget(journal_buf, selected == JOURNALING_BUFFER);
				if (!journal_buf_sufficient())
					s.sub_scope<Left_align>(Text(" Minimum: ", min_journal_buf(capacity.as_num_bytes()), " "));

				if (capacity_sufficient() && journal_buf_sufficient()) {
					s.sub_scope<Left_align>("");
					s.sub_scope<Left_align>(Text(" Image size: ", image_size(), " "));
				}
				if (ready_to_setup()) {
					s.sub_scope<Left_align>("");
					s.widget(start_button);
				}
			});
		}

		void click(Clicked_at const &at)
		{
			passphrase.propagate(at, [&] { selected = PASSPHRASE; });
			show_passphrase.propagate(at);
			capacity.propagate(at, [&] { selected = CAPACITY; });
			journal_buf.propagate(at, [&] { selected = JOURNALING_BUFFER; });
			start_button.propagate(at, [&] { setup(); });
		}

		void select_next()
		{
			switch (selected) {
			case PASSPHRASE: selected = CAPACITY; break;
			case CAPACITY: selected = JOURNALING_BUFFER; break;
			case JOURNALING_BUFFER: selected = PASSPHRASE; break;
			}
			main.main_view.refresh();
		}

		void forward_to_selected(Dialog::Event const &event)
		{
			switch (selected) {
			case PASSPHRASE: passphrase.handle_event(event, main); break;
			case CAPACITY: capacity.handle_event(event, main); break;
			case JOURNALING_BUFFER: journal_buf.handle_event(event, main); break;
			}
		}

		void setup()
		{
			main.setup(*this);
			passphrase.text = "";
			capacity.text = "";
			journal_buf.text = "";
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode key, Codepoint) {
				switch (key) {
				case Input::KEY_ENTER:

					if (ready_to_setup())
						setup();
					break;

				case Input::KEY_TAB: select_next(); break;
				default: forward_to_selected(event); break;
				}
			});
		}
	};

	struct Wait_frame : Widget<Frame>
	{
		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Label>(" Please wait ... ");
		}
	};

	struct Controls_frame : Widget<Frame>
	{
		enum Tab { HOME, ENCRYPTION_KEY, CAPACITY, JOURNALING_BUFFER };

		struct Navigation_bar : Widget<Float>
		{
			Controls_frame &controls;
			Hosted<Float, Hbox, Back_button> back_button { Id("Back") };

			Navigation_bar(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Float> &s, Text const &text) const
			{
				s.attribute("west", "yes");
				s.sub_scope<Hbox>([&] (Scope<Float, Hbox> &s) {
					s.widget(back_button);
					s.node("float", [&] {
						s.attribute("west", "yes");
						s.named_sub_node("label", "label", [&] {
							s.attribute("font", "title/regular");
							s.attribute("text", text); }); }); });
			}

			void click(Clicked_at const &at) { back_button.propagate(at, [&] { controls.visible_tab = HOME; }); }
		};

		struct Home : Widget<Vbox>
		{
			Controls_frame &controls;
			Hosted<Vbox, Action_button> capacity_button { Id("Capacity") };
			Hosted<Vbox, Action_button> journal_buf_button { Id("Journaling Buffer") };
			Hosted<Vbox, Action_button> encrypt_key_button { Id("Encryption Key") };

			Home(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Vbox> &s) const
			{
				if (!controls.main.ui_report->num_clients.value)
					s.widget(capacity_button);

				s.widget(journal_buf_button);
				s.widget(encrypt_key_button);

				if (controls.main.ui_report->num_clients.value) {
					s.sub_scope<Left_align>("");
					s.sub_scope<Left_align>(" Capacity unchangeable when in use! ");
				}
			}

			void click(Clicked_at const &at)
			{
				capacity_button.propagate(at, [&] { controls.visible_tab = CAPACITY; });
				journal_buf_button.propagate(at, [&] { controls.visible_tab = JOURNALING_BUFFER; });
				encrypt_key_button.propagate(at, [&] { controls.visible_tab = ENCRYPTION_KEY; });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode, Codepoint code) {
					switch (code.value) {
					case 'c': controls.switch_to_tab(CAPACITY); break;
					case 'j': controls.switch_to_tab(JOURNALING_BUFFER); break;
					case 'e': controls.switch_to_tab(ENCRYPTION_KEY); break;
					default: break;
					}
				});
			}
		};

		template <Ui_config::Extend::Tree TREE>
		struct Dimension_tab : Widget<Vbox>
		{
			enum { MIN_NUM_BYTES = 4096 };

			Controls_frame &controls;
			Hosted<Vbox, Navigation_bar> navigation_bar { Id("Navigation Bar"), controls };
			Hosted<Vbox, Size_prompt> num_bytes_prompt { Id("Number Of Bytes") };
			Hosted<Vbox, Action_button> extend_button { Id("Extend") };

			Dimension_tab(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Vbox> &s) const
			{
				s.widget(navigation_bar,
					TREE == Ui_config::Extend::VIRTUAL_BLOCK_DEVICE ? "Capacity " :
					TREE == Ui_config::Extend::FREE_TREE ? "Journaling Buffer " : "");

				if (controls.main.ready_to_extend()) {
					s.widget(num_bytes_prompt, true);
					if (num_bytes_prompt.as_num_bytes() >= MIN_NUM_BYTES)
						s.widget(extend_button);
					else
						s.sub_scope<Left_align>(Text(" Minimum: ", Number_of_bytes(MIN_NUM_BYTES), " "));
				} else
					s.sub_scope<Left_align>(" Please wait ... ");
			}

			void extend()
			{
				controls.main.extend<TREE>(*this);
				num_bytes_prompt.text = "";
			}

			void click(Clicked_at const &at)
			{
				navigation_bar.propagate(at);
				extend_button.propagate(at, [&] { extend(); });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode key, Codepoint) {
					switch (key) {
					case Input::KEY_ENTER:

						if (controls.main.ready_to_extend() && num_bytes_prompt.as_num_bytes() >= MIN_NUM_BYTES)
							extend();
						break;

					case Input::KEY_ESC: controls.switch_to_tab(HOME); break;
					case Input::KEY_TAB: break;
					default:

						if (controls.main.ready_to_extend())
							num_bytes_prompt.handle_event(event, controls.main);
						break;
					}
				});
			}
		};

		struct Encryption_key : Widget<Vbox>
		{
			Controls_frame &controls;
			Hosted<Vbox, Navigation_bar> navigation_bar { Id("Navigation Bar"), controls };
			Hosted<Vbox, Action_button> replace_button { Id("Replace") };

			Encryption_key(Controls_frame &controls) : controls(controls) { }

			void view(Scope<Vbox> &s) const
			{
				s.widget(navigation_bar, "Encryption Key ");
				if (controls.main.ready_to_rekey())
					s.widget(replace_button);
				else
					s.sub_scope<Left_align>(" Please wait ... ");
			}

			void click(Clicked_at const &at)
			{
				navigation_bar.propagate(at);
				replace_button.propagate(at, [&] { controls.main.rekey(); });
			}

			void handle_event(Dialog::Event const &event)
			{
				event.event.handle_press([&] (Input::Keycode key, Codepoint) {
					switch (key) {
					case Input::KEY_ENTER:

						if (controls.main.ready_to_rekey())
							controls.main.rekey();
						break;

					case Input::KEY_ESC: controls.switch_to_tab(HOME); break;
					default: break;
					}
				});
			}
		};

		Main &main;
		Tab visible_tab { HOME };
		Hosted<Frame, Vbox, Home> home { Id("Home"), *this };
		Hosted<Frame, Vbox, Dimension_tab<Ui_config::Extend::VIRTUAL_BLOCK_DEVICE> > capacity { Id("Capacity"), *this };
		Hosted<Frame, Vbox, Dimension_tab<Ui_config::Extend::FREE_TREE> > journal_buf { Id("Journaling Buffer"), *this };
		Hosted<Frame, Vbox, Encryption_key> encryption_key { Id("Encryption Key"), *this };
		Hosted<Frame, Vbox, Action_button> lock_button { Id("Lock") };

		Controls_frame(Main &main) : main(main) { }

		void switch_to_tab(Tab tab)
		{
			visible_tab = tab;
			main.main_view.refresh();
		}

		void view(Scope<Frame> &s) const
		{
			s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
				switch (visible_tab) {
				case HOME: s.widget(home); break;
				case ENCRYPTION_KEY: s.widget(encryption_key); break;
				case CAPACITY: s.widget(capacity); break;
				case JOURNALING_BUFFER: s.widget(journal_buf); break;
				}
				s.sub_scope<Left_align>("");
				s.sub_scope<Left_align>(Text(" Image: ", main.ui_report->image_size, " "));
				s.sub_scope<Left_align>(Text(" Capacity: ", main.ui_report->capacity, " "));
				s.sub_scope<Left_align>(Text(" Clients: ", main.ui_report->num_clients.value, " "));
				s.sub_scope<Left_align>("");
				s.widget(lock_button);
			});
		}

		void lock()
		{
			main.lock();
			visible_tab = HOME;
			capacity.num_bytes_prompt.text = "";
			journal_buf.num_bytes_prompt.text = "";
		}

		void click(Clicked_at const &at)
		{
			switch (visible_tab) {
			case HOME: home.propagate(at); break;
			case ENCRYPTION_KEY: encryption_key.propagate(at); break;
			case CAPACITY: capacity.propagate(at); break;
			case JOURNALING_BUFFER: journal_buf.propagate(at); break;
			default: break;
			}
			lock_button.propagate(at, [&] { lock(); });
		}

		void handle_event(Dialog::Event const &event)
		{
			event.event.handle_press([&] (Input::Keycode, Codepoint code) {
				switch (code.value) {
				case 'l': lock(); break;
				default:

					switch (visible_tab) {
					case HOME: home.handle_event(event); break;
					case CAPACITY: capacity.handle_event(event); break;
					case JOURNALING_BUFFER: journal_buf.handle_event(event); break;
					case ENCRYPTION_KEY: encryption_key.handle_event(event); break;
					}
					break;
				}
			});
		}

		void handle_signal()
		{
			if (visible_tab == CAPACITY && main.ui_report->num_clients.value)
				visible_tab = HOME;
		}
	};

	struct Main_dialog : Top_level_dialog
	{
		Main &main;
		Hosted<Unlock_frame> unlock_frame { Id("unlock"), main };
		Hosted<Setup_frame> setup_frame { Id("setup"), main };
		Hosted<Wait_frame> wait_frame { Id("wait") };
		Hosted<Controls_frame> controls_frame { Id("controls"), main };

		Main_dialog(Name const &name, Main &main) : Top_level_dialog(name), main(main) { }

		void handle_event(Dialog::Event const &event)
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.handle_event(event); break;
			case UNLOCK: unlock_frame.handle_event(event); break;
			case CONTROLS: controls_frame.handle_event(event); break;
			default: break;
			}
		}

		void handle_signal()
		{
			if (main.active_dialog == CONTROLS)
				controls_frame.handle_signal();
		}

		void view(Scope<> &s) const override
		{
			switch (main.active_dialog) {
			case UNLOCK: s.widget(unlock_frame); break;
			case SETUP: s.widget(setup_frame); break;
			case WAIT: s.widget(wait_frame); break;
			case CONTROLS: s.widget(controls_frame); break;
			case NONE: s.node("empty", [&] { }); break;
			}
		}

		void click(Clicked_at const &at) override
		{
			switch (main.active_dialog) {
			case SETUP: setup_frame.click(at); break;
			case CONTROLS: controls_frame.click(at); break;
			case UNLOCK: unlock_frame.click(at); break;
			default: break;
			}
		}
	};

	Env &env;
	Dialog_type active_dialog { NONE };
	Heap heap { env.ram(), env.rm() };
	Runtime runtime { env, heap };
	Main_dialog main_dialog { "main", *this };
	Runtime::View main_view { runtime, main_dialog };
	Runtime::Event_handler<Main> event_handler { runtime, *this, &Main::handle_event };
	Ui_config ui_config { };
	Expanding_reporter ui_config_reporter { env, "ui_config", "ui_config" };
	Attached_rom_dataspace ui_report_rom { env, "ui_report" };
	Signal_handler<Main> signal_handler { env.ep(), *this, &Main::handle_signal };
	Reconstructible<Ui_report> ui_report { };

	void handle_event(Dialog::Event const &event)
	{
		main_dialog.handle_event(event);
	}

	void setup(Setup_frame const &setup_frame)
	{
		ui_config.client_fs_size = setup_frame.capacity.as_num_bytes();
		ui_config.journaling_buf_size = setup_frame.journal_buf.as_num_bytes();
		ui_config.passphrase = setup_frame.passphrase.text;
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void unlock(Unlock_frame const &unlock_frame)
	{
		ui_config.passphrase = unlock_frame.passphrase.text;
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void lock()
	{
		ui_config.passphrase = "";
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	bool ready_to_extend() const
	{
		if (!ui_config.extend.constructed())
			return true;

		if (!ui_report->extend.constructed())
			return false;

		return ui_report->extend->id.value == ui_config.extend->id.value && ui_report->extend->finished;
	}

	bool ready_to_rekey() const
	{
		if (!ui_config.rekey.constructed())
			return true;

		if (!ui_report->rekey.constructed())
			return false;

		return ui_report->rekey->id.value == ui_config.rekey->id.value && ui_report->rekey->finished;
	}

	void rekey()
	{
		Operation_id id { ui_report->rekey.constructed() ? ui_report->rekey->id.value + 1 : 0 };
		ui_config.rekey.construct(id);
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	template <Ui_config::Extend::Tree TREE>
	void extend(Controls_frame::Dimension_tab<TREE> const &dimension_tab)
	{
		Operation_id id { ui_report->extend.constructed() ? ui_report->extend->id.value + 1 : 0 };
		ui_config.extend.construct(id, TREE, dimension_tab.num_bytes_prompt.as_num_bytes());
		ui_config_reporter.generate([&] (Xml_generator &xml) { ui_config.generate(xml); });
	}

	void handle_signal()
	{
		ui_report_rom.update();
		ui_report.construct(ui_report_rom.xml());
		switch (ui_report->state) {
		case Ui_report::INVALID: active_dialog = WAIT; break;
		case Ui_report::UNINITIALIZED: active_dialog = SETUP; break;
		case Ui_report::INITIALIZING: active_dialog = WAIT; break;
		case Ui_report::UNLOCKING: active_dialog = WAIT; break;
		case Ui_report::UNLOCKED: active_dialog = CONTROLS; break;
		case Ui_report::LOCKING: active_dialog = WAIT; break;
		case Ui_report::LOCKED: active_dialog = UNLOCK; break;
		}
		main_dialog.handle_signal();
		main_view.refresh();
	}

	Main(Env &env) : env(env)
	{
		ui_report_rom.sigh(signal_handler);
		handle_signal();
	}

	void refresh_prompt() override { main_view.refresh(); }
};


void Component::construct(Genode::Env &env) { static Main main(env); }
