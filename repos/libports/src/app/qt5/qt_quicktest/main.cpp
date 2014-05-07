/*
 * \brief   QtQuick test
 * \author  Christian Prochaska
 * \date    2013-11-26
 */

#include <QGuiApplication>
#include <QQuickView>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQuickView view;
    view.setSource(QUrl("qrc:/qt_quicktest.qml"));
    view.show();

    return app.exec();
}
