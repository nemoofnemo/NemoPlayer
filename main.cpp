#include "NemoPlayer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NemoPlayer w;
    w.show();
    return a.exec();
}
