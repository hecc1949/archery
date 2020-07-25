#include "devinterface.h"
#include <QSettings>        //读写ini配置文件
#include <QDebug>


//DevInterface devIntf;       //

DevInterface::DevInterface(QObject *parent)
    : QObject(parent)
{
    loadCameraLinks();
    cameraLinkStatus = 0;
    //
#ifdef ARM
    gpio.gpioOpen_sysfs(GPIO_LED_CameraVideo, 0);
    gpio.gpioOpen_sysfs(GPIO_Relay, 0);
    gpio.gpioOpen_sysfs(GPIO_Buzzle, 0);

    gpio.gpioOpen_sysfs(GPIO_LED_0, 0);
#endif
    buzzle_puslephase = 0;
}

DevInterface::~DevInterface()
{
    if (onvif_proc != NULL)
    {
//        if (onvif_proc->Running)
        {
            onvif_proc->kill();
            onvif_proc->waitForFinished(1);
        }
    }
}

void DevInterface::closeProcs()
{
    if (onvif_proc != NULL)
    {
        onvif_proc->kill();
        onvif_proc->waitForFinished(1);
    }
}

/**
 * @brief (public)启动onviftool摄像头搜索
 * @return
 */
int DevInterface::startOnvifStrobe()
{
    if (onvif_proc != NULL)
    {
        if (onvif_proc->state() != QProcess::NotRunning)
            return(-1);
    }

    onvif_proc = new QProcess(this);
    onvif_proc->setProcessChannelMode(QProcess::MergedChannels);   //
    connect(onvif_proc,SIGNAL(readyReadStandardOutput()),this,SLOT(onvif_strobeMsg_slots()));
    connect(onvif_proc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onvif_strobeEnd(int, QProcess::ExitStatus)));

    QStringList args;       //注意，args中String不能包含空格！

    onvif_proc->start(tr("./onviftool"), args);
    if (onvif_proc->waitForStarted(200))
    {
        strobCameras.clear();
        strobingCamera.id = -1;
        return(0);
    }
    else
    {
        return(-1);
    }
}

/**
 * @brief Slot函数，接收onviftool摄像头搜索的输出信息，转换成Camera状态，放在strobCameras List
 */
void DevInterface::onvif_strobeMsg_slots()
{
    int sepIndex, i;
    static int strobe_hot_streamId = 0;

    while(onvif_proc->canReadLine())
    {
        QByteArray buffer(onvif_proc->readLine());
        if (buffer.startsWith('#'))
        {
            //line-1: #x: Device_XAddr=xxxxxx
            if (strobingCamera.id > 0 && strobingCamera.name.length()>0)
            {
                strobCameras.append(strobingCamera);
            }
            buffer.remove(0,1);
            sepIndex = buffer.indexOf(':');
            strobingCamera.id = buffer.left(sepIndex).toInt();

            strobingCamera.name = "";
            strobingCamera.model = "";
            strobingCamera.serial_no = "";
            strobingCamera.streamNum = 0;
            strobingCamera.usingSteamId = 0;
            strobingCamera.tcp_transport = 0;
            for(i=0; i<2; i++)
            {
                strobingCamera.stream_url[i] = "";
                strobingCamera.snapshot_url[i] = "";
            }
        }
        else if (strobingCamera.id > 0)
        {
            //line-2: Name=xxx  Model=xxxx  SerialNo=xxxxx
            if(buffer.startsWith("Name="))
            {
                sepIndex = buffer.indexOf('\t');
                strobingCamera.name = buffer.mid(5,sepIndex-5);

                buffer.remove(0,sepIndex+1);
                if(buffer.startsWith("Model="))
                {
                    sepIndex = buffer.indexOf('\t');
                    strobingCamera.model = buffer.mid(6,sepIndex-6);
                    buffer.remove(0,sepIndex+1);
                }
                if(buffer.startsWith("SerialNo="))
                {
                    buffer.remove(0,9);
                    buffer.replace(QByteArray("\n"), QByteArray(""));
                    buffer.replace(QByteArray("\r"), QByteArray(""));
                    strobingCamera.serial_no = buffer;
                    buffer.clear();
                }
            }
            //line-3: Profiles:x
            if(buffer.startsWith("Profiles:"))
            {
                buffer.replace(QByteArray("\n"), QByteArray(""));
                buffer.replace(QByteArray("\r"), QByteArray(""));
                strobingCamera.streamNum = buffer.mid(9).toInt();
                buffer.clear();
            }
            //line-4/group: MainStreamProfile/MainStreamProfileToken, SencondStream..
            if (buffer.startsWith("MainStream"))
            {
                strobe_hot_streamId = 0;
            }
            else if (buffer.startsWith("SecondStream"))
            {
                strobe_hot_streamId = 1;
            }
            //capture resolustion & frame rate value
            sepIndex = buffer.indexOf("Resolution=");
            if (sepIndex >0)
            {
                buffer.remove(0,sepIndex + 11);
                sepIndex = buffer.indexOf('x');
                int sepIndex2 = buffer.indexOf(',');
                int w1 = buffer.left(sepIndex).toInt();
                int h1 = buffer.mid(sepIndex+1, sepIndex2 - sepIndex-1).toInt();
                strobingCamera.pixelFormat[strobe_hot_streamId] = QSize(w1,h1);
            }
            //capture url & snapshot_url
            if (buffer.startsWith("->url="))
            {
                buffer.remove(0, 6);
                buffer.replace(QByteArray("\n"), QByteArray(""));
                buffer.replace(QByteArray("\r"), QByteArray(""));
                strobingCamera.stream_url[strobe_hot_streamId] = buffer;
                if (strobe_hot_streamId ==1)
                {
                    strobingCamera.streamNum = 2;
                }
            }
            if (buffer.startsWith("->snapshot="))
            {
                buffer.remove(0, 11);
                buffer.replace(QByteArray("\n"), QByteArray(""));
                buffer.replace(QByteArray("\r"), QByteArray(""));
                strobingCamera.snapshot_url[strobe_hot_streamId] = buffer;
            }
            //last line
            if (buffer.startsWith("detect end"))
            {
                if (strobingCamera.id > 0 && strobingCamera.name.length()>0)
                {
                    strobCameras.append(strobingCamera);
                }
                break;
            }
        }
    }

}

/**
 * @brief Slot函数，onviftool结束
 * @param exitCode， exitStatus
 */
void DevInterface::onvif_strobeEnd(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    int res = 0;
    if ((cameraLinkStatus & 0x01) ==0)
    {
        res = updateCameraLink();
    }

    emit externProcDone(res);
}

/**
 * @brief (public)从IPC摄像头捕捉单帧图像
 * @param ipcInfo
 * @return
 */
int DevInterface::ipcSnapshot(IPCameraInfos *ipcInfo)
{
    char filename[128] = "/tmp/snapshot0.jpg";
#if 0
    char cmd[256];
    sprintf(cmd, "wget -O %s '%s'", filename, ipcInfo->snapshot_url[0].toStdString().c_str());    // 使用wget下载图片
    return(system(cmd));         //这是阻塞式执行。返回值是debug出来的，不一定正确
#endif
    QProcess snapshot(this);
    QStringList args;
    args << "-O";
    args <<filename;
    args << ipcInfo->snapshot_url[0];       //不能加''
    snapshot.start("wget", args);
    if (!snapshot.waitForFinished(2000))
        return(-1);
    return(0);
}

/**
 * @brief 判断Camera与Drone的连接状态
 * @param camera
 * @return ： 1(/-1) -已连接， <0的值表示IP已经改变
 */
int DevInterface::isLinkedCamera(IPCameraInfos *camera)
{
    if (camera->serial_no == DroneCameras.serial_no)
    {
        if (camera->stream_url[0] == DroneCameras.stream_url[0] &&
                camera->stream_url[1] == DroneCameras.stream_url[1] &&
                camera->snapshot_url[0] == DroneCameras.snapshot_url[0])
            return(1);
        else
            return(-1);
    }

    return(0);
}

/**
 * @brief(public) 摄像头和靶之间建立连接，保存到ini-file
 * @param src_id
 * @param
 * @return
 */
int  DevInterface::LinkCameraToDrone(int src_id)
{
    IPCameraInfos *camera = &(strobCameras[src_id]);

    QSettings ini("./config.ini",QSettings::IniFormat);
    QString section;

    section = tr("/Camera/");
    ini.setValue(section+"model", camera->model);       //保留的
    ini.setValue(section+"SerialNo", camera->serial_no);
    ini.setValue(section+"url", camera->stream_url[0]);
    ini.setValue(section+"url2", camera->stream_url[1]);
    ini.setValue(section+"snapshot", camera->snapshot_url[0]);

    ini.setValue(section+"useStreamId", camera->usingSteamId);
    ini.setValue(section+"tcp_transport", camera->tcp_transport);

    loadCameraLinks();
    return 0;
}


/**
 * @brief 读取ini-file中的Camera-Drone配置
 */
void DevInterface::loadCameraLinks()
{
    QSettings ini("./config.ini",QSettings::IniFormat);
    QString s1;

    s1 = ini.value("/Camera/SerialNo").toString();
    DroneCameras.serial_no = s1;
    s1 = ini.value("/Camera/url").toString();
    DroneCameras.stream_url[0] = s1;
    s1 = ini.value("/Camera/url2").toString();
    DroneCameras.stream_url[1] = s1;
    s1 = ini.value("/Camera/snapshot").toString();
    DroneCameras.snapshot_url[0] = s1;

    DroneCameras.usingSteamId = ini.value("/Camera/useStreamId").toInt();
    DroneCameras.tcp_transport = ini.value("/Camera/tcp_transport").toInt();
}

/**
 * @brief 根据Srobe检测到的Camera List, 修改Drone-Camera配置，确保Camera IP修改后动态调整
 * @return
 */
int DevInterface::updateCameraLink()
{
    int res = 0, i, tmp1;

    if ((cameraLinkStatus & 0x01)==0 && DroneCameras.serial_no.length()>0)
    {
        for(i=0; i<strobCameras.count(); i++)
        {
            tmp1 = isLinkedCamera(&(strobCameras[i]));
            if (abs(tmp1) ==1)
            {
                res = 1;
                if (tmp1 <0)
                {
                    res = -1;
                    LinkCameraToDrone(i);
                }
                //为了显示一致，反向更新strobCameras[i]非网络部分的参数
                strobCameras[i].usingSteamId = DroneCameras.usingSteamId;
                strobCameras[i].tcp_transport = DroneCameras.tcp_transport;
            }
        }
    }
    cameraLinkStatus |= abs(res);

    return(res);
}


void DevInterface::setGpio(int gpioNo, int value)
{
#ifdef ARM
    if (value <2)
    {
        gpio.gpioWrite_sysfs(gpioNo,value);
    }
    else
    {
        gpio.gpioToggle_sysfs(gpioNo);
    }
#else
    Q_UNUSED(gpioNo);
    Q_UNUSED(value);
#endif
}

void DevInterface::timerEvent(QTimerEvent *event)        //重载
{
    if (buzzle_puslephase>0)
    {
        buzzle_puslephase--;
        if ((buzzle_puslephase % 2)!=0)
        {
            gpio.gpioWrite_sysfs(GPIO_Buzzle, 1);
        }
        else
        {
            gpio.gpioWrite_sysfs(GPIO_Buzzle, 0);
            if (buzzle_puslephase ==0)
                killTimer(event->timerId());
        }
    }
}

void DevInterface::setBuzzle(int pusleNum, int pusleWidth)
{
    if (buzzle_puslephase !=0)
        return;
    if (pusleNum <=0)
    {
        gpio.gpioWrite_sysfs(GPIO_Buzzle, 0);
        return;
    }
    buzzle_puslephase = pusleNum*2-1;
    gpio.gpioWrite_sysfs(GPIO_Buzzle, 1);
    startTimer(pusleWidth);
}


//=================================================================================================

GpioSysfs::GpioSysfs()
{
}

GpioSysfs::~GpioSysfs()
{
//    int cnt = activeGpio.count();
    int cnt = validGpio.count();
    for(int i=0; i<cnt; i++)
    {
//        gpioClose_sysfs(activeGpio[i]);
        gpioClose_sysfs(validGpio[i].gpiono);
    }
    validGpio.clear();
}

//int GpioSysfs::gpioOpen_sysfs(int bank, int id, int direction)
int GpioSysfs::gpioOpen_sysfs(int gpiono, int direction)
{
    int fd, len;    //, gpiono;
    char buf[128];

    QMutexLocker lock(&mutex_gpio);
    //export
    fd = ::open("/sys/class/gpio/export", O_WRONLY);
    if (fd <0)
    {
        return(-1);
    }
//    gpiono = bank*32 + id;
    len = ::snprintf(buf, sizeof(buf), "%d", gpiono);
    ::write(fd, buf, len);
    ::close(fd);

    //direction
    len = ::snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpiono);
    fd = ::open(buf, O_WRONLY);
    if (fd <0)
    {
        return(-1);
    }
    if (direction ==0)
        ::write(fd, "out", 4);
    else
        ::write(fd, "in", 3);
    ::close(fd);
    //set edge trig..

//    activeGpio.append(gpiono);
    GpioPinTag pin;
    pin.gpiono = gpiono;
    pin.direction = direction;
    pin.value = 0;
    validGpio.append(pin);

//    qDebug()<<"gpio open:"<<gpiono<<direction;
    return(gpiono);
}

void GpioSysfs::gpioClose_sysfs(int gpiono)
{
    int fd, len;
    char buf[128];

    QMutexLocker lock(&mutex_gpio);
    //unexport
    fd = ::open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd <0)
    {
        return;
    }
    len = ::snprintf(buf, sizeof(buf), "%d", gpiono);
    ::write(fd, buf, len);
    ::close(fd);

//    activeGpio.removeOne((int)gpiono);
}

bool GpioSysfs::gpioOpened(int gpiono)
{
//    return(activeGpio.indexOf((int)gpiono) >=0);
    bool valid;
    for(int i=0; i<validGpio.count(); i++)
    {
        if (validGpio[i].gpiono == gpiono)
        {
            valid = true;
            break;
        }
    }
    return(valid);
}

int GpioSysfs::gpioWrite_sysfs(int gpiono, int value)
{
    int fd;
    char buf[128];

    QMutexLocker lock(&mutex_gpio);
    ::snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpiono);
    fd = ::open(buf, O_WRONLY);
    if (fd <0)
    {
        return(-1);
    }
    if (value)
        ::write(fd, "1", 2);
    else
        ::write(fd, "0", 2);
    ::close(fd);

    for(int i=0; i<validGpio.count(); i++)
    {
        if (validGpio[i].gpiono == gpiono)
        {
            validGpio[i].value = (uint8_t)value;
            break;
        }
    }
//    qDebug()<<"write gpio:"<<gpiono<<value;
    return(0);
}

int GpioSysfs::gpioToggle_sysfs(int gpiono)
{
    uint8_t value=0;

    for(int i=0; i<validGpio.count(); i++)
    {
        if (validGpio[i].gpiono == gpiono)
        {
            value = validGpio[i].value;
            break;
        }
    }
    value ^= 0x01;
    gpioWrite_sysfs(gpiono, value);
    return(0);
}

int GpioSysfs::gpioRead_sysfs(int gpiono, int *value)
{
    int fd;
    char buf[128];
    char ch;

    QMutexLocker lock(&mutex_gpio);
    ::snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpiono);
    fd = ::open(buf, O_RDONLY);
    if (fd <0)
    {
        return(-1);
    }
    ::read(fd, &ch,1);
    if (ch != '0')
        *value = 1;
    else
        *value = 0;
    ::close(fd);
#if 0
    for(int i=0; i<validGpio.count(); i++)
    {
        if (validGpio[i].gpiono == gpiono)
        {
            validGpio[i].value = (uint8_t)(*value);
            break;
        }
    }
#endif
    return(0);
}

