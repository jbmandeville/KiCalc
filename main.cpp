#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow win;

    // read command line, import data, initial analysis
    win.readCommandLine();

    win.show();

    return app.exec();
}
