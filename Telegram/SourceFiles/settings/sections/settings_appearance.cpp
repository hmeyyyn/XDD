#include "ui/ui_utility.h"
#include "ui/widgets/buttons.h"
#include "settings/sections/settings_appearance.h"

#include "core/appearance.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "settings/settings_common_session.h"
#include "window/window_session_controller.h"
#include "dialogs/dialogs_key.h"
#include "history/history.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "lang/lang_keys.h"
#include "boxes/enhanced_options_box.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/chat_filters_tabs_mode.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_settings.h"
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFontDialog>
#include <QtGui/QPainter>

namespace Settings {
namespace {
using Appearance::Key;
using Appearance::Tint;
using Phrase = const tr::phrase<>*;
enum class Group { Palette, Chats, Messages, Motion, Presets };
Phrase Title(Group group) {
	switch (group) {
	case Group::Palette: return &tr::lng_look_palette;
	case Group::Chats: return &tr::lng_look_chats;
	case Group::Messages: return &tr::lng_look_messages;
	case Group::Motion: return &tr::lng_look_motion;
	case Group::Presets: return &tr::lng_look_presets;
	}
	Unexpected("Appearance group");
}
struct Choice {
	Key key;
	Group group;
	Phrase title;
	std::vector<Phrase> values;
};
const std::vector<Choice> &Choices() {
	static const auto rows = std::vector<Choice>{
		{ Key::Meta, Group::Messages, &tr::lng_look_meta,
			{ &tr::lng_look_value_default, &tr::lng_look_value_compact_time, &tr::lng_look_value_time_only, &tr::lng_look_own_line } },
		{ Key::Menu, Group::Messages, &tr::lng_look_menu,
			{ &tr::lng_look_value_full, &tr::lng_look_value_move_translate_to_more, &tr::lng_look_reply_last } },
		{ Key::Density, Group::Chats, &tr::lng_look_density,
			{ &tr::lng_look_value_default, &tr::lng_look_value_compact, &tr::lng_look_value_spacious } },
		{ Key::Journal, Group::Messages, &tr::lng_look_journal,
			{ &tr::lng_look_value_label, &tr::lng_look_value_stroke, &tr::lng_look_value_hidden } },
		{ Key::InCorners, Group::Messages, &tr::lng_look_incorners,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_small, &tr::lng_look_value_square } },
		{ Key::OutCorners, Group::Messages, &tr::lng_look_outcorners,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_small, &tr::lng_look_value_square } },
		{ Key::Tails, Group::Messages, &tr::lng_look_tails,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Key::Grouping, Group::Messages, &tr::lng_look_grouping,
			{ &tr::lng_look_value_group, &tr::lng_look_value_separate, &tr::lng_look_line_group } },
		{ Key::Quote, Group::Messages, &tr::lng_look_quote,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_rule, &tr::lng_look_value_no_background, &tr::lng_look_value_frame } },
		{ Key::QuoteLines, Group::Messages, &tr::lng_look_quotelines,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_1, &tr::lng_look_value_2, &tr::lng_look_value_3 } },
		{ Key::Mono, Group::Palette, &tr::lng_look_mono,
			{ &tr::lng_look_value_off, &tr::lng_look_value_on } },
		{ Key::Grain, Group::Palette, &tr::lng_look_grain,
			{ &tr::lng_look_value_off, &tr::lng_look_value_light, &tr::lng_look_value_strong } },
		{ Key::Unread, Group::Chats, &tr::lng_look_unread,
			{ &tr::lng_look_value_count, &tr::lng_look_value_dot, &tr::lng_look_value_mentions_only } },
		{ Key::Pinned, Group::Chats, &tr::lng_look_pinned,
			{ &tr::lng_look_value_pin, &tr::lng_look_value_divider, &tr::lng_look_value_none } },
		{ Key::Composer, Group::Messages, &tr::lng_look_composer,
			{ &tr::lng_look_value_default, &tr::lng_look_value_hide_emoji_button } },
		{ Key::Voice, Group::Messages, &tr::lng_look_voice,
			{ &tr::lng_look_value_waveform, &tr::lng_look_value_progress_line, &tr::lng_look_value_duration_only } },
		{ Key::Cursor, Group::Messages, &tr::lng_look_cursor,
			{ &tr::lng_look_value_off, &tr::lng_look_value_on } },
		{ Key::Dates, Group::Messages, &tr::lng_look_dates,
			{ &tr::lng_look_value_center, &tr::lng_look_value_left, &tr::lng_look_value_while_scrolling } },
		{ Key::Ghost, Group::Messages, &tr::lng_look_ghost,
			{ &tr::lng_look_value_off, &tr::lng_look_value_in_composer } },
		{ Key::Tray, Group::Motion, &tr::lng_look_tray,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_full, &tr::lng_look_value_outline, &tr::lng_look_silhouette } },
		{ Key::TrayCount, Group::Motion, &tr::lng_look_traycount,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_dot, &tr::lng_look_value_none } },
		{ Key::Reactions, Group::Motion, &tr::lng_look_reactions,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Key::Panels, Group::Motion, &tr::lng_look_panels,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Key::Transitions, Group::Motion, &tr::lng_look_transitions,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_off } },
		{ Key::AvatarSize, Group::Chats, &tr::lng_look_avatarsize,
			{ &tr::lng_look_avatarsize_0, &tr::lng_look_avatarsize_1, &tr::lng_look_avatarsize_2 } },
		{ Key::ListLines, Group::Chats, &tr::lng_look_listlines,
			{ &tr::lng_look_listlines_0, &tr::lng_look_listlines_1, &tr::lng_look_listlines_2 } },
		{ Key::FolderBar, Group::Chats, &tr::lng_look_folderbar,
			{ &tr::lng_look_folderbar_0, &tr::lng_look_folderbar_1 } },
		{ Key::BadgeMotion, Group::Motion, &tr::lng_look_badgemotion,
			{ &tr::lng_look_value_theme, &tr::lng_look_value_pause } },
	};
	return rows;
}

template <typename F>
rpl::producer<QString> WatchLabel(F callback) {
	return rpl::single(callback()) | rpl::then(Appearance::Changes() | rpl::map([=] { return callback(); }));
}

class Sample final : public Ui::RpWidget {
public:
	explicit Sample(QWidget *parent) : RpWidget(parent) {
		setAccessibleName(tr::lng_look_preview(tr::now));
		Appearance::Changes() | rpl::on_next([=] { update(); }, lifetime());
	}
private:
	int resizeGetHeight(int) override { return 7 * fontMetrics().height(); }
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto color = [](Tint tint, QColor fallback) {
			auto c = Appearance::Color(tint);
			if (!c.isValid()) c = fallback;
			if (Appearance::Get(Key::Mono)) { const auto v = qGray(c.rgb()); c.setRgb(v,v,v); }
			return c;
		};
		const auto line = fontMetrics().height();
		p.fillRect(rect(), color(Tint::Wallpaper, st::windowBgOver->c));
		const auto bubble = [&](bool out, QString text) {
			const auto w = std::clamp(fontMetrics().horizontalAdvance(text) + 3 * line, 0, std::max(0, width() - 2 * line));
			const auto area = QRect(out ? width() - w - line : line, out ? 4 * line : line / 2, w, 5 * line / 2);
			const auto shape = Appearance::Get(out ? Key::OutCorners : Key::InCorners);
			const auto radius = shape == 2 ? 0 : shape == 1 ? 3 : 10;
			p.setBrush(color(out ? Tint::Outgoing : Tint::Incoming, out ? st::msgOutBg->c : st::msgInBg->c));
			p.setPen(Qt::NoPen);
			p.drawRoundedRect(area, radius, radius);
			p.setPen(color(Tint::Ink, st::windowFg->c));
			p.drawText(area.adjusted(8, 2, -8, 0), Qt::AlignTop, text);
			p.drawText(area.adjusted(4, 0, -8, -2), Qt::AlignRight | Qt::AlignBottom,
				out && Appearance::Get(Key::Meta) != 2 ? u"12:42  ✓✓"_q : u"12:42"_q);
		};
		bubble(false, tr::lng_look_sample(tr::now));
		bubble(true, tr::lng_look_answer(tr::now));
	}
};

void PickColor(QWidget *parent, QColor start, Fn<void(QColor)> apply) {
	auto dialog = new QColorDialog(start.isValid() ? start : st::windowBgActive->c, parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowModality(Qt::WindowModal);
	QObject::connect(dialog, &QColorDialog::colorSelected, dialog, [=](QColor c) { apply(c); });
	dialog->show();
}

void PaletteControls(not_null<Ui::VerticalLayout*> page) {
	const auto row = [&](Tint tint, Phrase title) {
		AddButtonWithLabel(page, (*title)(), WatchLabel([=] {
			const auto c = Appearance::Color(tint);
			return c.isValid() ? c.name().toUpper() : tr::lng_look_value_theme(tr::now);
		}), st::settingsButtonNoIcon)->addClickHandler([=] {
			PickColor(page, Appearance::Color(tint), [=](QColor c) { Appearance::SetColor(tint, c); });
		});
	};
	row(Tint::Accent, &tr::lng_look_accent);
	row(Tint::SettingsBg, &tr::lng_look_settingsbg);
	row(Tint::Surface, &tr::lng_look_surface);
	row(Tint::Ink, &tr::lng_look_ink);
	row(Tint::Incoming, &tr::lng_look_incoming);
	row(Tint::Outgoing, &tr::lng_look_outgoing);
	row(Tint::Wallpaper, &tr::lng_look_wallpaper);
	AddButtonWithIcon(page, tr::lng_look_palette_reset(), st::settingsButtonNoIcon)
		->addClickHandler([] { Appearance::ResetColors(); });
}

template <Group group>
class Detail final : public Section<Detail<group>> {
public:
	Detail(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section<Detail<group>>(parent, controller) {
		const auto page = Ui::CreateChild<Ui::VerticalLayout>(this);
		if constexpr (group == Group::Palette || group == Group::Messages) {
			page->add(object_ptr<Sample>(page));
		}
		if constexpr (group == Group::Palette) PaletteControls(page);
		for (const auto &row : Choices()) {
			if (row.group != group) continue;
			const auto choice = &row;
			AddButtonWithLabel(page, (*row.title)(), WatchLabel([=] {
				return (*choice->values[Appearance::Get(choice->key)])(tr::now);
			}), st::settingsButtonNoIcon)->addClickHandler([=] {
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
					auto values = std::vector<QString>();
					for (const auto v : choice->values) values.push_back((*v)(tr::now));
					SingleChoiceBox(box, { .title = (*choice->title)(), .options = std::move(values),
						.initialSelection = Appearance::Get(choice->key),
						.callback = [=](int i) { Appearance::Set(choice->key, i); } });
				}));
			});
		}
		if constexpr (group == Group::Chats) {
			const auto fontRow = [&](Appearance::FontRole role, Phrase title) {
				AddButtonWithLabel(page, (*title)(), WatchLabel([=] {
					const auto family = Appearance::Family(role);
					return family.isEmpty() ? tr::lng_look_value_theme(tr::now) : family;
				}), st::settingsButtonNoIcon)->addClickHandler([=] {
					auto dialog = new QFontDialog(QFont(Appearance::Family(role)), page);
					dialog->setAttribute(Qt::WA_DeleteOnClose);
					dialog->setWindowModality(Qt::WindowModal);
					QObject::connect(dialog, &QFontDialog::fontSelected, dialog, [=](const QFont &font) { Appearance::SetFont(role, font.family()); });
					dialog->show();
				});
			};
			fontRow(Appearance::FontRole::Time, &tr::lng_look_font);
			fontRow(Appearance::FontRole::Sender, &tr::lng_look_sender_font);
			fontRow(Appearance::FontRole::Title, &tr::lng_look_title_font);
			AddButtonWithIcon(page, tr::lng_look_folders(), st::settingsButtonNoIcon)->addClickHandler([=] {
				controller->show(Box([](not_null<Ui::GenericBox*> box) {
					SingleChoiceBox(box, { .title = tr::lng_look_folders(),
						.options = { tr::lng_filters_tabs_default(tr::now), tr::lng_filters_tabs_text_icons(tr::now), tr::lng_filters_tabs_icons(tr::now), tr::lng_filters_tabs_text(tr::now) },
						.initialSelection = int(Core::App().settings().chatFiltersTabsMode()),
						.callback = [](int i) { Core::App().settings().setChatFiltersTabsMode(Ui::ChatsFiltersTabsMode(i)); Core::App().saveSettingsDelayed(); } });
				}));
			});
			if (const auto history = controller->activeChatCurrent().history()) {
				const auto account = controller->session().uniqueId();
				const auto peer = history->peer->id.value;
				AddButtonWithLabel(page, tr::lng_look_mark(), rpl::single(history->peer->name()), st::settingsButtonNoIcon)->addClickHandler([=] {
					PickColor(page, Appearance::Mark(account, peer), [=](QColor c) { Appearance::SetMark(account, peer, c); });
				});
				AddButtonWithIcon(page, tr::lng_look_nomark(), st::settingsButtonNoIcon)->addClickHandler([=] { Appearance::SetMark(account, peer, {}); });
			}
		}
		if constexpr (group == Group::Messages) {
			AddButtonWithIcon(page, tr::lng_look_context(), st::settingsButtonNoIcon)->addClickHandler([=] { controller->show(Box<ExtraContextMenuBox>()); });
		}
		if constexpr (group == Group::Presets) {
			for (int i = 1; i <= 3; ++i) {
				Ui::AddSubsectionTitle(page, rpl::single(tr::lng_look_slot(tr::now) + ' ' + QString::number(i)));
				AddButtonWithIcon(page, tr::lng_look_store(), st::settingsButtonNoIcon)->addClickHandler([=] { Appearance::SaveSlot(i); });
				AddButtonWithLabel(page, tr::lng_look_load(), WatchLabel([=] {
					return Appearance::HasSlot(i) ? QString() : tr::lng_look_empty(tr::now);
				}), st::settingsButtonNoIcon)->addClickHandler([=] { Appearance::LoadSlot(i); });
			}
			AddButtonWithIcon(page, tr::lng_look_reset(), st::settingsButtonNoIcon)->addClickHandler([] { Appearance::Reset(); });
		}
		Ui::ResizeFitChild(this, page);
	}
	rpl::producer<QString> title() override { return (*Title(group))(); }
};
class Customization final : public Section<Customization> {
public:
	Customization(QWidget *parent, not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
		const auto page = Ui::CreateChild<Ui::VerticalLayout>(this);
		Ui::AddSubsectionTitle(page, tr::lng_look_preview());
		page->add(object_ptr<Sample>(page));
		const auto add = [&](Group group, Type id) {
			AddButtonWithIcon(page, (*Title(group))(), st::settingsButtonNoIcon)->addClickHandler([=] { showOther(id); });
		};
		add(Group::Palette, Detail<Group::Palette>::Id());
		add(Group::Messages, Detail<Group::Messages>::Id());
		add(Group::Chats, Detail<Group::Chats>::Id());
		add(Group::Motion, Detail<Group::Motion>::Id());
		add(Group::Presets, Detail<Group::Presets>::Id());
		Ui::AddDividerText(page, tr::lng_look_help());
		Ui::ResizeFitChild(this, page);
	}
	rpl::producer<QString> title() override { return tr::lng_settings_enhanced_customization(); }
};
} // namespace
Type AppearanceId() { return Customization::Id(); }
} // namespace Settings
