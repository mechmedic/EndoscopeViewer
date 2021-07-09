// --------------------------------------------------------------- //
// CKim - C++ class for handling USB video using v4l2 library
// Last modified : 20200813 CKim
// --------------------------------------------------------------- //

#ifndef USBVIDEO_H
#define USBVIDEO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <QString>
#include <QLabel>
#include <QImage>

//QT_BEGIN_NAMESPACE
//class QImage;
//QT_END_NAMESPACE

//#include <opencv2/opencv.hpp>
//#include <opencv2/core.hpp>
//#include <opencv2/imgcodecs.hpp>
//#include <opencv2/highgui.hpp>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
        void   *start;
        size_t  length;
};

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

class UsbVideo : public QThread
{
    Q_OBJECT

public:
    UsbVideo(QObject* parent=0);

    int OpenDevice(const char* dev_name);
    int InitializeDevice(io_method a, int format = 0);
    int StartCapture();
    int StopCapture();
    int ClearBuffer();
    int CloseDevice();

    int ClearTimeoutError();
    int RestartCapture();

    void PrintCapability();
    void PrintInputInfo();
    void PrintFormatInfo();

    const QString&  GetErrStr()     {   return m_errStr;    }
    const QString&  GetMsgStr()     {   return m_msgStr;    }

    void  GetFrameSize(int& width, int& height)  {   width = m_pixformat.width;   height = m_pixformat.height; }

signals:
    void renderedImage(const QImage &image);
    void reportError(const QString& str);
    void timeoutError();

protected:
    void run() override;

 private:

    // CKim - Handle to the USB Video
    char m_deviceName[100];
    int m_fd;
    int m_iomethod;
    int m_numBuffers;
    struct v4l2_capability  m_cap;
    struct v4l2_format      m_format;
    struct v4l2_pix_format  m_pixformat;
    struct buffer*  m_buffers;

    bool runThread;

    int xioctl(int fh, int request, void *arg);
    void errno_exit(const char *s);

    int init_mmap();
    int process_image(void *p, int size);

    //void StreamingThread();
    int decodeFrame();

    QString m_errStr;
    QString m_msgStr;
    QImage  m_convertedImage;
    QImage  m_renderedImage;

};

#endif // USBVIDEO_H
