#include "nitpickerplugin.h"

#include "nitpickerpluginwidget.h"

#include <qdebug.h>

NitpickerPlugin::NitpickerPlugin()
    : m_loaded(false)
    , m_enabled(false)
{
}

void NitpickerPlugin::load()
{
    if (m_loaded)
        return;
    m_loaded = true;
    QSettings settings;
    settings.beginGroup(QLatin1String("webplugin/nitpicker"));
    settings.endGroup();
    settings.beginGroup(QLatin1String("websettings"));
//    m_enabled = settings.value(QLatin1String("enableNitpicker"), false).toBool();
    m_enabled = true;
}

void NitpickerPlugin::save()
{
}

QWidget *NitpickerPlugin::create(const QString &mimeType, const QUrl &url,
                                 const QStringList &argumentNames, const QStringList &argumentValues)
{
    load();
    if (!m_enabled)
        return 0;
    Q_UNUSED(mimeType);

    QString args_string = argumentNames.indexOf("args") > 0 ?
    		              argumentValues[argumentNames.indexOf("args")] :
    		              QString();

    QString max_width_string = argumentNames.indexOf("width") > 0 ?
	                           argumentValues[argumentNames.indexOf("width")] :
	                           QString("-1");
    int max_width = max_width_string.remove("px").toInt();

    QString max_height_string = argumentNames.indexOf("height") > 0 ?
	                            argumentValues[argumentNames.indexOf("height")] :
	                            QString("-1");
    int max_height = max_height_string.remove("px").toInt();

    NitpickerPluginWidget *m_widget = new NitpickerPluginWidget(this, url, args_string, max_width, max_height);
    m_widget->url = url;
    m_widget->argumentNames = argumentNames;
    m_widget->argumentValues = argumentValues;
    return m_widget;
}

QWebPluginFactory::Plugin NitpickerPlugin::metaPlugin()
{
    QWebPluginFactory::Plugin plugin;
    plugin.name = QLatin1String("NitpickerPlugin");
    QWebPluginFactory::MimeType mimeType;
    mimeType.name = QLatin1String("application/x-genode-plugin");
    plugin.mimeTypes.append(mimeType);
    return plugin;
}

void NitpickerPlugin::configure()
{
}

bool NitpickerPlugin::isAnonymous() const
{
    return true;
}
