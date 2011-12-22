#ifndef NITPICKERPLUGIN_H
#define NITPICKERPLUGIN_H

#include "arorawebplugin.h"

class NitpickerPlugin : public AroraWebPlugin
{

public:
    NitpickerPlugin();

    QWebPluginFactory::Plugin metaPlugin();
    QWidget *create(const QString &mimeType, const QUrl &url,
                    const QStringList &argumentNames, const QStringList &argumentValues);
    void configure();
    bool isAnonymous() const;

private:
    void load();
    void save();

    bool m_loaded;
    bool m_enabled;
};

#endif // NITPICKERPLUGIN_H

