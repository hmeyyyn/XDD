/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

// Verifies a badge grant. Kept QtCore + OpenSSL only, like update_verify, so
// it can be reused by the client and by a focused test.

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>

namespace Core::Badges {

// Payload of a yg://badge/... link: role code, user id, Ed25519 signature.
struct Grant {
	uchar role = 0;
	quint64 userId = 0;
	QByteArray signature;
};

// Parses the base64url payload, returns nullopt on any structural problem.
[[nodiscard]] std::optional<Grant> ParseGrant(const QByteArray &payload);

// True when the signature over (role, userId) verifies with the issuer key.
[[nodiscard]] bool VerifyGrant(
	const Grant &grant,
	const QByteArray &publicKeyHex);

// Mints a tg://badge/... link for `userId` and `role` when the issuer private
// key is available next to the client. Returns an empty string otherwise,
// because a grant without a valid signature is worthless.
[[nodiscard]] QString MintGrant(quint64 userId, uchar role);

} // namespace Core::Badges
