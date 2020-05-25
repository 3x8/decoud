#pragma once

#include <QPlainTextEdit>

class OutConsole: public QPlainTextEdit {
  Q_OBJECT

  signals:
    void getData(const QByteArray &data);

  public:
    explicit OutConsole(QWidget *parent = nullptr);
    void putData(const QByteArray &data);
};
