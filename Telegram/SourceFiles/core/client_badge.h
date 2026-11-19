/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/not_null.h"

#include <QtCore/QString>

#include <optional>

class PeerData;
class QWidget;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace ClientBadge {

enum class Role : uchar {
	None = 0,
	Supporter = 1,
	BugHunter = 2,
	Developer = 3,
	Founder = 4,
};

[[nodiscard]] QString RoleName(Role role);
[[nodiscard]] std::optional<Role> RoleFromCode(uchar code);

// The mark is an invisible role code appended to the last name, so official
// clients keep showing an ordinary name while YandereGram recognises its own
// users from data the server hands to everyone.
[[nodiscard]] bool HasMark(const QString &text);
[[nodiscard]] Role MarkedRole(const QString &text);
[[nodiscard]] QString Marked(const QString &lastName, Role role);
[[nodiscard]] QString Unmarked(const QString &text);
[[nodiscard]] Role RoleOfPeer(not_null<PeerData*> peer);
[[nodiscard]] bool IsMarkedPeer(not_null<PeerData*> peer);

// Keeps the own account marked for the whole session lifetime, re-applying
// the mark after the user renames themselves.
void Start(not_null<Main::Session*> session);
void ChooseOwnRole(not_null<Main::Session*> session, Role role);

// A tg://badge/... link carries a grant signed by the issuer key. Only the
// issuer can mint one, so it is the only way to hand a role to someone else.
bool ApplyLink(not_null<Main::Session*> session, const QString &command);

// Copies a grant link for `userId` with `role`, minting it from the issuer
// private key when that key is present next to the client. Without the key
// nothing is copied: a role can't be granted without a valid signature.
void CopyGrantLink(
	not_null<Window::SessionController*> controller,
	quint64 userId,
	const QString &name,
	Role role);

void ShowToast(QWidget *parent);
void ShowRoleToast(QWidget *parent, Role role);

} // namespace ClientBadge
