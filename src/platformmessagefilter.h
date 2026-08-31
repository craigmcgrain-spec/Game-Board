#pragma once

#include <QString>

namespace PlatformMessageFilter {

bool shouldSuppress(const QString &platformName, const QString &message);
void install();

}
