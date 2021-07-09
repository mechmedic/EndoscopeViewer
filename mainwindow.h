#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "usbvideo.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnInit_clicked();
    void on_btnStart_clicked();
    void updatePixmap(const QImage &image);
    void printError(const QString& str);
    void recoverfromTimeout();
    void on_btnStop_clicked();

private:
    Ui::MainWindow *ui;

    UsbVideo* m_Video;
};

#endif // MAINWINDOW_H
