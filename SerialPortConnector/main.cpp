#include "widget.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);

  //  a.setStyle(QStyleFactory::create("Fusion"));
    Widget w;
    w.show();
    return a.exec();
}
