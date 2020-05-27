#include "outconsole.h"

#include <QScrollBar>

OutConsole::OutConsole(QWidget *parent) : QPlainTextEdit(parent) {
  // noop
}

void OutConsole::putData(const QByteArray &data) {
  insertPlainText(data);
  QScrollBar *bar = verticalScrollBar();
  bar->setValue(bar->maximum());
}
