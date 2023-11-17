#include "mainwindow.h"
#include <QtWidgets>
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QTime>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QtEndian>

#include <chrono>
#include <thread>

const uint8_t I240_010=1<<7;
const uint8_t I240_000=1<<6;
const uint8_t I240_020=1<<5;
const uint8_t I240_030=1<<4;
const uint8_t I240_040=1<<3;
const uint8_t I240_041=1<<2;
const uint8_t I240_048=1<<1;
const uint8_t FSPEC_EXTEND=1;
const uint8_t I240_049=1<<7;
const uint8_t I240_050=1<<6;
const uint8_t I240_051=1<<5;
const uint8_t I240_052=1<<4;
const uint8_t I240_140=1<<3;

typedef struct __attribute__((packed)){
    uint8_t cat;
    uint16_t length;
    uint8_t fspec0;
    uint8_t fspec1;
    //char *data_fields;
} asterix_data_block;

QMap<QString, QVariant> json;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),imageLabel(new QLabel), scrollArea(new QScrollArea)
{
    setWindowTitle("Asterix Player vSIAG2023a - Diego Martínez");
    setMinimumSize(640, 480);
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setVisible(false);
    setCentralWidget(scrollArea);
    //image = QImage(1024, 1024, QImage::Format_Grayscale8);
    QImageReader reader("blank.png");
    reader.setAutoTransform(true);
    image = reader.read();
    setImage();

    CreateActions();

    QApplication::setOrganizationName(QStringLiteral("siag"));
    QApplication::setApplicationName(QStringLiteral("asterix viewer"));
    settings = new QSettings("settings.ini", QSettings::IniFormat, this);
    qint16 port_local = settings->value("main/port_local", 20000).toInt();
    qint16 port_remote = settings->value("main/port_remote", 30000).toInt();
    QString ip_remote = settings->value("main/ip_remote", "127.0.0.1").toString();
    QString filename = settings->value("main/filename", "salida_zw06.bin").toString();
    // en scanview configurar Asterix Video Client
    // UDP IP source 127.0.0.1
    //     port 14433
    //     Device Any
    // click en Start Listening
    // click en Plugin Standard Display, en la esquina inferior izquierda
    //   radar sources Asterix video client
    //   radar fading enable, full sweeps only disable
    file = new QFile();
    file->setFileName(filename);
    file->open(QIODevice::ReadOnly);

    socket_asterix = new QUdpSocket(this);
    socket_asterix->bind(QHostAddress::AnyIPv4, port_local);
    //socket_asterix->connectToHost(QHostAddress::LocalHost, port_remote);
    socket_asterix->connectToHost(ip_remote, port_remote);
    connect(socket_asterix, SIGNAL(readyRead()), this, SLOT(readyRead()));
    //connect(socket_asterix, SIGNAL(bytesWriten(quint64)), this, SLOT(bytesWriten(quint64)));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(read_file()));
    qDebug() << QTime::currentTime();
    timer->start(200);
}

MainWindow::~MainWindow()
{
    file->close();
}

void MainWindow::CreateActions()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QToolBar *fileToolBar = addToolBar(tr("File"));
    const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/images/actions/document-open.png"));
    QAction *openAct = new QAction(openIcon, tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, &QAction::triggered, this, &MainWindow::open);
    fileMenu->addAction(openAct);
    fileToolBar->addAction(openAct);
}

void MainWindow::setImage()
{
//    image = newImage;
    if (image.colorSpace().isValid())
        image.convertToColorSpace(QColorSpace::SRgb);
    imageLabel->setPixmap(QPixmap::fromImage(image));

//    scaleFactor = 1.0;
    imageLabel->adjustSize();
    scrollArea->setVisible(true);

//    printAct->setEnabled(true);
//    fitToWindowAct->setEnabled(true);
//    updateActions();

//    if (!fitToWindowAct->isChecked())
//        imageLabel->adjustSize();
}

void MainWindow::readyRead()
{
    QByteArray Buffer;
    QHostAddress sender;
    quint16 senderPort;
    asterix_data_block *data_block;
    uint8_t *data_fields;
    int offset;

    while(socket_asterix->hasPendingDatagrams()){
        Buffer.resize(socket_asterix->pendingDatagramSize());
        socket_asterix->readDatagram(Buffer.data(), Buffer.size(), &sender, &senderPort);
        data_block = (asterix_data_block *)Buffer.data();
        data_fields = (uint8_t *)Buffer.data()+5; // CAT:1 byte LEN:2 bytes FSPEC:2 bytes
        offset = 0;
        qDebug() << "cat:" << data_block->cat << " length:" << qFromBigEndian(data_block->length) << " buffer size: " << Buffer.size();
        if(Buffer.size() != qFromBigEndian(data_block->length)){
            qDebug() << "incorrect header length, skipping packet";
            continue;
        }
        if(data_block->fspec0 & I240_010){
            // data source identifier
            json["SIC"] = data_fields[offset];
            json["SAC"] = data_fields[offset+1];
            offset+=2;
        }
        if(data_block->fspec0 & I240_000){
            // message type
            // data_fields[offset] = 1 video summary message
            //                     = 2 video message
            json["message_type"] = data_fields[offset];
            offset+=1;
        }
        if(json["message_type"] == 2){ // video message
            if(data_block->fspec0 & I240_020){
                // video record header;
                quint32 msg_index = *(quint32 *)(data_fields+offset);

                msg_index = qFromBigEndian(msg_index);
                json["msg_index"] = msg_index;
                offset+=4;
            }
            // puede seguir el nano o el femto
            if(data_block->fspec0 & I240_040){
                // video header nano
                quint16 start_az = *(quint16 *)(data_fields+offset);
                start_az = qFromBigEndian(start_az);
                quint16 end_az = *(quint16 *)(data_fields+offset+2);
                end_az = qFromBigEndian(end_az);
                quint16 start_rg = *(quint16 *)(data_fields+offset+4);
                start_rg = qFromBigEndian(start_rg);
                quint32 cell_dur = *(quint32 *)(data_fields+offset+8);
                cell_dur = qFromBigEndian(cell_dur);
                json["start_az"] = start_az;
                json["end_az"]   = end_az;
                json["start_rg"] = start_rg;
                json["cell_dur"] = cell_dur;
                offset += 12;
            }
            else if(data_block->fspec0 & I240_041){
                // video header femto
                quint16 start_az = *(quint16 *)(data_fields+offset);
                start_az = qFromBigEndian(start_az);
                quint16 end_az = *(quint16 *)(data_fields+offset+2);
                end_az = qFromBigEndian(end_az);
                quint16 start_rg = *(quint16 *)(data_fields+offset+4);
                start_rg = qFromBigEndian(start_rg);

                qDebug() << "Rango: " <<start_rg;


                quint32 cell_dur = *(quint32 *)(data_fields+offset+8);
                cell_dur = qFromBigEndian(cell_dur);
                json["start_az"] = start_az;
                json["end_az"]   = end_az;
                json["start_rg"] = start_rg;
                json["cell_dur"] = cell_dur;
                offset += 12;
            }
            if(data_block->fspec0 & I240_048){
                // cell resolution and compression
                bool compression = *(quint8 *)(data_fields+offset) & 1<<7;
                quint8 resolution = *(quint8 *)(data_fields+offset+1);
                json["compression"] = compression;
                json["resolution"] = resolution;
                offset += 2;
            }
            if(data_block->fspec1 & I240_049){
                // octets and cells counters
                quint16 valid_octets = *(quint16 *)(data_fields+offset);
                valid_octets = qFromBigEndian(valid_octets);
                quint32 valid_cells = *(quint32 *)(data_fields+offset+2) & 0xffffff;
                valid_cells = qFromBigEndian(valid_cells<<8);
                json["valid_octect"] = valid_octets;
                json["valid_cells"] = valid_cells;
                offset += 5;
            }

            if(data_block->fspec1 & I240_050){
                // video block low data volume
            }
            else if(data_block->fspec1 & I240_051){
                // video block medium data volume
                quint8 rep = *(quint8 *)(data_fields+offset);
                int size = rep * 64; // 64 bytes en 512 bits (tamaño en bits del bloque medium data volume)
                json["rep"] = rep;
                json["video_block"] = QByteArray::fromRawData((char *)data_fields+offset+1, size);
                //QByteArray video_block = QByteArray::fromRawData((char *)data_fields+offset+1, size);
                //json["video_block"] = QString(video_block.toBase64());
            }
            else if(data_block->fspec1 & I240_052){
                // video block high dat volume"
            }
            if(data_block->fspec1 & I240_140){
                // time of day
            }

            //socket_json->writeDatagram(QJsonDocument(json).toJson(), QHostAddress::LocalHost, 12345);
            file->write(Buffer);
        }
        else{ // video summary
            //este campo solo va cuando estamos en un video summary message
            //if(data_block->fspec0 & I240_030){
            //    qDebug() << "video summary";
            //}
        }
    }
}

void MainWindow::bytesWriten(quint64 bytes)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(bytes*100));
}

void MainWindow::read_file()
{
    asterix_data_block *data_block;
    int cat, len, sum_len=0;
    QByteArray Buffer;
    // https://stackoverflow.com/questions/34409892/qt-microseconds-timer-implementation
    //qDebug() << QTime::currentTime();
    while(true){
        if(file->atEnd()){
            qDebug() << "EOF. Seeking to 0";
            file->seek(0);
        }
        data_block = (asterix_data_block *) file->peek(3).data();
        cat = data_block->cat;
        len = qFromBigEndian(data_block->length);
        if(cat != 240){
            qDebug() << "Category != 240";
        }
        Buffer.resize(len);
        int len_read = file->read(Buffer.data(), len);
        if (len_read == -1){
            qDebug() << "read_file(): error reading input file";
        }
        else if (len_read != len){
            qDebug() << "read_file(): size read != expected size";
        }
        else{
            sum_len += len;

            socket_asterix->write(Buffer);
        }
        if(sum_len>50000) break;
    }
    /* hack, len-32 = cantidad de muestras en el paquete
     * divido por algo para tener una demora proporcional a la cantidad de las muestras
     * si divido por 16 que serian la cantidad de us no me queda un tiempo por barrido igual
     * al radar capturado, asi que voy a probar valores hasta que me quede bien */
    //struct timespec ts;
    //ts.tv_sec = 0;
    //ts.tv_nsec = (len-32)/512;
    //nanosleep(&ts, &ts);

    //std::this_thread::sleep_for(std::chrono::nanoseconds(len/5));
    timer->setInterval(1);
}

void MainWindow::open()
{
    QString fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty())
        qDebug() << fileName;
}

