/*
 * \brief Mixer frontend
 * \author Josef Soentgen
 * \date 2015-10-15
 */

/* Genode includes */
#include <base/log.h>
#include <base/thread.h>
#include <base/attached_rom_dataspace.h>
#include <libc/component.h>

/* Qt includes */
#include <QApplication>
#include <QDebug>
#include <QFile>

/* application includes */
#include "main_window.h"


enum { SIGNAL_EP_STACK_SIZE = 16*1024 };


struct Report_handler
{
	QMember<Report_proxy> proxy;

	Genode::Attached_rom_dataspace channels_rom;

	Genode::Entrypoint                     sig_ep;
	Genode::Signal_handler<Report_handler> channels_handler;

	Genode::Lock _report_lock { Genode::Lock::LOCKED };

	bool window_connected { false };

	void _report(char const *data, size_t size)
	{
		Genode::Xml_node node(data, size);
		proxy->report_changed(&_report_lock, &node);

		/* wait until the report was handled */
		_report_lock.lock();
	}

	void _handle_channels()
	{
		if (!window_connected)
			return;

		channels_rom.update();

		if (channels_rom.valid())
			_report(channels_rom.local_addr<char>(), channels_rom.size());
	}

	Report_handler(Genode::Env &env)
	:
		channels_rom(env, "channel_list"),
		sig_ep(env, SIGNAL_EP_STACK_SIZE, "signal ep",
		       Genode::Affinity::Location()),
		channels_handler(sig_ep, *this, &Report_handler::_handle_channels)
	{
		channels_rom.sigh(channels_handler);
	}

	void connect_window(Main_window *win)
	{
		QObject::connect(proxy, SIGNAL(report_changed(void *,void const*)),
		                 win,   SLOT(report_changed(void *, void const*)),
		                 Qt::QueuedConnection);

		window_connected = true;
	}
};


static inline void load_stylesheet()
{
	QFile file(":style.qss");
	if (!file.open(QFile::ReadOnly)) {
		qWarning() << "Warning:" << file.errorString()
			<< "opening file" << file.fileName();
		return;
	}

	qApp->setStyleSheet(QLatin1String(file.readAll()));
}


extern void initialize_qt_gui(Genode::Env &);

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_gui(env);

		int argc = 1;
		char const *argv[] = { "mixer_gui_qt", 0 };

		Report_handler *report_handler;
		try { report_handler = new Report_handler(env); }
		catch (...) {
		Genode::error("Could not create Report_handler");
		return -1;
		}

		QApplication app(argc, (char**)argv);

		load_stylesheet();

		QMember<Main_window> main_window(env);
		main_window->show();

		report_handler->connect_window(main_window);

		app.connect(&app, SIGNAL(lastWindowClosed()), SLOT(quit()));

		exit(app.exec());
	});
}
