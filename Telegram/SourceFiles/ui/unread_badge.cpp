/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/unread_badge.h"

#include "data/data_emoji_statuses.h"
#include "core/client_badge.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "data/data_session.h"
#include "data/stickers/data_custom_emoji.h"
#include "main/main_session.h"
#include "lang/lang_keys.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/power_saving.h"
#include "ui/text/text_custom_emoji.h"
#include "ui/unread_badge_paint.h"
#include "styles/style_dialogs.h"

#include <QtGui/QPainterPath>

#include <cmath>

namespace Ui {
namespace {

constexpr auto kPlayStatusLimit = 2;
constexpr auto kBotVerifiedScale = 0.88;

class ScaledBotVerifiedEmoji final : public Ui::Text::CustomEmoji {
public:
	ScaledBotVerifiedEmoji(
		std::unique_ptr<Ui::Text::CustomEmoji> wrapped,
		int innerSize,
		int outerSize);

	int width() override;
	QString entityData() override;
	void paint(QPainter &p, const Context &context) override;
	void unload() override;
	bool ready() override;
	bool readyInDefaultState() override;

private:
	const std::unique_ptr<Ui::Text::CustomEmoji> _wrapped;
	const int _innerSize = 0;
	const int _outerSize = 0;
	QImage _frame;
	QColor _frameColor;

};

ScaledBotVerifiedEmoji::ScaledBotVerifiedEmoji(
	std::unique_ptr<Ui::Text::CustomEmoji> wrapped,
	int innerSize,
	int outerSize)
: _wrapped(std::move(wrapped))
, _innerSize(innerSize)
, _outerSize(outerSize) {
}

int ScaledBotVerifiedEmoji::width() {
	return _outerSize;
}

QString ScaledBotVerifiedEmoji::entityData() {
	return _wrapped->entityData();
}

void ScaledBotVerifiedEmoji::paint(QPainter &p, const Context &context) {
	if (_frame.isNull() || _frameColor != context.textColor) {
		if (!_wrapped->ready()) {
			return;
		}
		const auto ratio = style::DevicePixelRatio();
		const auto sourcePx = Data::FrameSizeFromTag(
			Data::CustomEmojiSizeTag::Isolated);
		_frame = QImage(
			QSize(sourcePx, sourcePx),
			QImage::Format_ARGB32_Premultiplied);
		_frame.setDevicePixelRatio(ratio);
		_frame.fill(Qt::transparent);

		auto painter = QPainter(&_frame);
		painter.translate(-context.position);
		const auto was = context.internal.forceFirstFrame;
		context.internal.forceFirstFrame = true;
		_wrapped->paint(painter, context);
		context.internal.forceFirstFrame = was;
		painter.end();

		_frame = _frame.scaled(
			QSize(_innerSize, _innerSize) * ratio,
			Qt::IgnoreAspectRatio,
			Qt::SmoothTransformation);
		_frameColor = context.textColor;
	}
	const auto skip = (_outerSize - _innerSize) / 2;
	p.drawImage(context.position + QPoint(skip, skip), _frame);
}

void ScaledBotVerifiedEmoji::unload() {
	_wrapped->unload();
}

bool ScaledBotVerifiedEmoji::ready() {
	return !_frame.isNull() || _wrapped->ready();
}

bool ScaledBotVerifiedEmoji::readyInDefaultState() {
	return !_frame.isNull() || _wrapped->ready();
}

} // namespace

struct PeerBadge::EmojiStatus {
	EmojiStatusId id;
	std::unique_ptr<Ui::Text::CustomEmoji> emoji;
	QPoint lastPosition;
	QColor lastColor;
	int skip = 0;
	bool painted = false;
};

struct PeerBadge::BotVerifiedData {
	QImage cache;
	std::unique_ptr<Text::CustomEmoji> icon;
};

void UnreadBadge::setText(const QString &text, bool active) {
	_text = text;
	_active = active;
	const auto st = Dialogs::Ui::UnreadBadgeStyle();
	resize(
		std::max(st.font->width(text) + 2 * st.padding, st.size),
		st.size);
	update();
}

int UnreadBadge::textBaseline() const {
	const auto st = Dialogs::Ui::UnreadBadgeStyle();
	return ((st.size - st.font->height) / 2) + st.font->ascent;
}

void UnreadBadge::paintEvent(QPaintEvent *e) {
	if (_text.isEmpty()) {
		return;
	}

	auto p = QPainter(this);

	UnreadBadgeStyle unreadSt;
	unreadSt.muted = !_active;
	auto unreadRight = width();
	auto unreadTop = 0;
	PaintUnreadBadge(
		p,
		_text,
		unreadRight,
		unreadTop,
		unreadSt);
}

QString TextBadgeText(TextBadgeType type) {
	switch (type) {
	case TextBadgeType::Fake: return tr::lng_fake_badge(tr::now);
	case TextBadgeType::Scam: return tr::lng_scam_badge(tr::now);
	case TextBadgeType::Direct: return tr::lng_direct_badge(tr::now);
	}
	Unexpected("Type in TextBadgeText.");
}

QSize TextBadgeSize(TextBadgeType type) {
	const auto phrase = TextBadgeText(type);
	const auto phraseWidth = st::dialogsScamFont->width(phrase);
	const auto width = st::dialogsScamPadding.left()
		+ phraseWidth
		+ st::dialogsScamPadding.right();
	const auto height = st::dialogsScamPadding.top()
		+ st::dialogsScamFont->height
		+ st::dialogsScamPadding.bottom();
	return { width, height };
}

void DrawTextBadge(
		Painter &p,
		QRect rect,
		int outerWidth,
		const style::color &color,
		const QString &phrase,
		int phraseWidth) {
	PainterHighQualityEnabler hq(p);
	auto pen = color->p;
	pen.setWidth(st::lineWidth);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(rect, st::dialogsScamRadius, st::dialogsScamRadius);
	p.setFont(st::dialogsScamFont);
	if (style::DevicePixelRatio() > 1) {
		p.drawText(
			QRect(
				rect.x() + st::dialogsScamPadding.left(),
				rect.y() + st::dialogsScamPadding.top(),
				rect.width() - rect::m::sum::h(st::dialogsScamPadding),
				rect.height() - rect::m::sum::v(st::dialogsScamPadding)),
			Qt::AlignCenter,
			phrase);
	} else {
		p.drawTextLeft(
			rect.x() + st::dialogsScamPadding.left(),
			rect.y() + st::dialogsScamPadding.top(),
			outerWidth,
			phrase,
			phraseWidth);
	}
}

void DrawTextBadge(
		TextBadgeType type,
		Painter &p,
		QRect rect,
		int outerWidth,
		const style::color &color) {
	const auto phrase = TextBadgeText(type);
	DrawTextBadge(
		p,
		rect,
		outerWidth,
		color,
		phrase,
		st::dialogsScamFont->width(phrase));
}

PeerBadge::PeerBadge() = default;

PeerBadge::~PeerBadge() = default;

int PeerBadge::drawGetWidth(Painter &p, Descriptor &&descriptor) {
	Expects(descriptor.customEmojiRepaint != nullptr);

	const auto peer = descriptor.peer;
	_clientMarkRect = QRect();
	if ((descriptor.scam && (peer->isScam() || peer->isFake()))
		|| (descriptor.direct && peer->isMonoforum())) {
		if (_emojiStatus) {
			_emojiStatus->painted = false;
		}
		return drawTextBadge(p, descriptor);
	}
	const auto drawn = drawClientMark(p, descriptor);
	return drawn;
}

int PeerBadge::drawClientMark(Painter &p, const Descriptor &descriptor) {
	const auto drawn = drawExisting(p, descriptor);
	const auto peer = descriptor.peer;
	const auto role = ClientBadge::RoleOfPeer(peer);
	if (role == ClientBadge::Role::None) {
		return drawn;
	}
	const auto rectForName = descriptor.rectForName;
	const auto skip = st::dialogsChatTypeSkip;
	const auto size = std::min(st::emojiSize, rectForName.height());
	const auto x = rectForName.x()
		+ std::min(
			descriptor.nameWidth + drawn + skip,
			rectForName.width() - size);
	const auto rect = QRect(
		QPoint(x, rectForName.y() + (rectForName.height() - size) / 2),
		Size(size));
	_clientMarkRect = rect;
	DrawClientMark(p, rect, *descriptor.premiumFg, role, descriptor.now);
	return drawn + size + skip;
}

int PeerBadge::drawExisting(Painter &p, const Descriptor &descriptor) {
	const auto peer = descriptor.peer;
	const auto verifyCheck = descriptor.verified && peer->isVerified();
	const auto premiumMark = descriptor.premium
		&& peer->session().premiumBadgesShown();
	const auto emojiStatus = premiumMark
		&& peer->emojiStatusId()
		&& (peer->isPremium() || peer->isChannel())
		&& !peer->hideEmojiStatus();
	const auto premiumStar = premiumMark
		&& !emojiStatus
		&& peer->isPremium();

	const auto paintVerify = verifyCheck
		&& (descriptor.prioritizeVerification
			|| descriptor.bothVerifyAndStatus
			|| !emojiStatus);
	const auto paintEmoji = emojiStatus
		&& (!paintVerify || descriptor.bothVerifyAndStatus);
	const auto paintStar = premiumStar && !paintVerify;

	if (paintEmoji) {
		if (!paintVerify) {
			return drawPremiumEmojiStatus(p, descriptor);
		}
		auto layout = descriptor;
		layout.rectForName.setWidth(
			descriptor.rectForName.width() - descriptor.verified->width());
		const auto emojiWidth = drawPremiumEmojiStatus(p, layout);
		layout.rectForName = descriptor.rectForName;
		layout.nameWidth += emojiWidth;
		return emojiWidth + drawVerifyCheck(p, layout);
	} else if (_emojiStatus) {
		_emojiStatus->painted = false;
	}
	if (paintVerify) {
		return drawVerifyCheck(p, descriptor);
	} else if (paintStar) {
		return drawPremiumStar(p, descriptor);
	}
	return 0;
}

int PeerBadge::drawTextBadge(Painter &p, const Descriptor &descriptor) {
	const auto type = [&] {
		if (descriptor.peer->isScam()) {
			return TextBadgeType::Scam;
		} else if (descriptor.peer->isFake()) {
			return TextBadgeType::Fake;
		}
		return TextBadgeType::Direct;
	}();
	const auto phrase = TextBadgeText(type);
	const auto phraseWidth = st::dialogsScamFont->width(phrase);
	const auto width = st::dialogsScamPadding.left()
		+ phraseWidth
		+ st::dialogsScamPadding.right();
	const auto height = st::dialogsScamPadding.top()
		+ st::dialogsScamFont->height
		+ st::dialogsScamPadding.bottom();
	const auto rectForName = descriptor.rectForName;
	const auto rect = QRect(
		(rectForName.x()
			+ std::min(
				descriptor.nameWidth + st::dialogsScamSkip,
				rectForName.width() - width)),
		rectForName.y() + (rectForName.height() - height) / 2,
		width,
		height);
	DrawTextBadge(
		p,
		rect,
		descriptor.outerWidth,
		*((type == TextBadgeType::Direct)
			? descriptor.direct
			: descriptor.scam),
		phrase,
		phraseWidth);
	return st::dialogsScamSkip + width;
}

int PeerBadge::drawVerifyCheck(Painter &p, const Descriptor &descriptor) {
	const auto iconw = descriptor.verified->width();
	const auto rectForName = descriptor.rectForName;
	const auto nameWidth = descriptor.nameWidth;
	descriptor.verified->paint(
		p,
		rectForName.x() + std::min(nameWidth, rectForName.width() - iconw),
		rectForName.y(),
		descriptor.outerWidth);
	return iconw;
}

int PeerBadge::drawPremiumEmojiStatus(
		Painter &p,
		const Descriptor &descriptor) {
	const auto peer = descriptor.peer;
	const auto id = peer->emojiStatusId();
	const auto rectForName = descriptor.rectForName;
	const auto iconw = descriptor.premium->width();
	const auto iconx = rectForName.x()
		+ std::min(descriptor.nameWidth, rectForName.width() - iconw);
	const auto icony = rectForName.y();
	if (!_emojiStatus) {
		_emojiStatus = std::make_unique<EmojiStatus>();
		const auto size = st::emojiSize;
		const auto emoji = Ui::Text::AdjustCustomEmojiSize(size);
		_emojiStatus->skip = (size - emoji) / 2;
	}
	if (_emojiStatus->id != id) {
		using namespace Ui::Text;
		auto &manager = peer->session().data().customEmojiManager();
		_emojiStatus->id = id;
		_emojiStatus->emoji = MakeWrappedEmoji<LimitedLoopsEmoji>(
			manager.create(
				Data::EmojiStatusCustomId(id),
				descriptor.customEmojiRepaint),
			kPlayStatusLimit);
	}
	if (!_emojiStatus->emoji) {
		return 0;
	}
	_emojiStatus->lastPosition = QPoint(
		iconx - 2 * _emojiStatus->skip,
		icony + _emojiStatus->skip);
	_emojiStatus->lastColor = (*descriptor.premiumFg)->c;
	_emojiStatus->painted = true;
	_emojiStatus->emoji->paint(p, {
		.textColor = _emojiStatus->lastColor,
		.now = descriptor.now,
		.position = _emojiStatus->lastPosition,
		.paused = descriptor.paused || On(PowerSaving::kEmojiStatus),
	});
	return iconw - 4 * _emojiStatus->skip;
}

int PeerBadge::drawPremiumStar(Painter &p, const Descriptor &descriptor) {
	const auto rectForName = descriptor.rectForName;
	const auto iconw = descriptor.premium->width();
	const auto iconx = rectForName.x()
		+ std::min(descriptor.nameWidth, rectForName.width() - iconw);
	const auto icony = rectForName.y();
	_emojiStatus = nullptr;
	descriptor.premium->paint(p, iconx, icony, descriptor.outerWidth);
	return iconw;
}

QRect PeerBadge::clientMarkRect() const {
	return _clientMarkRect;
}

QRect PeerBadge::emojiStatusRect() const {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return QRect();
	}
	return QRect(
		_emojiStatus->lastPosition,
		Size(st::emojiSize - 2 * _emojiStatus->skip));
}

void PeerBadge::paintEmojiStatusFrame(
		QPainter &p,
		crl::time now,
		bool paused) {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return;
	}
	paintEmojiStatusFrame(p, now, paused, _emojiStatus->lastPosition);
}

void PeerBadge::paintEmojiStatusFrame(
		QPainter &p,
		crl::time now,
		bool paused,
		QPoint position) {
	if (!_emojiStatus || !_emojiStatus->emoji || !_emojiStatus->painted) {
		return;
	}
	_emojiStatus->emoji->paint(p, {
		.textColor = _emojiStatus->lastColor,
		.now = now,
		.position = position,
		.paused = paused || On(PowerSaving::kEmojiStatus),
	});
}

void PeerBadge::unload() {
	_emojiStatus = nullptr;
}

bool PeerBadge::ready(const BotVerifyDetails *details) const {
	if (!details || !*details) {
		_botVerifiedData = nullptr;
		return true;
	} else if (!_botVerifiedData) {
		return false;
	}
	if (!details->iconId) {
		_botVerifiedData->icon = nullptr;
	} else if (!_botVerifiedData->icon
		|| (_botVerifiedData->icon->entityData()
			!= Data::SerializeCustomEmojiId(details->iconId))) {
		return false;
	}
	return true;
}

void PeerBadge::set(
		not_null<const BotVerifyDetails*> details,
		Ui::Text::CustomEmojiFactory factory,
		Fn<void()> repaint) {
	if (!_botVerifiedData) {
		_botVerifiedData = std::make_unique<BotVerifiedData>();
	}
	if (details->iconId) {
		const auto outer = st::emojiSize;
		const auto inner = int(base::SafeRound(
			st::emojiSize * kBotVerifiedScale));
		_botVerifiedData->icon = MakeWrappedEmoji<ScaledBotVerifiedEmoji>(
			factory(
				Data::SerializeCustomEmojiId(details->iconId),
				{ .repaint = repaint }),
			inner,
			outer);
	}
}

int PeerBadge::drawVerified(
		QPainter &p,
		QPoint position,
		const style::VerifiedBadge &st) {
	const auto data = _botVerifiedData.get();
	if (!data) {
		return 0;
	}
	if (const auto icon = data->icon.get()) {
		icon->paint(p, {
			.textColor = st.color->c,
			.now = crl::now(),
			.position = position + st.position,
		});
		return icon->width();
	}
	return 0;
}

// The YandereGram mark next to the name of a peer that also runs the client.
// Each role has its own animated glyph; `now` drives the phase.
void DrawClientMark(
		Painter &p,
		QRect rect,
		const QColor &fg,
		ClientBadge::Role role,
		crl::time now) {
	if (rect.isEmpty() || role == ClientBadge::Role::None) {
		return;
	}
	const auto hq = PainterHighQualityEnabler(p);

	const auto side = float64(std::min(rect.width(), rect.height()));
	p.save();
	p.translate(rect.center());
	p.scale(side / 20., side / 20.);
	p.translate(-10., -10.);

	switch (role) {
	case ClientBadge::Role::Supporter: {
		// Ribbon bow with a heart, the ties swinging.
		const auto sway = std::sin(now / 340.) * 0.8;
		p.setPen(Qt::NoPen);
		p.setBrush(fg);
		auto tie = QPainterPath();
		tie.moveTo(9.2, 12.6);
		tie.lineTo(6.4 + sway, 18.2);
		tie.lineTo(9.6, 16.4);
		tie.closeSubpath();
		p.drawPath(tie);
		auto tie2 = QPainterPath();
		tie2.moveTo(10.8, 12.6);
		tie2.lineTo(13.6 - sway, 18.2);
		tie2.lineTo(10.4, 16.4);
		tie2.closeSubpath();
		p.drawPath(tie2);

		auto bow = QPainterPath();
		bow.moveTo(10., 11.4);
		bow.cubicTo(7.4, 6.2, 2.2, 7.4, 3.4, 11.2);
		bow.cubicTo(4.0, 13.2, 7.4, 13.4, 10., 11.4);
		bow.cubicTo(12.6, 6.2, 17.8, 7.4, 16.6, 11.2);
		bow.cubicTo(16.0, 13.2, 12.6, 13.4, 10., 11.4);
		p.drawPath(bow);

		auto heart = QPainterPath();
		const auto beat = 1. + std::sin(now / 260.) * 0.09;
		const auto r = 2.1 * beat;
		heart.moveTo(10., 11.2 + r * 0.9);
		heart.cubicTo(
			10. - r * 1.8, 11.2 - r * 0.3,
			10. - r * 0.7, 11.2 - r * 1.5,
			10., 11.2 - r * 0.45);
		heart.cubicTo(
			10. + r * 0.7, 11.2 - r * 1.5,
			10. + r * 1.8, 11.2 - r * 0.3,
			10., 11.2 + r * 0.9);
		p.drawPath(heart);
	} break;

	case ClientBadge::Role::BugHunter: {
		// Cracked heart with a drop sliding down.
		auto pen = QPen(fg);
		pen.setWidthF(1.7);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		auto heart = QPainterPath();
		heart.moveTo(10., 16.6);
		heart.cubicTo(2.2, 11.4, 3.2, 4.4, 7.0, 4.6);
		heart.cubicTo(8.8, 4.7, 9.6, 6.2, 10., 7.0);
		heart.cubicTo(10.4, 6.2, 11.2, 4.7, 13.0, 4.6);
		heart.cubicTo(16.8, 4.4, 17.8, 11.4, 10., 16.6);
		p.drawPath(heart);

		auto crack = QPainterPath();
		crack.moveTo(10.2, 7.4);
		crack.lineTo(8.6, 10.2);
		crack.lineTo(10.6, 11.6);
		crack.lineTo(9.2, 14.2);
		p.drawPath(crack);

		const auto phase = std::fmod(float64(now), 1400.) / 1400.;
		const auto offset = 5.4 * phase;
		const auto alpha = (phase > 0.82)
			? uchar(base::SafeRound(255. * (1. - (phase - 0.82) / 0.18)))
			: uchar(255);
		p.setPen(Qt::NoPen);
		auto drop = fg;
		drop.setAlpha(alpha);
		p.setBrush(drop);
		p.drawEllipse(QPointF(13.6, 8.2 + offset), 1.05, 1.05);
	} break;

	case ClientBadge::Role::Developer: {
		// Knife at an angle, a highlight running along the blade.
		auto pen = QPen(fg);
		pen.setWidthF(1.7);
		pen.setJoinStyle(Qt::RoundJoin);
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		auto blade = QPainterPath();
		blade.moveTo(5.0, 16.2);
		blade.cubicTo(8.4, 13.6, 12.6, 9.4, 15.8, 4.6);
		blade.lineTo(17.2, 5.8);
		blade.cubicTo(14.6, 10.6, 10.4, 14.4, 6.4, 17.4);
		blade.closeSubpath();
		p.drawPath(blade);

		auto guard = QPainterPath();
		guard.moveTo(3.2, 15.0);
		guard.lineTo(7.8, 19.0);
		p.drawPath(guard);

		auto handle = QPainterPath();
		handle.moveTo(5.4, 17.2);
		handle.lineTo(2.6, 19.6);
		p.drawPath(handle);

		const auto phase = std::fmod(float64(now), 1500.) / 1500.;
		if (phase < 0.6) {
			const auto t = phase / 0.6;
			const auto x = 6.6 + (15.4 - 6.6) * t;
			const auto y = 15.0 + (6.2 - 15.0) * t;
			auto shine = QPen(QColor(255, 255, 255, uchar(
				base::SafeRound(210. * std::sin(t * 3.14159)))));
			shine.setWidthF(1.5);
			shine.setCapStyle(Qt::RoundCap);
			p.setPen(shine);
			auto line = QPainterPath();
			line.moveTo(x - 1.1, y + 1.5);
			line.lineTo(x + 1.1, y - 1.5);
			p.drawPath(line);
		}
	} break;

	case ClientBadge::Role::Founder: {
		// Collar with a bell hanging on it, the bell swinging.
		auto pen = QPen(fg);
		pen.setWidthF(1.7);
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);

		auto band = QPainterPath();
		band.moveTo(3.0, 7.6);
		band.cubicTo(5.6, 11.4, 14.4, 11.4, 17.0, 7.6);
		p.drawPath(band);

		const auto swing = std::sin(now / 300.) * 1.5;
		p.setPen(Qt::NoPen);
		p.setBrush(fg);
		const auto bx = 10. + swing;
		p.drawEllipse(QPointF(bx, 13.6), 2.35, 2.35);
		p.setBrush(Qt::white);
		p.drawEllipse(QPointF(bx, 14.35), 0.42, 0.42);

		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		auto ring = QPainterPath();
		ring.moveTo(bx, 10.6);
		ring.lineTo(bx, 11.2);
		p.drawPath(ring);
	} break;

	case ClientBadge::Role::None:
		break;
	}

	p.restore();
}

void DrawClientMark(
		Painter &p,
		QRect rect,
		const style::color &fg,
		ClientBadge::Role role,
		crl::time now) {
	DrawClientMark(p, rect, fg->c, role, now);
}

} // namespace Ui
