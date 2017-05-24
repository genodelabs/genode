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


enum { THREAD_STACK_SIZE = 2 * 1024 * sizeof(long) };


struct Report_thread : Genode::Thread
{
	QMember<Report_proxy> proxy;

	Genode::Attached_rom_dataspace channels_rom;

	Genode::Signal_receiver                  sig_rec;
	Genode::Signal_dispatcher<Report_thread> channels_dispatcher;

	Genode::Lock _report_lock { Genode::Lock::LOCKED };

	void _report(char const *data, size_t size)
	{
		Genode::Xml_node node(data, size);
		proxy->report_changed(&_report_lock, &node);

		/* wait until the report was handled */
		_report_lock.lock();
	}

	void _handle_channels(unsigned)
	{
		channels_rom.update();

		if (channels_rom.valid())
			_report(channels_rom.local_addr<char>(), channels_rom.size());
	}

	Report_thread(Genode::Env &env)
	:
		Genode::Thread(env, "report_thread", THREAD_STACK_SIZE),
		channels_rom(env, "channel_list"),
		channels_dispatcher(sig_rec, *this, &Report_thread::_handle_channels)
	{
		channels_rom.sigh(channels_dispatcher);
	}

	void entry() override
	{
		using namespace Genode;
		while (true) {
			Signal sig = sig_rec.wait_for_signal();
			int num    = sig.num();

			Signal_dispatcher_base *dispatcher;
			dispatcher = dynamic_cast<Signal_dispatcher_base *>(sig.context());
			dispatcher->dispatch(num);
		}
	}

	void connect_window(Main_window *win)
	{
		QObject::connect(proxy, SIGNAL(report_changed(void *,void const*)),
		                 win,   SLOT(report_changed(void *, void const*)),
		                 Qt::QueuedConnection);
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


extern void initialize_qt_core(Genode::Env &);
extern void initialize_qt_gui(Genode::Env &);

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_core(env);
		initialize_qt_gui(env);

		int argc = 1;
		char const *argv[] = { "mixer_gui_qt", 0 };

		Report_thread *report_thread;
		try { report_thread = new Report_thread(env); }
		catch (...) {
		Genode::error("Could not create Report_thread");
		return -1;
		}

		QApplication app(argc, (char**)argv);

		load_stylesheet();

		QMember<Main_window> main_window(env);
		main_window->show();

		report_thread->connect_window(main_window);
		report_thread->start();

		app.connect(&app, SIGNAL(lastWindowClosed()), SLOT(quit()));

		exit(app.exec());
	});
}
