#pragma once
#include <QDebug>
#include <string>
inline QDebug operator<<(QDebug d, const std::string &s) { return d << QString::fromStdString(s); }
