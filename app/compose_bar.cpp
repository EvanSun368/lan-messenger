#include "compose_bar.h"

#include "icons.h"
#include "theme.h"
#include "i18n.h"
#include "popup_menu.h"

#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTextDocument>
#include <QTextOption>
#include <QSizePolicy>
#include <QtMath>

namespace {
QString hintText() {
	return i18n::tr("write_a_message");
}
}

ComposeEdit::ComposeEdit(QWidget *parent)
	: QTextEdit(parent) {
	// Official InputField::Inner is a QTextEdit (not QPlainTextEdit).
	// QPlainTextDocumentLayout reports one block for a long wrapped
	// line, which is why the slim field stayed one row on Windows.
	setFrameShape(QFrame::NoFrame);
	setFrameStyle(int(QFrame::NoFrame) | QFrame::Plain);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setTabChangesFocus(true);
	setAcceptRichText(false);
	setLineWrapMode(QTextEdit::WidgetWidth);
	setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	setPlaceholderText(hintText());
	setContentsMargins(0, 0, 0, 0);
	document()->setDocumentMargin(0);
	setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	viewport()->setAutoFillBackground(false);
	setAttribute(Qt::WA_InputMethodEnabled, true);
	connect(this, &QTextEdit::textChanged, this, &ComposeEdit::syncPlaceholder);
	applyTheme();
}

void ComposeEdit::applyTheme() {
	const auto &theme = Theme::current();
	setFont(theme.fontText);
	QPalette pal = palette();
	// Paint with the compose-bar color, not Qt::transparent. A
	// transparent QTextEdit viewport punches through to the window
	// behind the bar (black/dark hole), which is especially visible
	// on Windows styles that fill the document base.
	pal.setColor(QPalette::Base, theme.composeBg);
	pal.setColor(QPalette::Window, theme.composeBg);
	pal.setColor(QPalette::Text, theme.composeFg);
	pal.setColor(QPalette::PlaceholderText, theme.placeholderFg);
	pal.setColor(QPalette::Highlight, theme.windowBgActive);
	pal.setColor(QPalette::HighlightedText, theme.windowFgActive);
	setPalette(pal);
	viewport()->setPalette(pal);
	viewport()->setAutoFillBackground(true);
	setAutoFillBackground(true);
	// A stylesheet on the field makes the placeholder stay painted
	// on top of typed text. Keep colors on the palette only.
	setStyleSheet(QString());
	syncPlaceholder();
}

bool ComposeEdit::hasVisibleText() const {
	return composing_ || !toPlainText().isEmpty();
}

void ComposeEdit::syncPlaceholder() {
	if (hasVisibleText()) {
		if (!placeholderText().isEmpty()) {
			setPlaceholderText(QString());
		}
	} else if (placeholderText() != hintText()) {
		setPlaceholderText(hintText());
	}
}

void ComposeEdit::inputMethodEvent(QInputMethodEvent *event) {
	composing_ = !event->preeditString().isEmpty();
	QTextEdit::inputMethodEvent(event);
	if (event->preeditString().isEmpty() && event->commitString().isEmpty()) {
		composing_ = false;
	} else if (!event->commitString().isEmpty() && event->preeditString().isEmpty()) {
		composing_ = false;
	}
	syncPlaceholder();
}

void ComposeEdit::paintEvent(QPaintEvent *event) {
	// Re-check before Qt paints the placeholder so IME preedit hides it.
	syncPlaceholder();
	QTextEdit::paintEvent(event);
}

ComposeBar::ComposeBar(QWidget *parent)
	: QWidget(parent) {
	const auto &theme = Theme::current();
	setFixedHeight(theme.composeMinHeight);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAutoFillBackground(true);
	setMouseTracking(true);
	edit_ = new ComposeEdit(this);
	edit_->setEnabled(false);
	edit_->installEventFilter(this);
	connect(edit_, &QTextEdit::textChanged, this, [this]() {
		relayout();
		update();
	});
	connect(&Theme::notifier(), &ThemeNotifier::changed, this, &ComposeBar::applyTheme);
	connect(&i18n::notifier(), &LanguageNotifier::changed, this, &ComposeBar::applyTheme);
	applyTheme();
}

void ComposeBar::focusEdit() {
	edit_->setFocus();
}

void ComposeBar::insertText(const QString &text) {
	if (!enabled_ || text.isEmpty()) {
		return;
	}
	edit_->insertPlainText(text);
	edit_->syncPlaceholder();
	edit_->setFocus();
	relayout();
	update();
}

void ComposeBar::clearDraft() {
	edit_->clear();
	edit_->syncPlaceholder();
	relayout();
	update();
}

QSize ComposeBar::sizeHint() const {
	const int h = height() > 0 ? height() : Theme::current().composeMinHeight;
	const int w = width() > 0 ? width() : 400;
	return QSize(w, h);
}

QSize ComposeBar::minimumSizeHint() const {
	return QSize(144, Theme::current().composeMinHeight);
}

void ComposeBar::setEnabledChat(bool enabled) {
	enabled_ = enabled;
	edit_->setEnabled(enabled);
	if (!enabled) {
		edit_->clear();
		edit_->syncPlaceholder();
	}
	update();
}

void ComposeBar::applyTheme() {
	const auto &theme = Theme::current();
	QPalette pal = palette();
	pal.setColor(QPalette::Window, theme.composeBg);
	pal.setColor(QPalette::Base, theme.composeBg);
	setPalette(pal);
	setAutoFillBackground(true);
	edit_->applyTheme();
	relayout();
	update();
}

QRect ComposeBar::attachRect() const {
	// Official TD keeps attach/emoji/send on the bottom of the compose
	// strip as the field grows (historyAttach sits at buttonsBottom).
	return QRect(8, qMax(8, height() - 8 - 36), 36, 36);
}

QRect ComposeBar::emojiRect() const {
	return QRect(width() - 88, qMax(8, height() - 8 - 36), 36, 36);
}

QRect ComposeBar::sendRect() const {
	return QRect(width() - 44, qMax(8, height() - 8 - 36), 36, 36);
}

int ComposeBar::fieldContentHeight(int fieldWidth) const {
	// Official InputField::heightAutoupdated:
	//   contentHeight = ceil(document()->size().height())
	//                 + textMargins + additionalMargins
	//   newHeight = clamp(contentHeight, minHeight, maxHeight)
	// historyComposeField textMargins are 0. QTextEdit's document
	// layout (unlike QPlainTextDocumentLayout) counts wrapped lines,
	// including a long paste with no spaces.
	auto *doc = edit_->document();
	doc->setDocumentMargin(0);
	doc->setTextWidth(qMax(1, fieldWidth));
	const auto &theme = Theme::current();
	const QFontMetrics fm(edit_->font().family().isEmpty() ? theme.fontText : edit_->font());
	const int lineH = qMax(1, fm.lineSpacing());
	return qMax(lineH, int(qCeil(doc->size().height())));
}

void ComposeBar::resizeEvent(QResizeEvent *) {
	relayout();
}

void ComposeBar::relayout() {
	if (inHeightCheck_) {
		return;
	}
	inHeightCheck_ = true;
	const auto &theme = Theme::current();
	const int fieldW = qMax(1, width() - 48 - 96);
	// Official: clamp(ceil(document()->size().height()) + margins,
	//                 heightMin 36, historyComposeFieldMaxHeight 224).
	const int minFieldH = theme.composeFieldMinHeight;
	const int maxFieldH = theme.composeFieldMaxHeight;
	const int contentH = fieldContentHeight(fieldW);
	const int fieldH = qBound(minFieldH, contentH, maxFieldH);
	// 8px padding above and below the field, like the 52px bar
	// around a 36px official field at rest.
	const int barH = qMax(theme.composeMinHeight, fieldH + 16);
	if (height() != barH) {
		setFixedHeight(barH);
		emit heightAdjusted();
	}
	// Official buttons sit at buttonsBottom as the field grows.
	edit_->setGeometry(48, qMax(8, height() - 8 - fieldH), fieldW, fieldH);
	edit_->setVerticalScrollBarPolicy(
		contentH > maxFieldH ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	inHeightCheck_ = false;
}

void ComposeBar::paintEvent(QPaintEvent *) {
	const auto &theme = Theme::current();
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), theme.composeBg);
	painter.fillRect(QRect(0, 0, width(), 1), theme.inputBorderFg);

	const auto attach = loadTintedIcon(
		QStringLiteral(":/icons/input_attach.png"),
		attachHover_ ? theme.composeIconFgOver : theme.composeIconFg,
		24);
	painter.drawPixmap(attachRect().center() - QPoint(12, 12), attach);

	const auto emoji = loadTintedIcon(
		QStringLiteral(":/icons/emoji_smile.png"),
		emojiHover_ ? theme.composeIconFgOver : theme.composeIconFg,
		24);
	painter.drawPixmap(emojiRect().center() - QPoint(12, 12), emoji);

	const bool hasText = enabled_ && edit_->hasVisibleText()
		&& !edit_->toPlainText().trimmed().isEmpty();
	const QColor sendColor = !enabled_
		? theme.composeIconFg
		: (sendHover_ || hasText ? theme.sendIconFg : theme.composeIconFg);
	const auto send = loadTintedIcon(QStringLiteral(":/icons/input_send.png"), sendColor, 24);
	if (!send.isNull() && send.width() > 0) {
		painter.drawPixmap(sendRect().center() - QPoint(send.width() / 2, send.height() / 2), send);
	} else {
		paintPlane(&painter, sendRect().adjusted(6, 6, -6, -6), sendColor);
	}
}

void ComposeBar::mousePressEvent(QMouseEvent *event) {
	if (!enabled_) {
		return;
	}
	if (attachRect().contains(event->pos())) {
		auto *menu = new PopupMenu(this);
		menu->setKind(PopupMenu::Kind::Dropdown);
		menu->setAttribute(Qt::WA_DeleteOnClose);
		QVector<PopupMenu::Item> items;
		PopupMenu::Item photo;
		photo.text = i18n::tr("photo_or_video");
		photo.icon = QStringLiteral(":/icons/menu_photo.png");
		photo.onClick = [this]() { emit attachPhoto(); };
		items.push_back(photo);
		PopupMenu::Item file;
		file.text = i18n::tr("file");
		file.icon = QStringLiteral(":/icons/menu_file.png");
		file.onClick = [this]() { emit attachFile(); };
		items.push_back(file);
		PopupMenu::Item folder;
		folder.text = i18n::tr("folder");
		folder.icon = QStringLiteral(":/icons/menu_folder.png");
		folder.onClick = [this]() { emit attachFolder(); };
		items.push_back(folder);
		menu->setItems(items);
		// Official DropdownMenu is parented to the history pane and
		// moveToLeft(0, buttonsBottom - height), with 10px shadow padding
		// so the card sits just above the compose strip, left-aligned.
		menu->popupAnchoredBottomLeft(mapToGlobal(QPoint(0, 0)));
		return;
	}
	if (emojiRect().contains(event->pos())) {
		emit emojiToggled();
		return;
	}
	if (sendRect().contains(event->pos())) {
		trySend();
	}
}

void ComposeBar::mouseMoveEvent(QMouseEvent *event) {
	const bool send = sendRect().contains(event->pos());
	const bool attach = attachRect().contains(event->pos());
	const bool emoji = emojiRect().contains(event->pos());
	if (send != sendHover_ || attach != attachHover_ || emoji != emojiHover_) {
		sendHover_ = send;
		attachHover_ = attach;
		emojiHover_ = emoji;
		update();
	}
}

bool ComposeBar::eventFilter(QObject *watched, QEvent *event) {
	if (watched == edit_ && event->type() == QEvent::KeyPress) {
		auto *key = static_cast<QKeyEvent *>(event);
		if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
			&& !(key->modifiers() & Qt::ShiftModifier)) {
			trySend();
			return true;
		}
	}
	return QWidget::eventFilter(watched, event);
}

void ComposeBar::trySend() {
	if (!enabled_) {
		return;
	}
	const auto text = edit_->toPlainText();
	if (text.trimmed().isEmpty()) {
		return;
	}
	emit sendText(text);
	// Do not clear here. The parent clears only after a successful send
	// so a blocked offline send keeps the draft.
}
