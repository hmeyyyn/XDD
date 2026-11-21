#include "settings/sections/settings_yandere_panel.h"

#include "core/appearance.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "lang/lang_keys.h"
#include <QtGui/QPainter>
#include <QtWidgets/QAbstractButton>
#include <QtCore/QByteArrayList>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>

namespace Settings {
namespace {
using Page = PanelPage;
constexpr auto kFavorites = "yanderegram.panel.favorites";
QString Title(Page page) {
	switch (page) {
	case Page::Interface: return tr::lng_settings_interface(tr::now);
	case Page::Messages: return tr::lng_settings_messages(tr::now);
	case Page::Behavior: return tr::lng_settings_behavior(tr::now);
	case Page::Translation: return tr::lng_settings_translation(tr::now);
	case Page::Calls: return tr::lng_settings_voice_chat(tr::now);
	case Page::Other: return tr::lng_settings_other(tr::now);
	case Page::Favorites: return tr::lng_yg_panel_favorites(tr::now);
	case Page::Appearance: return tr::lng_settings_enhanced_customization(tr::now);
	case Page::BotUse: return tr::lng_bot_use_settings(tr::now);
	case Page::Count: break;
	}
	Unexpected("Unknown panel page");
}
QColor Color(Appearance::Tint key, const char *fallback) {
	const auto c = Appearance::Color(key);
	return c.isValid() ? c : QColor(QLatin1String(fallback));
}
class SwitchCard final : public QAbstractButton {
public:
	using QAbstractButton::QAbstractButton;
	QString detail;
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto accent = Color(Appearance::Tint::Accent, "#ff626d");
		const auto ink = Color(Appearance::Tint::Ink, "#f1eef0");
		p.setBrush(isChecked() ? QColor(63,36,43) : QColor(30,29,33));
		p.setPen(hasFocus() ? accent : isChecked() ? QColor(87,53,61) : QColor(49,46,52));
		p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 8, 8);
		p.setPen(ink);
		auto f = font(); f.setBold(true); p.setFont(f);
		const auto h = p.fontMetrics().height();
		p.drawText(QRect(10,7,width()-37,2*h+2), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text());
		p.setFont(font()); p.setPen(QColor(188,179,186));
		const auto area = QRect(10,height()-h-8,width()-55,h+1);
		p.drawText(area, Qt::AlignVCenter, p.fontMetrics().elidedText(detail,Qt::ElideRight,area.width()));
		const auto x = width()-37, y = height()-22;
		p.setPen(Qt::NoPen); p.setBrush(isChecked() ? accent : QColor(74,68,77));
		p.drawRoundedRect(QRect(x,y,27,14),7,7);
		p.setBrush(isChecked() ? Qt::white : QColor(191,182,191));
		p.drawEllipse(QRect(x+(isChecked()?15:2),y+2,10,10));
	}
};
} // namespace

class YanderePanel::Card final : public Ui::RpWidget {
public:
	Card(QWidget *parent, PanelPage group) : RpWidget(parent), group(group) {
		button = new SwitchCard(this);
		button->setCheckable(true);
		button->setFocusPolicy(Qt::StrongFocus);
		star = new QToolButton(this);
		star->setAccessibleName(Title(Page::Favorites));
		star->setToolTip(Title(Page::Favorites));
		star->setCheckable(true);
	}
	PanelPage group;
	SwitchCard *button;
	QToolButton *star;
	QByteArray id;
	void resizeEvent(QResizeEvent *) override {
		button->setGeometry(rect()); star->setGeometry(width()-27,4,23,23);
	}
};

YanderePanel::YanderePanel(QWidget *parent, Fn<void(Page,bool)> changed, Fn<void(Page)> open)
: RpWidget(parent), _changed(std::move(changed)), _open(std::move(open)) {
	_favorites = Core::App().settings().readPref<QByteArray>(kFavorites).split('\n');
	_favorites.removeAll(QByteArray());
	setAttribute(Qt::WA_StyledBackground);
	setObjectName(u"ygPanel"_q);
	_brand = new QLabel(u"YandereGram"_q,this); _brand->setObjectName(u"brand"_q);
	_logo = new QLabel(this); _logo->setScaledContents(true); _logo->setPixmap(QPixmap(u":/gui/art/logo_256_no_margin.png"_q));
	_search = new QLineEdit(this); _search->setPlaceholderText(tr::lng_yg_panel_search(tr::now));
	_search->setClearButtonEnabled(true);
	_heading = new QLabel(this); _heading->setObjectName(u"heading"_q);
	_empty = new QLabel(tr::lng_yg_panel_empty(tr::now),this); _empty->setWordWrap(true);
	for (int i = 0; i < int(Page::Count); ++i) {
		const auto page = Page(i);
		const auto name = Title(page);
		auto nav = new QPushButton(name,this); nav->setCheckable(true);
		nav->setAccessibleName(name); nav->setToolTip(name); _nav.push_back(nav);
		connect(nav,&QPushButton::clicked,this,[=]{select(page);});
	}
	connect(_search,&QLineEdit::textChanged,this,[=]{refresh();});
	const auto palette = [=] {
		setStyleSheet(u"QWidget#ygPanel{background:%1} QLabel{background:transparent;color:%2} QLabel#brand,QLabel#heading{font-weight:600} QPushButton,QLineEdit{border:1px solid #343137;border-radius:6px;background:#1e1d21;color:#f1eef0;padding:5px;text-align:left} QPushButton:checked{background:#44242c;border-left:3px solid %3} QPushButton:hover,QToolButton:hover{background:#393038} QToolButton{border:0;background:transparent;color:#aea5ad} QToolButton:checked{color:%3}"_q
			.arg(Color(Appearance::Tint::SettingsBg,"#111113").name(), Color(Appearance::Tint::Ink,"#f1eef0").name(), Color(Appearance::Tint::Accent,"#ff626d").name()));
		update();
	};
	palette(); Appearance::Changes() | rpl::on_next(palette,lifetime());
}

Ui::RpWidget *YanderePanel::addCard(Page page, EnhancedSettings::Key<bool> key, rpl::producer<QString> about, Fn<void()> toggle) {
	auto c = new Card(this,page); _cards.push_back(c);
	c->id = EnhancedSettings::ControlId(key.id).toUtf8();
	auto b = c->button;
	b->setText(EnhancedSettings::OptionTitle(key.id)); b->setAccessibleName(b->text());
	std::move(about) | rpl::on_next([=](QString text) {
		b->detail = text; b->setAccessibleDescription(text); b->setToolTip(b->text()+ '\n' +text); b->update();
	}, c->lifetime());
	EnhancedSettings::Watch(key) | rpl::on_next([=](bool on){b->setChecked(on);},c->lifetime());
	connect(b,&QAbstractButton::clicked,c,[=]{toggle(); b->setChecked(EnhancedSettings::Get(key));});
	c->star->setChecked(_favorites.contains(c->id));
	c->star->setText(c->star->isChecked() ? u"★"_q : u"☆"_q);
	connect(c->star,&QToolButton::clicked,c,[=](bool on){
		_favorites.removeAll(c->id); if (on) _favorites.push_back(c->id);
		Core::App().settings().writePref<QByteArray>(kFavorites,QByteArrayList(_favorites).join('\n'));
		Core::App().saveSettingsDelayed(); c->star->setText(on ? u"★"_q : u"☆"_q); refresh();
	});
	return c;
}
void YanderePanel::select(Page page) {
	if (page == Page::Appearance || page == Page::BotUse) {
		_nav[int(page)]->setChecked(false); _open(page); return;
	}
	_active = page; _search->clear(); refresh();
}
void YanderePanel::refresh() {
	_changed(_active,!_search->text().trimmed().isEmpty());
	resizeToWidth(width()); update();
}
int YanderePanel::resizeGetHeight(int w) {
	const auto wide = w >= 570;
	const auto x = wide ? 160 : 10, right = std::max(1,w-x-10);
	const auto h = std::max(30,fontMetrics().height()+14);
	_logo->setGeometry(10,10,28,28); _brand->setGeometry(44,10,110,28);
	// In the narrow Telegram layer use visible category chips, not a hidden menu.
	const auto across = std::clamp((w-16)/100, 1, 4);
	const auto navWidth = wide ? 144 : std::max(1,(w-16-4*(across-1))/across);
	for (int i = 0; i != int(_nav.size()); ++i) {
		auto b = _nav[i]; b->setChecked(i == int(_active));
		b->setText(fontMetrics().elidedText(b->accessibleName(),Qt::ElideRight,navWidth-15));
		b->setGeometry(wide ? 8 : 8+(i%across)*(navWidth+4),48+(wide ? i : i/across)*(h+3),navWidth,h);
	}
	auto y = wide ? 10 : 52+((int(_nav.size())+across-1)/across)*(h+3);
	_search->setGeometry(x,y,right,h); y+=h+6;
	const auto query = _search->text().trimmed();
	const auto cols = (right >= 306) ? 2 : 1;
	const auto ch = std::max(72,3*fontMetrics().height()+24), cw = (right-(cols-1)*6)/cols;
	int n = 0; _heading->setGeometry(x,y,right,h); y+=h+3;
	for (const auto c : _cards) {
		const auto visible = query.isEmpty() ? ((_active == Page::Favorites) ? _favorites.contains(c->id) : c->group == _active)
			: (c->button->text().contains(query,Qt::CaseInsensitive) || c->button->detail.contains(query,Qt::CaseInsensitive));
		c->setVisible(visible); if (!visible) continue;
		c->setGeometry(x+(n%cols)*(cw+6),y+(n/cols)*(ch+6),cw,ch); ++n;
	}
	_heading->setText((query.isEmpty()?Title(_active):tr::lng_yg_panel_search(tr::now)) + u"  ·  "_q + QString::number(n));
	_empty->setVisible(!n); _empty->setGeometry(x,y,right,52);
	return std::max(wide ? 52+int(_nav.size())*(h+3) : 0,y+(n ? ((n+cols-1)/cols)*(ch+6) : 52)+8);
}
} // namespace Settings
