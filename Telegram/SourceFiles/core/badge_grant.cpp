/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/badge_grant.h"

#include "settings.h"

#include <QtCore/QByteArray>
#include <QtCore/QFile>

extern "C" {
#include <openssl/evp.h>
} // extern "C"

#include <memory>

namespace Core::Badges {
namespace {

constexpr auto kRoleSize = 1;
constexpr auto kUserIdSize = 8;
constexpr auto kSignatureSize = 64;
constexpr auto kPayloadSize = kRoleSize + kUserIdSize + kSignatureSize;
constexpr auto kPublicKeySize = 32;

struct EvpPkeyDeleter {
	void operator()(EVP_PKEY *value) {
		EVP_PKEY_free(value);
	}
};
using EvpPkey = std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>;

struct EvpMdCtxDeleter {
	void operator()(EVP_MD_CTX *value) {
		EVP_MD_CTX_free(value);
	}
};
using EvpMdCtx = std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter>;

[[nodiscard]] QByteArray SigningMessage(uchar role, quint64 userId) {
	return "yg-badge:v1:" + QByteArray::number(qulonglong(userId))
		+ ':' + QByteArray::number(int(role));
}

// Reads the raw Ed25519 seed from badges/issuer.key next to the client data,
// the same file the issuer tool writes. Absent or malformed means "no rights
// to grant", never a fallback key.
[[nodiscard]] QByteArray ReadIssuerSeed() {
	const auto path = cWorkingDir() + u"badges/issuer.key"_q;
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly)) {
		return QByteArray();
	}
	const auto text = QString::fromLatin1(
		file.readAll()).trimmed().toLatin1();
	return QByteArray::fromHex(text);
}

[[nodiscard]] EvpPkey ReadPublicKey(const QByteArray &hex) {
	if (hex.size() != kPublicKeySize * 2) {
		return nullptr;
	}
	const auto raw = QByteArray::fromHex(hex);
	if (raw.size() != kPublicKeySize) {
		return nullptr;
	}
	return EvpPkey(EVP_PKEY_new_raw_public_key(
		EVP_PKEY_ED25519,
		nullptr,
		reinterpret_cast<const uchar*>(raw.constData()),
		kPublicKeySize));
}

} // namespace

std::optional<Grant> ParseGrant(const QByteArray &payload) {
	const auto decoded = QByteArray::fromBase64Encoding(
		payload,
		QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
	if (!decoded || decoded.decoded.size() != kPayloadSize) {
		return std::nullopt;
	}
	const auto &bytes = decoded.decoded;
	auto result = Grant();
	result.role = uchar(bytes[0]);
	result.userId = 0;
	for (auto i = 0; i != kUserIdSize; ++i) {
		result.userId = (result.userId << 8)
			| quint64(uchar(bytes[kRoleSize + i]));
	}
	result.signature = bytes.mid(kRoleSize + kUserIdSize, kSignatureSize);
	if (!result.userId || result.role == 0) {
		return std::nullopt;
	}
	return result;
}

bool VerifyGrant(const Grant &grant, const QByteArray &publicKeyHex) {
	if (grant.signature.size() != kSignatureSize) {
		return false;
	}
	const auto key = ReadPublicKey(publicKeyHex);
	if (!key) {
		return false;
	}
	const auto context = EvpMdCtx(EVP_MD_CTX_new());
	if (!context
		|| EVP_DigestVerifyInit(
			context.get(),
			nullptr,
			nullptr,
			nullptr,
			key.get()) != 1) {
		return false;
	}
	const auto message = SigningMessage(grant.role, grant.userId);
	return EVP_DigestVerify(
		context.get(),
		reinterpret_cast<const uchar*>(grant.signature.constData()),
		kSignatureSize,
		reinterpret_cast<const uchar*>(message.constData()),
		size_t(message.size())) == 1;
}

QString MintGrant(quint64 userId, uchar role) {
	if (!userId || !role) {
		return QString();
	}
	const auto seed = ReadIssuerSeed();
	if (seed.size() != kPublicKeySize) {
		return QString();
	}
	const auto key = EvpPkey(EVP_PKEY_new_raw_private_key(
		EVP_PKEY_ED25519,
		nullptr,
		reinterpret_cast<const uchar*>(seed.constData()),
		size_t(seed.size())));
	if (!key) {
		return QString();
	}
	const auto context = EvpMdCtx(EVP_MD_CTX_new());
	if (!context
		|| EVP_DigestSignInit(
			context.get(),
			nullptr,
			nullptr,
			nullptr,
			key.get()) != 1) {
		return QString();
	}
	const auto message = SigningMessage(role, userId);
	auto signature = QByteArray(kSignatureSize, '\0');
	auto size = size_t(kSignatureSize);
	if (EVP_DigestSign(
			context.get(),
			reinterpret_cast<uchar*>(signature.data()),
			&size,
			reinterpret_cast<const uchar*>(message.constData()),
			size_t(message.size())) != 1
		|| size != kSignatureSize) {
		return QString();
	}
	auto payload = QByteArray();
	payload.reserve(kPayloadSize);
	payload.append(char(role));
	for (auto shift = 56; shift >= 0; shift -= 8) {
		payload.append(char((userId >> shift) & 0xFF));
	}
	payload.append(signature);
	return u"tg://badge/"_q + QString::fromLatin1(
		payload.toBase64(QByteArray::Base64UrlEncoding
			| QByteArray::OmitTrailingEquals));
}

} // namespace Core::Badges
