#pragma once

#include "base/basic_types.h"
#include <QtGui/QColor>

class QWidget;

namespace Core {

void ShowMaterialColorPicker(
	QWidget *parent,
	QColor initial,
	Fn<void(QColor)> apply,
	bool allowGenerateFullTheme = true,
	bool allowReset = true);

} // namespace Core
