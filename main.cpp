#include "NemoPlayer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NemoPlayer w;
    w.show();
    int ret = a.exec();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return ret;
}
