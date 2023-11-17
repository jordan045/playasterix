#include "mainwindow.h"
#include <QApplication>
#include <QSurfaceFormat>

// https://code.qt.io/cgit/qt/qtbase.git/tree/examples/widgets/widgets/imageviewer?h=5.15

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
//    QSurfaceFormat format;
//    format.setDepthBufferSize(24);
//    format.setStencilBufferSize(8);
//    format.setColorSpace(QSurfaceFormat::sRGBColorSpace);
//    format.setSamples(4);
//    format.setProfile(QSurfaceFormat::CompatibilityProfile);
//    //format.setProfile(QSurfaceFormat::CoreProfile);
//    QSurfaceFormat::setDefaultFormat(format);

    MainWindow w;
    w.show();
    return a.exec();
}
