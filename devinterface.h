#ifndef DEVINTERFACE_H
#define DEVINTERFACE_H

#include <QObject>
#include <QSize>
#include <QProcess>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>     //file_id
#include <fcntl.h>      //open, close
#include <QMutex>
#include <QMutexLocker>
#include <QTimerEvent>

#define GPIO_LED_CameraVideo    (32 + 30)       //GPIOB30
#define GPIO_Relay              (32 + 31)       //GPIOB31
#define GPIO_Buzzle             (4*32 + 4)      //GPIOE4
#define GPIO_LED_0              (4*32 + 6)      //GPIOE6

struct IPCameraInfos
{
    int id;
    QString name;
    QString model;
    QString serial_no;
    QString stream_url[2];
    QString snapshot_url[2];
    int streamNum;
    QSize pixelFormat[2];
    int frameRate[2];

    int usingSteamId;       //0,1
    int tcp_transport;      //0-udp, 1-tcp
};

struct GpioPinTag
{
    uint16_t gpiono;
    uint8_t direction;
    uint8_t value;
};

class GpioSysfs
{
public:
    GpioSysfs();
    ~GpioSysfs();

    static int getGpiono(int bank, int id)
    {
        return(bank*32 +id);
    }
    bool gpioOpened(int gpiono);

//    int gpioOpen_sysfs(int bank, int id, int direction);
    int gpioOpen_sysfs(int gpiono, int direction);
    void gpioClose_sysfs(int gpiono);
    int gpioWrite_sysfs(int gpiono, int value);
    int gpioToggle_sysfs(int gpiono);
    int gpioRead_sysfs(int gpiono, int *value);

private:
//    QList<int> activeGpio;
    QList <GpioPinTag> validGpio;
    QMutex mutex_gpio;
};


class DevInterface : public QObject
{
    Q_OBJECT
public:
//    explicit DevInterface(QObject *parent = 0);
    ~DevInterface();

    static DevInterface *GetInstance()      //这是(全局)单例化的类, 特有的实现模式
    {
        static DevInterface m_Instance;     //饿汉式，加载时就实现了一个实例供系统使用，不再改变。是线程安全的。
        return &m_Instance;
    }


    QList <IPCameraInfos> strobCameras;
    IPCameraInfos DroneCameras;
    int cameraLinkStatus;       // 1=linked

    int startOnvifStrobe();
    int ipcSnapshot(IPCameraInfos *ipcInfo);
    int  LinkCameraToDrone(int src_id);
    int isLinkedCamera(IPCameraInfos *camera);
    void closeProcs();

    void setGpio(int gpioNo, int value);
    void setBuzzle(int pusleNum, int pusleWidth =200);
protected:
    void timerEvent(QTimerEvent *);     //使用QObject内置Timer

private:
    //这是对全局单例化类的特殊处理:
    explicit DevInterface(QObject *parent = 0);     //构造函数必须是private的，约束外部程序只能用GetInstance()生成实例
    DevInterface(const DevInterface &);             //不实现，禁止拷贝构造函数
    DevInterface & operator=(const DevInterface &);     //不实现，禁止赋值拷贝构造函数

    QProcess *onvif_proc;
    IPCameraInfos strobingCamera;

    int updateCameraLink();

    GpioSysfs gpio;
    int buzzle_puslephase;
private slots:
    void onvif_strobeMsg_slots();
    void onvif_strobeEnd(int exitCode, QProcess::ExitStatus exitStatus);

    void loadCameraLinks();

signals:
    void externProcDone(int exitCode);

public slots:

};


#endif // DEVINTERFACE_H
