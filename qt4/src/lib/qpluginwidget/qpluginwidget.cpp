/*
 * \brief  A Qt Widget that can load a plugin application and show its Nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

#include <base/env.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <util/arg_string.h>

#include <string.h>
#include <zlib.h>

#include <QtGui>
#include <private/qwindowsurface_qws_p.h>

#include <qpluginwidget/qpluginwidget.h>

QPluginWidget *QPluginWidget::_last = 0;

PluginStarter::PluginStarter(QUrl plugin_url, QString &args,
                             int max_width, int max_height)
: _plugin_url(plugin_url),
  _args(args.toAscii()),
  _max_width(max_width),
  _max_height(max_height),
  _pc(0),
  _plugin_loading_state(LOADING),
  _qnam(0),
  _reply(0)
{

}


void PluginStarter::_start_plugin(QString &file_name, QByteArray const &file_buf)
{
	if (file_name.endsWith(".gz")) {
		file_name.remove(".gz");

		uint32_t file_size = *(uint32_t*)(file_buf.constData() + file_buf.size() - sizeof(uint32_t));

		PDBG("file_size_uncompressed = %u", file_size);

		size_t ram_quota = Genode::Arg_string::find_arg(_args.constData(), "ram_quota").long_value(0) + file_size;

		if ((long)Genode::env()->ram_session()->avail() - (long)ram_quota < QPluginWidget::RAM_QUOTA) {
			PERR("quota exceeded");
			_plugin_loading_state = QUOTA_EXCEEDED_ERROR;
			return;
		}

		QByteArray ram_quota_string("ram_quota=" + QByteArray::number((unsigned long long) ram_quota));
		QByteArray ds_size_string("ds_size=" + QByteArray::number((unsigned long long)file_size));
		QByteArray plugin_connection_args(ram_quota_string + "," + ds_size_string);
		_pc = new Loader::Connection(plugin_connection_args.constData());

		Genode::Dataspace_capability ds = _pc->dataspace();
		if (ds.valid()) {
			void *ds_addr = Genode::env()->rm_session()->attach(ds);

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
				PERR("inflateInit2() failed");
				_plugin_loading_state = INFLATE_ERROR;

				inflateEnd(&zs);
				Genode::env()->rm_session()->detach(ds_addr);
				return;
			}

			/* uncompress */
			if (inflate(&zs, Z_SYNC_FLUSH) != Z_STREAM_END) {
				PERR("inflate() failed");
				_plugin_loading_state = INFLATE_ERROR;

				inflateEnd(&zs);
				Genode::env()->rm_session()->detach(ds_addr);
				return;
			}

			inflateEnd(&zs);

			Genode::env()->rm_session()->detach(ds_addr);
		}
	} else {
		size_t ram_quota = Genode::Arg_string::find_arg(_args.constData(), "ram_quota").long_value(0);

		if ((long)Genode::env()->ram_session()->avail() - (long)ram_quota < QPluginWidget::RAM_QUOTA) {
			_plugin_loading_state = QUOTA_EXCEEDED_ERROR;
			return;
		}

		QByteArray ram_quota_string("ram_quota=" + QByteArray::number((unsigned long long) ram_quota));
		QByteArray ds_size_string("ds_size=" + QByteArray::number((unsigned long long)file_buf.size()));
		QByteArray plugin_connection_args(ram_quota_string + "," + ds_size_string);
		_pc = new Loader::Connection(plugin_connection_args.constData());

		Genode::Dataspace_capability ds = _pc->dataspace();
		if (ds.valid()) {
			void *ds_addr = Genode::env()->rm_session()->attach(_pc->dataspace());

			memcpy(ds_addr, file_buf.constData(), file_buf.size());

			Genode::env()->rm_session()->detach(ds_addr);
		}
	}

	try {
		_pc->start(_args.constData(), _max_width, _max_height, 10000, file_name.toLatin1().constData());
		_plugin_loading_state = LOADED;
	} catch (Loader::Session::Rom_access_failed) {
		_plugin_loading_state = ROM_CONNECTION_FAILED_EXCEPTION;
	} catch (Loader::Session::Plugin_start_timed_out) {
		_plugin_loading_state = TIMEOUT_EXCEPTION;
	}
}


void PluginStarter::run()
{
	if (_plugin_url.scheme() == "rom") {

		QString file_name = _plugin_url.path().remove("/");

		try {
			Genode::Rom_connection rc(file_name.toLatin1().constData());

			Genode::Dataspace_capability rom_ds = rc.dataspace();

			char const *rom_ds_addr = (char const *)Genode::env()->rm_session()->attach(rom_ds);

			QByteArray file_buf = QByteArray::fromRawData(rom_ds_addr, Genode::Dataspace_client(rom_ds).size());

			_start_plugin(file_name, file_buf);

			Genode::env()->rm_session()->detach(rom_ds_addr);

		} catch (Genode::Rom_connection::Rom_connection_failed) {
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


Nitpicker::View_capability PluginStarter::plugin_view(int *w, int *h, int *buf_x, int *buf_y)
{
	return _pc->view(w, h, buf_x, buf_y);
}


QPluginWidget::QPluginWidget(QWidget *parent, QUrl plugin_url, QString &args,
                             int max_width, int max_height)
:
	QNitpickerViewWidget(parent),
	_plugin_loading_state(LOADING),
	_plugin_starter(0),
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

	_plugin_starter = new PluginStarter(plugin_url, args, max_width, max_height);
	_plugin_starter->moveToThread(_plugin_starter);
	connect(_plugin_starter, SIGNAL(finished()), this, SLOT(pluginStartFinished()));
	_plugin_starter->start();
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
		/* terminate the Genode thread */
		_plugin_starter->terminate();
		/* delete the QThread object */
		delete _plugin_starter;
		_plugin_starter = 0;
		delete vc;
		vc = 0;
	}
}


void QPluginWidget::paintEvent(QPaintEvent *event)
{
	if (_plugin_loading_state == LOADED)
		QNitpickerViewWidget::paintEvent(event);
	else {
		QWidget::paintEvent(event);
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
			case QUOTA_EXCEEDED_ERROR:
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


void QPluginWidget::pluginStartFinished()
{
	_plugin_loading_state = _plugin_starter->plugin_loading_state();

	if (_plugin_loading_state == LOADED) {
		Nitpicker::View_capability view = _plugin_starter->plugin_view(&orig_w, &orig_h, &orig_buf_x, &orig_buf_y);

		vc = new Nitpicker::View_client(view);

		setFixedSize((_max_width > -1) ? qMin(orig_w, _max_width) : orig_w,
					 (_max_height > -1) ? qMin(orig_h, _max_height) : orig_h);
	} else {
		_plugin_loading_error_string = _plugin_starter->plugin_loading_error_string();
		setFixedSize((_max_width > -1) ? _max_width : 100,
					 (_max_height > -1) ? _max_height : 100);
		cleanup();
	}

	update();
}

