/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "ui/vertical_list.h"

#include "core/appearance.h"
#include "lang/lang_text_entity.h"
#include "ui/painter.h"
#include "ui/widgets/box_content_divider.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"

namespace Ui {
namespace {

class Md3AwareDivider final : public Ui::BoxContentDivider {
public:
	using Ui::BoxContentDivider::BoxContentDivider;

protected:
	void paintEvent(QPaintEvent *e) override {
		if (!Appearance::IsMaterialV2()) {
			Ui::BoxContentDivider::paintEvent(e);
			return;
		}
		QPainter p(this);
		PainterHighQualityEnabler hq(p);
		p.fillRect(rect(), st::boxDividerBg);
		const int cap = std::min(height() / 3, style::ConvertScale(5));
		const int rad = style::ConvertScale(10);
		if (cap > 0) {
			p.setPen(Qt::NoPen);
			p.setBrush(st::boxBg);
			p.save();
			p.setClipRect(QRect(0, 0, width(), cap));
			p.drawRoundedRect(QRect(0, -2 * rad, width(), 2 * rad + cap), rad, rad);
			p.restore();
			p.save();
			p.setClipRect(QRect(0, height() - cap, width(), cap));
			p.drawRoundedRect(QRect(0, height() - cap, width(), 2 * rad + cap), rad, rad);
			p.restore();
		}
	}
};

} // namespace

void AddSkip(not_null<Ui::VerticalLayout*> container) {
	AddSkip(container, st::defaultVerticalListSkip);
}

void AddSkip(not_null<Ui::VerticalLayout*> container, int skip) {
	container->add(object_ptr<Ui::FixedHeightWidget>(container, skip));
}

void AddDivider(
		not_null<Ui::VerticalLayout*> container,
		const style::DividerBar &st) {
	container->add(object_ptr<Md3AwareDivider>(
		container,
		st::boxDividerHeight,
		st));
}

not_null<Ui::FlatLabel*> AddDividerText(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		const style::margins &margins,
		const style::DividerLabel &st,
		RectParts parts) {
	return AddDividerText(
		container,
		std::move(text) | rpl::map(tr::marked),
		margins,
		st,
		parts);
}

not_null<Ui::FlatLabel*> AddDividerText(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<TextWithEntities> text,
		const style::margins &margins,
		const style::DividerLabel &st,
		RectParts parts) {
	auto label = object_ptr<Ui::FlatLabel>(
		container,
		std::move(text),
		st.label);
	const auto result = label.data();
	container->add(object_ptr<Ui::DividerLabel>(
		container,
		std::move(label),
		margins,
		st.bar,
		parts));
	return result;
}

not_null<Ui::FlatLabel*> AddSubsectionTitle(
		not_null<Ui::VerticalLayout*> container,
		rpl::producer<QString> text,
		style::margins addPadding,
		const style::FlatLabel *st) {
	return container->add(
		object_ptr<Ui::FlatLabel>(
			container,
			std::move(text),
			st ? *st : st::defaultSubsectionTitle),
		st::defaultSubsectionTitlePadding + addPadding);
}

} // namespace Ui
