#include "usbvideo.h"
#include <QImage>

UsbVideo::UsbVideo(QObject* parent) : QThread(parent)
{
    m_fd = -1;
    runThread = false;
}

int UsbVideo::xioctl(int fh, int request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

int UsbVideo::OpenDevice(const char* dev_name)
{
    // CKim - Open device. In Linux, everything is file, even the devices
    // <sys/stat.h> is the header in the C POSIX library for the C programming language that contains
    // constructs that facilitate getting information about files attributes.
    struct stat st;

    if (-1 == stat(dev_name, &st)) {
        m_errStr.sprintf("Cannot identify '%s': %d, %s", dev_name, errno, strerror(errno));
        return 0;
    }

    if (!S_ISCHR(st.st_mode)) {
        m_errStr.sprintf("%s is no device", dev_name);
        return 0;
    }

    // CKim - To open and close V4L2 devices applications use the open() and close() function, respectively.
    m_fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

    if (-1 == m_fd) {
        m_errStr.sprintf("Cannot open '%s': %d, %s",dev_name, errno, strerror(errno));
        return 0;   //exit(EXIT_FAILURE);
    }

    // CKim - Query capabilities using VIDIOC_QUERYCAP ioctl. Devices are programmed using the ioctl() function.
    // ioctl(file descriptor, request, ...) function performs I/O control operation specified by the 2nd argument 'request'
    // See https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/vidioc-querycap.html#vidioc-querycap
    if (-1 == xioctl(m_fd, VIDIOC_QUERYCAP, &m_cap))
    {
        if (EINVAL == errno) {
            m_errStr.sprintf("%s is no V4L2 device", dev_name);
            return 0;   //exit(EXIT_FAILURE);
        }
        else {
            m_errStr.sprintf("VIDIOC_QUERYCAP error %d, %s", errno, strerror(errno));
            return 0;
        }
    }

    if (!(m_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        m_errStr.sprintf("%s is no video capture device", dev_name);
        return 0;
    }

    sprintf(m_deviceName,"%s",dev_name);
    m_msgStr.sprintf("Successfully opened %s",m_deviceName);

    return 1;
}

int UsbVideo::InitializeDevice(io_method io, int vformat)
{
    // CKim - Return if handle is not valid
    if (-1 == m_fd) {
        m_errStr.sprintf("Device not opened!!\n");
        return 0;
    }

    // CKim - Get current video format set by the device
    m_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;    // type must be set before calling VIDIOC_G_FMT
    if (-1 == xioctl(m_fd, VIDIOC_G_FMT, &m_format))
    {
         m_errStr.sprintf("VIDIOC_G_FMT error %d, %s\n", errno, strerror(errno));
         return 0;
     }
    m_pixformat = m_format.fmt.pix;

    // CKim - In future, dependng on the vformat, change image format
    // CKim - Negotiate data format. Asks for a particular format and the driver selects and reports the
    // best the hardware can do to satisfy the request. Of course applications can also just query the current selection.


    // CKim - Prepare memory for streaming
    // 2. The streaming I/O method with a) memory mapped or b) user buffers
    m_iomethod = io;
    if(io == IO_METHOD_MMAP)
    {
        // 2a. Using memory map,
        int res = init_mmap();
        if(!res)    {   return 0;   }
    }
    else if (io == IO_METHOD_USERPTR)
    {
        // 2b. Using pointer to user buffer
        //init_userp(format.fmt.pix.sizeimage);
    }

    m_msgStr.sprintf("Successfully Initialized Video");
    return 1;
}

int UsbVideo::StartCapture()
{
    // CKim - For capturing applications with streaming
    // it is customary to first enqueue all mapped buffers, then to start capturing and enter the read loop.
    enum v4l2_buf_type type;

    switch (m_iomethod) {

    case IO_METHOD_MMAP:
        for (int i = 0; i < m_numBuffers; ++i)
        {
            // CKim - Prepare buffer
            struct v4l2_buffer buf;

            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            // CKim - To enqueue a buffer use the VIVIOC_QBUF ioctl.
            if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
            {
                m_errStr.sprintf("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
                return 0;
            }
        }

        // CKim - To start capturing call the VIDIOC_STREAMON ioctl.
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(m_fd, VIDIOC_STREAMON, &type))
        {
            m_errStr.sprintf("VIDIOC_STREAMON error %d, %s\n", errno, strerror(errno));
            return 0;
        }
        break;

//    case IO_METHOD_USERPTR:
//        for (i = 0; i < n_buffers; ++i)
//        {
//            // CKim - Prepare buffer
//            struct v4l2_buffer buf;

//            CLEAR(buf);
//            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//            buf.memory = V4L2_MEMORY_USERPTR;
//            buf.index = i;
//            buf.m.userptr = (unsigned long)buffers[i].start;
//            buf.length = buffers[i].length;

//            // CKim - To enqueue a buffer use the VIVIOC_QBUF ioctl.
//            if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
//                errno_exit("VIDIOC_QBUF");
//        }

//        // CKim - To start capturing call the VIDIOC_STREAMON ioctl.
//        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//        if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
//            errno_exit("VIDIOC_STREAMON");
//        break;
    }

    // CKim - Start Qthread
    // https://doc.qt.io/qt-5/qtcore-threads-mandelbrot-example.html
    if (!this->isRunning())
    {
        runThread = true;
        this->start(HighestPriority);
    }
    m_msgStr.sprintf("Capturing Started");
    return 1;
}

int UsbVideo::StopCapture()
{
    runThread = false;
    while(this->isRunning())    {}
    enum v4l2_buf_type type;

    switch (m_iomethod) {
    case IO_METHOD_READ:
        /* Nothing to do. */
        break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(m_fd, VIDIOC_STREAMOFF, &type)) {
            m_errStr.sprintf("VIDIOC_STREAMOFF error %d, %s\n", errno, strerror(errno));
            return 0;
        }
        break;
    }

    return 1;
}

int UsbVideo::ClearTimeoutError()
{
    runThread = false;
    while(this->isRunning())    {}

    StopCapture();
    ClearBuffer();
    return 1;
}

int UsbVideo::RestartCapture()
{
    // 2a. Using memory map,
    int res = init_mmap();
    if(!res)    {   return 0;   }
    StartCapture();
    return 1;
}

int UsbVideo::ClearBuffer()
{
    // CKim - Unmap and free buffers
    switch (m_iomethod) {
//    case IO_METHOD_READ:
//        free(buffers[0].start);
//        break;

    case IO_METHOD_MMAP:
        for (int i = 0; i < m_numBuffers; ++i)
            if (-1 == munmap(m_buffers[i].start, m_buffers[i].length)) {
                m_errStr.sprintf("munmap error %d, %s\n", errno, strerror(errno));
                return 0;
            }
        break;

    case IO_METHOD_USERPTR:
        for (int i = 0; i < m_numBuffers; ++i)
            free(m_buffers[i].start);
        break;
    }

    free(m_buffers);
    return 1;
}

int UsbVideo::CloseDevice()
{
    // CKim - Close handle
    if (-1 == close(m_fd))
    {
        m_errStr.sprintf("close error %d, %s\n", errno, strerror(errno));
        return 0;  //errno_exit("close");
    }
    m_msgStr.sprintf("\nClosing Device \n");
    m_fd = -1;
}

void UsbVideo::run()
{
    m_msgStr.sprintf("mainloop() : Dequeue and process buffers");
    emit reportError(m_msgStr);

    // CKim - Here the application waits until a filled buffer can be dequeued,
    while (runThread)
    {
        fd_set fds;     struct timeval tv;      int r;
        struct v4l2_buffer buf;

        for (;;)
        {
            // CKim - Prepare select() function
            FD_ZERO(&fds);  FD_SET(m_fd, &fds);
            //tv.tv_sec = 1;  tv.tv_usec = 0;
            tv.tv_sec = 0;  tv.tv_usec = 500000;

            // CKim - Use select() to wait until queue is available
            r = select(m_fd + 1, &fds, NULL, NULL, &tv);

            // returns -1 for error
            if (-1 == r)
            {
                if (EINTR == errno) {   continue;   }
                m_errStr.sprintf("select error %d, %s", errno, strerror(errno));
                emit reportError(m_errStr);
                return;
            }

            // returns 0 for timeout
            if (0 == r)
            {
                m_errStr.sprintf("select timeout");
                emit reportError(m_errStr);
                emit timeoutError();
                return;
                //break;
                //continue;
                //exit(EXIT_FAILURE);
            }

            // CKim - deque buffer, currently only for im_method_mmap case
            CLEAR(buf);
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            // CKim - To dequeue a buffer use the VIVIOC_DQBUF ioctl.
            if (-1 == xioctl(m_fd, VIDIOC_DQBUF, &buf))
            {
                switch (errno) {
                case EAGAIN:
                    continue;

                case EIO:  /* Could ignore EIO, see spec. */
                default:
                    m_errStr.sprintf("VIDIOC_DQBUF error %d, %s", errno, strerror(errno));
                    emit reportError(m_errStr);
                    return;  //errno_exit("VIDIOC_DQBUF");
                }
            }

            assert(buf.index < m_numBuffers);

            // CKim - Process image.
            // buffers[i].start has pointer to the memory of the ith buffer
            // buf.bytesused has size of the filled data, different from sizeimage due to varying compression
            process_image(m_buffers[buf.index].start, buf.bytesused);

            // CKim - re-enqueues the buffer
            if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
            {
                m_errStr.sprintf("VIDIOC_QBUF error %d, %s\n", errno, strerror(errno));
                emit reportError(m_errStr);
                return;
            }

            break;
        }
    }
}

int UsbVideo::process_image(void *p, int size)
{
    // CKim - decode jpg by using Qt QImage's loading function
    //bool res = m_convertedImage.loadFromData((const uchar*)p,size,"JPG");
    bool res = m_renderedImage.loadFromData((const uchar*)p,size,"JPG");
    if(res)
    {
//        // CKim - Perform deepcopy and send it for rendering.
//        m_renderedImage = m_convertedImage.copy();
        emit renderedImage(m_renderedImage);
    }
    else
    {
        m_errStr.sprintf("Failed to Decode!!!");
        emit reportError(m_errStr);
    }
}

int UsbVideo::init_mmap()
{
    // CKim - Streaming is an I/O method where only pointers to buffers are exchanged between application and driver,
    // the data itself is not copied. Memory mapping is primarily intended to map buffers in device memory into
    // the application’s address space.

    // CKim - To allocate device buffers
    struct v4l2_requestbuffers req;

    // CKim - Buffers should be initialized as below, before requesting allocation
    CLEAR(req);
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;     // CKim - buffer type. Should be same as that from the queried struct v4l2_format
    req.count = 4;//16;                         // CKim - Desired number of buffers
    req.memory = V4L2_MEMORY_MMAP;              // CKim - V4L2_MEMORY_OVERLAY or V4L2_MEMORY_USERPTR or V4L2_MEMORY_DMABUF

    // CKim - to determine if the memory mapping flavor is supported applications must call the
    // ioctl VIDIOC_REQBUFS ioctl with the memory type set to V4L2_MEMORY_MMAP
    if (-1 == xioctl(m_fd, VIDIOC_REQBUFS, &req))
    {
        if (EINVAL == errno) {
            m_errStr.sprintf("%s does not support memory mappingn", m_deviceName);
            return 0; //exit(EXIT_FAILURE);
        }
        else {
            m_errStr.sprintf("VIDIOC_REQBUFS error %d, %s\n", errno, strerror(errno));
            return 0;
        }
    }

    m_msgStr.sprintf("Allocated %d buffers\n", req.count);

    // CKim - 'count' is updated with the actual number of allocated buffer. At least
    // 2 buffers (1 for app, 1 for filling) are needed for capture
    if (req.count < 2) {
        m_errStr.sprintf("Insufficient buffer memory on %s\n",m_deviceName);
        return 0;
    }

    // CKim - calloc(num,size) allocated array of num elements, each being size bytes long and initializes them to zero
    m_buffers = (buffer*)calloc(req.count, sizeof(*m_buffers));

    if (!m_buffers) {
        m_errStr.sprintf("Out of memory\n");
        return 0;  //exit(EXIT_FAILURE);
    }

    // CKim - Before applications can access the buffers they must map them into their address space with the mmap() function.
    // The location of the buffers in device memory can be determined with the ioctl VIDIOC_QUERYBUF ioctl.
    // One needs to supply pointer to struct v4l2_buffers
    for(int n_buffers = 0; n_buffers < req.count; ++n_buffers)
    {
        // CKim - struct v4l2_buffer
        struct v4l2_buffer buf;

        CLEAR(buf);
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // CKim - Type, same as the requested
        buf.memory      = V4L2_MEMORY_MMAP;             // CKim - memory, same as the requested
        buf.index       = n_buffers;                    // CKim - number of buffers

        // CKim - Query the status of buffer such as size, device memory location for memory map.
        if (-1 == xioctl(m_fd, VIDIOC_QUERYBUF, &buf)) {
            m_errStr.sprintf("VIDIOC_QUERYBUF error %d, %s\n", errno, strerror(errno));
            return 0;
        }

        // CKim - map address of buffer in device memory to application memory
        // In the single-planar API case, the m.offset and length returned in a struct v4l2_buffer are passed as sixth and
        // second parameter to the mmap() function. mmap() / munmap() function maps / unmaps files or devices into memory
        // buf.m.offset is the offset of the buffer from the start of the device memory
        m_buffers[n_buffers].length = buf.length;
        m_buffers[n_buffers].start =
                mmap(NULL /* start anywhere */,
                     buf.length,
                     PROT_READ | PROT_WRITE /* required */,
                     MAP_SHARED /* recommended */,
                     m_fd, buf.m.offset);

        if (MAP_FAILED == m_buffers[n_buffers].start)
        {
            m_errStr.sprintf("mmap error %d, %s\n", errno, strerror(errno));
            return 0;
        }
    }

    m_numBuffers = req.count;
    return 1;
}

void UsbVideo::PrintCapability()
{

    printf("Queried Capability\n");
    printf("Driver : %s\n",m_cap.driver);
    printf("Version : %u.%u.%u\n",(m_cap.version >> 16) & 0xFF, (m_cap.version >> 8) & 0xFF, m_cap.version & 0xFF);
    printf("Card : %s\n",m_cap.card);
    printf("Bus Info : %s\n",m_cap.bus_info);
    printf("Capability : 0x%08x\n",m_cap.capabilities);
    printf("Device Capability : 0x%08x\n",m_cap.device_caps);

    // CKim - Check some of the capabilities...
    if(m_cap.device_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
    {
        printf("Multi-planar device\n");
    }
    if (m_cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)
    {
        printf("Single-planar device\n");
    }
    if (m_cap.device_caps & V4L2_CAP_READWRITE)
    {
        printf("read(), write() I/O supported\n");
    }
    if (m_cap.device_caps & V4L2_CAP_STREAMING)
    {
        printf("Streaming device\n");
    }


}

void UsbVideo::PrintInputInfo()
{
    // ---------------------------------------------------- //
    // CKim - Learn about the number and attributes of the available inputs and outputs
    // Enumerate them with the ioctl VIDIOC_ENUMINPUT and ioctl VIDIOC_ENUMOUTPUT ioctl, respectively.
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/video.html
    struct v4l2_input input;

    // CKim - Get the index of current input
    int index;
    if (-1 == xioctl(m_fd, VIDIOC_G_INPUT, &index))
    {
        perror("VIDIOC_G_INPUT");
        exit(EXIT_FAILURE);
    }

    memset(&input, 0, sizeof(input));
    input.index = index;


    // ---------------------------------------------------- //
    // CKim - Get the attribute of the current video input (analog)
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/vidioc-enuminput.html#vidioc-enuminput
    if (-1 == xioctl(m_fd, VIDIOC_ENUMINPUT, &input)) {
        perror("VIDIOC_ENUMINPUT");
        exit(EXIT_FAILURE);
    }

    printf("\nCurrent input : %s\n", input.name);
    printf("Index : %d\n", input.index);
    printf("InputType : %d\n",input.type);   // 1 is tuner, 2 is camera, 3 is touch device
    printf("Standard : %d\n",input.std);     // Standards like NSTC, PAL. 0 for USB Cams....
    printf("Status : %d\n",input.status);    // 0 is OK.
    printf("Capability : %d\n",input.capabilities);  // 0 for webcam....

    // CKim - The VIDIOC_G_INPUT and VIDIOC_G_OUTPUT ioctls return the index of the current video input or output.
    // To select a different input or output applications call the VIDIOC_S_INPUT and VIDIOC_S_OUTPUT ioctls.
    // Notice G means 'get', S means 'set' in ioctl request
    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/video.html


    // Special rules apply to devices such as USB cameras where the notion of video standards makes little sense.
    // More generally for any capture or output device which is:
    //   incapable of capturing fields or frames at the nominal rate of the video standard, or
    //   that does not support the video standard formats at all.
    // Here the driver shall set the std field of struct v4l2_input and struct v4l2_output to zero and
    // the VIDIOC_G_STD, VIDIOC_S_STD, ioctl VIDIOC_QUERYSTD, VIDIOC_SUBDEV_QUERYSTD and
    // ioctl VIDIOC_ENUMSTD, VIDIOC_SUBDEV_ENUMSTD ioctls shall return the ENOTTY error code or the EINVAL error code.
//    struct v4l2_standard standard;
//    if (-1 == xioctl(fd, VIDIOC_ENUMSTD, &standard)) {
//        perror("VIDIOC_ENUMINPUT");
//        exit(EXIT_FAILURE);
//    }

    // ---------------------------------------------------- //
    // CKim - Get the attribute of the current video input (digital) - Not meaningful for USB Cams
//    struct v4l2_dv_timings dvTiming;
//    if (-1 == xioctl(fd, VIDIOC_G_DV_TIMINGS, &dvTiming)) {
//        perror("VIDIOC_G_DV_TIMINGS");
//        exit(EXIT_FAILURE);
//    }

    // ---------------------------------------------------- //
    // CKim - Get the available controls, brightness, contrasts... Extended and Camera controls as well etc.
    // Later .....

}

void UsbVideo::PrintFormatInfo()
{
//    printf("\nData stream type : %d\n", m_format.type);  // 1 is V4L2_BUF_TYPE_VIDEO_CAPTURE
//    printf("Height %d x Width %d\n", m_format.fmt.pix.height, format.fmt.pix.width);

//    // 'sizeimage' tells size in bytes of the buffer to hold a complete image, set by the driver.
//    // Usually this is bytes perline times height. When the image consists of variable length compressed data
//    // this is the number of bytes required by the codec to support the worst-case compression scenario.
//    printf("Size image : %d\n", format.fmt.pix.sizeimage);
//    printf("Bytes per line : %d\n", format.fmt.pix.bytesperline);

//    // CKim - Struct v4l2_pix_format has info on single planar image data
//    // Decode the 32bit number in 'pixelformat' as follows. This is inverse of v4l2_fourcc(a,b,c,d)
//    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/vidioc-enum-fmt.html#v4l2-fourcc
//    // Laptop webcam is MJPG, "Compressed format used by the Zoran driver"
//    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/pixfmt-reserved.html#v4l2-pix-fmt-mjpeg
//    struct v4l2_pix_format pixFormat = format.fmt.pix;
//    printf("Pixel format is : [");
//    printf("'%c',",((pixFormat.pixelformat) >> 0) & 0x000000FF);
//    printf("'%c',",((pixFormat.pixelformat) >> 8) & 0x000000FF);
//    printf("'%c',",((pixFormat.pixelformat) >> 16) & 0x000000FF);
//    printf("'%c']\n",((pixFormat.pixelformat) >> 24) & 0x000000FF);
//    printf("Pixel format 0x%08x is 0x%08x\n",pixFormat.pixelformat,V4L2_PIX_FMT_MJPEG);
//    printf("Field %d\n",pixFormat.field);      //fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

//    // https://linuxtv.org/downloads/v4l-dvb-apis-new/userspace-api/v4l/vidioc-enum-fmt.html#vidioc-enum-fmt
//    // CKim - a special ioctl to enumerate all image formats supported by video capture, overlay or output devices
//    // CKim - initialize the type, index fields
//    struct v4l2_fmtdesc formatDesc;
//    formatDesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
//    formatDesc.index = 0;  // 0 GIVE MJPG, 1 GIVES YUYV 4:2:2
//    if (-1 == xioctl(fd, VIDIOC_ENUM_FMT, &formatDesc)) {
//        perror("VIDIOC_ENUM_FMT");
//        exit(EXIT_FAILURE);
//    }
//    printf("\nFirst supported image format : %s\n",formatDesc.description);
//    printf("Image format flag : %d\n",formatDesc.flags);
//    printf("Pixel format 0x%08x\n",formatDesc.pixelformat);

}
