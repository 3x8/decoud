#include "widget.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
  //QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  QApplication a(argc, argv);

  Widget w;
  w.show();
  return (a.exec());
}
