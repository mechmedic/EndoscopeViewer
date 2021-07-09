#include "mainwindow.h"
#include "ui_mainwindow.h"



MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // CKim - Open Video
    m_Video = new UsbVideo();
    //int ret = m_Video->OpenDevice("/dev/video2");
    int ret = m_Video->OpenDevice("/dev/video0");
    if(!ret)    {
        ui->lblMsg->setText(m_Video->GetErrStr());    }
    else {
        ui->lblMsg->setText(m_Video->GetMsgStr());    }

    connect(m_Video, SIGNAL(reportError(QString)), this, SLOT(printError(QString)));
    connect(m_Video, SIGNAL(timeoutError()), this, SLOT(recoverfromTimeout()));
}

MainWindow::~MainWindow()
{
    m_Video->CloseDevice();
    delete ui;
}

void MainWindow::on_btnInit_clicked()
{
    int ret = m_Video->InitializeDevice(IO_METHOD_MMAP, 0);
    if(!ret)    {
        ui->lblMsg->setText(m_Video->GetErrStr());    }
    else {
        ui->lblMsg->setText(m_Video->GetMsgStr());    }

    int w, h;
    m_Video->GetFrameSize(w,h);
    ui->lblImage->resize(w,h);
}

void MainWindow::on_btnStart_clicked()
{
    connect(m_Video, SIGNAL(renderedImage(QImage)), this, SLOT(updatePixmap(QImage)));
    int ret = m_Video->StartCapture();
    if(!ret)    {
        ui->lblMsg->setText(m_Video->GetErrStr());    }
    else {
        ui->lblMsg->setText(m_Video->GetMsgStr());    }
}

void MainWindow::on_btnStop_clicked()
{
    int ret = m_Video->StopCapture();
    if(!ret)    {
        ui->lblMsg->setText(m_Video->GetErrStr());    }
    else {
        ui->lblMsg->setText(m_Video->GetMsgStr());    }

    ret = m_Video->ClearBuffer();
    if(!ret)    {
        ui->lblMsg->setText(m_Video->GetErrStr());    }
    else {
        ui->lblMsg->setText(m_Video->GetMsgStr());    }
}

void MainWindow::updatePixmap(const QImage &image)
{
    // https://doc.qt.io/qt-5/qtwidgets-widgets-imageviewer-example.html
    QPixmap pixmap = QPixmap::fromImage(image);
    ui->lblImage->setPixmap(pixmap);
}

void MainWindow::printError(const QString &str)
{
    ui->lblMsg->setText(str);
}

void MainWindow::recoverfromTimeout()
{
    m_Video->ClearTimeoutError();
    ui->lblMsg->setText("Sparta!!!!");
    //sleep(1);
    usleep(500000);
    m_Video->RestartCapture();
}

