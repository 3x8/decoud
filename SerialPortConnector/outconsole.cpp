#include "outconsole.h"

#include <QScrollBar>

OutConsole::OutConsole(QWidget *parent) : QPlainTextEdit(parent) {
}

void OutConsole::putData(const QByteArray &data) {
    insertPlainText(data);
    QScrollBar *bar = verticalScrollBar();
    bar->setValue(bar->maximum());
}
