#include <QCoreApplication>
#include <QDir>
#include "roomhub.h"

static const quint16 DEFAULT_PORT = 9000;

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Ensure data directory exists for persistence
    QDir appDir(QCoreApplication::applicationDirPath());
    if (!appDir.exists("data")) {
        if (!appDir.mkpath("data")) {
            qCritical() << "Failed to create data directory";
            return 1;
        }
    }

    quint16 port = DEFAULT_PORT;
    if (app.arguments().size() >= 2) {
        bool ok = false; 
        int p = app.arguments().at(1).toInt(&ok);
        if (ok && p > 0 && p < 65536) {
            port = static_cast<quint16>(p);
        }
    }

    RoomHub hub;
    if (!hub.start(port)) {
        qCritical() << "Failed to start RoomHub on port" << port;
        return 2;
    }

    qInfo() << "CloudMeeting server started on port" << port;
    qInfo() << "Data directory:" << appDir.absoluteFilePath("data");

    return app.exec();
}