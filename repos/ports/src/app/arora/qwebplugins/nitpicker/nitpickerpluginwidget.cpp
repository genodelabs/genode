#include "nitpickerpluginwidget.h"

#include <qfile.h>
#include <qmenu.h>
#include <qwebframe.h>
#include <qwebview.h>
#include <qwebelement.h>

#include <qdebug.h>

NitpickerPluginWidget::NitpickerPluginWidget(NitpickerPlugin *plugin, QUrl plugin_url, QString &args,
		                                     int max_width, int max_height, QWidget *parent)
    : QPluginWidget(parent, plugin_url, args, max_width, max_height)
    , m_swapping(false)
    , m_plugin(plugin)
{
}

void NitpickerPluginWidget::configure()
{
    m_plugin->configure();
}

void NitpickerPluginWidget::loadAll()
{
    load(true);
}

void NitpickerPluginWidget::load(bool loadAll)
{
    QWidget *parent = parentWidget();
    QWebView *view = 0;
    while (parent) {
        if (QWebView *aView = qobject_cast<QWebView*>(parent)) {
            view = aView;
            break;
        }
        parent = parent->parentWidget();
    }
    if (!view)
        return;

    const QString selector = QLatin1String("%1[type=\"application/x-genode-plugin\"]");
    const QString mime = QLatin1String("application/x-genode-plugin");

    hide();
    m_swapping = true;
    QList<QWebFrame*> frames;
    frames.append(view->page()->mainFrame());
    while (!frames.isEmpty()) {
        QWebFrame *frame = frames.takeFirst();
        QWebElement docElement = frame->documentElement();

        QWebElementCollection elements;
        elements.append(docElement.findAll(selector.arg(QLatin1String("object"))));
        elements.append(docElement.findAll(selector.arg(QLatin1String("embed"))));

        QWebElement element;
        foreach (element, elements) {
            if (!loadAll) {
                if (!element.evaluateJavaScript(QLatin1String("this.swapping")).toBool())
                    continue;
            }

            QWebElement substitute = element.clone();
            substitute.setAttribute(QLatin1String("type"), mime);
            element.replace(substitute);
        }

        frames += frame->childFrames();
    }
    m_swapping = false;
}


void NitpickerPluginWidget::deleteLater()
{
	cleanup();
	QObject::deleteLater();
}
