#include "core/yg_features.h"

#include "core/appearance.h"
#include "core/enhanced_settings.h"
#include "data/data_peer.h"

#include <QtCore/QAbstractAnimation>
#include <QtCore/QDate>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QVariantAnimation>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>

#include <algorithm>
#include <array>
#include <cmath>

namespace Core::YgFeatures {
namespace {

QSet<uint64> &AllowedReadPeers() {
	static auto set = QSet<uint64>();
	return set;
}

const QRegularExpression &ZalgoRegex() {
	static const auto re = QRegularExpression(
		u"\\p{Mn}{3,}|[\\x{202A}-\\x{202E}\\x{2066}-\\x{2069}\\x{200E}\\x{200F}\\x{061C}]"_q,
		QRegularExpression::UseUnicodePropertiesOption);
	return re;
}

class MathParser final {
public:
	explicit MathParser(const QString &text) : _text(text) {}

	std::optional<double> parse() {
		const auto res = expression();
		skipSpaces();
		if (!res || _pos != _text.size()) return std::nullopt;
		return res;
	}

private:
	void skipSpaces() {
		while (_pos < _text.size() && _text[_pos].isSpace()) ++_pos;
	}

	bool take(QChar ch) {
		skipSpaces();
		if (_pos < _text.size() && _text[_pos] == ch) {
			++_pos;
			return true;
		}
		return false;
	}

	std::optional<double> number() {
		skipSpaces();
		const auto start = _pos;
		while (_pos < _text.size()
			&& (_text[_pos].isDigit() || _text[_pos] == '.' || _text[_pos] == ',')) {
			++_pos;
		}
		if (start == _pos) return std::nullopt;
		auto ok = false;
		const auto val = _text.mid(start, _pos - start).replace(',', '.').toDouble(&ok);
		return ok ? std::make_optional(val) : std::nullopt;
	}

	std::optional<double> factor() {
		if (take('+')) return factor();
		if (take('-')) {
			const auto v = factor();
			return v ? std::make_optional(-*v) : std::nullopt;
		}
		if (take('(')) {
			const auto v = expression();
			return (v && take(')')) ? v : std::nullopt;
		}
		return number();
	}

	std::optional<double> power() {
		auto left = factor();
		if (left && take('^')) {
			const auto right = power();
			return right ? std::make_optional(std::pow(*left, *right)) : std::nullopt;
		}
		return left;
	}

	std::optional<double> term() {
		auto left = power();
		while (left) {
			if (take('*') || take(QChar(0x00D7))) {
				const auto r = power();
				left = r ? std::make_optional(*left * *r) : std::nullopt;
			} else if (take('/') || take(QChar(0x00F7))) {
				const auto r = power();
				left = (r && *r != 0.0) ? std::make_optional(*left / *r) : std::nullopt;
			} else if (take('%')) {
				const auto r = power();
				left = (r && *r != 0.0) ? std::make_optional(std::fmod(*left, *r)) : std::nullopt;
			} else {
				break;
			}
		}
		return left;
	}

	std::optional<double> expression() {
		auto left = term();
		while (left) {
			if (take('+')) {
				const auto r = term();
				left = r ? std::make_optional(*left + *r) : std::nullopt;
			} else if (take('-')) {
				const auto r = term();
				left = r ? std::make_optional(*left - *r) : std::nullopt;
			} else {
				break;
			}
		}
		return left;
	}

	QString _text;
	int _pos = 0;
};

class SmoothWheelFilter final : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override {
		if (event->type() != QEvent::Wheel
			|| Appearance::Get(Appearance::Key::ClientStyle) != 1) {
			return false;
		}
		auto *wheel = static_cast<QWheelEvent*>(event);
		if (wheel->modifiers() != Qt::NoModifier
			|| !wheel->pixelDelta().isNull()
			|| wheel->angleDelta().y() == 0
			|| (std::abs(wheel->angleDelta().y()) % 120 != 0)) {
			return false;
		}
		auto *widget = qobject_cast<QWidget*>(watched);
		while (widget && !qobject_cast<QAbstractScrollArea*>(widget)) {
			widget = widget->parentWidget();
		}
		auto *area = qobject_cast<QAbstractScrollArea*>(widget);
		if (!area || !area->verticalScrollBar() || !area->verticalScrollBar()->isVisible()) {
			return false;
		}
		auto *bar = area->verticalScrollBar();
		const auto step = -wheel->angleDelta().y() * 90 / 120;
		auto *anim = bar->findChild<QVariantAnimation*>(u"_ygSmooth"_q, Qt::FindDirectChildrenOnly);
		auto target = bar->value() + step;
		if (anim && anim->state() == QAbstractAnimation::Running) {
			target = anim->endValue().toInt() + step;
			anim->stop();
		} else if (!anim) {
			anim = new QVariantAnimation(bar);
			anim->setObjectName(u"_ygSmooth"_q);
			anim->setEasingCurve(QEasingCurve::OutCubic);
			connect(anim, &QVariantAnimation::valueChanged, bar, [=](const QVariant &v) {
				bar->setValue(v.toInt());
			});
		}
		target = std::clamp(target, bar->minimum(), bar->maximum());
		if (target == bar->value()) return false;
		anim->setDuration(165);
		anim->setStartValue(bar->value());
		anim->setEndValue(target);
		anim->start();
		event->accept();
		return true;
	}
};

} // namespace

bool GhostActiveAny() {
	using namespace EnhancedSettings;
	return Get(Option::GhostMode)
		|| Get(Option::GhostNoRead)
		|| Get(Option::GhostNoStories)
		|| Get(Option::GhostNoOnline)
		|| Get(Option::GhostNoTyping);
}

bool GhostBlockRead() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostMode)
		|| EnhancedSettings::Get(EnhancedSettings::Option::GhostNoRead);
}

bool GhostBlockStories() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostMode)
		|| EnhancedSettings::Get(EnhancedSettings::Option::GhostNoStories);
}

bool GhostBlockOnline() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostMode)
		|| EnhancedSettings::Get(EnhancedSettings::Option::GhostNoOnline);
}

bool GhostBlockTyping() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostMode)
		|| EnhancedSettings::Get(EnhancedSettings::Option::GhostNoTyping);
}

bool GhostOfflineAfterSend() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostOfflineAfterSend);
}

bool GhostReadOnSend() {
	return EnhancedSettings::Get(EnhancedSettings::Option::GhostReadOnSend);
}

void AllowReadOnSendOnce(uint64 peerId) {
	if (peerId) AllowedReadPeers().insert(peerId);
}

bool ConsumeReadOnSendOnce(uint64 peerId) {
	return AllowedReadPeers().remove(peerId);
}

bool StreamerActive() {
	return EnhancedSettings::Get(EnhancedSettings::Option::StreamerMode);
}

bool StreamerCaptureProtected() {
	return StreamerActive()
		&& EnhancedSettings::Get(EnhancedSettings::Option::StreamerCaptureProtection);
}

bool StreamerHideSensitive() {
	return StreamerActive()
		&& EnhancedSettings::Get(EnhancedSettings::Option::StreamerHideSensitive);
}

bool StreamerHidePrivateChats() {
	return StreamerActive()
		&& EnhancedSettings::Get(EnhancedSettings::Option::StreamerHidePrivateChats);
}

bool StreamerHidePrivatePeer(not_null<const PeerData*> peer) {
	return StreamerActive()
		&& EnhancedSettings::Get(EnhancedSettings::Option::StreamerHidePrivateChats)
		&& peer->isUser()
		&& !peer->isSelf()
		&& !peer->isServiceUser()
		&& !peer->isRepliesChat()
		&& !peer->isVerifyCodes();
}

bool StreamerMuteNotifications() {
	return StreamerActive()
		&& EnhancedSettings::Get(EnhancedSettings::Option::StreamerMuteNotifications);
}

QString MaskSensitiveText(const QString &value) {
	if (value.isEmpty()) return value;
	if (value.startsWith('@')) return u"@••••••••"_q;
	if (value.startsWith('+')) return u"+•• ••• ••• •• ••"_q;
	return u"••••••••••"_q;
}

QString FilterZalgoText(QString text) {
	if (EnhancedSettings::Get(EnhancedSettings::Option::FilterZalgo)
		&& ZalgoRegex().match(text).hasMatch()) {
		text.remove(ZalgoRegex());
	}
	return text;
}

void FilterZalgoEntities(TextWithEntities &text) {
	if (!EnhancedSettings::Get(EnhancedSettings::Option::FilterZalgo)
		|| text.text.size() < 3
		|| !ZalgoRegex().match(text.text).hasMatch()) {
		return;
	}
	const auto size = int(text.text.size());
	auto drop = std::vector<bool>(size, false);
	auto it = ZalgoRegex().globalMatch(text.text);
	while (it.hasNext()) {
		const auto m = it.next();
		for (auto i = int(m.capturedStart()); i < int(m.capturedEnd()); ++i) {
			if (i >= 0 && i < size) drop[i] = true;
		}
	}
	auto shift = std::vector<int>(size + 1, 0);
	QString out;
	out.reserve(size);
	for (int i = 0; i < size; ++i) {
		shift[i] = int(out.size());
		if (!drop[i]) out.append(text.text[i]);
	}
	shift[size] = int(out.size());
	EntitiesInText entities;
	entities.reserve(text.entities.size());
	for (const auto &e : text.entities) {
		const auto from = std::clamp(e.offset(), 0, size);
		const auto till = std::clamp(e.offset() + e.length(), from, size);
		const auto len = shift[till] - shift[from];
		if (len > 0) {
			entities.push_back(EntityInText(e.type(), shift[from], len, e.data()));
		}
	}
	text.text = std::move(out);
	text.entities = std::move(entities);
}

std::optional<MathReplacement> EvaluateInlineMath(
		const QString &text,
		int cursorPosition) {
	if (!EnhancedSettings::Get(EnhancedSettings::Option::InlineMath)
		|| cursorPosition <= 2
		|| cursorPosition > text.size()
		|| text[cursorPosition - 1] != '=') {
		return std::nullopt;
	}
	const auto bodyEnd = cursorPosition - 1;
	auto start = bodyEnd;
	auto hasOp = false;
	auto digits = 0;
	auto parens = 0;
	while (start > 0) {
		const auto ch = text[start - 1];
		if (ch.isDigit()) {
			++digits;
			--start;
		} else if (ch == '.' || ch == ',' || ch.isSpace()) {
			--start;
		} else if (ch == ')') {
			++parens;
			--start;
		} else if (ch == '(') {
			if (parens == 0) break;
			--parens;
			--start;
		} else if (QStringView(u"+-*/^%\u00D7\u00F7").contains(ch)) {
			hasOp = true;
			--start;
		} else {
			break;
		}
	}
	while (start < bodyEnd && text[start].isSpace()) {
		++start;
	}
	if (start > 0 && !text[start - 1].isSpace()) {
		return std::nullopt;
	}
	if (!hasOp || digits < 2 || parens != 0 || start >= bodyEnd) {
		return std::nullopt;
	}
	const auto expr = text.mid(start, bodyEnd - start).trimmed();
	const auto value = MathParser(expr).parse();
	if (!value || !std::isfinite(*value)) {
		return std::nullopt;
	}
	const auto rounded = std::round(*value * 1e9) / 1e9;
	return MathReplacement{
		.replaceStart = start,
		.replaceLength = cursorPosition - start,
		.replacement = QString::number(rounded, 'g', 14),
	};
}

QString EstimateRegistrationDate(uint64 bareUserId) {
	struct Anchor { uint64 id; int months; }; // months = year * 12 + (month - 1)
	static constexpr auto kAnchors = std::array<Anchor, 16>{{
		{ 100000ULL, 2013 * 12 + 7 },
		{ 10000000ULL, 2013 * 12 + 11 },
		{ 50000000ULL, 2014 * 12 + 5 },
		{ 100000000ULL, 2015 * 12 + 1 },
		{ 190000000ULL, 2016 * 12 + 0 },
		{ 310000000ULL, 2017 * 12 + 0 },
		{ 500000000ULL, 2018 * 12 + 1 },
		{ 760000000ULL, 2019 * 12 + 0 },
		{ 1100000000ULL, 2020 * 12 + 1 },
		{ 1620000000ULL, 2021 * 12 + 0 },
		{ 2130000000ULL, 2021 * 12 + 10 },
		{ 5100000000ULL, 2022 * 12 + 2 },
		{ 5850000000ULL, 2023 * 12 + 0 },
		{ 6800000000ULL, 2024 * 12 + 0 },
		{ 7750000000ULL, 2025 * 12 + 0 },
		{ 8650000000ULL, 2026 * 12 + 2 },
	}};
	if (!bareUserId) return QString();
	if (bareUserId <= kAnchors.front().id) {
		return u"≤ 08.2013"_q;
	}
	if (bareUserId >= kAnchors.back().id) {
		return u"≥ 03.2026"_q;
	}
	for (size_t i = 1; i < kAnchors.size(); ++i) {
		if (bareUserId <= kAnchors[i].id) {
			const auto &a = kAnchors[i - 1];
			const auto &b = kAnchors[i];
			const auto t = double(bareUserId - a.id) / double(b.id - a.id);
			const auto m = int(std::round(a.months + t * (b.months - a.months)));
			const auto year = m / 12;
			const auto month = (m % 12) + 1;
			return u"≈ "_q + QLocale().toString(QDate(year, month, 1), u"MM.yyyy"_q);
		}
	}
	return QString();
}

void InstallSmoothScrollFilter() {
	static bool installed = false;
	if (!installed && qApp) {
		installed = true;
		qApp->installEventFilter(new SmoothWheelFilter(qApp));
	}
}

} // namespace Core::YgFeatures
