#ifndef NITPICKERPLUGINWIDGET_H
#define NITPICKERPLUGINWIDGET_H

#include <QtGui>

#include "nitpickerplugin.h"

#include <qpluginwidget/qpluginwidget.h>

#include <qurl.h>

class NitpickerPluginWidget : public QPluginWidget
{
    Q_OBJECT
    Q_PROPERTY(bool swapping READ swapping)

public:
    NitpickerPluginWidget(NitpickerPlugin *plugin, QUrl plugin_url, QString &args,
    		              int max_width, int max_height, QWidget *parent = 0);
    QUrl url;
    QStringList argumentNames;
    QStringList argumentValues;
    bool swapping() const { return m_swapping; }

public slots:
	void deleteLater();

private slots:
    void configure();
    void loadAll();
    void load(bool loadAll = false);

private:
    bool m_swapping;
    NitpickerPlugin *m_plugin;
};

#endif // NITPICKERPLUGINWIDGET_H

