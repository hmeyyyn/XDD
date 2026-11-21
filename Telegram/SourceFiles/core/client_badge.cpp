/*
This file is part of YandereGram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/client_badge.h"

#include "apiwrap.h"
#include "core/app_name.h"
#include "core/badge_grant.h"
#include "core/badge_keys.h"
#include "data/data_changes.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "window/window_session_controller.h"
#include "styles/style_chat_helpers.h"

namespace ClientBadge {
namespace {

// U+2063 INVISIBLE SEPARATOR followed by a variation selector, which carries
// the role. Both live in the BMP, have no glyph and take no space, so every
// other client renders the name exactly as it was typed. Variation selectors
// give 16 codes, enough for the roles and any future one.
const auto kMarkPrefix = u"\u2063"_q;
constexpr auto kRoleFirst = ushort(0xFE00);
constexpr auto kMarkLength = 2;

[[nodiscard]] QChar RoleChar(Role role) {
	return QChar(kRoleFirst + ushort(role));
}

[[nodiscard]] QString MarkFor(Role role) {
	return kMarkPrefix + RoleChar(role);
}

[[nodiscard]] Role RoleAt(const QString &text, int index) {
	const auto at = index + 1;
	if (index < 0 || at >= text.size()) {
		return Role::None;
	}
	const auto value = text[at].unicode();
	if (value < kRoleFirst || value >= kRoleFirst + 16) {
		return Role::None;
	}
	return RoleFromCode(uchar(value - kRoleFirst)).value_or(Role::None);
}

} // namespace

QString RoleName(Role role) {
	switch (role) {
	case Role::User: return tr::lng_yg_role_user(tr::now);
	case Role::Supporter: return tr::lng_client_badge_supporter(tr::now);
	case Role::BugHunter: return tr::lng_client_badge_bug_hunter(tr::now);
	case Role::Developer: return tr::lng_client_badge_developer(tr::now);
	case Role::Founder: return tr::lng_client_badge_founder(tr::now);
	case Role::None: break;
	}
	return QString();
}

std::optional<Role> RoleFromCode(uchar code) {
	if (code == 0 || code > uchar(Role::User)) {
		return std::nullopt;
	}
	return Role(code);
}

bool HasMark(const QString &text) {
	return MarkedRole(text) != Role::None;
}

Role MarkedRole(const QString &text) {
	const auto index = text.indexOf(kMarkPrefix);
	return (index < 0) ? Role::None : RoleAt(text, index);
}

QString Marked(const QString &lastName, Role role) {
	return (role == Role::None)
		? lastName
		: (Unmarked(lastName) + MarkFor(role));
}

QString Unmarked(const QString &text) {
	auto result = text;
	while (true) {
		const auto index = result.indexOf(kMarkPrefix);
		if (index < 0) {
			break;
		}
		result.remove(index, kMarkLength);
	}
	return result;
}

Role RoleOfPeer(not_null<PeerData*> peer) {
	const auto user = peer->asUser();
	if (user && user->isSelf()) {
		if (const auto saved = user->session().settings().ownClientBadge(); saved && *saved != Role::None) {
			return *saved;
		}
		const auto mark = MarkedRole(user->lastName);
		return (mark == Role::None) ? Role::User : mark;
	}
	return user ? MarkedRole(user->lastName) : Role::None;
}

bool IsMarkedPeer(not_null<PeerData*> peer) {
	return RoleOfPeer(peer) != Role::None;
}

void CopyGrantLink(
		not_null<Window::SessionController*> controller,
		quint64 userId,
		const QString &name,
		Role role) {
	const auto link = Core::Badges::MintGrant(userId, uchar(role));
	if (link.isEmpty()) {
		controller->showToast(tr::lng_client_badge_no_key(tr::now));
		return;
	}
	TextUtilities::SetClipboardText({ link });
	controller->showToast({
		.text = tr::lng_client_badge_grant_copied(
			tr::now,
			lt_user,
			tr::bold(name),
			tr::marked),
		.iconLottie = u"toast/contact_check"_q,
		.iconLottieSize = st::toastLottieIconSize,
	});
}

void ShowToast(QWidget *parent) {
	if (!parent) {
		return;
	}
	Ui::Toast::Show(parent, {
		.text = {
			tr::lng_client_badge_toast(
				tr::now,
				lt_app,
				tr::bold(AppName.utf16()),
				tr::marked)
		},
		.attach = RectPart::Top,
		.duration = 4 * crl::time(1000),
	});
}

void ShowRoleToast(QWidget *parent, Role role) {
	if (!parent || role == Role::None) {
		return;
	}
	Ui::Toast::Show(parent, {
		.text = { tr::lng_client_badge_role_toast(
			tr::now,
			lt_role,
			tr::bold(RoleName(role)),
			lt_app,
			tr::bold(AppName.utf16()),
			tr::marked) },
		.attach = RectPart::Top,
		.duration = 4 * crl::time(1000),
	});
}

bool ApplyLink(not_null<Main::Session*> session, const QString &command) {
	const auto prefix = u"badge/"_q;
	if (!command.startsWith(prefix)) {
		return false;
	}
	const auto grant = Core::Badges::ParseGrant(
		command.mid(prefix.size()).toLatin1());
	if (!grant
		|| !Core::Badges::VerifyGrant(
			*grant,
			Core::Badges::kIssuerPublicKey.utf8())) {
		return false;
	}
	if (grant->userId != session->userId().bare) {
		return false;
	}
	const auto role = RoleFromCode(grant->role);
	if (!role) {
		return false;
	}
	ChooseOwnRole(session, *role);
	return true;
}

void ChooseOwnRole(not_null<Main::Session*> session, Role role) {
	const auto user = session->user();
	const auto previous = RoleOfPeer(user);
	session->settings().setOwnClientBadge(role);
	session->saveSettingsDelayed();
	if (previous != role) {
		session->changes().peerUpdated(user, Data::PeerUpdate::Flag::Name);
	} else if (MarkedRole(user->lastName) == role) {
		return;
	}

	// Other clients can only see a mark if Telegram retains it in the profile.
	const auto localOnly = [=] {
		if (session->settings().ownClientBadge() == role) {
			Ui::Toast::Show({ .text = { tr::lng_client_badge_local_only(tr::now) } });
		}
	};
	const auto updated = Marked(Unmarked(user->lastName), role);
	using Flag = MTPaccount_UpdateProfile::Flag;
	session->api().request(MTPaccount_UpdateProfile(
		MTP_flags(Flag::f_last_name),
		MTPstring(),
		MTP_string(updated),
		MTPstring()
	)).done([=](const MTPUser &result) {
		session->data().processUser(result);
		if (MarkedRole(user->lastName) != role) {
			localOnly();
		}
	}).fail(localOnly).send();
}

} // namespace ClientBadge
