#include "platformmessagefilter.h"

#include <QGuiApplication>
#include <QtLogging>

namespace {
QtMessageHandler previousHandler = nullptr;
bool installed = false;

void messageHandler(
    QtMsgType type,
    const QMessageLogContext &context,
    const QString &message)
{
    const QString platformName =
        QGuiApplication::instance() ? QGuiApplication::platformName() : QString();
    if (type == QtWarningMsg
        && PlatformMessageFilter::shouldSuppress(platformName, message)) {
        return;
    }

    if (previousHandler) {
        previousHandler(type, context, message);
    } else {
        qt_message_output(type, context, message);
    }
}
}

bool PlatformMessageFilter::shouldSuppress(
    const QString &platformName,
    const QString &message)
{
    return platformName.startsWith(QStringLiteral("wayland"))
        && message == QStringLiteral(
            "This plugin supports grabbing the mouse only for popup windows");
}

void PlatformMessageFilter::install()
{
    if (installed) {
        return;
    }
    previousHandler = qInstallMessageHandler(messageHandler);
    installed = true;
}
