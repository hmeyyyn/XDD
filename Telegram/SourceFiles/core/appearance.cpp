#include "core/appearance.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "rpl/event_stream.h"
#include "ui/widgets/chat_filters_tabs_mode.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <array>

namespace Appearance {
namespace {
constexpr auto kPref = "yanderegram.appearance.v1";
constexpr auto Names = std::array{ "Meta", "Menu", "Density", "Journal", "InCorners", "OutCorners", "Tails", "Grouping", "Quote", "QuoteLines", "Mono", "Grain", "Unread", "Pinned", "Composer", "Voice", "Cursor", "Dates", "Ghost", "Tray", "TrayCount", "Reactions", "Panels", "Transitions", "BadgeMotion", "AvatarSize", "ListLines", "FolderBar" };
constexpr auto Sizes = std::array{ 4, 3, 3, 3, 3, 3, 2, 3, 4, 4, 2, 3, 3, 3, 2, 3, 2, 3, 2, 4, 3, 2, 2, 2, 2, 3, 3, 2 };
constexpr auto Colors = std::array{ "Accent", "SettingsBg", "Surface", "Ink", "Incoming", "Outgoing", "Wallpaper" };
static_assert(Names.size() == size_t(Key::Count) && Names.size() == Sizes.size());
static_assert(Colors.size() == size_t(Tint::Count));
struct State {
	std::array<int, Names.size()> values = {};
	std::array<QColor, Colors.size()> tints;
	std::array<QString, 3> fonts;
};
State Current;
QJsonObject Slots, Markers;
rpl::event_stream<> Changed;
constexpr auto FontKeys = std::array{ "timeFont", "senderFont", "titleFont" };
QString MarkerKey(quint64 account, quint64 peer) {
	return QString::number(account) + '/' + QString::number(peer);
}
QJsonObject Encode() {
	auto result = QJsonObject();
	for (auto i = size_t(0); i < Names.size(); ++i) result[Names[i]] = Current.values[i];
	for (auto i = size_t(0); i < Colors.size(); ++i) {
		if (Current.tints[i].isValid()) result[Colors[i]] = Current.tints[i].name();
	}
	for (auto i = size_t(); i < FontKeys.size(); ++i) result[FontKeys[i]] = Current.fonts[i];
	return result;
}
void Decode(const QJsonObject &json) {
	Current = State();
	for (auto i = size_t(0); i < Names.size(); ++i) {
		const auto n = json.value(Names[i]).toInt();
		Current.values[i] = (n >= 0 && n < Sizes[i]) ? n : 0;
	}
	for (auto i = size_t(0); i < Colors.size(); ++i) {
		Current.tints[i] = QColor(json.value(Colors[i]).toString());
	}
	for (auto i = size_t(); i < FontKeys.size(); ++i) Current.fonts[i] = json.value(FontKeys[i]).toString().left(128);
}
void Save() {
	const auto json = QJsonObject{{ "look", Encode() }, { "slots", Slots }, { "markers", Markers }};
	Core::App().settings().writePref<QByteArray>(kPref, QJsonDocument(json).toJson(QJsonDocument::Compact));
	Core::App().saveSettingsDelayed();
	Changed.fire({});
}
} // namespace
void Start() {
	const auto json = QJsonDocument::fromJson(Core::App().settings().readPref<QByteArray>(kPref)).object();
	Decode(json.value("look").toObject());
	Slots = json.value("slots").toObject();
	Markers = json.value("markers").toObject();
}
int Get(Key k) { return Current.values[size_t(k)]; }
QColor Color(Tint k) { return Current.tints[size_t(k)]; }
QString Family(FontRole role) { return Current.fonts[size_t(role)]; }
QColor Mark(quint64 account, quint64 peer) {
	return Markers.isEmpty() ? QColor() : QColor(Markers.value(MarkerKey(account, peer)).toString());
}
void SetMark(quint64 account, quint64 peer, QColor color) {
	const auto key = MarkerKey(account, peer);
	if (color.isValid()) Markers[key] = color.name(); else Markers.remove(key);
	Save();
}
void Set(Key k, int value) {
	if (value < 0 || value >= Sizes[size_t(k)] || Get(k) == value) return;
	Current.values[size_t(k)] = value;
	Save();
}
void SetColor(Tint k, QColor c) { Current.tints[size_t(k)] = c; Save(); }
void ResetColors() { Current.tints = {}; Save(); }
void SetFont(FontRole role, QString family) { Current.fonts[size_t(role)] = family.left(128); Save(); }
void Reset() {
	Current = State();
	Core::App().settings().setChatFiltersTabsMode(Ui::ChatsFiltersTabsMode::Default);
	Save();
}
bool HasSlot(int slot) { return Slots.value(QString::number(slot)).isObject(); }
void SaveSlot(int slot) {
	auto snapshot = Encode();
	snapshot["folders"] = int(Core::App().settings().chatFiltersTabsMode());
	Slots[QString::number(slot)] = snapshot;
	Save();
}
bool LoadSlot(int slot) {
	if (!HasSlot(slot)) return false;
	const auto snapshot = Slots.value(QString::number(slot)).toObject();
	Decode(snapshot);
	Core::App().settings().setChatFiltersTabsMode(Ui::ChatsFiltersTabsMode(std::clamp(snapshot.value("folders").toInt(), 0, 3)));
	Save();
	return true;
}
rpl::producer<> Changes() { return Changed.events(); }
} // namespace Appearance
