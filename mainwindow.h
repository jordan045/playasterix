#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>

QT_BEGIN_NAMESPACE
class QSettings;
class QFile;
class QTimer;
//class QAction;
class QLabel;
//class QMenu;
class QScrollArea;
//class QScrollBar;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    void CreateActions();
    void setImage();

    QUdpSocket *socket_asterix;
    QSettings *settings;
    QFile *file;
    QTimer *timer;
    QLabel *imageLabel;
    QScrollArea *scrollArea;
    QImage image;
private slots:
    void readyRead();
    void bytesWriten(quint64 bytes);
    void read_file();
    void open();
};
#endif // MAINWINDOW_H
