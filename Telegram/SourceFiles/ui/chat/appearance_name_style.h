#pragma once
#include "core/appearance.h"
#include "styles/style_widgets.h"
#include <map>

namespace Ui {
inline const style::TextStyle &AppearanceNameStyle(const style::TextStyle &base, const QString &family) {
	if (family.isEmpty()) return base;
	static std::map<std::pair<QString, int>, style::TextStyle> styles;
	const auto [it, added] = styles.try_emplace({family, base.font->size()}, base);
	if (added) it->second.font = style::font(base.font->size(), base.font->flags(), family);
	return it->second;
}
} // namespace Ui
