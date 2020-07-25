#ifndef FFVIDEO_H
#define FFVIDEO_H

//为了兼容C和C99标准,必须加以下内容,否则编译不能通过
#ifndef INT64_C
#define INT64_C
#define UINT64_C
#endif

extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavdevice/avdevice.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/time.h>
    #include <libavfilter/avfilter.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}


#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QImage>
#include <QThread>
#include <QSemaphore>
#include <QTimer>
#include <QEventLoop>
#include "devinterface.h"

//---------------------------------------
//#define DEF_USE_AUDIO
#define NO_ONLY_NETSTREAM
//#define SEM_CHAIN_ToFB
//#define RENDER_FRAMERATE_LIM    20
#define RENDER_FRAMERATE_LIM    0

#define VIDEO_PACKETQUE_SIZE_UP     128
#define VIDEO_FRAME_QUEUE_SIZE      8
//---------------------------------------

struct PacketQueue
{
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int packets_limUp;      //最大容量
    int abort_request;
    QMutex *mutex;
    QWaitCondition *que_wait;
};

typedef struct FrameQueue
{
    AVFrame* frames[VIDEO_FRAME_QUEUE_SIZE];
    int rd_index;
    int wr_index;
//    int count;

    QSemaphore sem_empty;   //可被解码线程写人的frames数
    QSemaphore sem_valid;   //已被解码写入，可用于显示的帧数
}   FrameQueue;

enum PlayState//表示播放状态的值
{
    OfflineState,
    PlayingState,
    PausedState,
    StoppedState
};

class VideoRender :public QThread
{
    Q_OBJECT
public:
    explicit VideoRender();
    ~VideoRender();

    bool sync_req;
//    int db_step;
//    QMutex mutex_draw;

    void setContexts(AVCodecContext*ctx, SwsContext *sws, QSize *pRenderSize, float frameperiod, float ptsFactor);
//    void setPlayState(PlayState state)  {   QMutexLocker locker(&mutex); _state = state;    }
    void setPlayState(PlayState state)  {   _state = state;    }
    void setFrameQue(FrameQueue *frame_que) { framebuf = frame_que;  }
    void setDevIntf(DevInterface *dev)  { devIntf = dev;    }
private:
    AVCodecContext *pAVCodecContext;
    SwsContext * pSwsContext;
    AVFrame *pScaledFrame;
    FrameQueue *framebuf;

    float framePeriod;
    float pts_ms_factor;

    QSize renderSize;
    PlayState _state;    

    DevInterface *devIntf;
protected:
    void run();
signals:
    void GetImage(const QImage &image, long playTime);     //export
};


class VideoDecoder :public QThread
{
    Q_OBJECT
public:
    explicit VideoDecoder();
    ~VideoDecoder();

    QSemaphore sem_pktvalid;

    void setContexts    (AVCodecContext*ctx, float frameperiod, float ptsFactor, bool isNet);
    void setPlayState(PlayState state)  {_state = state;    }
    void setBufQue(PacketQueue *pkg_que,FrameQueue *frame_que) { pkgbuf = pkg_que; framebuf = frame_que; }
private:
    AVCodecContext *pAVCodecContext;

    PacketQueue *pkgbuf;
    FrameQueue *framebuf;

    float framePeriod;
    float pts_ms_factor;
    bool isNetStream;
    PlayState _state;

protected:
    void run();
};

class Ffvideo : public QObject
{
    Q_OBJECT

public:
    explicit Ffvideo(QObject *parent = 0);
    ~Ffvideo();
    int getPictureWidth()   {return pictureWidth;   }
    int getPictureHeigth()  {return pictureHeight;  }
    PlayState getPlayeState() {return _state; }

    bool open(QString fileUrl, int tcp_transport);
    void pause();
    void resume();
    bool close();
protected:

private:
    AVFormatContext *pAVFormatContext;
    AVCodecContext *pAVCodecContext;
    AVCodecContext *audCodecContext;
    SwsContext * pSwsContext;
    AVPacket pAVPacket;

    long frameCount;
    PlayState _state;
    bool isNetStream;
//    bool _process_closed;
//    int _close_step;

    int pictureWidth;
    int pictureHeight;    
    float framePeriod;
    int videoStreamIndex;
    int audioStreamIndex;

    QSize renderSize;

    VideoDecoder videoDecoder;
//    PacketQueue *videoq;

    VideoRender videoRender;
//    FrameQueue *frameq;

    DevInterface *devIntf;
    QMutex mutex;
    void closeContext();    
signals:
    void GetImage(const QImage &image, long playTime);     //export
    void stateChanged(PlayState state);
//    void stopPainter();

private slots:

public slots:
    void proccess();
};

#ifdef SEM_CHAIN_ToFB
extern QSemaphore sem_RenderFbValid;
#endif

#endif // FFVIDEO_H
