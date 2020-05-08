/*
 * \brief  A Qt Widget that can load a plugin application and show its Nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/* Genode includes */
#include <base/env.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <util/arg_string.h>

/* libc includes */
#include <assert.h>
#include <string.h>
#include <zlib.h>

/* Qt includes */
#include <QtGui>

#include <qnitpickerplatformwindow.h>

#include <qpluginwidget/qpluginwidget.h>

Libc::Env     *QPluginWidget::_env = 0;
QPluginWidget *QPluginWidget::_last = 0;

using namespace Genode;


const char *config = " \
<config> \
    <parent-provides> \
        <service name=\"CPU\"/> \
        <service name=\"LOG\"/> \
        <service name=\"PD\"/> \
        <service name=\"RAM\"/> \
        <service name=\"RM\"/> \
        <service name=\"ROM\"/> \
        <service name=\"Timer\"/> \
        <service name=\"Nitpicker\"/> \
    </parent-provides> \
    <default-route> \
        <any-service> <parent/> <any-child/> </any-service> \
    </default-route> \
    <start name=\"tar_rom\" caps=\"100\"> \
        <resource name=\"RAM\" quantum=\"1M\"/> \
        <provides> <service name=\"ROM\"/> </provides> \
        <config> \
            <archive name=\"plugin.tar\"/> \
        </config> \
    </start> \
    <start name=\"init\" caps=\"2000\"> \
        <resource name=\"RAM\" quantum=\"2G\"/> \
        <route> \
            <service name=\"ROM\" label=\"config\"> \
                <child name=\"tar_rom\" label=\"config.plugin\"/> \
            </service> \
            <any-service> <parent /> </any-service> \
        </route> \
    </start> \
</config> \
";


class Signal_wait_thread : public QThread
{
	private:

		Signal_receiver &_signal_receiver;
		QMutex &_timeout_mutex;

	protected:

		void run()
		{
			_signal_receiver.wait_for_signal();
			_timeout_mutex.unlock();
		}

	public:

		Signal_wait_thread(Signal_receiver &signal_receiver, QMutex &timeout_mutex)
		: _signal_receiver(signal_receiver),
		  _timeout_mutex(timeout_mutex) { }
};


PluginStarter::PluginStarter(Libc::Env *env,
                             QUrl plugin_url, QString &args,
                             int max_width, int max_height,
                             Nitpicker::View_capability parent_view)
:
	_env(env),
	_plugin_url(plugin_url),
	_args(args.toLatin1()),
	_max_width(max_width),
	_max_height(max_height),
	_parent_view(parent_view),
	_pc(0),
	_plugin_loading_state(LOADING),
	_qnam(0),
	_reply(0)
{ }


void PluginStarter::_start_plugin(QString &file_name, QByteArray const &file_buf)
{
	Genode::size_t caps = Arg_string::find_arg(_args.constData(), "caps").ulong_value(0);

	if ((long)_env->pd().avail_caps().value - (long)caps < QPluginWidget::PRESERVED_CAPS) {
		Genode::error("Cannot donate ", caps, " capabilities to the plugin (quota exceeded).");
		_plugin_loading_state = CAP_QUOTA_EXCEEDED_ERROR;
		return;
	}

	if (file_name.endsWith(".gz")) {
		file_name.remove(".gz");

		uint32_t file_size = *(uint32_t*)(file_buf.constData() +
		                     file_buf.size() - sizeof(uint32_t));

		Genode::log(__func__, ": file_size_uncompressed=", file_size);

		Genode::size_t ram_quota = Arg_string::find_arg(_args.constData(),
		                                                "ram_quota").ulong_value(0) +
		                                                file_size;

		if (((long)_env->pd().avail_ram().value - (long)ram_quota) <
		    QPluginWidget::PRESERVED_RAM_QUOTA) {
			Genode::error("Cannot donate ", ram_quota, " bytes of RAM to the plugin (quota exceeded).");
			_plugin_loading_state = RAM_QUOTA_EXCEEDED_ERROR;
			return;
		}

		_pc = new Loader::Connection(*_env,
		                             Genode::Ram_quota{ram_quota},
		                             Genode::Cap_quota{caps});

		Dataspace_capability ds = _pc->alloc_rom_module(file_name.toUtf8().constData(), file_size);
		if (ds.valid()) {
			void *ds_addr = _env->rm().attach(ds);

			z_stream zs;
			zs.next_in = (Bytef*)(file_buf.data());
			zs.avail_in = file_buf.size();
			zs.total_in = 0;
			zs.next_out = (Bytef*)ds_addr;
			zs.avail_out = file_size;
			zs.total_out = 0;
			zs.zalloc = Z_NULL;
			zs.zfree = Z_NULL;

			/* enable gzip format detection */
			if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {
				Genode::error("inflateInit2() failed");
				_plugin_loading_state = INFLATE_ERROR;

				inflateEnd(&zs);
				_env->rm().detach(ds_addr);
				return;
			}

			/* uncompress */
			if (inflate(&zs, Z_SYNC_FLUSH) != Z_STREAM_END) {
				Genode::error("inflate() failed");
				_plugin_loading_state = INFLATE_ERROR;

				inflateEnd(&zs);
				_env->rm().detach(ds_addr);
				return;
			}

			inflateEnd(&zs);

			_env->rm().detach(ds_addr);
			_pc->commit_rom_module(file_name.toUtf8().constData());
		}
	} else {
		Genode::size_t ram_quota = Arg_string::find_arg(_args.constData(), "ram_quota").ulong_value(0);

		if (((long)_env->pd().avail_ram().value - (long)ram_quota) <
		    QPluginWidget::PRESERVED_RAM_QUOTA) {
			_plugin_loading_state = RAM_QUOTA_EXCEEDED_ERROR;
			return;
		}

		_pc = new Loader::Connection(*_env,
		                             Genode::Ram_quota{ram_quota},
		                             Genode::Cap_quota{caps});

		Dataspace_capability plugin_ds = _pc->alloc_rom_module("plugin.tar", file_buf.size());
		if (plugin_ds.valid()) {
			void *plugin_ds_addr = _env->rm().attach(plugin_ds);
			::memcpy(plugin_ds_addr, file_buf.constData(), file_buf.size());
			_env->rm().detach(plugin_ds_addr);
			_pc->commit_rom_module("plugin.tar");
		}
	}

	Dataspace_capability config_ds = _pc->alloc_rom_module("config", ::strlen(config) + 1);
	if (config_ds.valid()) {
		void *config_ds_addr = _env->rm().attach(config_ds);
		::memcpy(config_ds_addr, config, ::strlen(config) + 1);
		_env->rm().detach(config_ds_addr);
		_pc->commit_rom_module("config");
	}

	Signal_context sig_ctx;
	Signal_receiver sig_rec;

	_pc->view_ready_sigh(sig_rec.manage(&sig_ctx));
	_pc->constrain_geometry(Loader::Area(_max_width, _max_height));
	_pc->parent_view(_parent_view);
	_pc->start("init", "init");

	QMutex view_ready_mutex;
	Signal_wait_thread signal_wait_thread(sig_rec, view_ready_mutex);
	signal_wait_thread.start();
	if (view_ready_mutex.tryLock(10000)) {
		_plugin_loading_state = LOADED;
	} else {
		_plugin_loading_state = TIMEOUT_EXCEPTION;
		signal_wait_thread.terminate();
	}
	signal_wait_thread.wait();
}


void PluginStarter::run()
{
	if (_plugin_url.scheme() == "rom") {

		QString file_name = _plugin_url.path().remove("/");

		try {
			Rom_connection rc(*_env, file_name.toLatin1().constData());

			Dataspace_capability rom_ds = rc.dataspace();

			char const *rom_ds_addr = (char const *)_env->rm().attach(rom_ds);

			QByteArray file_buf = QByteArray::fromRawData(rom_ds_addr, Dataspace_client(rom_ds).size());

			_start_plugin(file_name, file_buf);

			_env->rm().detach(rom_ds_addr);

		} catch (Rom_connection::Rom_connection_failed) {
			_plugin_loading_state = ROM_CONNECTION_FAILED_EXCEPTION;
		}

		emit finished();

	} else if (_plugin_url.scheme() == "http") {

		_qnam = new QNetworkAccessManager();
		_reply = _qnam->get(QNetworkRequest(_plugin_url));

		connect(_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
	}

	exec();

	delete _pc;

	moveToThread(QApplication::instance()->thread());
}


void PluginStarter::networkReplyFinished()
{
	if (_reply->error() != QNetworkReply::NoError) {
		_plugin_loading_state = NETWORK_ERROR;
		_plugin_loading_error_string = _reply->errorString();
		_reply->deleteLater();
		emit finished();
		return;
	}

	qDebug() << "download finished, size = " << _reply->size();

	QString file_name = _plugin_url.path().remove("/");;
	QByteArray file_buf = _reply->readAll();

	_start_plugin(file_name, file_buf);

	_reply->deleteLater();
	_qnam->deleteLater();

	emit finished();
}


Loader::Area PluginStarter::view_size()
{
	return _pc ? _pc->view_size() : Loader::Area();
}


void PluginStarter::view_geometry(Loader::Rect rect, Loader::Point offset)
{
	if (_pc)
		_pc->view_geometry(rect, offset);
}


/*******************
 ** QPluginWidget **
 *******************/

QPluginWidget::QPluginWidget(QWidget *parent, QUrl plugin_url, QString &args,
                             int max_width, int max_height)
:
	QEmbeddedViewWidget(parent),
	_plugin_loading_state(LOADING),
	_plugin_starter(0),
	_plugin_starter_started(false),
	_plugin_url(plugin_url),
	_plugin_args(args),
	_max_width(max_width),
	_max_height(max_height)
{
	qDebug() << "plugin_url = " << plugin_url;
	qDebug() << "plugin_url.scheme() = " << plugin_url.scheme();
	qDebug() << "plugin_url.path() = " << plugin_url.path();
	qDebug() << "plugin_url.toLocalFile() = " << plugin_url.toLocalFile();
	qDebug() << "args =" << args;

	if (_last) {
		_last->cleanup();
		_last = this;
	}
}


QPluginWidget::~QPluginWidget()
{
	cleanup();

	if (_last == this)
		_last = 0;
}


void QPluginWidget::cleanup()
{
	if (_plugin_starter) {
		/* make the thread leave the event loop */
		_plugin_starter->exit();
		/* wait until the thread has left the run() function */
		_plugin_starter->wait();
		/* delete the QThread object */
		delete _plugin_starter;
		_plugin_starter = 0;
	}
}


void QPluginWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	if (_plugin_loading_state == LOADED) {

		QEmbeddedViewWidget::View_geometry const view_geometry =
			QEmbeddedViewWidget::_calc_view_geometry();

		if (mask().isEmpty()) {

			Loader::Rect const geometry(Loader::Point(view_geometry.x,
			                                          view_geometry.y),
			                            Loader::Area(view_geometry.w,
			                                         view_geometry.h));

			Loader::Point const offset(view_geometry.buf_x,
			                           view_geometry.buf_y);

			_plugin_starter->view_geometry(geometry, offset);

		} else {

			Loader::Rect const
				geometry(Loader::Point(mapToGlobal(mask().boundingRect().topLeft()).x(),
				                       mapToGlobal(mask().boundingRect().topLeft()).y()),
				         Loader::Area(mask().boundingRect().width(),
				                      mask().boundingRect().height()));

			Loader::Point const offset(view_geometry.buf_x, view_geometry.buf_y);

			_plugin_starter->view_geometry(geometry, offset);
		}

	} else {

		QPainter painter(this);
		painter.drawRect(0, 0, width() - 1, height() - 1);
		switch (_plugin_loading_state) {
			case LOADING:
				painter.drawText(rect(), Qt::AlignCenter, tr("Loading plugin..."));
				break;
			case NETWORK_ERROR:
				painter.drawText(rect(), Qt::AlignCenter, tr("Could not load plugin: ") +
				                                             _plugin_loading_error_string);
				break;
			case INFLATE_ERROR:
				painter.drawText(rect(), Qt::AlignCenter,
						         tr("Could not load plugin: error decompressing gzipped file."));
				break;
			case CAP_QUOTA_EXCEEDED_ERROR:
				painter.drawText(rect(), Qt::AlignCenter,
						         tr("Could not load plugin: not enough capabilities."));
				break;
			case RAM_QUOTA_EXCEEDED_ERROR:
				painter.drawText(rect(), Qt::AlignCenter,
						         tr("Could not load plugin: not enough memory."));
				break;
			case TIMEOUT_EXCEPTION:
				painter.drawText(rect(), Qt::AlignCenter, tr("Could not load plugin: timeout."));
				break;
			case ROM_CONNECTION_FAILED_EXCEPTION:
				painter.drawText(rect(), Qt::AlignCenter, tr("Could not load plugin: file not found."));
				break;
			default:
				break;
		}
	}
}


void QPluginWidget::showEvent(QShowEvent *event)
{
	/* only now do we know the parent widget for sure */

	if (!_plugin_starter_started) {

		QNitpickerPlatformWindow *platform_window =
			dynamic_cast<QNitpickerPlatformWindow*>(window()->windowHandle()->handle());

		assert(_env != nullptr);

		_plugin_starter = new PluginStarter(_env,
		                                    _plugin_url, _plugin_args,
		                                    _max_width, _max_height,
		                                    platform_window->view_cap());
		_plugin_starter->moveToThread(_plugin_starter);
		connect(_plugin_starter, SIGNAL(finished()), this, SLOT(pluginStartFinished()));
		_plugin_starter->start();

		_plugin_starter_started = true;
	}

	QEmbeddedViewWidget::showEvent(event);
}


void QPluginWidget::pluginStartFinished()
{
	_plugin_loading_state = _plugin_starter->plugin_loading_state();

	if (_plugin_loading_state == LOADED) {

		Loader::Area const size = _plugin_starter->view_size();

		QEmbeddedViewWidget::_orig_geometry(size.w(), size.h(), 0, 0);

		int w = (_max_width  > -1) ? qMin(size.w(), (unsigned)_max_width)  : size.w();
		int h = (_max_height > -1) ? qMin(size.h(), (unsigned)_max_height) : size.h();

		setFixedSize(w, h);

	} else {
		_plugin_loading_error_string = _plugin_starter->plugin_loading_error_string();
		setFixedSize((_max_width  > -1) ? _max_width  : 100,
		             (_max_height > -1) ? _max_height : 100);
		cleanup();
	}

	update();
}


void QPluginWidget::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);

	if (_plugin_loading_state == LOADED) {

		QEmbeddedViewWidget::View_geometry const view_geometry =
			QEmbeddedViewWidget::_calc_view_geometry();

		Loader::Rect geometry(Loader::Point(mapToGlobal(pos()).x(),
		                                    mapToGlobal(pos()).y()),
		                      Loader::Area(0, 0));

		Loader::Point offset(view_geometry.buf_x, view_geometry.buf_y);

		_plugin_starter->view_geometry(geometry, offset);
	}
}

