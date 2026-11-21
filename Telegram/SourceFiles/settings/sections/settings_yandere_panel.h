#pragma once

#include "core/enhanced_settings.h"
#include "ui/rp_widget.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace Settings {

enum class PanelPage { Interface, Messages, Behavior, Translation, Calls, Other, Favorites, Appearance, BotUse, Count };

// The settings layer owns scrolling; this widget only measures its visible cards.
class YanderePanel final : public Ui::RpWidget {
public:
	YanderePanel(QWidget *parent, Fn<void(PanelPage, bool)> changed, Fn<void(PanelPage)> open);
	Ui::RpWidget *addCard(PanelPage group, EnhancedSettings::Key<bool> key,
		rpl::producer<QString> about, Fn<void()> toggle);
	void select(PanelPage page);
	void refresh();
private:
	int resizeGetHeight(int width) override;
	class Card;
	PanelPage _active = PanelPage::Interface;
	Fn<void(PanelPage, bool)> _changed;
	Fn<void(PanelPage)> _open;
	QLabel *_brand, *_logo, *_heading, *_empty;
	QLineEdit *_search;
	std::vector<QPushButton*> _nav;
	std::vector<Card*> _cards;
	QList<QByteArray> _favorites;
};

} // namespace Settings
