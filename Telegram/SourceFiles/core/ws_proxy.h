#pragma once

#include "rpl/producer.h"
#include <QtCore/QString>

namespace Core::WsProxy {

void Start();
void SetEnabled(bool enabled);
[[nodiscard]] bool Enabled();
[[nodiscard]] int Port();
[[nodiscard]] int ActiveSessions();
[[nodiscard]] QString StatusText();
[[nodiscard]] rpl::producer<> Changes();

} // namespace Core::WsProxy
