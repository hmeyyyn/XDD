// Local visual preferences. Never store account or privacy settings here.
#pragma once

#include "rpl/producer.h"
#include <QtGui/QColor>
#include <array>

namespace Appearance {
enum class Key {
	ClientStyle,
	Meta, Menu, Density, Journal, InCorners, OutCorners, Tails, Grouping,
	Quote, QuoteLines, Mono, Grain, Unread, Pinned, Composer, Voice, Cursor,
	Dates, Ghost, Tray, TrayCount, Reactions, Panels, Transitions,
	BadgeMotion, AvatarSize, ListLines, FolderBar, Count
};
enum class Tint { Accent, SettingsBg, Surface, Ink, Incoming, Outgoing, Wallpaper, Count };
int Get(Key key);
[[nodiscard]] inline bool IsMaterialV2() { return Get(Key::ClientStyle) == 1; }
QColor Color(Tint tint);
int AvatarRadiusPercent();
enum class FontRole { Time, Sender, Title };
QString Family(FontRole role);
QColor Mark(quint64 account, quint64 peer);
void Set(Key key, int value);
void SetColor(Tint tint, QColor color);
void SetAvatarRadiusPercent(int percent);
std::array<QColor, 7> MaterialYouTones(QColor seed);
void ApplyMaterialYouSeed(QColor seed, bool dark = true);
void SetFont(FontRole role, QString family);
void SetMark(quint64 account, quint64 peer, QColor color);
void ResetColors();
void Reset();
void SaveSlot(int slot);
bool LoadSlot(int slot);
bool HasSlot(int slot);
void Start();
rpl::producer<> Changes();
} // namespace Appearance
