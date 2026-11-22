#include "core/material_color_picker.h"

#include "core/appearance.h"
#include "lang/lang_keys.h"

#include <QtGui/QLinearGradient>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>

#include <algorithm>
#include <array>

namespace Core {
namespace {

constexpr auto kSeeds = std::array<uint32, 8>{
	0xFF626Du, // Sakura Crimson
	0xC39BFFu, // Monet Lavender
	0x7CACF8u, // Pixel Sky
	0x5DDBACu, // Mint Jade
	0xFFB951u, // Amber Honey
	0xFF8A80u, // Coral Blossom
	0x4FD8EBu, // Ice Cyan
	0xB0BEC5u, // Neutral Slate
};

class SvCanvas final : public QWidget {
public:
	explicit SvCanvas(QWidget *parent, Fn<void(double, double)> changed)
	: QWidget(parent), _changed(std::move(changed)) {
		setCursor(Qt::CrossCursor);
	}

	void setHsvValue(double h, double s, double v) {
		const auto hueInt = int(std::round(h * 359.0));
		if (_cachedHue != hueInt) {
			_cachedHue = hueInt;
			_cache = QImage();
		}
		_h = h;
		_s = std::clamp(s, 0.0, 1.0);
		_v = std::clamp(v, 0.0, 1.0);
		update();
	}

protected:
	void resizeEvent(QResizeEvent *) override {
		_cache = QImage();
	}

	void paintEvent(QPaintEvent *) override {
		if (width() <= 2 || height() <= 2) return;
		if (_cache.size() != size()) {
			_cache = QImage(size(), QImage::Format_ARGB32_Premultiplied);
			for (int y = 0; y < height(); ++y) {
				const auto v = 1.0 - double(y) / std::max(1, height() - 1);
				auto *line = reinterpret_cast<QRgb*>(_cache.scanLine(y));
				for (int x = 0; x < width(); ++x) {
					const auto s = double(x) / std::max(1, width() - 1);
					line[x] = QColor::fromHsvF(_h, s, v).rgba();
				}
			}
		}
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QPainterPath clip;
		clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
		p.save();
		p.setClipPath(clip);
		p.drawImage(0, 0, _cache);
		p.restore();
		p.setPen(QPen(QColor(255, 255, 255, 38), 1.0));
		p.setBrush(Qt::NoBrush);
		p.drawPath(clip);

		const auto cx = _s * (width() - 1);
		const auto cy = (1.0 - _v) * (height() - 1);
		p.setPen(QPen(QColor(0, 0, 0, 170), 3.0));
		p.setBrush(QColor::fromHsvF(_h, _s, _v));
		p.drawEllipse(QPointF(cx, cy), 8.0, 8.0);
		p.setPen(QPen(Qt::white, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(QPointF(cx, cy), 8.0, 8.0);
	}

	void mousePressEvent(QMouseEvent *e) override { pick(e->pos()); }
	void mouseMoveEvent(QMouseEvent *e) override {
		if (e->buttons() & Qt::LeftButton) pick(e->pos());
	}

private:
	void pick(QPoint pos) {
		_s = std::clamp(double(pos.x()) / std::max(1, width() - 1), 0.0, 1.0);
		_v = std::clamp(1.0 - double(pos.y()) / std::max(1, height() - 1), 0.0, 1.0);
		update();
		_changed(_s, _v);
	}

	Fn<void(double, double)> _changed;
	QImage _cache;
	int _cachedHue = -1;
	double _h = 0.98;
	double _s = 0.62;
	double _v = 1.0;
};

class HueBar final : public QWidget {
public:
	explicit HueBar(QWidget *parent, Fn<void(double)> changed)
	: QWidget(parent), _changed(std::move(changed)) {
		setCursor(Qt::PointingHandCursor);
	}

	void setHue(double h) {
		_h = std::clamp(h, 0.0, 1.0);
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto r = QRectF(rect()).adjusted(2, 4, -2, -4);
		QLinearGradient grad(r.left(), 0, r.right(), 0);
		for (int i = 0; i <= 6; ++i) {
			grad.setColorAt(i / 6.0, QColor::fromHsvF(std::min(0.999, i / 6.0), 0.92, 0.98));
		}
		p.setPen(QPen(QColor(255, 255, 255, 35), 1.0));
		p.setBrush(grad);
		p.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);

		const auto cx = r.left() + _h * r.width();
		p.setPen(QPen(QColor(0, 0, 0, 150), 3.0));
		p.setBrush(QColor::fromHsvF(std::min(0.999, _h), 0.95, 0.98));
		p.drawEllipse(QPointF(cx, rect().center().y()), 8.5, 8.5);
		p.setPen(QPen(Qt::white, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(QPointF(cx, rect().center().y()), 8.5, 8.5);
	}

	void mousePressEvent(QMouseEvent *e) override { pick(e->pos()); }
	void mouseMoveEvent(QMouseEvent *e) override {
		if (e->buttons() & Qt::LeftButton) pick(e->pos());
	}

private:
	void pick(QPoint pos) {
		_h = std::clamp(double(pos.x() - 2) / std::max(1, width() - 4), 0.0, 0.999);
		update();
		_changed(_h);
	}

	Fn<void(double)> _changed;
	double _h = 0.98;
};

class SwatchRow final : public QWidget {
public:
	SwatchRow(QWidget *parent, bool circular, Fn<void(QColor)> picked)
	: QWidget(parent), _circular(circular), _picked(std::move(picked)) {
		setCursor(Qt::PointingHandCursor);
	}

	void setColors(std::vector<QColor> colors, QColor active) {
		_colors = std::move(colors);
		_active = active;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		if (_colors.empty()) return;
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto count = int(_colors.size());
		const auto gap = 6.0;
		const auto cellW = (width() - gap * (count - 1)) / count;
		for (int i = 0; i < count; ++i) {
			const auto x = i * (cellW + gap);
			const auto c = _colors[i];
			const auto selected = _active.isValid() && (c.rgb() == _active.rgb());
			if (_circular) {
				const auto side = std::min(cellW, double(height() - 4));
				const auto box = QRectF(x + (cellW - side) / 2.0, (height() - side) / 2.0, side, side);
				p.setPen(selected ? QPen(Qt::white, 2.2) : QPen(QColor(255, 255, 255, 35), 1.0));
				p.setBrush(c);
				p.drawEllipse(box);
			} else {
				const auto radius = (i == 0 || i == count - 1) ? 10.0 : 5.0;
				const auto box = QRectF(x, 2, cellW, height() - 4);
				p.setPen(selected ? QPen(Qt::white, 2.0) : QPen(QColor(255, 255, 255, 28), 1.0));
				p.setBrush(c);
				p.drawRoundedRect(box, radius, radius);
			}
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (_colors.empty()) return;
		const auto count = int(_colors.size());
		const auto idx = std::clamp(int(e->pos().x() * count / std::max(1, width())), 0, count - 1);
		_picked(_colors[idx]);
	}

private:
	bool _circular = false;
	Fn<void(QColor)> _picked;
	std::vector<QColor> _colors;
	QColor _active;
};

class MaterialColorDialog final : public QDialog {
public:
	MaterialColorDialog(
		QWidget *parent,
		QColor initial,
		Fn<void(QColor)> apply,
		bool allowFullTheme,
		bool allowReset)
	: QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint)
	, _apply(std::move(apply)) {
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_DeleteOnClose);
		setModal(true);
		setFixedSize(444, allowFullTheme ? 520 : 476);

		const auto accent = Appearance::Color(Appearance::Tint::Accent);
		const auto start = initial.isValid()
			? initial
			: (accent.isValid() ? accent : QColor(0xFF, 0x62, 0x6D));
		const auto hf = start.hsvHueF();
		_h = (hf < 0) ? 0.98 : double(hf);
		_s = double(start.hsvSaturationF());
		_v = double(start.valueF());

		setStyleSheet(uR"(
			QLabel { color: #f1eef0; background: transparent; font-size: 12px; }
			QLabel#title { font-size: 15px; font-weight: 600; }
			QLabel#sub { color: #a79fa8; font-size: 11px; }
			QLineEdit {
				background: #1d1b22; color: #f5f2f6; border: 1px solid #38333f;
				border-radius: 9px; padding: 6px 10px; font-family: Consolas, monospace; font-size: 13px;
			}
			QLineEdit:focus { border-color: #ff626d; }
			QPushButton {
				background: #232029; color: #ece7ee; border: 1px solid #37323e;
				border-radius: 10px; padding: 7px 14px; font-size: 12px; font-weight: 500;
			}
			QPushButton:hover { background: #302b38; }
			QPushButton#primary {
				background: #ee5463; color: #ffffff; border: none; font-weight: 600;
			}
			QPushButton#primary:hover { background: #ff6473; }
			QPushButton#tonal {
				background: #38232b; color: #ffd9de; border: 1px solid #5e3541; font-weight: 600;
			}
			QPushButton#tonal:hover { background: #482b35; }
			QPushButton#closeBtn {
				background: transparent; border: none; color: #b5aeb7; font-size: 16px; padding: 0;
			}
			QPushButton#closeBtn:hover { background: #2f2a35; color: #ffffff; border-radius: 8px; }
		)"_q);

		auto *title = new QLabel(tr::lng_look_material_you_title(tr::now), this);
		title->setObjectName(u"title"_q);
		title->setGeometry(20, 16, 360, 22);

		auto *sub = new QLabel(tr::lng_look_material_you_sub(tr::now), this);
		sub->setObjectName(u"sub"_q);
		sub->setGeometry(20, 38, 380, 18);

		auto *closeBtn = new QPushButton(u"✕"_q, this);
		closeBtn->setObjectName(u"closeBtn"_q);
		closeBtn->setGeometry(width() - 44, 14, 28, 28);
		connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

		_canvas = new SvCanvas(this, [=](double s, double v) {
			_s = s;
			_v = v;
			syncFromHsv(true);
		});
		_canvas->setGeometry(20, 64, 404, 148);

		_hue = new HueBar(this, [=](double h) {
			_h = h;
			syncFromHsv(true);
		});
		_hue->setGeometry(20, 220, 404, 22);

		auto *seedsLbl = new QLabel(tr::lng_look_material_you_seeds(tr::now), this);
		seedsLbl->setObjectName(u"sub"_q);
		seedsLbl->setGeometry(20, 248, 404, 16);

		_seeds = new SwatchRow(this, true, [=](QColor c) { setColor(c, true); });
		_seeds->setGeometry(20, 267, 404, 32);

		auto *tonesLbl = new QLabel(tr::lng_look_material_you_tones(tr::now), this);
		tonesLbl->setObjectName(u"sub"_q);
		tonesLbl->setGeometry(20, 305, 404, 16);

		_tones = new SwatchRow(this, false, [=](QColor c) { setColor(c, true); });
		_tones->setGeometry(20, 324, 404, 30);

		auto *hexLbl = new QLabel(tr::lng_look_material_you_hex(tr::now), this);
		hexLbl->setObjectName(u"sub"_q);
		hexLbl->setGeometry(20, 364, 150, 16);

		_hex = new QLineEdit(this);
		_hex->setGeometry(20, 383, 128, 34);
		_hex->setMaxLength(7);
		_hex->setValidator(new QRegularExpressionValidator(
			QRegularExpression(u"^#?[0-9A-Fa-f]{0,6}$"_q),
			_hex));
		connect(_hex, &QLineEdit::textEdited, this, [=](const QString &text) {
			const auto clean = text.startsWith('#') ? text : ('#' + text);
			if (clean.size() == 7) {
				const QColor parsed(clean);
				if (parsed.isValid()) setColor(parsed, false);
			}
		});

		auto y = 428;
		if (allowFullTheme) {
			auto *fullBtn = new QPushButton(tr::lng_look_material_you_apply_all(tr::now), this);
			fullBtn->setObjectName(u"tonal"_q);
			fullBtn->setGeometry(20, y, 404, 34);
			connect(fullBtn, &QPushButton::clicked, this, [=] {
				const auto c = currentColor();
				Appearance::ApplyMaterialYouSeed(c, true);
				if (_apply) _apply(Appearance::Color(Appearance::Tint::Accent));
				accept();
			});
			y += 42;
		}

		if (allowReset) {
			auto *resetBtn = new QPushButton(tr::lng_look_palette_reset(tr::now), this);
			resetBtn->setGeometry(20, y, 118, 34);
			connect(resetBtn, &QPushButton::clicked, this, [=] {
				if (_apply) _apply(QColor());
				accept();
			});
		}

		auto *cancelBtn = new QPushButton(tr::lng_cancel(tr::now), this);
		cancelBtn->setGeometry(width() - 228, y, 94, 34);
		connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

		auto *okBtn = new QPushButton(tr::lng_look_material_you_pick(tr::now), this);
		okBtn->setObjectName(u"primary"_q);
		okBtn->setGeometry(width() - 126, y, 106, 34);
		connect(okBtn, &QPushButton::clicked, this, [=] {
			if (_apply) _apply(currentColor());
			accept();
		});

		syncFromHsv(true);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto box = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
		p.setPen(QPen(QColor(0x38, 0x33, 0x3F), 1.2));
		p.setBrush(QColor(0x14, 0x13, 0x18));
		p.drawRoundedRect(box, 18.0, 18.0);

		// Live Material You preview pill next to HEX input.
		const auto c = currentColor();
		const auto preview = QRectF(158, 366, 266, 51);
		p.setPen(QPen(QColor(0x33, 0x2F, 0x3A), 1.0));
		p.setBrush(QColor(0x0E, 0x0D, 0x11));
		p.drawRoundedRect(preview, 12.0, 12.0);

		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawEllipse(QRectF(168, 376, 31, 31));

		const auto tones = Appearance::MaterialYouTones(c);
		p.setBrush(tones[1]);
		p.drawRoundedRect(QRectF(208, 374, 96, 16), 8.0, 8.0);
		p.setBrush(c);
		p.drawRoundedRect(QRectF(312, 392, 102, 17), 8.5, 8.5);
		p.setPen(qGray(c.rgb()) > 155 ? QColor(20, 18, 22) : Qt::white);
		p.drawText(QRectF(312, 392, 102, 17), Qt::AlignCenter, c.name().toUpper());
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton && e->pos().y() < 58) {
			_dragging = true;
			_dragOffset = e->globalPos() - frameGeometry().topLeft();
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (_dragging && (e->buttons() & Qt::LeftButton)) {
			move(e->globalPos() - _dragOffset);
		}
	}

	void mouseReleaseEvent(QMouseEvent *) override {
		_dragging = false;
	}

private:
	QColor currentColor() const {
		return QColor::fromHsvF(std::clamp(_h, 0.0, 0.999), std::clamp(_s, 0.0, 1.0), std::clamp(_v, 0.0, 1.0));
	}

	void setColor(QColor c, bool updateHex) {
		const auto hf = c.hsvHueF();
		if (hf >= 0) _h = double(hf);
		_s = double(c.hsvSaturationF());
		_v = double(c.valueF());
		syncFromHsv(updateHex);
	}

	void syncFromHsv(bool updateHex) {
		const auto c = currentColor();
		_canvas->setHsvValue(_h, _s, _v);
		_hue->setHue(_h);
		if (updateHex) {
			_hex->setText(c.name().toUpper());
		}
		auto seeds = std::vector<QColor>();
		seeds.reserve(kSeeds.size());
		for (const auto rgb : kSeeds) seeds.emplace_back(QRgb(0xFF000000u | rgb));
		_seeds->setColors(std::move(seeds), c);

		const auto tonesArr = Appearance::MaterialYouTones(c);
		_tones->setColors(std::vector<QColor>(tonesArr.begin(), tonesArr.end()), c);
		update();
	}

	Fn<void(QColor)> _apply;
	SvCanvas *_canvas = nullptr;
	HueBar *_hue = nullptr;
	SwatchRow *_seeds = nullptr;
	SwatchRow *_tones = nullptr;
	QLineEdit *_hex = nullptr;
	double _h = 0.98;
	double _s = 0.62;
	double _v = 1.0;
	bool _dragging = false;
	QPoint _dragOffset;
};

} // namespace

void ShowMaterialColorPicker(
		QWidget *parent,
		QColor initial,
		Fn<void(QColor)> apply,
		bool allowGenerateFullTheme,
		bool allowReset) {
	auto *dialog = new MaterialColorDialog(
		parent ? parent->window() : nullptr,
		initial,
		std::move(apply),
		allowGenerateFullTheme,
		allowReset);
	if (const auto host = parent ? parent->window() : nullptr) {
		const auto geo = host->frameGeometry();
		dialog->move(
			geo.center().x() - dialog->width() / 2,
			geo.center().y() - dialog->height() / 2);
	}
	dialog->show();
	dialog->raise();
	dialog->activateWindow();
}

} // namespace Core
