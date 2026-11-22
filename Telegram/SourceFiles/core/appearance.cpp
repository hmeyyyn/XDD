#include "core/appearance.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "rpl/event_stream.h"
#include "ui/widgets/chat_filters_tabs_mode.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <algorithm>
#include <array>

namespace Appearance {
namespace {
constexpr auto kPref = "yanderegram.appearance.v1";
constexpr auto Names = std::array{ "ClientStyle", "Meta", "Menu", "Density", "Journal", "InCorners", "OutCorners", "Tails", "Grouping", "Quote", "QuoteLines", "Mono", "Grain", "Unread", "Pinned", "Composer", "Voice", "Cursor", "Dates", "Ghost", "Tray", "TrayCount", "Reactions", "Panels", "Transitions", "BadgeMotion", "AvatarSize", "ListLines", "FolderBar" };
constexpr auto Sizes = std::array{ 3, 4, 3, 3, 3, 3, 3, 2, 3, 4, 4, 2, 3, 3, 3, 2, 3, 2, 3, 2, 4, 3, 2, 2, 2, 2, 3, 3, 2 };
constexpr auto Colors = std::array{ "Accent", "SettingsBg", "Surface", "Ink", "Incoming", "Outgoing", "Wallpaper" };
static_assert(Names.size() == size_t(Key::Count) && Names.size() == Sizes.size());
static_assert(Colors.size() == size_t(Tint::Count));
struct State {
	std::array<int, Names.size()> values = {};
	std::array<QColor, Colors.size()> tints;
	std::array<QString, 3> fonts;
	int avatarRadius = 50;
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
	result["avatarRadius"] = Current.avatarRadius;
	return result;
}
void Decode(const QJsonObject &json) {
	Current = State();
	for (auto i = size_t(0); i < Names.size(); ++i) {
		const auto n = json.value(Names[i]).toInt();
		Current.values[i] = (n >= 0 && n < Sizes[i]) ? n : 0;
	}
	if (Current.values[size_t(Key::ClientStyle)] >= 2) {
		Current.values[size_t(Key::ClientStyle)] = 0;
	}
	for (auto i = size_t(0); i < Colors.size(); ++i) {
		Current.tints[i] = QColor(json.value(Colors[i]).toString());
	}
	for (auto i = size_t(); i < FontKeys.size(); ++i) Current.fonts[i] = json.value(FontKeys[i]).toString().left(128);
	if (json.contains("avatarRadius")) {
		Current.avatarRadius = std::clamp(json.value("avatarRadius").toInt(50), 0, 50);
	}
}
void Save() {
	const auto json = QJsonObject{{ "look", Encode() }, { "slots", Slots }, { "markers", Markers }};
	Core::App().settings().writePref<QByteArray>(kPref, QJsonDocument(json).toJson(QJsonDocument::Compact));
	Core::App().saveSettingsDelayed();
	Changed.fire({});
}
QColor ToneHsl(double h, double s, double l) {
	return QColor::fromHslF(
		std::clamp(h, 0.0, 1.0),
		std::clamp(s, 0.0, 1.0),
		std::clamp(l, 0.0, 1.0));
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
int AvatarRadiusPercent() { return Current.avatarRadius; }
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
	if (k == Key::ClientStyle && value == 2) return; // Liquid Glass is WIP.
	if (value < 0 || value >= Sizes[size_t(k)] || Get(k) == value) return;
	Current.values[size_t(k)] = value;
	Save();
}
void SetColor(Tint k, QColor c) { Current.tints[size_t(k)] = c; Save(); }
void SetAvatarRadiusPercent(int percent) {
	const auto clamped = std::clamp(percent, 0, 50);
	if (Current.avatarRadius == clamped) return;
	Current.avatarRadius = clamped;
	Save();
}
std::array<QColor, 7> MaterialYouTones(QColor seed) {
	if (!seed.isValid()) seed = QColor(0xFF, 0x62, 0x6D);
	const auto rawHue = seed.hslHueF();
	const auto h = (rawHue < 0.0) ? 0.98 : rawHue;
	const auto s = (seed.hslSaturation() < 14) ? 0.04 : std::clamp(double(seed.hslSaturationF()), 0.35, 0.90);
	constexpr auto lights = std::array{ 0.14, 0.24, 0.36, 0.48, 0.62, 0.76, 0.88 };
	auto out = std::array<QColor, 7>();
	for (size_t i = 0; i < out.size(); ++i) {
		out[i] = ToneHsl(h, s * (i >= 5 ? 0.85 : 1.0), lights[i]);
	}
	return out;
}
void ApplyMaterialYouSeed(QColor seed, bool dark) {
	if (!seed.isValid()) return;
	const auto rawHue = seed.hslHueF();
	const auto h = (rawHue < 0.0) ? 0.98 : rawHue;
	const auto gray = seed.hslSaturation() < 14;
	const auto s = gray ? 0.03 : std::clamp(double(seed.hslSaturationF()), 0.40, 0.90);
	if (dark) {
		Current.tints[size_t(Tint::Accent)] = ToneHsl(h, gray ? 0.05 : std::min(0.86, s), 0.66);
		Current.tints[size_t(Tint::SettingsBg)] = ToneHsl(h, s * 0.18, 0.075);
		Current.tints[size_t(Tint::Surface)] = ToneHsl(h, s * 0.16, 0.11);
		Current.tints[size_t(Tint::Wallpaper)] = ToneHsl(h, s * 0.20, 0.06);
		Current.tints[size_t(Tint::Incoming)] = ToneHsl(h, s * 0.20, 0.165);
		Current.tints[size_t(Tint::Outgoing)] = ToneHsl(h, gray ? 0.06 : std::min(0.56, s * 0.68), 0.24);
		Current.tints[size_t(Tint::Ink)] = ToneHsl(h, s * 0.15, 0.93);
	} else {
		Current.tints[size_t(Tint::Accent)] = ToneHsl(h, gray ? 0.05 : std::min(0.80, s), 0.45);
		Current.tints[size_t(Tint::SettingsBg)] = ToneHsl(h, s * 0.18, 0.95);
		Current.tints[size_t(Tint::Surface)] = ToneHsl(h, s * 0.15, 0.98);
		Current.tints[size_t(Tint::Wallpaper)] = ToneHsl(h, s * 0.24, 0.91);
		Current.tints[size_t(Tint::Incoming)] = QColor(255, 255, 255);
		Current.tints[size_t(Tint::Outgoing)] = ToneHsl(h, gray ? 0.05 : std::min(0.65, s * 0.72), 0.87);
		Current.tints[size_t(Tint::Ink)] = ToneHsl(h, s * 0.20, 0.12);
	}
	Save();
}
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
