#pragma once

#include "core/enhanced_settings.h"
#include "ui/rp_widget.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace Window {
class SessionController;
} // namespace Window

namespace Settings {

enum class PanelPage {
	Interface,
	Messages,
	Ghost,
	Streamer,
	Behavior,
	Network,
	Feed,
	Journal,
	Appearance,
	BotUse,
	Translation,
	Calls,
	Other,
	Favorites,
	Count,
};

void ShowYandereWindow(
	not_null<Window::SessionController*> controller,
	PanelPage initialPage = PanelPage::Interface);
void ToggleYandereWindow(not_null<Window::SessionController*> controller);
[[nodiscard]] bool IsYandereWindowOpen();
void ShowChannelsFeedBox(not_null<Window::SessionController*> controller);

// Customisable left (main-menu) drawer items, edited from the
// Customization page of the YandereGram panel. Ids: profile, create_group,
// create_supergroup, create_channel, contacts, calls, saved, settings,
// yandere, feed, night, phone, screenshot, ghost, streamer.
[[nodiscard]] bool DrawerItemVisible(const QByteArray &id);
void SetDrawerItemVisible(const QByteArray &id, bool visible);

// Embedded fallback panel (all controls styled in CSGUI dark/crimson).
class YanderePanel final : public Ui::RpWidget {
public:
	YanderePanel(
		QWidget *parent,
		Fn<void(PanelPage, bool)> changed,
		Fn<void(PanelPage)> open);
	Ui::RpWidget *addCard(
		PanelPage group,
		EnhancedSettings::Key<bool> key,
		rpl::producer<QString> about,
		Fn<void()> toggle);
	Ui::RpWidget *addSliderCard(
		PanelPage group,
		QByteArray id,
		QString title,
		QString about,
		int minValue,
		int maxValue,
		Fn<int()> getter,
		Fn<QString(int)> formatter,
		Fn<void(int)> setter,
		rpl::producer<> externalChanges);
	void select(PanelPage page);
	void refresh();

private:
	int resizeGetHeight(int width) override;
	struct Entry {
		PanelPage group = PanelPage::Interface;
		QByteArray id;
		Ui::RpWidget *widget = nullptr;
		Fn<QString()> searchText;
		bool fullWidth = false;
	};
	PanelPage _active = PanelPage::Interface;
	Fn<void(PanelPage, bool)> _changed;
	Fn<void(PanelPage)> _open;
	QLabel *_brand = nullptr;
	QLabel *_logo = nullptr;
	QLabel *_heading = nullptr;
	QLabel *_empty = nullptr;
	QLineEdit *_search = nullptr;
	std::vector<QPushButton*> _nav;
	std::vector<Entry> _entries;
	QList<QByteArray> _favorites;
};

} // namespace Settings
