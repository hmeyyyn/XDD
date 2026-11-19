/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

// The issuer public key compiled into the client. Generate a key pair with
//   node tools/badges/issue.js keygen
// and paste the printed line here. Grants signed by any other key are
// rejected, so nobody can hand out roles without the private key.

#include "base/const_string.h"

namespace Core::Badges {

inline constexpr auto kIssuerPublicKey = "6853a71a40d6cd360787e042c3528ada0557d53d3d02609a4ee71e8285341dbb"_cs;

} // namespace Core::Badges
