#pragma once
class QWidget; class QPushButton; class QLineEdit; class QPlainTextEdit;
// FULL mode renders the actual upstream controls. The hidden Qt fields retain
// the mail controller's tested state API; they are never the visible controls.
QWidget *telegramMailAction(QPushButton *state, bool primary=false);
QWidget *telegramMailInput(QLineEdit *state);
QWidget *telegramMailText(QPlainTextEdit *state);
