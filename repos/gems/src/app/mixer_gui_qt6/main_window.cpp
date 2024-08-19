/*
 * \brief   Main window of the mixer frontend
 * \author  Josef Soentgen
 * \date    2015-10-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <mixer/channel.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <rom_session/connection.h>

/* Qt includes */
#include <QCheckBox>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QSlider>
#include <QWidget>

/* application includes */
#include "main_window.h"


/************
 ** helper **
 ************/

using Channel = Mixer::Channel;

static struct Names {
	char const      *name;
	Channel::Number  number;
} names[] = {
	{ "left", Channel::Number::LEFT },
	{ "front left", Channel::Number::LEFT },
	{ "right", Channel::Number::RIGHT },
	{ "front right", Channel::Number::RIGHT },
	{ nullptr, Channel::Number::MAX_CHANNELS }
};


static char const *channel_string_from_number(Channel::Number ch)
{
	for (Names *n = names; n->name; ++n)
		if (ch == n->number)
			return n->name;
	return nullptr;
}


/* keep sorted! */
static struct Types {
	char const    *name;
	Channel::Type  type;
} types[] = {
	{ "invalid", Channel::Type::TYPE_INVALID },
	{ "input",   Channel::Type::INPUT },
	{ "output",  Channel::Type::OUTPUT }
};


static char const *type_to_string(Channel::Type t) { return types[t].name; }


class Channel_widget : public Compound_widget<QFrame, QVBoxLayout>,
                       public Genode::List<Channel_widget>::Element
{
	Q_OBJECT

	private:

		Channel::Number _number;
		Channel::Type   _type;

		QCheckBox   _muted_checkbox;
		QSlider     _slider { Qt::Vertical };
		QHBoxLayout _slider_hbox;

	Q_SIGNALS:

		void channel_changed();

	public:

		Channel_widget(Channel::Type type, Channel::Number number)
		:
			_number(number), _type(type),
			_muted_checkbox("mute")
		{
			_slider.setMinimum(Channel::Volume_level::MIN);
			_slider.setMaximum(Channel::Volume_level::MAX);

			_slider_hbox.addStretch();
			_slider_hbox.addWidget(&_slider, Qt::AlignCenter);
			_slider_hbox.addStretch();

			_layout->addLayout(&_slider_hbox);
			_layout->addWidget(&_muted_checkbox);

			connect(&_slider, SIGNAL(sliderReleased()),
			        this,     SIGNAL(channel_changed()));
			connect(&_muted_checkbox, SIGNAL(clicked(bool)),
			        this,             SIGNAL(channel_changed()));
		}

		Channel::Number number() const { return _number; }
		Channel::Type   type()   const { return _type; }
		int  volume()            const { return _slider.value(); }
		void volume(int v)             { _slider.setValue(v); }
		bool muted()             const { return _muted_checkbox.checkState() == Qt::Checked; }
		void muted(bool v)             { _muted_checkbox.setChecked(v); }
};


class Client_widget : public Compound_widget<QFrame, QVBoxLayout>,
                      public Genode::List<Client_widget>::Element
{
	Q_OBJECT

	public:

		bool valid { true };

		void _sorted_insert(Channel_widget *cw)
		{
			Channel::Number const nr   = cw->number();

			Channel_widget const *last = nullptr;
			Channel_widget const *w    = _list.first();
			for (; w; w = w->next()) {
				if (w->number() > nr)
					break;
				last = w;
			}
			_list.insert(cw, last);
		}

	private:

		Genode::List<Channel_widget>  _list;
		Channel::Label                _label;

		QLabel      _name;
		QHBoxLayout _hlayout;

		static char const *_strip_label(Channel::Label const &label)
		{
			char const * str = label.string();
			int pos = 0;
			for (int i = 0; str[i]; i++)
				if (str[i] == '>') pos = i+1;

			return str+pos;
		}

	Q_SIGNALS:

		void client_changed();

	public:

		Client_widget(Channel::Label const &label)
		:
			_label(label),
			_name(_strip_label(_label))
		{
			setFrameStyle(QFrame::Panel | QFrame::Raised);
			setLineWidth(4);
			setToolTip(_label.string());

			_name.setAlignment(Qt::AlignCenter);
			_name.setContentsMargins(0, 0, 0, 5);

			_layout->addWidget(&_name);
			_layout->addLayout(&_hlayout);
			_layout->setContentsMargins(10, 10, 10, 10);
		}

		~Client_widget()
		{
			while (Channel_widget *ch = _list.first()) {
				disconnect(ch, SIGNAL(channel_changed()));
				_hlayout.removeWidget(ch);
				_list.remove(ch);
				delete ch;
			}
		}

		Channel::Label const &label() const { return _label; }

		Channel_widget* lookup_channel(Channel::Number const number)
		{
			for (Channel_widget *ch = _list.first(); ch; ch = ch->next())
				if (number == ch->number())
					return ch;
			return nullptr;
		}

		Channel_widget* add_channel(Channel::Type const   type,
		                            Channel::Number const number)
		{
			Channel_widget *ch = new Channel_widget(type, number);
			connect(ch,   SIGNAL(channel_changed()),
			        this, SIGNAL(client_changed()));

			_sorted_insert(ch);
			_hlayout.addWidget(ch);

			return ch;
		}

		Channel_widget const* first_channel() const { return _list.first(); }

		void only_show_first()
		{
			Channel_widget *cw = _list.first();
			while ((cw = cw->next())) cw->hide();
		}

		bool combined_control() const
		{
			/*
			 * Having a seperate volume control widget for each channel is
			 * nice-to-have but for now it is unnecessary. We therefore disable
			 * it the hardcoded way.
			 */
			return true;
		}
};


class Client_widget_registry : public QObject
{
	Q_OBJECT

	private:

		Genode::List<Client_widget>  _list;

		void _remove_destroy(Client_widget *c)
		{
			disconnect(c, SIGNAL(client_changed()));
			_list.remove(c);
			delete c;
		}

	Q_SIGNALS:

		void registry_changed();

	public:

		Client_widget_registry() : QObject() { }

		Client_widget* first() { return _list.first(); }

		Client_widget* lookup(Channel::Label const &label)
		{
			for (Client_widget *c = _list.first(); c; c = c->next()) {
				if (label == c->label())
					return c;
			}
			return nullptr;
		}

		Client_widget* alloc_insert(Channel::Label const &label)
		{
			Client_widget *c = lookup(label);
			if (c == nullptr) {
				c = new Client_widget(label);
				connect(c,    SIGNAL(client_changed()),
				        this, SIGNAL(registry_changed()));
				_list.insert(c);
			}
			return c;
		}

		void invalidate_all()
		{
			for (Client_widget *c = _list.first(); c; c = c->next())
				c->valid = false;
		}

		void remove_invalid()
		{
			for (Client_widget *c = _list.first(); c; c = c->next())
				if (c->valid == false) _remove_destroy(c);
		}
};


static Client_widget_registry *client_registry()
{
	static Client_widget_registry inst;
	return &inst;
}


static char const * const config_file = "/config/mixer.config";


static int write_config(char const *file, char const *data, size_t length)
{
	if (length == 0) return 0;

	QFile mixer_file(file);
	if (!mixer_file.open(QIODevice::WriteOnly)) {
		Genode::error("could not open '", file, "'");
		return -1;
	}

	mixer_file.write(data, length);
	mixer_file.close();

	return 0;
}


void Main_window::_update_config()
{
	char   xml_data[2048];
	size_t xml_used = 0;

	try {
		Genode::Xml_generator xml(xml_data, sizeof(xml_data), "config", [&] {

			xml.node("default", [&] {
				xml.attribute("out_volume", _default_out_volume);
				xml.attribute("volume",     _default_volume);
				xml.attribute("muted",      _default_muted);
			});

			xml.node("channel_list", [&] {
				for (Client_widget const *c = client_registry()->first(); c; c = c->next()) {
					bool const combined = c->combined_control();

					static int  vol = 0;
					static bool muted = true;
					if (combined) {
						Channel_widget const *w = c->first_channel();
						vol   = w->volume();
						muted = w->muted();
					}

					for (Channel_widget const *w = c->first_channel(); w; w = w->next()) {
						Channel::Number const nr = w->number();
						xml.node("channel", [&] {
							xml.attribute("type",   type_to_string(w->type()));
							xml.attribute("label",  c->label().string());
							xml.attribute("name",   channel_string_from_number(nr));
							xml.attribute("number", nr);
							xml.attribute("volume", combined ? vol   : w->volume());
							xml.attribute("muted",  combined ? muted : w->muted());
						});

						if (_verbose)
							Genode::log("label: '", c->label(), "' "
							            "volume: ", combined ? vol   : w->volume(), " "
							            "muted: ",  combined ? muted : w->muted());
					}
				}
			});
		});
		xml_used = xml.used();

	} catch (...) { Genode::warning("could generate 'mixer.config'"); }

	write_config(config_file, xml_data, xml_used);
}


void Main_window::_update_clients(Genode::Xml_node &channels)
{
	for (Client_widget *c = client_registry()->first(); c; c = c->next())
		_layout->removeWidget(c);

	client_registry()->invalidate_all();

	channels.for_each_sub_node("channel", [&] (Genode::Xml_node const &node) {
		try {
			Channel ch(node);

			Client_widget *c = client_registry()->lookup(ch.label);
			if (c == nullptr)
				c = client_registry()->alloc_insert(ch.label);

			Channel_widget *w = c->lookup_channel(ch.number);
			if (w == nullptr)
				w = c->add_channel(ch.type, ch.number);

			w->volume(ch.volume);
			w->muted(ch.muted);

			if (c->combined_control()) c->only_show_first();
			else w->show();

			c->valid = true;

			_layout->addWidget(c);
			resize(sizeHint());
		} catch (Channel::Invalid_channel) { Genode::warning("invalid channel node"); }
	});

	client_registry()->remove_invalid();
}


/**
 * Gets called from the Genode to Qt proxy object when the report was
 * updated with a pointer to the XML document.
 */
void Main_window::report_changed(void *l, void const *p)
{
	Genode::Blockade &blockade = *reinterpret_cast<Genode::Blockade*>(l);
	Genode::Xml_node &node = *((Genode::Xml_node*)p);

	if (node.has_type("channel_list"))
		_update_clients(node);

	blockade.wakeup();
}


Main_window::Main_window(Genode::Env &env)
:
	_default_out_volume(0),
	_default_volume(0),
	_default_muted(true)
{
	connect(client_registry(), SIGNAL(registry_changed()),
	        this, SLOT(_update_config()));

	using namespace Genode;

	Attached_rom_dataspace const config(env, "config");
	_verbose = config.xml().attribute_value("verbose", false);
	config.xml().with_sub_node("default",
		[&] (Xml_node const &node) {
			_default_out_volume = node.attribute_value("out_volume", 0L);
			_default_volume     = node.attribute_value("volume", 0L);
			_default_muted      = node.attribute_value("muted", 1L);
		},
		[&] { warning("no <default> node found, fallback is 'muted=1'"); }
	);
}


Main_window::~Main_window()
{
	disconnect(client_registry(), SIGNAL(registry_changed()));
}

#include "main_window.moc"
