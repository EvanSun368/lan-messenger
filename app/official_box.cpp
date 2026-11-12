#include "official_box.h"

#include "icons.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace {
constexpr int kTitleHeight = 48;
constexpr int kTitleLeft = 24;
constexpr int kTitleTop = 13;
constexpr int kPadLeft = 24;
constexpr int kPadRight = 24;
constexpr int kPadBottom = 8;
constexpr int kButtonPadRight = 10;
constexpr int kButtonPadBottom = 10;
constexpr int kButtonPadTop = 10;
constexpr int kButtonHeight = 34;
}

OfficialBox::OfficialBox(QWidget *parent)
	: QDialog(parent) {
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setAttribute(Qt::WA_TranslucentBackground);
	setModal(true);
	setMouseTracking(true);

	card_ = new QWidget(this);
	card_->setAttribute(Qt::WA_TranslucentBackground);
	auto *frameLayout = new QVBoxLayout(card_);
	frameLayout->setContentsMargins(0, kTitleHeight, 0, 0);
	frameLayout->setSpacing(0);

	body_ = new QWidget(card_);
	bodyLayout_ = new QVBoxLayout(body_);
	bodyLayout_->setContentsMargins(kPadLeft, 8, kPadRight, kPadBottom);
	bodyLayout_->setSpacing(10);
	frameLayout->addWidget(body_, 1);

	buttons_ = new QWidget(card_);
	buttons_->setFixedHeight(kButtonPadTop + kButtonHeight + kButtonPadBottom);
	auto *btnLayout = new QHBoxLayout(buttons_);
	btnLayout->setContentsMargins(kPadLeft, kButtonPadTop, kButtonPadRight, kButtonPadBottom);
	btnLayout->setSpacing(4);
	btnLayout->addStretch();
	frameLayout->addWidget(buttons_);

	applyChrome();
}

void OfficialBox::setBoxTitle(const QString &title) {
	title_ = title;
	setWindowTitle(title);
	update();
}

void OfficialBox::setBoxWidth(int width) {
	boxWidth_ = width;
	layoutLayer();
}

QRect OfficialBox::cardRect() const {
	const int w = boxWidth_;
	const int h = card_ ? card_->height() : 200;
	const int x = (width() - w) / 2;
	const int y = (height() - h) / 2;
	return QRect(x, y, w, h);
}

QRect OfficialBox::closeRect() const {
	const QRect card = cardRect();
	return QRect(card.right() - kTitleHeight + 1, card.top(), kTitleHeight, kTitleHeight);
}

void OfficialBox::styleTextButton(QPushButton *button, bool attention) {
	const auto &theme = Theme::current();
	const QColor fg = attention ? theme.attentionFg : theme.windowActiveTextFg;
	button->setCursor(Qt::PointingHandCursor);
	button->setFont(theme.fontBoxButton);
	button->setFixedHeight(kButtonHeight);
	button->setMinimumWidth(30);
	button->setStyleSheet(
		QStringLiteral(
			"QPushButton { background: transparent; color: %1; border: none;"
			"  padding: 0 12px; font-weight: 600; border-radius: 4px; }"
			" QPushButton:hover { background: %2; }"
			" QPushButton:disabled { color: %3; }")
			.arg(fg.name(), theme.lightButtonBgOver.name(QColor::HexArgb),
				theme.windowSubTextFg.name()));
}

QPushButton *OfficialBox::addConfirmButton(const QString &text) {
	auto *button = new QPushButton(text, buttons_);
	styleTextButton(button);
	static_cast<QHBoxLayout *>(buttons_->layout())->addWidget(button);
	connect(button, &QPushButton::clicked, this, &QDialog::accept);
	return button;
}

QPushButton *OfficialBox::addCancelButton(const QString &text) {
	auto *button = new QPushButton(text, buttons_);
	styleTextButton(button);
	auto *layout = static_cast<QHBoxLayout *>(buttons_->layout());
	const int index = layout->count();
	if (index > 1) {
		layout->insertWidget(index - 1, button);
	} else {
		layout->addWidget(button);
	}
	connect(button, &QPushButton::clicked, this, &QDialog::reject);
	return button;
}

QPushButton *OfficialBox::addCloseButton(const QString &text) {
	return addConfirmButton(text);
}

void OfficialBox::applyChrome() {
	const auto &theme = Theme::current();
	auto pal = palette();
	pal.setColor(QPalette::Window, Qt::transparent);
	pal.setColor(QPalette::WindowText, theme.windowFg);
	pal.setColor(QPalette::Text, theme.windowFg);
	setPalette(pal);
	update();
}

void OfficialBox::layoutLayer() {
	QWidget *host = parentWidget() ? parentWidget()->window() : nullptr;
	if (host) {
		const QRect g = host->frameGeometry();
		setGeometry(g);
	} else if (width() < boxWidth_ + 80) {
		resize(qMax(800, boxWidth_ + 80), 560);
	}
	const int cardH = qMax(card_->sizeHint().height(), 160);
	const int w = boxWidth_;
	const int x = (width() - w) / 2;
	const int y = (height() - cardH) / 2;
	card_->setGeometry(x, y, w, cardH);
	update();
}

void OfficialBox::showEvent(QShowEvent *event) {
	applyChrome();
	adjustSize();
	layoutLayer();
	QDialog::showEvent(event);
	layoutLayer();
}

void OfficialBox::keyPressEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape) {
		reject();
		return;
	}
	QDialog::keyPressEvent(event);
}

void OfficialBox::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), theme.layerBg);

	const QRect card = cardRect();
	for (int i = shadow_; i >= 1; --i) {
		const qreal t = qreal(i) / qreal(shadow_);
		const int alpha = qRound(22.0 * t * t);
		painter.setPen(QColor(0, 0, 0, alpha));
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(
			card.adjusted(-i, -i, i, i + (i > shadow_ / 2 ? 1 : 0)),
			theme.boxRadius + i * 0.3,
			theme.boxRadius + i * 0.3);
	}
	QPainterPath path;
	path.addRoundedRect(card, theme.boxRadius, theme.boxRadius);
	painter.fillPath(path, theme.boxBg);

	painter.setFont(theme.fontBoxTitle);
	painter.setPen(theme.boxTitleFg);
	const QRect titleRect(
		card.x() + kTitleLeft,
		card.y() + kTitleTop,
		card.width() - kTitleLeft - kTitleHeight,
		24);
	painter.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title_);

	if (closeHover_) {
		painter.setPen(Qt::NoPen);
		painter.setBrush(theme.windowBgOver);
		painter.drawEllipse(closeRect().adjusted(8, 8, -8, -8));
	}
	const auto icon = loadTintedIcon(
		QStringLiteral(":/icons/box_button_close.png"),
		closeHover_ ? theme.windowSubTextFgOver : theme.boxTitleCloseFg,
		16);
	if (!icon.isNull()) {
		const QRect r = closeRect();
		painter.drawPixmap(
			r.center().x() - icon.width() / 2,
			r.center().y() - icon.height() / 2,
			icon);
	}
}

void OfficialBox::mousePressEvent(QMouseEvent *event) {
	if (closeRect().contains(event->pos())) {
		reject();
		return;
	}
	if (!cardRect().contains(event->pos())) {
		reject();
		return;
	}
	QDialog::mousePressEvent(event);
}

void OfficialBox::mouseMoveEvent(QMouseEvent *event) {
	const bool hover = closeRect().contains(event->pos());
	if (hover != closeHover_) {
		closeHover_ = hover;
		update();
	}
	QDialog::mouseMoveEvent(event);
}

void OfficialBox::resizeEvent(QResizeEvent *event) {
	QDialog::resizeEvent(event);
	layoutLayer();
}
