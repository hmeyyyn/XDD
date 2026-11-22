#include "settings/sections/settings_yandere_panel.h"

#include "apiwrap.h"
#include "base/unixtime.h"
#include "base/weak_ptr.h"
#include "bot_use/bot_use_manager.h"
#include "bot_use/bot_use_settings.h"
#include "boxes/enhanced_options_box.h"
#include "boxes/link_preview_rules_box.h"
#include "boxes/llm_translate_config_box.h"
#include "core/appearance.h"
#include "core/application.h"
#include "core/client_badge.h"
#include "core/core_settings.h"
#include "core/enhanced_settings.h"
#include "core/material_color_picker.h"
#include "core/ws_proxy.h"
#include "data/data_folder.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"
#include "lang/lang_instance.h"
#include "lang/lang_keys.h"
#include "lang/translate_provider.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "mainwindow.h"
#include "settings/sections/settings_appearance.h"
#include "settings/sections/settings_bot_use.h"
#include "styles/style_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/toast/toast.h"
#include "ui/widgets/chat_filters_tabs_mode.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include <QtCore/QByteArrayList>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QFontDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>

#include <algorithm>
#include <cmath>

namespace Settings {
namespace {

using Page = PanelPage;
constexpr auto kFavorites = "yanderegram.panel.favorites";
const auto kClipboardPrefix = u"yanderegram-settings:"_q;

QColor AccentColor() {
	const auto c = Appearance::Color(Appearance::Tint::Accent);
	return c.isValid() ? c : QColor(0xFF, 0x62, 0x6D);
}

QColor InkColor() {
	const auto c = Appearance::Color(Appearance::Tint::Ink);
	return c.isValid() ? c : QColor(0xF1, 0xEE, 0xF0);
}

QColor PanelBgColor() {
	const auto c = Appearance::Color(Appearance::Tint::SettingsBg);
	return c.isValid() ? c : QColor(0x11, 0x10, 0x14);
}

QString Title(Page page) {
	switch (page) {
	case Page::Interface: return tr::lng_settings_interface(tr::now);
	case Page::Messages: return tr::lng_settings_messages(tr::now);
	case Page::Ghost: return tr::lng_yg_cat_privacy(tr::now);
	case Page::Streamer: return tr::lng_yg_cat_streamer(tr::now);
	case Page::Behavior: return tr::lng_settings_behavior(tr::now);
	case Page::Network: return tr::lng_yg_cat_network(tr::now);
	case Page::Feed: return tr::lng_yg_cat_feed(tr::now);
	case Page::Journal: return tr::lng_yg_journal(tr::now);
	case Page::Appearance: return tr::lng_settings_enhanced_customization(tr::now);
	case Page::BotUse: return tr::lng_bot_use_settings(tr::now);
	case Page::Translation: return tr::lng_settings_translation(tr::now);
	case Page::Calls: return tr::lng_settings_voice_chat(tr::now);
	case Page::Other: return tr::lng_settings_other(tr::now);
	case Page::Favorites: return tr::lng_yg_panel_favorites(tr::now);
	case Page::Count: break;
	}
	return u"YandereGram"_q;
}

QString OptionDesc(EnhancedSettings::OptionId id) {
	const auto &d = EnhancedSettings::DescriptorFor(id);
	return d.description ? (*d.description)(tr::now) : QString();
}

// Custom dark/crimson popup selector menu (no system white menu).
class GuiPopupMenu final : public QWidget {
public:
	GuiPopupMenu(
		QWidget *parent,
		const std::vector<QString> &options,
		int selected,
		Fn<void(int)> onPicked)
	: QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
	, _options(options)
	, _selected(selected)
	, _onPicked(std::move(onPicked)) {
		setAttribute(Qt::WA_DeleteOnClose);
		setAttribute(Qt::WA_TranslucentBackground);
		setMouseTracking(true);
		int maxW = 180;
		const QFontMetrics fm(font());
		for (const auto &opt : _options) {
			maxW = std::max(maxW, fm.horizontalAdvance(opt) + 46);
		}
		maxW = std::min(maxW, 340);
		const int h = int(_options.size()) * kRowH + 12;
		resize(maxW, h);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = AccentColor();
		p.setPen(QPen(QColor(74, 52, 62), 1));
		p.setBrush(QColor(22, 20, 26));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

		for (int i = 0; i < int(_options.size()); ++i) {
			const QRect r(6, 6 + i * kRowH, width() - 12, kRowH);
			const bool active = (i == _selected);
			const bool hov = (i == _hover);
			if (active || hov) {
				p.setPen(Qt::NoPen);
				p.setBrush(active ? QColor(68, 36, 44) : QColor(42, 37, 47));
				p.drawRoundedRect(r, 6, 6);
			}
			if (active) {
				p.setPen(Qt::NoPen);
				p.setBrush(accent);
				p.drawRoundedRect(QRect(r.left() + 4, r.top() + 7, 3, r.height() - 14), 1.5, 1.5);
			}
			p.setPen(active ? Qt::white : QColor(232, 226, 232));
			auto f = font();
			f.setWeight(active ? QFont::DemiBold : QFont::Normal);
			p.setFont(f);
			p.drawText(
				r.adjusted(14, 0, -10, 0),
				Qt::AlignVCenter | Qt::AlignLeft,
				p.fontMetrics().elidedText(_options[i], Qt::ElideRight, r.width() - 24));
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		const int idx = (e->pos().y() - 6) / kRowH;
		const int next = (idx >= 0 && idx < int(_options.size())) ? idx : -1;
		if (_hover != next) {
			_hover = next;
			update();
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		const int idx = (e->pos().y() - 6) / kRowH;
		if (idx >= 0 && idx < int(_options.size())) {
			const auto cb = _onPicked;
			close();
			if (cb) cb(idx);
		} else {
			close();
		}
	}

private:
	static constexpr int kRowH = 32;
	std::vector<QString> _options;
	int _selected = 0;
	int _hover = -1;
	Fn<void(int)> _onPicked;
};

// 1. Custom CSGUI Switch Card
class SwitchCard final : public QAbstractButton {
public:
	using QAbstractButton::QAbstractButton;
	QString detail;

	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = AccentColor();
		const auto ink = InkColor();
		const bool hov = underMouse();
		p.setBrush(isChecked()
			? (hov ? QColor(72, 41, 49) : QColor(60, 34, 41))
			: (hov ? QColor(36, 34, 40) : QColor(28, 27, 32)));
		p.setPen(hasFocus()
			? accent
			: (isChecked() ? QColor(96, 56, 66) : QColor(49, 46, 54)));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);

		p.setPen(ink);
		auto f = font();
		f.setWeight(QFont::DemiBold);
		p.setFont(f);
		const auto h = p.fontMetrics().height();
		p.drawText(
			QRect(12, 8, width() - 46, 2 * h + 2),
			Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
			text());

		p.setFont(font());
		p.setPen(QColor(178, 169, 178));
		const auto area = QRect(12, height() - h - 9, width() - 64, h + 2);
		p.drawText(
			area,
			Qt::AlignVCenter | Qt::AlignLeft,
			p.fontMetrics().elidedText(detail, Qt::ElideRight, area.width()));

		// Custom CSGUI pill toggle
		const int tx = width() - 42;
		const int ty = height() - 25;
		p.setPen(Qt::NoPen);
		p.setBrush(isChecked() ? accent : QColor(68, 63, 73));
		p.drawRoundedRect(QRect(tx, ty, 30, 16), 8, 8);
		p.setBrush(isChecked() ? Qt::white : QColor(194, 186, 196));
		p.drawEllipse(QRect(tx + (isChecked() ? 16 : 2), ty + 2, 12, 12));
	}
};

class GuiToggleTile final : public Ui::RpWidget {
public:
	GuiToggleTile(
		QWidget *parent,
		QByteArray id,
		QString title,
		QString subtitle,
		Fn<bool()> getter,
		Fn<void(bool)> setter,
		rpl::producer<> changes,
		bool starred,
		Fn<void(bool)> onStar)
	: RpWidget(parent)
	, _id(std::move(id))
	, _getter(std::move(getter))
	, _setter(std::move(setter)) {
		_btn = new SwitchCard(this);
		_btn->setCheckable(true);
		_btn->setCursor(Qt::PointingHandCursor);
		_btn->setText(title);
		_btn->detail = subtitle;
		_btn->setChecked(_getter());
		_btn->setToolTip(subtitle.isEmpty() ? title : (title + '\n' + subtitle));

		connect(_btn, &QAbstractButton::clicked, this, [=] {
			const bool next = !_getter();
			_setter(next);
			_btn->setChecked(_getter());
			_btn->update();
		});

		std::move(changes) | rpl::on_next([=] {
			_btn->setChecked(_getter());
			_btn->update();
		}, lifetime());

		if (!_id.isEmpty() && onStar) {
			_star = new QToolButton(this);
			_star->setCheckable(true);
			_star->setCursor(Qt::PointingHandCursor);
			_star->setChecked(starred);
			_star->setText(starred ? u"★"_q : u"☆"_q);
			_star->setStyleSheet(u"QToolButton{border:0;background:transparent;color:#9f95a0;font-size:13px;} QToolButton:checked{color:#ff626d;} QToolButton:hover{color:#ffffff;}"_q);
			connect(_star, &QToolButton::clicked, this, [=](bool on) {
				_star->setText(on ? u"★"_q : u"☆"_q);
				onStar(on);
			});
		}
	}

	[[nodiscard]] QString searchBlob() const {
		return _btn->text() + ' ' + _btn->detail;
	}

	void setSubtitle(const QString &sub) {
		_btn->detail = sub;
		_btn->update();
	}

protected:
	void resizeEvent(QResizeEvent *) override {
		_btn->setGeometry(rect());
		if (_star) {
			_star->setGeometry(width() - 28, 5, 22, 22);
		}
	}

private:
	QByteArray _id;
	Fn<bool()> _getter;
	Fn<void(bool)> _setter;
	SwitchCard *_btn = nullptr;
	QToolButton *_star = nullptr;
};

// 2. Custom CSGUI Slider Card (100% custom dark/crimson track & thumb, zero green)
class GuiSliderTile final : public Ui::RpWidget {
public:
	GuiSliderTile(
		QWidget *parent,
		QString title,
		QString subtitle,
		int minVal,
		int maxVal,
		Fn<int()> getter,
		Fn<QString(int)> formatter,
		Fn<void(int)> setter,
		rpl::producer<> changes)
	: RpWidget(parent)
	, _title(std::move(title))
	, _subtitle(std::move(subtitle))
	, _min(minVal)
	, _max(std::max(minVal + 1, maxVal))
	, _getter(std::move(getter))
	, _formatter(std::move(formatter))
	, _setter(std::move(setter)) {
		setCursor(Qt::PointingHandCursor);
		setMouseTracking(true);
		_current = std::clamp(_getter(), _min, _max);
		std::move(changes) | rpl::on_next([=] {
			if (!_dragging) {
				_current = std::clamp(_getter(), _min, _max);
				update();
			}
		}, lifetime());
	}

	[[nodiscard]] QString searchBlob() const {
		return _title + ' ' + _subtitle;
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = AccentColor();
		const auto ink = InkColor();

		p.setBrush(underMouse() ? QColor(34, 32, 39) : QColor(28, 27, 32));
		p.setPen(_dragging ? accent : QColor(52, 48, 58));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);

		// Value pill on top right
		const auto valStr = _formatter(_current);
		auto badgeFont = font();
		badgeFont.setWeight(QFont::DemiBold);
		p.setFont(badgeFont);
		const int pillW = std::max(44, p.fontMetrics().horizontalAdvance(valStr) + 16);
		const QRect pillRect(width() - pillW - 12, 8, pillW, 20);
		p.setPen(QPen(QColor(96, 54, 64), 1));
		p.setBrush(QColor(58, 32, 40));
		p.drawRoundedRect(pillRect, 6, 6);
		p.setPen(accent.lighter(120));
		p.drawText(pillRect, Qt::AlignCenter, valStr);

		// Title
		p.setPen(ink);
		p.drawText(
			QRect(12, 8, width() - pillW - 28, 22),
			Qt::AlignVCenter | Qt::AlignLeft,
			p.fontMetrics().elidedText(_title, Qt::ElideRight, width() - pillW - 28));

		// Subtitle
		if (!_subtitle.isEmpty()) {
			p.setFont(font());
			p.setPen(QColor(168, 160, 170));
			p.drawText(
				QRect(12, 30, width() - 24, 18),
				Qt::AlignVCenter | Qt::AlignLeft,
				p.fontMetrics().elidedText(_subtitle, Qt::ElideRight, width() - 24));
		}

		// Slider track
		const QRect track = trackRect();
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(52, 48, 59));
		p.drawRoundedRect(track, 3, 3);

		const double ratio = double(_current - _min) / double(_max - _min);
		const int fillW = int(std::round(ratio * track.width()));
		if (fillW > 0) {
			p.setBrush(accent);
			p.drawRoundedRect(QRect(track.left(), track.top(), fillW, track.height()), 3, 3);
		}

		const int cx = track.left() + fillW;
		const int cy = track.center().y();
		p.setBrush(accent);
		p.drawEllipse(QPoint(cx, cy), 8, 8);
		p.setBrush(Qt::white);
		p.drawEllipse(QPoint(cx, cy), 4, 4);
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton) {
			_dragging = true;
			applyPos(e->pos().x());
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (_dragging) {
			applyPos(e->pos().x());
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton && _dragging) {
			_dragging = false;
			applyPos(e->pos().x());
			update();
		}
	}

private:
	QRect trackRect() const {
		return QRect(14, height() - 18, std::max(20, width() - 28), 6);
	}

	void applyPos(int x) {
		const auto tr = trackRect();
		const double ratio = std::clamp(double(x - tr.left()) / double(tr.width()), 0.0, 1.0);
		const int val = _min + int(std::round(ratio * (_max - _min)));
		if (_current != val) {
			_current = val;
			_setter(val);
			update();
		}
	}

	QString _title;
	QString _subtitle;
	int _min = 0;
	int _max = 100;
	int _current = 0;
	bool _dragging = false;
	Fn<int()> _getter;
	Fn<QString(int)> _formatter;
	Fn<void(int)> _setter;
};

// 3. Custom CSGUI Choice / Selector Card
class GuiChoiceTile final : public Ui::RpWidget {
public:
	GuiChoiceTile(
		QWidget *parent,
		QString title,
		QString subtitle,
		Fn<std::vector<QString>()> optionsFn,
		Fn<int()> getter,
		Fn<void(int)> setter,
		rpl::producer<> changes)
	: RpWidget(parent)
	, _title(std::move(title))
	, _subtitle(std::move(subtitle))
	, _optionsFn(std::move(optionsFn))
	, _getter(std::move(getter))
	, _setter(std::move(setter)) {
		setCursor(Qt::PointingHandCursor);
		std::move(changes) | rpl::on_next([=] { update(); }, lifetime());
	}

	[[nodiscard]] QString searchBlob() const {
		return _title + ' ' + _subtitle;
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = AccentColor();
		const auto ink = InkColor();

		p.setBrush(underMouse() ? QColor(36, 34, 42) : QColor(28, 27, 32));
		p.setPen(underMouse() ? QColor(82, 54, 64) : QColor(49, 46, 54));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);

		const auto opts = _optionsFn();
		const int idx = std::clamp(_getter(), 0, std::max(0, int(opts.size()) - 1));
		const QString currentLabel = opts.empty() ? QString() : (opts[idx] + u"  ▾"_q);

		auto f = font();
		f.setWeight(QFont::DemiBold);
		p.setFont(f);

		const int pillW = std::clamp(p.fontMetrics().horizontalAdvance(currentLabel) + 20, 68, std::max(80, width() / 2));
		const QRect pillRect(width() - pillW - 10, (height() - 26) / 2, pillW, 26);
		p.setPen(QPen(QColor(96, 56, 66), 1));
		p.setBrush(QColor(58, 33, 41));
		p.drawRoundedRect(pillRect, 7, 7);
		p.setPen(accent.lighter(122));
		p.drawText(
			pillRect.adjusted(8, 0, -8, 0),
			Qt::AlignCenter,
			p.fontMetrics().elidedText(currentLabel, Qt::ElideRight, pillRect.width() - 16));

		const int textW = std::max(40, pillRect.left() - 20);
		p.setPen(ink);
		if (_subtitle.isEmpty()) {
			p.drawText(
				QRect(12, 0, textW, height()),
				Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
				_title);
		} else {
			p.drawText(
				QRect(12, 9, textW, 22),
				Qt::AlignVCenter | Qt::AlignLeft,
				p.fontMetrics().elidedText(_title, Qt::ElideRight, textW));
			p.setFont(font());
			p.setPen(QColor(172, 163, 172));
			p.drawText(
				QRect(12, 33, textW, height() - 40),
				Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
				_subtitle);
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		if (e->button() != Qt::LeftButton || !rect().contains(e->pos())) return;
		const auto opts = _optionsFn();
		if (opts.empty()) return;
		const int cur = std::clamp(_getter(), 0, int(opts.size()) - 1);
		auto *popup = new GuiPopupMenu(window(), opts, cur, [=](int picked) {
			_setter(picked);
			update();
		});
		const QPoint globalBottomRight = mapToGlobal(QPoint(width() - 10, height() - 6));
		popup->move(globalBottomRight.x() - popup->width(), globalBottomRight.y());
		popup->show();
	}

private:
	QString _title;
	QString _subtitle;
	Fn<std::vector<QString>()> _optionsFn;
	Fn<int()> _getter;
	Fn<void(int)> _setter;
};

// 4. Custom CSGUI Material You Color Card
class GuiColorTile final : public Ui::RpWidget {
public:
	GuiColorTile(
		QWidget *parent,
		QString title,
		Fn<QColor()> getter,
		Fn<void()> onClick,
		rpl::producer<> changes)
	: RpWidget(parent)
	, _title(std::move(title))
	, _getter(std::move(getter))
	, _onClick(std::move(onClick)) {
		setCursor(Qt::PointingHandCursor);
		std::move(changes) | rpl::on_next([=] { update(); }, lifetime());
	}

	[[nodiscard]] QString searchBlob() const { return _title; }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		p.setBrush(underMouse() ? QColor(36, 34, 42) : QColor(28, 27, 32));
		p.setPen(underMouse() ? AccentColor() : QColor(49, 46, 54));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);

		const QColor c = _getter();
		const QString hex = c.isValid() ? c.name().toUpper() : tr::lng_look_value_theme(tr::now);

		// Color circle
		const QRect sw(width() - 34, (height() - 22) / 2, 22, 22);
		p.setPen(QPen(QColor(255, 255, 255, 70), 1));
		p.setBrush(c.isValid() ? c : QColor(74, 70, 82));
		p.drawEllipse(sw);

		auto f = font();
		f.setWeight(QFont::DemiBold);
		p.setFont(f);
		p.setPen(InkColor());
		p.drawText(QRect(12, 10, width() - 56, 22), Qt::AlignVCenter | Qt::AlignLeft, _title);

		p.setFont(font());
		p.setPen(QColor(172, 163, 172));
		p.drawText(QRect(12, 33, width() - 56, 20), Qt::AlignVCenter | Qt::AlignLeft, hex);
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && _onClick) {
			_onClick();
		}
	}

private:
	QString _title;
	Fn<QColor()> _getter;
	Fn<void()> _onClick;
};

// 5. Custom CSGUI Action / Post / Journal Card
class GuiActionTile final : public Ui::RpWidget {
public:
	GuiActionTile(
		QWidget *parent,
		QString title,
		QString subtitle,
		QString badge,
		Fn<void()> onClick)
	: RpWidget(parent)
	, _title(std::move(title))
	, _subtitle(std::move(subtitle))
	, _badge(std::move(badge))
	, _onClick(std::move(onClick)) {
		if (_onClick) {
			setCursor(Qt::PointingHandCursor);
		}
	}

	[[nodiscard]] QString searchBlob() const {
		return _title + ' ' + _subtitle + ' ' + _badge;
	}

	void setBadge(QString badge) {
		_badge = std::move(badge);
		update();
	}

	void setSubtitle(QString sub) {
		_subtitle = std::move(sub);
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = AccentColor();
		p.setBrush((_onClick && underMouse()) ? QColor(38, 34, 44) : QColor(26, 25, 30));
		p.setPen((_onClick && underMouse()) ? accent : QColor(49, 46, 54));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 9, 9);

		auto f = font();
		f.setWeight(QFont::DemiBold);
		p.setFont(f);

		int rightUsed = 14;
		if (!_badge.isEmpty()) {
			const int bw = std::min(180, p.fontMetrics().horizontalAdvance(_badge) + 18);
			const QRect br(width() - bw - 10, 9, bw, 22);
			p.setPen(QPen(QColor(92, 54, 64), 1));
			p.setBrush(QColor(54, 31, 39));
			p.drawRoundedRect(br, 6, 6);
			p.setPen(accent.lighter(120));
			p.drawText(
				br.adjusted(6, 0, -6, 0),
				Qt::AlignCenter,
				p.fontMetrics().elidedText(_badge, Qt::ElideRight, br.width() - 12));
			rightUsed = bw + 20;
		}

		p.setPen(InkColor());
		p.drawText(
			QRect(12, 9, std::max(40, width() - rightUsed - 12), 22),
			Qt::AlignVCenter | Qt::AlignLeft,
			p.fontMetrics().elidedText(_title, Qt::ElideRight, std::max(40, width() - rightUsed - 12)));

		if (!_subtitle.isEmpty()) {
			p.setFont(font());
			p.setPen(QColor(178, 170, 179));
			p.drawText(
				QRect(12, 33, width() - 24, height() - 41),
				Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
				_subtitle);
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton && rect().contains(e->pos()) && _onClick) {
			_onClick();
		}
	}

private:
	QString _title;
	QString _subtitle;
	QString _badge;
	Fn<void()> _onClick;
};

// Live Chat Preview Card inside CSGUI Customization
class GuiPreviewTile final : public Ui::RpWidget {
public:
	explicit GuiPreviewTile(QWidget *parent) : RpWidget(parent) {
		Appearance::Changes() | rpl::on_next([=] { update(); }, lifetime());
	}

	[[nodiscard]] QString searchBlob() const {
		return tr::lng_look_preview(tr::now);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto color = [](Appearance::Tint tint, QColor fallback) {
			auto c = Appearance::Color(tint);
			if (!c.isValid()) c = fallback;
			if (Appearance::Get(Appearance::Key::Mono)) {
				const auto v = qGray(c.rgb());
				c.setRgb(v, v, v);
			}
			return c;
		};
		p.setPen(QColor(54, 48, 60));
		p.setBrush(color(Appearance::Tint::Wallpaper, QColor(18, 18, 23)));
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);

		const bool isMd3 = Appearance::IsMaterialV2();
		const auto drawBubble = [&](bool out, const QString &msg, int y) {
			const int w = std::min(width() - 60, fontMetrics().horizontalAdvance(msg) + 86);
			const QRect b(out ? (width() - w - 16) : 16, y, w, 34);
			const int shape = Appearance::Get(out ? Appearance::Key::OutCorners : Appearance::Key::InCorners);
			const int rad = shape == 2 ? 2 : shape == 1 ? 5 : (isMd3 ? 16 : 11);
			p.setPen(Qt::NoPen);
			p.setBrush(color(
				out ? Appearance::Tint::Outgoing : Appearance::Tint::Incoming,
				out ? QColor(74, 36, 46) : QColor(34, 33, 41)));
			p.drawRoundedRect(b, rad, rad);
			p.setPen(color(Appearance::Tint::Ink, QColor(242, 238, 242)));
			p.drawText(b.adjusted(12, 0, -54, 0), Qt::AlignVCenter | Qt::AlignLeft, msg);
			p.setPen(QColor(175, 166, 176));
			p.drawText(b.adjusted(0, 0, -10, -4), Qt::AlignBottom | Qt::AlignRight, out ? u"12:42 ✓✓"_q : u"12:41"_q);
		};
		drawBubble(false, tr::lng_look_sample(tr::now), 12);
		drawBubble(true, tr::lng_look_answer(tr::now), 52);
	}
};

struct AppearanceChoiceSpec {
	Appearance::Key key;
	const tr::phrase<> *title;
	std::vector<const tr::phrase<>*> values;
};

const std::vector<AppearanceChoiceSpec> &AllAppearanceChoices() {
	static const auto rows = std::vector<AppearanceChoiceSpec>{
		{ Appearance::Key::Meta, &tr::lng_look_meta,
			{ &tr::lng_look_value_default, &tr::lng_look_value_compact_time, &tr::lng_look_value_time_only, &tr::lng_look_own_line } },
		{ Appearance::Key::Menu, &tr::lng_look_menu,
			{ &tr::lng_look_value_full, &tr::lng_look_value_move_translate_to_more, &tr::lng_look_reply_last } },
		{ Appearance::Key::Density, &tr::lng_look_density,
			{ &tr::lng_look_value_default, &tr::lng_look_value_compact, &tr::lng_look_value_spacious } },
		{ Appearance::Key::Journal, &tr::lng_look_journal,
			{ &tr::lng_look_value_label, &tr::lng_look_value_stroke, &tr::lng_look_value_hidden } },
		{ Appearance::Key::InCorners, &tr::lng_look_incorners,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_small, &tr::lng_look_value_square } },
		{ Appearance::Key::OutCorners, &tr::lng_look_outcorners,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_small, &tr::lng_look_value_square } },
		{ Appearance::Key::Tails, &tr::lng_look_tails,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Appearance::Key::Grouping, &tr::lng_look_grouping,
			{ &tr::lng_look_value_group, &tr::lng_look_value_separate, &tr::lng_look_line_group } },
		{ Appearance::Key::Quote, &tr::lng_look_quote,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_rule, &tr::lng_look_value_no_background, &tr::lng_look_value_frame } },
		{ Appearance::Key::QuoteLines, &tr::lng_look_quotelines,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_1, &tr::lng_look_value_2, &tr::lng_look_value_3 } },
		{ Appearance::Key::Mono, &tr::lng_look_mono,
			{ &tr::lng_look_value_off, &tr::lng_look_value_on } },
		{ Appearance::Key::Grain, &tr::lng_look_grain,
			{ &tr::lng_look_value_off, &tr::lng_look_value_light, &tr::lng_look_value_strong } },
		{ Appearance::Key::Unread, &tr::lng_look_unread,
			{ &tr::lng_look_value_count, &tr::lng_look_value_dot, &tr::lng_look_value_mentions_only } },
		{ Appearance::Key::Pinned, &tr::lng_look_pinned,
			{ &tr::lng_look_value_pin, &tr::lng_look_value_divider, &tr::lng_look_value_none } },
		{ Appearance::Key::Composer, &tr::lng_look_composer,
			{ &tr::lng_look_value_default, &tr::lng_look_value_hide_emoji_button } },
		{ Appearance::Key::Voice, &tr::lng_look_voice,
			{ &tr::lng_look_value_waveform, &tr::lng_look_value_progress_line, &tr::lng_look_value_duration_only } },
		{ Appearance::Key::Cursor, &tr::lng_look_cursor,
			{ &tr::lng_look_value_off, &tr::lng_look_value_on } },
		{ Appearance::Key::Dates, &tr::lng_look_dates,
			{ &tr::lng_look_value_center, &tr::lng_look_value_left, &tr::lng_look_value_while_scrolling } },
		{ Appearance::Key::Ghost, &tr::lng_look_ghost,
			{ &tr::lng_look_value_off, &tr::lng_look_value_in_composer } },
		{ Appearance::Key::Tray, &tr::lng_look_tray,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_full, &tr::lng_look_value_outline, &tr::lng_look_silhouette } },
		{ Appearance::Key::TrayCount, &tr::lng_look_traycount,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_dot, &tr::lng_look_value_none } },
		{ Appearance::Key::Reactions, &tr::lng_look_reactions,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Appearance::Key::Panels, &tr::lng_look_panels,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Appearance::Key::Transitions, &tr::lng_look_transitions,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Appearance::Key::AvatarSize, &tr::lng_look_avatarsize,
			{ &tr::lng_look_avatarsize_0, &tr::lng_look_avatarsize_1, &tr::lng_look_avatarsize_2 } },
		{ Appearance::Key::ListLines, &tr::lng_look_listlines,
			{ &tr::lng_look_listlines_0, &tr::lng_look_listlines_1, &tr::lng_look_listlines_2 } },
		{ Appearance::Key::FolderBar, &tr::lng_look_folderbar,
			{ &tr::lng_look_folderbar_0, &tr::lng_look_folderbar_1 } },
		{ Appearance::Key::BadgeMotion, &tr::lng_look_badgemotion,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_pause } },
	};
	return rows;
}

// Standalone Frameless Draggable CSGUI Window
class YandereWindow final : public Ui::RpWidget {
public:
	YandereWindow(
		not_null<Window::SessionController*> controller,
		Page initialPage)
	: RpWidget(nullptr)
	, _controller(base::make_weak(controller.get()))
	, _active(initialPage) {
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setAttribute(Qt::WA_DeleteOnClose);
		setWindowTitle(u"YandereGram CSGUI"_q);
		setMinimumSize(760, 460);
		resize(960, 590);

		_favorites = Core::App().settings().readPref<QByteArray>(kFavorites).split('\n');
		_favorites.removeAll(QByteArray());

		setupChrome();
		populateAllPages();
		applyStyleSheet();
		Appearance::Changes() | rpl::on_next([=] {
			applyStyleSheet();
			update();
		}, lifetime());

		selectPage(_active);

		// Center over main window
		if (auto *mainWidget = controller->window().widget().get()) {
			const auto center = mainWidget->frameGeometry().center();
			move(center.x() - width() / 2, center.y() - height() / 2);
		} else if (const auto *scr = QGuiApplication::primaryScreen()) {
			const auto center = scr->availableGeometry().center();
			move(center.x() - width() / 2, center.y() - height() / 2);
		}
	}

	void selectPage(Page page) {
		_active = page;
		if (_search) _search->clear();
		if (page == Page::Feed) {
			rebuildDynamicFeed();
		} else if (page == Page::Journal) {
			rebuildDynamicJournal();
		}
		relayoutContent();
		if (_scroll) _scroll->verticalScrollBar()->setValue(0);
		update();
	}

	void showBanner(const QString &msg) {
		_bannerText = msg;
		_bannerLabel->setText(msg);
		_bannerLabel->show();
		_bannerLabel->raise();
		layoutChrome();
		_bannerTimer->start(2600);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto bg = PanelBgColor();
		const auto accent = AccentColor();

		// Outer shell
		p.setPen(QPen(QColor(68, 48, 58), 1.2));
		p.setBrush(bg);
		p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);

		// Sidebar background
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(20, 18, 24));
		p.drawRoundedRect(QRect(2, 50, kSidebarW - 2, height() - 52), 10, 10);

		// Header divider & sidebar divider
		p.setPen(QPen(QColor(46, 40, 52), 1));
		p.drawLine(1, 50, width() - 2, 50);
		p.drawLine(kSidebarW, 50, kSidebarW, height() - 2);

		// Accent top glow line
		p.setPen(QPen(accent, 2));
		p.drawLine(18, 1, 148, 1);

		// Resize grip dots in bottom-right corner
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(110, 98, 114));
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j <= i; ++j) {
				p.drawEllipse(QPoint(width() - 7 - j * 4, height() - 7 - (i - j) * 4), 1, 1);
			}
		}
	}

	void resizeEvent(QResizeEvent *) override {
		layoutChrome();
		relayoutContent();
	}

	void keyPressEvent(QKeyEvent *e) override {
		if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Insert) {
			close();
			return;
		}
		RpWidget::keyPressEvent(e);
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton) {
			if (e->pos().x() >= width() - 18 && e->pos().y() >= height() - 18) {
				_resizing = true;
				_dragStartGlobal = e->globalPos();
				_startSize = size();
				return;
			}
			if (e->pos().y() <= 50 || e->pos().x() <= kSidebarW) {
				_dragging = true;
				_dragOffset = e->globalPos() - frameGeometry().topLeft();
				return;
			}
		}
		RpWidget::mousePressEvent(e);
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (_resizing) {
			const QPoint delta = e->globalPos() - _dragStartGlobal;
			resize(
				std::max(minimumWidth(), _startSize.width() + delta.x()),
				std::max(minimumHeight(), _startSize.height() + delta.y()));
			return;
		}
		if (_dragging) {
			move(e->globalPos() - _dragOffset);
			return;
		}
		RpWidget::mouseMoveEvent(e);
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		_dragging = false;
		_resizing = false;
		RpWidget::mouseReleaseEvent(e);
	}

private:
	static constexpr int kSidebarW = 198;

	struct TileItem {
		Page page = Page::Interface;
		QByteArray favId;
		Ui::RpWidget *widget = nullptr;
		Fn<QString()> searchFn;
		int preferredHeight = 74;
		bool fullWidth = false;
		bool dynamicItem = false;
	};

	void setupChrome() {
		_logo = new QLabel(this);
		_logo->setScaledContents(true);
		_logo->setPixmap(QPixmap(u":/gui/art/logo_256_no_margin.png"_q));
		_logo->setAttribute(Qt::WA_TransparentForMouseEvents);

		_brand = new QLabel(u"YandereGram"_q, this);
		_brand->setObjectName(u"ygBrand"_q);
		_brand->setAttribute(Qt::WA_TransparentForMouseEvents);

		_hotkeyTag = new QLabel(u"Insert"_q, this);
		_hotkeyTag->setObjectName(u"ygTag"_q);
		_hotkeyTag->setAlignment(Qt::AlignCenter);
		_hotkeyTag->setAttribute(Qt::WA_TransparentForMouseEvents);

		_search = new QLineEdit(this);
		_search->setPlaceholderText(tr::lng_yg_panel_search(tr::now));
		_search->setClearButtonEnabled(true);
		connect(_search, &QLineEdit::textChanged, this, [=] { relayoutContent(); });

		_exportBtn = new QPushButton(tr::lng_export_start(tr::now), this);
		_exportBtn->setCursor(Qt::PointingHandCursor);
		connect(_exportBtn, &QPushButton::clicked, this, [=] { exportSettings(); });

		_closeBtn = new QPushButton(u"✕"_q, this);
		_closeBtn->setObjectName(u"ygClose"_q);
		_closeBtn->setCursor(Qt::PointingHandCursor);
		connect(_closeBtn, &QPushButton::clicked, this, [=] { close(); });

		// Sidebar scroll + buttons
		_navScroll = new QScrollArea(this);
		_navScroll->setFrameShape(QFrame::NoFrame);
		_navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		_navScroll->setWidgetResizable(false);
		_navContainer = new QWidget(_navScroll);
		_navScroll->setWidget(_navContainer);

		for (int i = 0; i < int(Page::Count); ++i) {
			const auto pg = Page(i);
			const auto name = Title(pg);
			auto *b = new QPushButton(name, _navContainer);
			b->setObjectName(u"ygNav"_q);
			b->setCheckable(true);
			b->setCursor(Qt::PointingHandCursor);
			b->setToolTip(name);
			connect(b, &QPushButton::clicked, this, [=] { selectPage(pg); });
			_navBtns.push_back(b);
		}

		// Heading + content scroll area
		_heading = new QLabel(this);
		_heading->setObjectName(u"ygHeading"_q);

		_scroll = new QScrollArea(this);
		_scroll->setFrameShape(QFrame::NoFrame);
		_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		_scroll->setWidgetResizable(false);
		_canvas = new QWidget(_scroll);
		_canvas->setObjectName(u"ygCanvas"_q);
		_scroll->setWidget(_canvas);

		_empty = new QLabel(tr::lng_yg_panel_empty(tr::now), _canvas);
		_empty->setWordWrap(true);
		_empty->setObjectName(u"ygEmpty"_q);

		_bannerLabel = new QLabel(this);
		_bannerLabel->setObjectName(u"ygBanner"_q);
		_bannerLabel->setAlignment(Qt::AlignCenter);
		_bannerLabel->hide();

		_bannerTimer = new QTimer(this);
		_bannerTimer->setSingleShot(true);
		connect(_bannerTimer, &QTimer::timeout, this, [=] { _bannerLabel->hide(); });
	}

	void applyStyleSheet() {
		const auto accent = AccentColor().name();
		const auto ink = InkColor().name();
		setStyleSheet(uR"(
			QLabel { background: transparent; color: %1; }
			QLabel#ygBrand { font-size: 15px; font-weight: 700; color: #ffffff; }
			QLabel#ygTag { background: #2b1d24; border: 1px solid #5b3542; border-radius: 5px; color: %2; font-size: 11px; font-weight: 600; }
			QLabel#ygHeading { font-size: 14px; font-weight: 700; color: %1; }
			QLabel#ygEmpty { color: #9c929e; font-size: 13px; }
			QLabel#ygBanner { background: #3f2029; border: 1px solid %2; border-radius: 8px; color: #ffffff; font-weight: 600; padding: 6px 14px; }
			QLineEdit { background: #1d1b22; border: 1px solid #383240; border-radius: 7px; color: #f4eff4; padding: 4px 10px; selection-background-color: %2; }
			QLineEdit:focus { border: 1px solid %2; }
			QPushButton { background: #221f28; border: 1px solid #3b3444; border-radius: 7px; color: #ece6ec; padding: 4px 10px; font-weight: 600; }
			QPushButton:hover { background: #342936; border-color: #613d4b; }
			QPushButton#ygClose { background: #2a1d24; border: 1px solid #52323e; color: #ff8a93; font-size: 13px; padding: 0px; }
			QPushButton#ygClose:hover { background: %2; color: #ffffff; }
			QPushButton#ygNav { background: transparent; border: 0px; border-radius: 7px; color: #cfc6d0; text-align: left; padding: 6px 12px; font-weight: 500; }
			QPushButton#ygNav:hover { background: #292330; color: #ffffff; }
			QPushButton#ygNav:checked { background: #41232c; border-left: 3px solid %2; color: #ffffff; font-weight: 700; }
			QScrollArea, QWidget#ygCanvas { background: transparent; }
			QScrollBar:vertical { background: #16141a; width: 8px; margin: 2px; border-radius: 4px; }
			QScrollBar::handle:vertical { background: #4a3542; min-height: 28px; border-radius: 4px; }
			QScrollBar::handle:vertical:hover { background: %2; }
			QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
		)"_q.arg(ink, accent));
	}

	void layoutChrome() {
		_logo->setGeometry(14, 11, 28, 28);
		_brand->setGeometry(48, 11, 108, 28);
		_hotkeyTag->setGeometry(154, 15, 44, 20);

		const int searchLeft = kSidebarW + 14;
		const int closeX = width() - 40;
		const int exportW = 92;
		_closeBtn->setGeometry(closeX, 11, 28, 28);
		_exportBtn->setGeometry(closeX - exportW - 8, 11, exportW, 28);
		_search->setGeometry(searchLeft, 11, std::max(140, _exportBtn->x() - searchLeft - 10), 28);

		_navScroll->setGeometry(6, 56, kSidebarW - 10, height() - 64);
		const int navBtnH = 33;
		_navContainer->setGeometry(0, 0, _navScroll->viewport()->width(), int(_navBtns.size()) * (navBtnH + 3) + 8);
		for (int i = 0; i < int(_navBtns.size()); ++i) {
			_navBtns[i]->setGeometry(4, 4 + i * (navBtnH + 3), _navContainer->width() - 8, navBtnH);
		}

		_heading->setGeometry(kSidebarW + 16, 58, width() - kSidebarW - 32, 26);
		_scroll->setGeometry(kSidebarW + 8, 88, width() - kSidebarW - 14, height() - 96);

		if (_bannerLabel->isVisible()) {
			const int bw = std::clamp(_bannerLabel->fontMetrics().horizontalAdvance(_bannerText) + 48, 180, width() - kSidebarW - 40);
			_bannerLabel->setGeometry(kSidebarW + (width() - kSidebarW - bw) / 2, height() - 48, bw, 34);
		}
	}

	void toggleFavorite(const QByteArray &id, bool on) {
		_favorites.removeAll(id);
		if (on) _favorites.push_back(id);
		Core::App().settings().writePref<QByteArray>(
			kFavorites,
			QByteArrayList(_favorites).join('\n'));
		Core::App().saveSettingsDelayed();
		if (_active == Page::Favorites) {
			relayoutContent();
		}
	}

	void addEnhancedToggle(Page page, EnhancedSettings::Key<bool> key, QString customDesc = {}) {
		const auto id = EnhancedSettings::ControlId(key.id).toUtf8();
		const auto title = EnhancedSettings::OptionTitle(key.id);
		const auto desc = customDesc.isEmpty() ? OptionDesc(key.id) : customDesc;
		auto *tile = new GuiToggleTile(
			_canvas,
			id,
			title,
			desc,
			[=] { return EnhancedSettings::Get(key); },
			[=](bool val) {
				if (const auto ctrl = _controller.get()) {
					EnhancedSettings::ApplyOption(ctrl, key, val);
				} else {
					EnhancedSettings::Set(key, val);
				}
			},
			EnhancedSettings::Changes(key),
			_favorites.contains(id),
			[=](bool on) { toggleFavorite(id, on); });
		_items.push_back({
			.page = page,
			.favId = id,
			.widget = tile,
			.searchFn = [=] { return tile->searchBlob(); },
			.preferredHeight = 76,
		});
	}

	void addCustomTile(
			Page page,
			Ui::RpWidget *w,
			Fn<QString()> searchFn,
			int height = 74,
			bool fullWidth = false,
			bool dynamicItem = false) {
		_items.push_back({
			.page = page,
			.widget = w,
			.searchFn = std::move(searchFn),
			.preferredHeight = height,
			.fullWidth = fullWidth,
			.dynamicItem = dynamicItem,
		});
	}

	void runOnController(Fn<void(not_null<Window::SessionController*>)> fn) {
		if (const auto ctrl = _controller.get()) {
			ctrl->window().widget()->activateWindow();
			ctrl->window().widget()->raise();
			fn(ctrl);
		}
	}

	void populateAllPages() {
		using namespace EnhancedSettings;

		// --- 1. INTERFACE ---
		addEnhancedToggle(Page::Interface, Option::ExactCounts);
		addEnhancedToggle(Page::Interface, Option::RelativeOnline);
		addEnhancedToggle(Page::Interface, Option::ExtendedStickers);
		addEnhancedToggle(Page::Interface, Option::DisablePremiumAnimation);
		addEnhancedToggle(Page::Interface, Option::ShowGroupSenderAvatar);
		addEnhancedToggle(Page::Interface, Option::ShowGroupSenderOnlineStatus);
		addEnhancedToggle(Page::Interface, Option::ShowSeconds);
		{
			auto *slider = new GuiSliderTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::StickerHeight.id),
				OptionDesc(Option::StickerHeight.id),
				64,
				256,
				[] {
					const int v = EnhancedSettings::Get(Option::StickerHeight);
					return v ? v : 256;
				},
				[](int v) { return QString::number(v) + u" px"_q; },
				[=](int v) {
					if (const auto ctrl = _controller.get()) {
						EnhancedSettings::ApplyOption(ctrl, Option::StickerHeight, v);
					} else {
						EnhancedSettings::Set(Option::StickerHeight, v);
					}
				},
				EnhancedSettings::Changes(Option::StickerHeight));
			addCustomTile(Page::Interface, slider, [=] { return slider->searchBlob(); }, 76);
		}
		addEnhancedToggle(Page::Interface, Option::ShowEmojiButtonAsText, tr::lng_show_emoji_button_as_text_desc(tr::now));
		addEnhancedToggle(Page::Interface, Option::MoreRightActionComments);
		addEnhancedToggle(Page::Interface, Option::ShowScheduledButton);
		addEnhancedToggle(Page::Interface, Option::HideAllChats);
		addEnhancedToggle(Page::Interface, Option::ReplaceEditButton);
		addEnhancedToggle(Page::Interface, Option::HideCounter);
		addEnhancedToggle(Page::Interface, Option::HideStories);
		addEnhancedToggle(Page::Interface, Option::HideStarRatings);
		addEnhancedToggle(Page::Interface, Option::HideGramBalance);
		addEnhancedToggle(Page::Interface, Option::ForceMobile);
		addEnhancedToggle(Page::Interface, Option::ShowRecentForwardTargets);
		addEnhancedToggle(Page::Interface, Option::HideDeleteForOthersCheckbox);

		// --- 2. MESSAGES & COMPOSER CONVENIENCES ---
		addEnhancedToggle(Page::Messages, Option::InlineMath);
		addEnhancedToggle(Page::Messages, Option::ComposerCharCounter);
		addEnhancedToggle(Page::Messages, Option::FilterZalgo);
		addEnhancedToggle(Page::Messages, Option::QuickContextActions);
		{
			auto *dblChoice = new GuiChoiceTile(
				_canvas,
				tr::lng_yg_double_click(tr::now),
				tr::lng_yg_double_click_desc(tr::now),
				[] {
					return std::vector<QString>{
						tr::lng_yg_dbl_default(tr::now),
						tr::lng_yg_dbl_reply(tr::now),
						tr::lng_yg_dbl_react(tr::now),
						tr::lng_yg_dbl_copy(tr::now),
						tr::lng_context_edit_msg(tr::now),
					};
				},
				[] { return EnhancedSettings::Get(Option::DoubleClickAction); },
				[=](int idx) {
					if (const auto ctrl = _controller.get()) {
						EnhancedSettings::ApplyOption(ctrl, Option::DoubleClickAction, idx);
					} else {
						EnhancedSettings::Set(Option::DoubleClickAction, idx);
					}
				},
				EnhancedSettings::Changes(Option::DoubleClickAction));
			addCustomTile(Page::Messages, dblChoice, [=] { return dblChoice->searchBlob(); }, 76);
		}
		addEnhancedToggle(Page::Messages, Option::SaveDeleted);
		addEnhancedToggle(Page::Messages, Option::SaveEdits);
		addEnhancedToggle(Page::Messages, Option::ShowMessagesId);
		addEnhancedToggle(Page::Messages, Option::ShowMessageForwardCount);
		addEnhancedToggle(Page::Messages, Option::ForceShowWebPagePreview);
		addEnhancedToggle(Page::Messages, Option::DisableAutoFetchWebPagePreview);
		addEnhancedToggle(Page::Messages, Option::RemoveMediaSpoiler);
		addEnhancedToggle(Page::Messages, Option::ShowMediaMetadata);
		addEnhancedToggle(Page::Messages, Option::ShowGifMediaControls);
		addEnhancedToggle(Page::Messages, Option::HideBlockedMessages);
		{
			auto *richCard = new GuiActionTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::RichMessagePreviewBlocksLimit.id),
				OptionDesc(Option::RichMessagePreviewBlocksLimit.id),
				u"›"_q,
				[=] { runOnController([](auto ctrl) { ctrl->show(Box<RichMessagePreviewBlocksBox>()); }); });
			addCustomTile(Page::Messages, richCard, [=] { return richCard->searchBlob(); }, 76);
		}

		// --- 3. GRANULAR GHOST MODE ---
		addEnhancedToggle(Page::Ghost, Option::GhostMode, tr::lng_yg_ghost_about(tr::now));
		addEnhancedToggle(Page::Ghost, Option::GhostNoRead);
		addEnhancedToggle(Page::Ghost, Option::GhostNoStories);
		addEnhancedToggle(Page::Ghost, Option::GhostNoOnline);
		addEnhancedToggle(Page::Ghost, Option::GhostNoTyping);
		addEnhancedToggle(Page::Ghost, Option::GhostOfflineAfterSend);
		addEnhancedToggle(Page::Ghost, Option::GhostReadOnSend);

		// --- 4. STREAMER MODE ---
		addEnhancedToggle(Page::Streamer, Option::StreamerMode);
		addEnhancedToggle(Page::Streamer, Option::StreamerCaptureProtection);
		addEnhancedToggle(Page::Streamer, Option::StreamerHideSensitive);
		addEnhancedToggle(Page::Streamer, Option::StreamerHidePrivateChats);
		addEnhancedToggle(Page::Streamer, Option::StreamerMuteNotifications);

		// --- 5. BEHAVIOR ---
		addEnhancedToggle(Page::Behavior, Option::OnlineSpam);
		addEnhancedToggle(Page::Behavior, Option::ShowSimilarOnJoined);
		addEnhancedToggle(Page::Behavior, Option::SendCommentAfterForwarding);
		addEnhancedToggle(Page::Behavior, Option::DisableCloudDraftSync);
		addEnhancedToggle(Page::Behavior, Option::DisableSyncDraftToCloud);
		addEnhancedToggle(Page::Behavior, Option::DisableLinkWarning);
		addEnhancedToggle(Page::Behavior, Option::DisableGlobalSearch);
		addEnhancedToggle(Page::Behavior, Option::SkipToNext, tr::lng_settings_skip_message_desc(tr::now));
		addEnhancedToggle(Page::Behavior, Option::CommunityChatClick);
		addEnhancedToggle(Page::Behavior, Option::RepeaterReplyToOriginal);
		{
			auto *ctxCard = new GuiActionTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::ExtraContextMenuOptions.id),
				{},
				u"›"_q,
				[=] { runOnController([](auto ctrl) { ctrl->show(Box<ExtraContextMenuBox>()); }); });
			addCustomTile(Page::Behavior, ctxCard, [=] { return ctxCard->searchBlob(); }, 72);

			auto *rulesCard = new GuiActionTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::LinkPreviewRules.id),
				{},
				u"›"_q,
				[=] { runOnController([](auto ctrl) { ctrl->show(Box(LinkPreviewRulesBox)); }); });
			addCustomTile(Page::Behavior, rulesCard, [=] { return rulesCard->searchBlob(); }, 72);
		}

		// --- 6. NETWORK & WS PROXY ---
		{
			auto *wsStatus = new GuiActionTile(
				_canvas,
				tr::lng_yg_ws_proxy(tr::now),
				Core::WsProxy::StatusText(),
				u"WSS / 443"_q,
				nullptr);
			Core::WsProxy::Changes() | rpl::on_next([=] {
				wsStatus->setSubtitle(Core::WsProxy::StatusText());
			}, wsStatus->lifetime());
			addCustomTile(Page::Network, wsStatus, [=] { return wsStatus->searchBlob(); }, 74, true);
		}
		addEnhancedToggle(Page::Network, Option::WsProxyEnabled);
		addEnhancedToggle(Page::Network, Option::DownloadBoost);
		addEnhancedToggle(Page::Network, Option::UploadBoost);

		// --- 7. CHANNELS FEED (static header controls; posts populated dynamically) ---
		addEnhancedToggle(Page::Feed, Option::FeedHideArchived);
		{
			auto *refreshFeed = new GuiActionTile(
				_canvas,
				tr::lng_yg_feed_refresh(tr::now),
				tr::lng_yg_feed_about(tr::now),
				u"↻"_q,
				[=] {
					fetchMissingChannelPosts();
					rebuildDynamicFeed();
					relayoutContent();
				});
			addCustomTile(Page::Feed, refreshFeed, [=] { return refreshFeed->searchBlob(); }, 76);
		}

		// --- 8. MESSAGE JOURNAL (static header controls; entries populated dynamically) ---
		addEnhancedToggle(Page::Journal, Option::SaveDeleted);
		addEnhancedToggle(Page::Journal, Option::SaveEdits);
		{
			auto *clearJournal = new GuiActionTile(
				_canvas,
				tr::lng_yg_journal_clear(tr::now),
				tr::lng_yg_journal_about(tr::now),
				u"✕"_q,
				[=] {
					if (const auto ctrl = _controller.get()) {
						ctrl->session().settings().clearJournal();
						ctrl->session().saveSettings();
					}
					rebuildDynamicJournal();
					relayoutContent();
				});
			addCustomTile(Page::Journal, clearJournal, [=] { return clearJournal->searchBlob(); }, 76, true);
		}

		// --- 9. CUSTOMIZATION (APPEARANCE + CLIENT STYLE + BADGE ROLE + MATERIAL YOU + AVATAR RADIUS) ---
		{
			auto *preview = new GuiPreviewTile(_canvas);
			addCustomTile(Page::Appearance, preview, [=] { return preview->searchBlob(); }, 98, true);

			// Client Visual Style: Material v1 / Material v2 / Liquid Glass ("В разработке")
			auto *styleTile = new GuiChoiceTile(
				_canvas,
				tr::lng_look_client_style(tr::now),
				u"Material v1  ·  Material v2 (exDesktop MD3)  ·  Liquid Glass"_q,
				[] {
					return std::vector<QString>{
						tr::lng_look_client_style_v1(tr::now),
						tr::lng_look_client_style_v2(tr::now),
						tr::lng_look_client_style_liquid(tr::now),
					};
				},
				[] { return Appearance::Get(Appearance::Key::ClientStyle); },
				[=](int idx) {
					if (idx == 2) {
						showBanner(tr::lng_look_liquid_glass_wip(tr::now));
						if (const auto ctrl = _controller.get()) {
							ctrl->showToast(tr::lng_look_liquid_glass_wip(tr::now));
						}
						return;
					}
					Appearance::Set(Appearance::Key::ClientStyle, idx);
				},
				Appearance::Changes());
			addCustomTile(Page::Appearance, styleTile, [=] { return styleTile->searchBlob(); }, 76, true);

			// Free Self Badge Role Picker right inside CSGUI
			static constexpr std::array<ClientBadge::Role, 6> kRoles = {
				ClientBadge::Role::Founder,
				ClientBadge::Role::Developer,
				ClientBadge::Role::BugHunter,
				ClientBadge::Role::Supporter,
				ClientBadge::Role::User,
				ClientBadge::Role::None,
			};
			auto *roleTile = new GuiChoiceTile(
				_canvas,
				tr::lng_yg_role_label(tr::now),
				tr::lng_yg_publish_badge(tr::now),
				[=] {
					std::vector<QString> names;
					for (const auto r : kRoles) {
						names.push_back(r == ClientBadge::Role::None
							? tr::lng_client_badge_none(tr::now)
							: ClientBadge::RoleName(r));
					}
					return names;
				},
				[=] {
					const auto ctrl = _controller.get();
					if (!ctrl) return 5;
					const auto cur = ClientBadge::RoleOfPeer(ctrl->session().user());
					for (int i = 0; i < int(kRoles.size()); ++i) {
						if (kRoles[i] == cur) return i;
					}
					return 5;
				},
				[=](int idx) {
					if (const auto ctrl = _controller.get()) {
						const auto role = kRoles[std::clamp(idx, 0, int(kRoles.size()) - 1)];
						ClientBadge::ChooseOwnRole(&ctrl->session(), role);
					}
				},
				Appearance::Changes());
			addCustomTile(Page::Appearance, roleTile, [=] { return roleTile->searchBlob(); }, 76);

			// Avatar Corner Radius CSGUI Slider (0%..50%)
			auto *avatarSlider = new GuiSliderTile(
				_canvas,
				tr::lng_look_avatar_radius(tr::now),
				tr::lng_look_avatar_radius_desc(tr::now),
				0,
				50,
				[] { return Appearance::AvatarRadiusPercent(); },
				[](int v) { return QString::number(v) + u"%"_q; },
				[](int v) { Appearance::SetAvatarRadiusPercent(v); },
				Appearance::Changes());
			addCustomTile(Page::Appearance, avatarSlider, [=] { return avatarSlider->searchBlob(); }, 76);

			// Google Material You Color Studio Swatches
			const auto addTint = [&](Appearance::Tint tint, const tr::phrase<> &phrase) {
				auto *ct = new GuiColorTile(
					_canvas,
					phrase(tr::now),
					[=] { return Appearance::Color(tint); },
					[=] {
						const auto cur = Appearance::Color(tint);
						Core::ShowMaterialColorPicker(
							this,
							cur.isValid() ? cur : AccentColor(),
							[=](QColor picked) { Appearance::SetColor(tint, picked); },
							true,
							true);
					},
					Appearance::Changes());
				addCustomTile(Page::Appearance, ct, [=] { return ct->searchBlob(); }, 66);
			};
			addTint(Appearance::Tint::Accent, tr::lng_look_accent);
			addTint(Appearance::Tint::SettingsBg, tr::lng_look_settingsbg);
			addTint(Appearance::Tint::Surface, tr::lng_look_surface);
			addTint(Appearance::Tint::Ink, tr::lng_look_ink);
			addTint(Appearance::Tint::Incoming, tr::lng_look_incoming);
			addTint(Appearance::Tint::Outgoing, tr::lng_look_outgoing);
			addTint(Appearance::Tint::Wallpaper, tr::lng_look_wallpaper);

			auto *resetPal = new GuiActionTile(
				_canvas,
				tr::lng_look_palette_reset(tr::now),
				{},
				u"↺"_q,
				[] { Appearance::ResetColors(); });
			addCustomTile(Page::Appearance, resetPal, [=] { return resetPal->searchBlob(); }, 66);

			// All Appearance Choice Cards
			for (const auto &spec : AllAppearanceChoices()) {
				const auto key = spec.key;
				const auto titlePhrase = spec.title;
				const auto vals = spec.values;
				auto *choice = new GuiChoiceTile(
					_canvas,
					(*titlePhrase)(tr::now),
					{},
					[=] {
						std::vector<QString> list;
						for (const auto *ph : vals) list.push_back((*ph)(tr::now));
						return list;
					},
					[=] { return Appearance::Get(key); },
					[=](int idx) { Appearance::Set(key, idx); },
					Appearance::Changes());
				addCustomTile(Page::Appearance, choice, [=] { return choice->searchBlob(); }, 68);
			}

			// Preset Slots 1..3 + Full Reset
			for (int slot = 1; slot <= 3; ++slot) {
				const auto slotTitle = tr::lng_look_slot(tr::now) + ' ' + QString::number(slot);
				auto *saveCard = new GuiActionTile(
					_canvas,
					slotTitle + u" · "_q + tr::lng_look_store(tr::now),
					{},
					u"Save"_q,
					[=] {
						Appearance::SaveSlot(slot);
						showBanner(slotTitle + u" ✓"_q);
					});
				addCustomTile(Page::Appearance, saveCard, [=] { return saveCard->searchBlob(); }, 64);

				auto *loadCard = new GuiActionTile(
					_canvas,
					slotTitle + u" · "_q + tr::lng_look_load(tr::now),
					{},
					u"Load"_q,
					[=] { Appearance::LoadSlot(slot); });
				addCustomTile(Page::Appearance, loadCard, [=] { return loadCard->searchBlob(); }, 64);
			}
			auto *resetAll = new GuiActionTile(
				_canvas,
				tr::lng_look_reset(tr::now),
				{},
				u"Reset"_q,
				[] { Appearance::Reset(); });
			addCustomTile(Page::Appearance, resetAll, [=] { return resetAll->searchBlob(); }, 66, true);
		}

		// --- 10. BOT USE (Embedded directly inside CSGUI) ---
		if (const auto ctrl = _controller.get()) {
			auto *mgr = &ctrl->session().domain().botUse();
			const auto addBotOpt = [&](BotUse::Settings::Key<bool> key) {
				const auto title = BotUse::Settings::OptionTitle(key.id);
				auto *tile = new GuiToggleTile(
					_canvas,
					BotUse::Settings::ControlId(key.id).toUtf8(),
					title,
					{},
					[=] {
						const auto c = _controller.get();
						return c ? c->session().domain().botUse().option(key) : false;
					},
					[=](bool val) {
						if (const auto c = _controller.get()) {
							(void)c->session().domain().botUse().setOption(key, val);
						}
					},
					mgr->optionChanges(key),
					false,
					nullptr);
				addCustomTile(Page::BotUse, tile, [=] { return tile->searchBlob(); }, 74);
			};
			addBotOpt(BotUse::Settings::Option::DisableTypingStatus);
			addBotOpt(BotUse::Settings::Option::AutoSwitchRichEditor);
			addBotOpt(BotUse::Settings::Option::AllowReplyAndRepeatBots);

			auto *manageBots = new GuiActionTile(
				_canvas,
				tr::lng_bot_use_add(tr::now) + u" / API"_q,
				u"Bots: "_q + QString::number(mgr->bots().size()),
				u"Open ›"_q,
				[=] {
					runOnController([](auto c) {
						c->showSettings(BotUseSettingsId());
					});
				});
			mgr->changes() | rpl::on_next([=] {
				if (const auto c = _controller.get()) {
					manageBots->setSubtitle(
						u"Bots: "_q + QString::number(c->session().domain().botUse().bots().size()));
				}
			}, manageBots->lifetime());
			addCustomTile(Page::BotUse, manageBots, [=] { return manageBots->searchBlob(); }, 74);
		}

		// --- 11. TRANSLATION ---
		{
			auto *provChoice = new GuiChoiceTile(
				_canvas,
				tr::lng_translate_provider_setting(tr::now),
				OptionDesc(Option::TranslateProvider.id),
				[] {
					std::vector<QString> list;
					for (const auto &p : Ui::TranslateProviders()) {
						list.push_back(p.name);
					}
					return list;
				},
				[] {
					const auto providers = Ui::TranslateProviders();
					const auto cur = EnhancedSettings::Get(Option::TranslateProvider);
					for (int i = 0; i < int(providers.size()); ++i) {
						if (providers[i].id == cur) return i;
					}
					return 0;
				},
				[=](int idx) {
					const auto providers = Ui::TranslateProviders();
					if (idx >= 0 && idx < int(providers.size())) {
						const auto &chosen = providers[idx];
						if (chosen.selectable) {
							if (const auto c = _controller.get()) {
								EnhancedSettings::ApplyOption(c, Option::TranslateProvider, chosen.id);
							} else {
								EnhancedSettings::Set(Option::TranslateProvider, chosen.id);
							}
						} else {
							showBanner(tr::lng_translate_provider_unavailable(tr::now));
						}
					}
				},
				EnhancedSettings::Changes(Option::TranslateProvider));
			addCustomTile(Page::Translation, provChoice, [=] { return provChoice->searchBlob(); }, 76);

			auto *llmCard = new GuiActionTile(
				_canvas,
				tr::lng_translate_llm_config(tr::now),
				{},
				u"LLM ›"_q,
				[=] {
					runOnController([](auto c) {
						c->show(Box([](not_null<Ui::GenericBox*> box) {
							Ui::LLMTranslateConfigBox(box);
						}));
					});
				});
			addCustomTile(Page::Translation, llmCard, [=] { return llmCard->searchBlob(); }, 76);
		}
		addEnhancedToggle(Page::Translation, Option::LlmTranslateContext, tr::lng_translate_llm_context_desc(tr::now));

		// --- 12. VOICE CHAT / CALLS (Custom CSGUI Bitrate Slider) ---
		addEnhancedToggle(Page::Calls, Option::AutoUnmute, tr::lng_auto_unmute_desc(tr::now));
		addEnhancedToggle(Page::Calls, Option::HdVideo);
		addEnhancedToggle(Page::Calls, Option::MprisCallHangup);
		{
			auto *bitrateSlider = new GuiSliderTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::Bitrate.id),
				{},
				0,
				7,
				[] { return std::clamp(EnhancedSettings::Get(Option::Bitrate), 0, 7); },
				[](int v) { return BitrateController::BitrateLabel(std::clamp(v, 0, 7)); },
				[=](int v) {
					if (const auto c = _controller.get()) {
						EnhancedSettings::ApplyOption(c, Option::Bitrate, v);
					} else {
						EnhancedSettings::Set(Option::Bitrate, v);
					}
				},
				EnhancedSettings::Changes(Option::Bitrate));
			addCustomTile(Page::Calls, bitrateSlider, [=] { return bitrateSlider->searchBlob(); }, 76);

			auto *radioCard = new GuiActionTile(
				_canvas,
				EnhancedSettings::OptionTitle(Option::RadioController.id),
				tr::lng_radio_controller_desc(tr::now),
				u"›"_q,
				[=] { runOnController([](auto c) { c->show(Box<RadioController>()); }); });
			addCustomTile(Page::Calls, radioCard, [=] { return radioCard->searchBlob(); }, 76);
		}

		// --- 13. OTHER (Profile RegDate & DC, Peer ID, Server Config) ---
		addEnhancedToggle(Page::Other, Option::ShowRegDateAndDc);
		addEnhancedToggle(Page::Other, Option::ShowPeerId);
		{
			auto *idTypeChoice = new GuiChoiceTile(
				_canvas,
				tr::lng_settings_peer_id_type(tr::now),
				{},
				[] { return std::vector<QString>{ u"Bot API"_q, u"MTProto"_q }; },
				[] { return EnhancedSettings::Get(Option::PeerIdType); },
				[=](int idx) {
					if (const auto c = _controller.get()) {
						EnhancedSettings::ApplyOption(c, Option::PeerIdType, idx);
					} else {
						EnhancedSettings::Set(Option::PeerIdType, idx);
					}
				},
				EnhancedSettings::Changes(Option::PeerIdType));
			addCustomTile(Page::Other, idTypeChoice, [=] { return idTypeChoice->searchBlob(); }, 72);
		}
		addEnhancedToggle(Page::Other, Option::ShowPeerInfo);
	}

	void clearDynamicItems(Page page) {
		for (auto it = _items.begin(); it != _items.end();) {
			if (it->dynamicItem && it->page == page) {
				delete it->widget;
				it = _items.erase(it);
			} else {
				++it;
			}
		}
	}

	void fetchMissingChannelPosts() {
		const auto ctrl = _controller.get();
		if (!ctrl) return;
		auto *owner = &ctrl->session().data();
		int requested = 0;
		for (const auto &row : owner->chatsList()->indexed()->all()) {
			const auto h = row->history();
			if (!h || !h->peer->isBroadcast() || h->lastMessage()) continue;
			ctrl->session().api().request(MTPmessages_GetHistory(
				h->peer->input(),
				MTP_int(0),
				MTP_int(0),
				MTP_int(0),
				MTP_int(3),
				MTP_int(0),
				MTP_int(0),
				MTP_long(0)
			)).done(crl::guard(this, [=](const MTPmessages_Messages &res) {
				res.match([&](const MTPDmessages_messages &d) {
					owner->processUsers(d.vusers());
					owner->processChats(d.vchats());
					owner->processMessages(d.vmessages(), NewMessageType::Existing);
				}, [&](const MTPDmessages_messagesSlice &d) {
					owner->processUsers(d.vusers());
					owner->processChats(d.vchats());
					owner->processMessages(d.vmessages(), NewMessageType::Existing);
				}, [&](const MTPDmessages_channelMessages &d) {
					owner->processUsers(d.vusers());
					owner->processChats(d.vchats());
					owner->processMessages(d.vmessages(), NewMessageType::Existing);
				}, [&](const MTPDmessages_messagesNotModified &) {});
				if (_active == Page::Feed) {
					rebuildDynamicFeed();
					relayoutContent();
				}
			})).send();
			if (++requested >= 8) break;
		}
	}

	void rebuildDynamicFeed() {
		clearDynamicItems(Page::Feed);
		const auto ctrl = _controller.get();
		if (!ctrl) return;
		auto *owner = &ctrl->session().data();
		const bool hideArchived = EnhancedSettings::Get(EnhancedSettings::Option::FeedHideArchived);

		struct FeedPost {
			not_null<History*> history;
			not_null<HistoryItem*> item;
			TimeId date = 0;
		};
		std::vector<FeedPost> posts;

		const auto collectFrom = [&](not_null<Dialogs::MainList*> list) {
			for (const auto &row : list->indexed()->all()) {
				const auto h = row->history();
				if (!h || !h->peer->isBroadcast()) continue;
				int perChannel = 0;
				for (auto b = h->blocks.rbegin(); b != h->blocks.rend() && perChannel < 4; ++b) {
					for (auto m = (*b)->messages.rbegin(); m != (*b)->messages.rend() && perChannel < 4; ++m) {
						const auto item = (*m)->data();
						if (!item->isRegular() || item->isService()) continue;
						posts.push_back({ h, item, item->date() });
						++perChannel;
					}
				}
				if (!perChannel) {
					if (const auto last = h->lastMessage(); last && last->isRegular() && !last->isService()) {
						posts.push_back({ h, last, last->date() });
					}
				}
			}
		};

		collectFrom(owner->chatsList());
		if (!hideArchived) {
			if (const auto archive = owner->folderLoaded(Data::Folder::kId)) {
				collectFrom(owner->chatsList(archive));
			}
		}

		std::sort(posts.begin(), posts.end(), [](const FeedPost &a, const FeedPost &b) {
			return a.date > b.date;
		});
		if (posts.size() > 60) posts.resize(60);

		if (posts.empty()) {
			auto *emptyCard = new GuiActionTile(
				_canvas,
				tr::lng_yg_feed_empty(tr::now),
				tr::lng_yg_feed_about(tr::now),
				{},
				nullptr);
			addCustomTile(Page::Feed, emptyCard, [=] { return emptyCard->searchBlob(); }, 74, true, true);
			return;
		}

		for (const auto &post : posts) {
			const auto peer = post.history->peer;
			const auto msgId = post.item->id;
			const auto when = QLocale().toString(
				QDateTime::fromSecsSinceEpoch(post.date),
				QLocale::ShortFormat);
			auto body = post.item->originalText().text.simplified();
			if (body.isEmpty()) {
				body = u"[Media / Post #"_q + QString::number(msgId.bare) + u"]"_q;
			}
			if (body.size() > 220) {
				body = body.left(217) + u"…"_q;
			}
			auto *card = new GuiActionTile(
				_canvas,
				peer->name(),
				body,
				when + u"  ·  "_q + tr::lng_yg_feed_open_post(tr::now),
				[=] {
					runOnController([=](auto c) {
						c->showPeerHistory(peer, Window::SectionShow::Way::Forward, msgId);
					});
				});
			card->show();
			addCustomTile(Page::Feed, card, [=] { return card->searchBlob(); }, 92, true, true);
		}
	}

	void rebuildDynamicJournal() {
		clearDynamicItems(Page::Journal);
		const auto ctrl = _controller.get();
		if (!ctrl) return;
		const auto cutoff = int64(base::unixtime::now()) - Main::SessionSettings::kJournalLifetime;
		const auto &journal = ctrl->session().settings().journal();
		int added = 0;
		for (auto i = journal.rbegin(); i != journal.rend(); ++i) {
			if (i->time < cutoff) continue;
			const auto when = QLocale().toString(
				QDateTime::fromSecsSinceEpoch(i->time),
				QLocale::ShortFormat);
			auto *card = new GuiActionTile(
				_canvas,
				i->heading,
				i->text,
				when,
				nullptr);
			card->show();
			addCustomTile(Page::Journal, card, [=] { return card->searchBlob(); }, 88, true, true);
			if (++added >= 80) break;
		}
		if (!added) {
			auto *emptyCard = new GuiActionTile(
				_canvas,
				tr::lng_yg_journal_empty(tr::now),
				{},
				{},
				nullptr);
			emptyCard->show();
			addCustomTile(Page::Journal, emptyCard, [=] { return emptyCard->searchBlob(); }, 68, true, true);
		}
	}

	void relayoutContent() {
		for (int i = 0; i < int(_navBtns.size()); ++i) {
			_navBtns[i]->setChecked(i == int(_active));
		}
		const int viewW = std::max(300, _scroll->viewport()->width());
		const QString query = _search->text().trimmed();
		const int pad = 8;
		const int gap = 8;
		const int availW = viewW - 2 * pad;
		const int cols = (availW >= 460) ? 2 : 1;
		const int colW = (availW - (cols - 1) * gap) / cols;

		int y = pad;
		int col = 0;
		int rowMaxH = 0;
		int shown = 0;

		for (const auto &item : _items) {
			bool vis = false;
			if (!query.isEmpty()) {
				vis = item.searchFn && item.searchFn().contains(query, Qt::CaseInsensitive);
			} else if (_active == Page::Favorites) {
				vis = !item.favId.isEmpty() && _favorites.contains(item.favId);
			} else {
				vis = (item.page == _active);
			}
			item.widget->setVisible(vis);
			if (!vis) continue;

			++shown;
			if (item.fullWidth || cols == 1) {
				if (col > 0) {
					y += rowMaxH + gap;
					col = 0;
					rowMaxH = 0;
				}
				item.widget->setGeometry(pad, y, availW, item.preferredHeight);
				y += item.preferredHeight + gap;
			} else {
				const int x = pad + col * (colW + gap);
				item.widget->setGeometry(x, y, colW, item.preferredHeight);
				rowMaxH = std::max(rowMaxH, item.preferredHeight);
				if (++col >= cols) {
					y += rowMaxH + gap;
					col = 0;
					rowMaxH = 0;
				}
			}
		}
		if (col > 0) {
			y += rowMaxH + gap;
		}

		_heading->setText(
			(query.isEmpty() ? Title(_active) : tr::lng_yg_panel_search(tr::now))
			+ u"  ·  "_q + QString::number(shown));

		_empty->setVisible(shown == 0);
		if (shown == 0) {
			_empty->setGeometry(pad + 4, pad + 8, availW - 8, 56);
			y = 84;
		}
		_canvas->setGeometry(0, 0, viewW, std::max(_scroll->viewport()->height(), y + 12));
	}

	void exportSettings() {
		const auto json = EnhancedSettings::Serialize();
		const auto flags = QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals;
		const auto encoded = kClipboardPrefix
			+ QString::fromLatin1(qCompress(json.toUtf8(), 9).toBase64(flags));
		TextUtilities::SetClipboardText({ encoded });
		showBanner(tr::lng_settings_enhanced_export_done(tr::now));
	}

	base::weak_ptr<Window::SessionController> _controller;
	Page _active = Page::Interface;
	QLabel *_logo = nullptr;
	QLabel *_brand = nullptr;
	QLabel *_hotkeyTag = nullptr;
	QLineEdit *_search = nullptr;
	QPushButton *_exportBtn = nullptr;
	QPushButton *_closeBtn = nullptr;
	QScrollArea *_navScroll = nullptr;
	QWidget *_navContainer = nullptr;
	std::vector<QPushButton*> _navBtns;
	QLabel *_heading = nullptr;
	QScrollArea *_scroll = nullptr;
	QWidget *_canvas = nullptr;
	QLabel *_empty = nullptr;
	QLabel *_bannerLabel = nullptr;
	QTimer *_bannerTimer = nullptr;
	QString _bannerText;
	std::vector<TileItem> _items;
	QList<QByteArray> _favorites;
	bool _dragging = false;
	bool _resizing = false;
	QPoint _dragOffset;
	QPoint _dragStartGlobal;
	QSize _startSize;
};

QPointer<YandereWindow> ActiveWindow;

} // namespace

void ShowYandereWindow(
		not_null<Window::SessionController*> controller,
		PanelPage initialPage) {
	if (ActiveWindow) {
		ActiveWindow->selectPage(initialPage);
		ActiveWindow->show();
		ActiveWindow->raise();
		ActiveWindow->activateWindow();
		return;
	}
	ActiveWindow = new YandereWindow(controller, initialPage);
	ActiveWindow->show();
	ActiveWindow->raise();
	ActiveWindow->activateWindow();
}

void ToggleYandereWindow(not_null<Window::SessionController*> controller) {
	if (ActiveWindow && ActiveWindow->isVisible()) {
		ActiveWindow->close();
		return;
	}
	ShowYandereWindow(controller, PanelPage::Interface);
}

bool IsYandereWindowOpen() {
	return ActiveWindow && ActiveWindow->isVisible();
}

void ShowChannelsFeedBox(not_null<Window::SessionController*> controller) {
	controller->window().hideSettingsAndLayer(anim::type::instant);
	ShowYandereWindow(controller, PanelPage::Feed);
}

// --- Embedded YanderePanel implementation (all custom CSGUI controls) ---

YanderePanel::YanderePanel(
	QWidget *parent,
	Fn<void(Page, bool)> changed,
	Fn<void(Page)> open)
: RpWidget(parent)
, _changed(std::move(changed))
, _open(std::move(open)) {
	_favorites = Core::App().settings().readPref<QByteArray>(kFavorites).split('\n');
	_favorites.removeAll(QByteArray());
	setAttribute(Qt::WA_StyledBackground);
	setObjectName(u"ygPanel"_q);
	_brand = new QLabel(u"YandereGram"_q, this);
	_brand->setObjectName(u"brand"_q);
	_logo = new QLabel(this);
	_logo->setScaledContents(true);
	_logo->setPixmap(QPixmap(u":/gui/art/logo_256_no_margin.png"_q));
	_search = new QLineEdit(this);
	_search->setPlaceholderText(tr::lng_yg_panel_search(tr::now));
	_search->setClearButtonEnabled(true);
	_heading = new QLabel(this);
	_heading->setObjectName(u"heading"_q);
	_empty = new QLabel(tr::lng_yg_panel_empty(tr::now), this);
	_empty->setWordWrap(true);

	for (int i = 0; i < int(Page::Count); ++i) {
		const auto page = Page(i);
		const auto name = Title(page);
		auto *nav = new QPushButton(name, this);
		nav->setCheckable(true);
		nav->setAccessibleName(name);
		nav->setToolTip(name);
		_nav.push_back(nav);
		connect(nav, &QPushButton::clicked, this, [=] { select(page); });
	}
	connect(_search, &QLineEdit::textChanged, this, [=] { refresh(); });

	const auto applyPalette = [=] {
		setStyleSheet(
			u"QWidget#ygPanel{background:%1} QLabel{background:transparent;color:%2} QLabel#brand,QLabel#heading{font-weight:600} QPushButton,QLineEdit{border:1px solid #343137;border-radius:6px;background:#1e1d21;color:#f1eef0;padding:5px;text-align:left} QPushButton:checked{background:#44242c;border-left:3px solid %3} QPushButton:hover{background:#393038}"_q
				.arg(PanelBgColor().name(), InkColor().name(), AccentColor().name()));
		update();
	};
	applyPalette();
	Appearance::Changes() | rpl::on_next(applyPalette, lifetime());
}

Ui::RpWidget *YanderePanel::addCard(
		Page page,
		EnhancedSettings::Key<bool> key,
		rpl::producer<QString> about,
		Fn<void()> toggle) {
	const auto id = EnhancedSettings::ControlId(key.id).toUtf8();
	auto *tile = new GuiToggleTile(
		this,
		id,
		EnhancedSettings::OptionTitle(key.id),
		{},
		[=] { return EnhancedSettings::Get(key); },
		[=](bool) { toggle(); },
		EnhancedSettings::Changes(key),
		_favorites.contains(id),
		[=](bool on) {
			_favorites.removeAll(id);
			if (on) _favorites.push_back(id);
			Core::App().settings().writePref<QByteArray>(
				kFavorites,
				QByteArrayList(_favorites).join('\n'));
			Core::App().saveSettingsDelayed();
			refresh();
		});
	std::move(about) | rpl::on_next([=](QString text) {
		tile->setSubtitle(text);
	}, tile->lifetime());
	_entries.push_back({
		.group = page,
		.id = id,
		.widget = tile,
		.searchText = [=] { return tile->searchBlob(); },
	});
	return tile;
}

Ui::RpWidget *YanderePanel::addSliderCard(
		Page group,
		QByteArray id,
		QString title,
		QString about,
		int minValue,
		int maxValue,
		Fn<int()> getter,
		Fn<QString(int)> formatter,
		Fn<void(int)> setter,
		rpl::producer<> externalChanges) {
	auto *tile = new GuiSliderTile(
		this,
		std::move(title),
		std::move(about),
		minValue,
		maxValue,
		std::move(getter),
		std::move(formatter),
		std::move(setter),
		std::move(externalChanges));
	_entries.push_back({
		.group = group,
		.id = std::move(id),
		.widget = tile,
		.searchText = [=] { return tile->searchBlob(); },
	});
	return tile;
}

void YanderePanel::select(Page page) {
	if (page == Page::Appearance || page == Page::BotUse) {
		_nav[int(page)]->setChecked(false);
		_open(page);
		return;
	}
	_active = page;
	_search->clear();
	refresh();
}

void YanderePanel::refresh() {
	_changed(_active, !_search->text().trimmed().isEmpty());
	resizeToWidth(width());
	update();
}

int YanderePanel::resizeGetHeight(int w) {
	const auto wide = w >= 570;
	const auto x = wide ? 160 : 10;
	const auto right = std::max(1, w - x - 10);
	const auto h = std::max(30, fontMetrics().height() + 14);
	_logo->setGeometry(10, 10, 28, 28);
	_brand->setGeometry(44, 10, 110, 28);

	const auto across = std::clamp((w - 16) / 105, 1, 4);
	const auto navWidth = wide ? 144 : std::max(1, (w - 16 - 4 * (across - 1)) / across);
	for (int i = 0; i != int(_nav.size()); ++i) {
		auto *b = _nav[i];
		b->setChecked(i == int(_active));
		b->setText(fontMetrics().elidedText(b->accessibleName(), Qt::ElideRight, navWidth - 15));
		b->setGeometry(
			wide ? 8 : 8 + (i % across) * (navWidth + 4),
			48 + (wide ? i : i / across) * (h + 3),
			navWidth,
			h);
	}
	auto y = wide ? 10 : 52 + ((int(_nav.size()) + across - 1) / across) * (h + 3);
	_search->setGeometry(x, y, right, h);
	y += h + 6;

	const auto query = _search->text().trimmed();
	const auto cols = (right >= 306) ? 2 : 1;
	const auto ch = std::max(74, 3 * fontMetrics().height() + 26);
	const auto cw = (right - (cols - 1) * 6) / cols;
	int n = 0;
	_heading->setGeometry(x, y, right, h);
	y += h + 3;
	for (const auto &entry : _entries) {
		const auto visible = query.isEmpty()
			? ((_active == Page::Favorites) ? _favorites.contains(entry.id) : entry.group == _active)
			: (entry.searchText && entry.searchText().contains(query, Qt::CaseInsensitive));
		entry.widget->setVisible(visible);
		if (!visible) continue;
		entry.widget->setGeometry(x + (n % cols) * (cw + 6), y + (n / cols) * (ch + 6), cw, ch);
		++n;
	}
	_heading->setText(
		(query.isEmpty() ? Title(_active) : tr::lng_yg_panel_search(tr::now))
		+ u"  ·  "_q + QString::number(n));
	_empty->setVisible(!n);
	_empty->setGeometry(x, y, right, 52);
	return std::max(
		wide ? 52 + int(_nav.size()) * (h + 3) : 0,
		y + (n ? ((n + cols - 1) / cols) * (ch + 6) : 52) + 8);
}

} // namespace Settings
