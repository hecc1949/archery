#include "ffvideo.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QSemaphore>
#include <QCoreApplication>
//#include <QAudioOutput>

#ifdef SEM_CHAIN_ToFB
QSemaphore sem_RenderFbValid;
#endif

#define M_VID_LEDON         devIntf->setGpio(GPIO_LED_CameraVideo,1)
#define M_VID_LEDOFF         devIntf->setGpio(GPIO_LED_CameraVideo,0)

static PacketQueue videoq;
static FrameQueue frameq;

void packet_queue_init(PacketQueue *q, int packetsUp)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = new QMutex();
    q->que_wait = new QWaitCondition();
    q->packets_limUp = packetsUp;
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;
    if (q->abort_request)
    {
        return -1;
    }
    pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
    {
        return -1;
    }
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

    q->mutex->lock();
    if (!q->last_pkt)
        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;    
    q->size += pkt1->pkt.size + sizeof(*pkt1);

    //包数达到上限时，不是就不能写了，只是降低速度。
    //缓存的包数越多延时就越的，对网络视频，20~30, 文件视频就可能100以上
    if (q->nb_packets >=q->packets_limUp)
    {
        q->que_wait->wait(q->mutex, 2000);
    }
    q->mutex->unlock();
    return(q->nb_packets);
}


void packet_queue_clear(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    q->abort_request = 1;
    q->mutex->lock();
    for (pkt = q->first_pkt; pkt; pkt = pkt1)
    {
        pkt1 = pkt->next;
        av_packet_unref(&pkt->pkt);     //
        av_freep(&pkt);     //free mem并置ptr为0
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->abort_request = 0;
    q->que_wait->wakeAll();
    q->mutex->unlock();
}

int packet_queue_get(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;
    int ret;

    if (q->abort_request)
        return -1;

    q->mutex->lock();
    pkt1 = q->first_pkt;
    if (pkt1)
    {
        q->first_pkt = pkt1->next;
        if (!q->first_pkt)
            q->last_pkt = NULL;
        q->nb_packets--;
        q->size -= pkt1->pkt.size + sizeof(*pkt1);
        *pkt = pkt1->pkt;
        av_freep(&pkt1);
        ret = 1;
    }
    else
    {
        ret = 0;
    }

    q->que_wait->wakeAll();
    q->mutex->unlock();
    return ret;
}

int packet_queue_validnum(PacketQueue *q)
{
    int n;
    q->mutex->lock();
    n = q->nb_packets;
    q->mutex->unlock();
    return (n);
}

//-----------------------------------------------------------------------------------------------------------------

int frame_queue_init(FrameQueue *q)
{
    int i;

    for(i=0; i<VIDEO_FRAME_QUEUE_SIZE; i++)
    {
        if (!(q->frames[i] = av_frame_alloc()))
            return AVERROR(ENOMEM);
    }
    q->rd_index = 0;
    q->wr_index = 0;

    q->sem_valid.acquire(q->sem_valid.available());
    q->sem_empty.acquire(q->sem_empty.available());
    q->sem_empty.release(VIDEO_FRAME_QUEUE_SIZE);
    return(0);
}

void frame_queue_free(FrameQueue *q)
{
    int i;
    for(i=0; i<VIDEO_FRAME_QUEUE_SIZE; i++)
    {
        av_frame_unref(q->frames[i]);
        av_frame_free(&(q->frames[i]));
    }
}

int frame_queue_get_wrframe(FrameQueue *q, AVFrame **pFrame)
{
    *pFrame = q->frames[q->wr_index];
    return(0);
}

void frame_queue_wrnext(FrameQueue *q)
{
    q->wr_index = (q->wr_index +1) % VIDEO_FRAME_QUEUE_SIZE;

    q->sem_valid.release(1);
}

int frame_queue_get_rdframe(FrameQueue *q, AVFrame **pFrame)
{
    *pFrame = q->frames[q->rd_index];

    return(0);
}

int frame_queue_rdnext(FrameQueue *q)
{
    q->rd_index = (q->rd_index +1) % VIDEO_FRAME_QUEUE_SIZE;
    q->sem_empty.release(1);
    return(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////


VideoRender::VideoRender()
{
    sync_req = false;
    pScaledFrame = NULL;
}

VideoRender::~VideoRender()
{
    av_freep(&pScaledFrame->data[0]);
    av_frame_free(&pScaledFrame);
    pScaledFrame = NULL;
}

void VideoRender::setContexts(AVCodecContext*ctx, SwsContext *sws, QSize *pRenderSize, float frameperiod, float ptsFactor)
{
    pAVCodecContext = ctx;
    pSwsContext = sws;
    renderSize = *pRenderSize;
    framePeriod = frameperiod;
    pts_ms_factor = ptsFactor;
}

void VideoRender::run()
{
    AVFrame *pDecodedFrame;
    QElapsedTimer rtime;

    int frameCount = 0;
    int skipframes = 0;
    int64_t r_pts = 0;
    float skipframe_limtime = framePeriod*2.0;
    int64_t prev_rtime = 0;

#if (RENDER_FRAMERATE_LIM >0)
    bool prevframe_drawed = false;
    int64_t frametime_lim = round((float)1000.0/RENDER_FRAMERATE_LIM);
#endif

    int64_t frametime=0;

    qDebug()<<"Render thread start.:"<<QThread::currentThread();    //<<"frameLim:"<<frametime_lim;

#ifdef SEM_CHAIN_ToFB
    sem_RenderFbValid.acquire(sem_RenderFbValid.available());
    sem_RenderFbValid.release(1);
    qDebug()<<"video render chain to framebuf";
#endif
    int pictureHeight = pAVCodecContext->height;

    if (pScaledFrame ==NULL)
    {
        pScaledFrame = av_frame_alloc();
        //*与avcodec_receive_frame不同，sws_scale()不会自动为AVFrame申请图像内存，要外程序申请
        av_image_alloc(pScaledFrame->data, pScaledFrame->linesize, renderSize.width(), renderSize.height(),AV_PIX_FMT_RGB24,16);
    }
    //
//    while(1)
    while (_state != StoppedState)
    {
//        db_step = 0;
//        framebuf->sem_valid.acquire(1);
/*        if (_state == StoppedState)
        {
            break;
        }
        */
        framebuf->sem_valid.acquire(1);        
//        db_step = 1;
        if (frame_queue_get_rdframe(framebuf, &pDecodedFrame)==0)
        {
            //paused截断显示输出。netstream模式只用这个控制。
            //==>PausedState可以通过QT界面接收emite QImage后，再次判断ffvideo._state, 在Paused状态不显示，不释放
            //  sem_RenderFbValid来阻塞，如果PausedState可以显示，会放出最后的几帧，延迟Paused
            if (_state == PausedState)
            {
//                db_step = 2;
                msleep(10);     //!
                frame_queue_rdnext(framebuf);
                continue;
            }
            //
#ifdef SEM_CHAIN_ToFB
            sem_RenderFbValid.acquire(1);
#endif
//            db_step = 3;
            if (frameCount ==0)
            {
                rtime.start();
            }
            else
            {
                //播放时标定义(r_pts): 对连续的网络视频，简单取r_pts=rtime.elapsed(), 但对文件播放的情况，有pause时就不好
                //处理。用帧间时间片累加的方式通用性好一些，这样r_ptr变成可修改的值，
                int64_t t1 = rtime.elapsed();
                frametime = (t1 - prev_rtime);
                r_pts += frametime;
                prev_rtime = t1;

                double delta = pDecodedFrame->pts*pts_ms_factor - r_pts -1.0;
#if (RENDER_FRAMERATE_LIM >0)
                if (delta < -skipframe_limtime || (prevframe_drawed && (frametime < frametime_lim)))
#else
                if (delta < -skipframe_limtime)
#endif
                {
                    //显示时间超过视频源时间，解码/显示慢，丢弃一些帧不显示（跳帧）
                    frame_queue_rdnext(framebuf);
                    skipframes++;                    
#if (RENDER_FRAMERATE_LIM >0)
                    prevframe_drawed = false;
#endif
#ifdef SEM_CHAIN_ToFB
                    //析出信号量：skip后没有emit信号传到QT界面，下次解码.acquire就等不到，会冻结显示
                    sem_RenderFbValid.release(1);
#endif
//                    qDebug()<<"..skipFrame, r_pts at:"<<r_pts<<",time over:"<<delta<<"played:"<<frameCount;
                    M_VID_LEDOFF;
                    continue;
                }
                else if (delta >3.0)
                {
                    //显示时间小于视频源时间(帧率)，快，延时等待流输入
                    msleep(delta);
                }
#ifdef NO_ONLY_NETSTREAM
                else if (sync_req)
                {
                    //重置播放时标r_pts与输入流的pts一致，这个策略对网络视频不能用！对文件流，用这个策略，paused-resume后，
                    //跳过一帧即可对齐后续pts，不用考虑PausedState，基本保持连续播放
                    r_pts = pDecodedFrame->pts*pts_ms_factor;
                    sync_req = false;
                    qDebug()<<"pts reset to:"<<r_pts;
                }
#endif

            }
//            long t_tmp = rtime.elapsed();
            /* 注1： debug显示，QT paint显示图片可能发生在“解码-时标计算-延时等待”之后, emit signal送显示之前，应为两边
             *  线程是异步的，这就导致本帧被插入一个QT Image显示时间（10ms级别)，影响定时精度。
             */
            //送显示
            //输出的scaledFrame.widhth/.height全是0，没用到的没写值
            sws_scale(pSwsContext,(const uint8_t* const *)pDecodedFrame->data,pDecodedFrame->linesize,0,
                        pictureHeight,pScaledFrame->data, pScaledFrame->linesize);  //YUV->RGB
//            long t_tmp2 = rtime.elapsed() - t_tmp;
            //发送获取一帧图像信号
//            qDebug()<<"Render frame:"<<frameCount<<"pDecodeframe pts:"<<pDecodedFrame->pts*pts_ms_factor<<"r_pts:"<<r_pts;
//            db_step = 4;
            QImage image(pScaledFrame->data[0],renderSize.width(), renderSize.height(),QImage::Format_RGB888);
//            mutex_draw.lock();
            emit GetImage(image, r_pts);
//            mutex_draw.unlock();
//            db_step = 5;
            //
#if (RENDER_FRAMERATE_LIM >0)
            prevframe_drawed = true;
#endif
            frameCount++;
            frame_queue_rdnext(framebuf);
            M_VID_LEDON;
//            qDebug()<<"Render frame:"<<frameCount<<"r_pts:"<<r_pts<<"time:"<<(rtime.elapsed()-t_tmp)<<"ms"<<"swscaleTime:"<<t_tmp2;
//            qDebug()<<"Render frame:"<<frameCount<<"r_pts:"<<r_pts<<"frameTime:"<<frametime;
        }
    }

    //paint显示是在QT主线程做的，如果emit GetImage()时本线程未阻塞，Close时先退出，释放了pScaledFrame；主线程后运行
    //paintEvent，引用Frame，空指针，引起异常退出。
/*
    av_freep(&pScaledFrame->data[0]);
    av_frame_free(&pScaledFrame);
*/
    qDebug()<<"Render done.:"<<r_pts<<"ms"<<"frames:"<<frameCount<<"Skiped:"<<skipframes;

}

//-----------------------------------------------------------------------------------------------------------------

VideoDecoder::VideoDecoder()
{
}

VideoDecoder::~VideoDecoder()
{
}

void VideoDecoder::setContexts(AVCodecContext*ctx, float frameperiod, float ptsFactor, bool isNet)
{
    pAVCodecContext = ctx;
    framePeriod = frameperiod;
    pts_ms_factor = ptsFactor;
    isNetStream = isNet;
}

void VideoDecoder::run()
{
    AVPacket pAVPacket;
    AVFrame *pDecodedFrame;
#ifdef NO_ONLY_NETSTREAM
    int64_t prev_pts = 0;
    int frameCount = 0;
#endif

    qDebug()<<"decoder thread start.:"<<QThread::currentThread();

    int ret,ret2;
    while(1)
    {
        if (_state == StoppedState)
            break;        
        //PausedState加这个处理使已读packet保存在队列中不播放，resume后保持连续; pause效果也能快速出现，
        //否则packet中的packet放出去还有几秒的时间，特别是对文件播放。
        //但是Pause冻结解码器，NetStream模式必然会使部分packet不被解码，视频场景被破坏，resume后几帧有跳动或花屏现象.
#ifdef NO_ONLY_NETSTREAM
        if (!isNetStream && _state == PausedState)
        {
            msleep(50);     //必须要
            continue;
        }
#endif
        if (packet_queue_get(pkgbuf, &pAVPacket))
        {
            do
            {
                //解码器阻塞时avcodec_send_packet()返回EAGAIN, 也要执行avcodec_receive_frame()，之后packet重发，
                //这个情况只是保护，没发生过。
                ret = avcodec_send_packet(pAVCodecContext, &pAVPacket);
/*                if (ret == AVERROR(EAGAIN))
                {
                    qDebug()<<"avcodec_send_packet() fault:"<<ret<<"retry";
                }
*/
                do
                {
                    //frame_queue_get_wrframe()可能存在一次avcodec_send_packet()输入返回多个解码帧的情况，要完全
                    //读出这些frame,在返回0(成功)时要继续读，直到返回EAGAIN（-11）为止。这个重读的过程增加了负担，可以
                    //不用，因为现实测试过的所有流都未发现这种情况，只是作为未来扩展的保护。
                    framebuf->sem_empty.acquire(1);
                    frame_queue_get_wrframe(framebuf, &pDecodedFrame);
                    ret2 = avcodec_receive_frame(pAVCodecContext, pDecodedFrame);
                    if (ret2 ==0)
                    {
                        if (pDecodedFrame->pts <0)
                        {
                            pDecodedFrame->pts = 0;
                        }
#ifdef NO_ONLY_NETSTREAM
                        //对不含pts/错误pts的帧，重构pts
                        if (frameCount>0 && pDecodedFrame->pts <= prev_pts)
                        {
                            pDecodedFrame->pts = prev_pts + framePeriod/pts_ms_factor;
                            qDebug()<<"Decoder rebuild pts:"<<pDecodedFrame->pts;
                        }
                        prev_pts = pDecodedFrame->pts;
                        frameCount++;
#endif
                        //
                        frame_queue_wrnext(framebuf);
//                      qDebug()<<"Decode a Frame, pts:"<<pDecodedFrame->pts<<"packet in Que"<<packet_queue_validnum(pkgbuf);
                    }
                    else
                    {
                        framebuf->sem_empty.release(1);     //未使用FrameQue缓存块，应反向释放，抵消前面的.acquire()
                    }
                }  while(ret2 !=AVERROR(EAGAIN));
            }   while(ret == AVERROR(EAGAIN));

            av_packet_unref(&pAVPacket);     //释放资源
        }
    }
    //
    qDebug()<<"decoder done. remain pkgs:"<<packet_queue_validnum(pkgbuf);
//    qDebug()<<"decoder done.:"<<QThread::currentThread();
}

/**
 * @brief Ffvideo::proccess 数据包接收处理线程函数
 *
 */
void Ffvideo::proccess()
{
    QElapsedTimer rtime;

    if ((_state != StoppedState && _state != OfflineState)
            || videoStreamIndex==-1 || pAVFormatContext==NULL || pAVCodecContext==NULL)
    {
        qDebug()<<"Proc Start Err";
        return;
    }
    qDebug()<<"Play thread start.:"<<QThread::currentThread();
    devIntf->setGpio(GPIO_LED_CameraVideo,1);

    //设定renderSize放在这里可以在open()之后再次设定图像的输出size, 这个三swscale()用的out size，但经反复
    //验证，在ARM上swscale()在输入图像size与输出size不同时，速度比较慢,不如输出原始尺寸的RGB图，由QT 的QImage转换
    //显示到不同size的显示区。不要用setRenderSize()!
    pSwsContext = sws_getContext(pictureWidth,pictureHeight, AV_PIX_FMT_YUV420P,    //pAVCodecContext->pix_fmt替代AV_PIX_FMT_YUV420P有警告
                        renderSize.width(),renderSize.height(),AV_PIX_FMT_RGB24,
                        SWS_FAST_BILINEAR,0,0,0);    //default是SWS_BICUBIC，还有SWS_FAST_BILINEAR等,SWS_POINT/AREA最快

    //配置后端解码器线程
    float ptsfactor =1000*av_q2d(pAVFormatContext->streams[videoStreamIndex]->time_base);   //for IPC=0.0111111

    packet_queue_init(&videoq, VIDEO_PACKETQUE_SIZE_UP);
    frame_queue_init(&frameq);

    videoDecoder.setContexts(pAVCodecContext,framePeriod, ptsfactor, isNetStream);
    videoDecoder.setBufQue(&videoq, &frameq);

    videoRender.setContexts(pAVCodecContext, pSwsContext, &renderSize, framePeriod, ptsfactor);
    connect(&videoRender, SIGNAL(GetImage(QImage,long)),this,SIGNAL(GetImage(QImage,long)),Qt::DirectConnection);  //for moveToThread

    videoRender.setFrameQue(&frameq);
    videoRender.setDevIntf(devIntf);

    if (videoRender.isRunning())
    {
        videoRender.quit();
        videoRender.wait();
    }
    videoRender.setPlayState(PlayingState);
    videoRender.setPriority(QThread::HighPriority);
    videoRender.start();

    if (videoDecoder.isRunning())
    {
        videoDecoder.quit();
        videoDecoder.wait();
    }
    videoDecoder.setPlayState(PlayingState);
    videoDecoder.setPriority(QThread::HighPriority);
    videoDecoder.start();

    //数据包接收处理主循环
    _state = PlayingState;
    emit stateChanged(PlayingState);
    rtime.start();
    int ret = 0;
    while(_state != StoppedState)
    {
        if (_state == PausedState && !isNetStream)
        {
            QCoreApplication::processEvents();
            continue;       //
        }

        ret = av_read_frame(pAVFormatContext, &pAVPacket);
        if (ret ==0)
        {
            if (pAVPacket.stream_index == videoStreamIndex)
            {
                if (packet_queue_put(&videoq, &pAVPacket) <0)
                {
                    av_packet_unref(&pAVPacket);
                    qDebug()<<"AVPacket Que error.";
                    break;
                }
                if (packet_queue_validnum(&videoq)==6)
                {
                    QThread::currentThread()->yieldCurrentThread();     //离开线程，试图降低延时，无效
//                    videoDecoder.wait(10);
//                    videoRender.wait(10);
                }
            }
            else        //audio..
            {
                av_packet_unref(&pAVPacket);
            }
            rtime.start();
        }
        else
        {
            if (isNetStream)
            {
                //网络掉线，超时退出
                if (rtime.elapsed() > 8000)
                {
                    qDebug()<<"--av stream timeout.";
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }

//    qDebug()<<"ffvideo state to Stopped, close videoRender...";

    //播放结束处理    
    if (!isNetStream && ret==AVERROR_EOF)
    {
        qDebug()<<"av file EOF.";
        //等packetQue全部播放完，否则最后一段画面没显示
        while(!videoDecoder.isFinished() && packet_queue_validnum(&videoq)>0)
        {
            QCoreApplication::processEvents();
        }
        //等frameQue全部显示完。总等不到最后一帧，?,保留。
        while(frameq.sem_empty.available() < VIDEO_FRAME_QUEUE_SIZE-1)
        {
            QCoreApplication::processEvents();
        }
    }

    /** 结束下级线程应注意：
     *  如果线程在.acquire()信号量，即使用thread.quit()也无法强制退出线程的！
     *  Close时videoRender可能在acquire frameBuf->sem_valid,sem_RenderFbValid, videoDecoder可能在
     * 等待framebuf->sem_empty，对界面控制stop的情况，基本上都是满足的，一般能退出; 但是在超时错误退出时，Frameq可能是
     * 空的（所有frame已显示), videoRender就在frameBuf->sem_valid.acquire()的地方被阻塞，无法退出线程。这里要用
     * 欺骗手段设置这两个信号量为有效，已确保videoDecoder/videoRender线程正确退出。
     *  sem_RenderFbValid信号量对videoRender线程退出可能也是有影响的，但屏幕QT线程不会阻塞，应该不用考虑。
      */
    videoRender.setPlayState(StoppedState);
    frameq.sem_valid.release(5);
    QCoreApplication::processEvents();
//    videoRender.mutex_draw.lock();
//    emit stopPainter();
    videoRender.wait(200);
//    videoRender.mutex_draw.unlock();

//    int cnt1=0;
    while(!videoRender.isFinished())
    {
/*        if (cnt1>20)
            videoRender.terminate();
        else */
        videoRender.quit();     //用.terminate()会出现“QWaitCondition: mutex destroy failure: 设备或资源忙”，崩溃
//        frameq.sem_valid.release(1);
//        videoRender.requestInterruption();
        //subthread.wait():在主线程中被调用的时候阻塞了主线程, 直到(.之前/关联的)子线程执行完run()或超时
        videoRender.wait(10);
//        QThread::currentThread()->wait(200);
//        qDebug()<<"...try close videoRender thread."<<videoRender.db_step;
//        cnt1++;
        QCoreApplication::processEvents();
    }
    disconnect(&videoRender, SIGNAL(GetImage(QImage,long)),this,SIGNAL(GetImage(QImage,long)));

//    qDebug()<<"ffvideo close videoDecoder.";

    videoDecoder.setPlayState(StoppedState);
    frameq.sem_empty.release(1);
    while(!videoDecoder.isFinished())
    {
        videoDecoder.quit();
        videoDecoder.wait(10);
        QCoreApplication::processEvents();
    }

    closeContext();       // _state = StoppedState

    packet_queue_clear(&videoq);
    frame_queue_free(&frameq);

    emit stateChanged(StoppedState);
    _state = StoppedState;
//    _process_closed = true;
//    _close_step = 2;

    devIntf->setGpio(GPIO_LED_CameraVideo,0);
    qDebug()<<"ffvideo thread close."<<QThread::currentThread();
}

/**
 * @brief Ffvideo::Ffvideo 构造函数，基本初始化
 * @param parent
 */
Ffvideo::Ffvideo(QObject *parent) : QObject(parent)
{
    av_register_all();          //注册库中所有可用的文件格式和解码器
    avformat_network_init();    //初始化网络流格式,使用RTSP网络流时必须先执行

    pAVFormatContext = avformat_alloc_context();    //申请一个AVFormatContext结构的内存,并进行简单初始化
    pAVCodecContext = NULL;
    audCodecContext = NULL;

    _state = OfflineState;
    qRegisterMetaType<PlayState>("PlayState");  //信号-槽机制，作为参数传送的自定义数据类型，要做这个注册处理

    devIntf = DevInterface::GetInstance();
}

Ffvideo::~Ffvideo()
{
    avformat_free_context(pAVFormatContext);
}

/**
 * @brief Ffvideo::open 打开视频流或文件, 建立播放环境
 * @param fileUrl
 * @return
 */
bool Ffvideo::open(QString fileUrl, int tcp_transport)
{
    if (_state != StoppedState && _state != OfflineState)
        return false;
    isNetStream = (fileUrl.indexOf("rtsp:")>=0);

    AVDictionary *avdic = NULL;
    if (isNetStream)
    {
        if (tcp_transport !=0)
        {
            char option_key[] = "rtsp_transport";
            char option_value[] = "tcp";
            av_dict_set(&avdic, option_key, option_value, 0);
            qDebug()<<"rtsp transport with TCP";
        }
        char option_key2[] = "max_delay";
//        char option_value2[] = "2000000";
        char option_value2[] = "1000000";
        av_dict_set(&avdic, option_key2, option_value2, 0);

        av_dict_set(&avdic, "stimeout", "5000000", 0);      //断线超时 5s
//        av_dict_set(&options, "buffer_size", "102400", 0); //设置缓存大小，1080p可将值调大 ?
    }
    //打开
    if (avformat_open_input(&pAVFormatContext, fileUrl.toStdString().c_str(), NULL, &avdic)!=0)
    {
        qDebug()<<"fault for open stream";
        return false;
    }

    //获取视频流信息 -> AVFormatContext
    if (avformat_find_stream_info(pAVFormatContext,NULL)<0)     //avformt.h
    {
        qDebug()<<"fault get videostreamInfo";
        return false;
    }

    //获取视频流索引
    videoStreamIndex = -1;
    audioStreamIndex = -1;
    for (uint i = 0; i < pAVFormatContext->nb_streams; i++)
    {
        if (pAVFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
        }
        if (pAVFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioStreamIndex = i;
        }
    }
    if (videoStreamIndex==-1 && audioStreamIndex==-1)
    {
        qDebug()<<"fault to get Video or Audio streamId";
        return false;
    }

    //打开图像解码器 ->AVCodecContext ->AVCodec
    if (videoStreamIndex !=-1)
    {
        //获取图像分辨率等参数
        AVCodec *pAVCodec = avcodec_find_decoder(pAVFormatContext->streams[videoStreamIndex]->codecpar->codec_id);
        qDebug()<<"File:"<<fileUrl<<"VCodec:"<<pAVCodec->name;    //<<pAVCodec->long_name;

        pAVCodecContext = avcodec_alloc_context3(pAVCodec);
        if (pAVCodecContext==NULL)
        {
            qDebug()<<"fault to alloc video_codec_context";
            return false;
        }

        avcodec_parameters_to_context(pAVCodecContext, pAVFormatContext->streams[videoStreamIndex]->codecpar);

        pictureWidth=pAVCodecContext->width;
        pictureHeight=pAVCodecContext->height;
        framePeriod = 0;
        AVRational vframerate;
        vframerate = pAVFormatContext->streams[videoStreamIndex]->r_frame_rate;
        if  (vframerate.den ==0 || vframerate.num ==0)
            vframerate = pAVFormatContext->streams[videoStreamIndex]->avg_frame_rate;
        if (vframerate.den !=0 && vframerate.num !=0)
        {
            framePeriod = 1000/av_q2d(vframerate);
        }
        if (framePeriod<5 || framePeriod>200)
        {
            framePeriod = 50;
            qDebug()<<"frameRate in stream err, set to 50ms";
        }
        qDebug()<<"frame period:"<<framePeriod<<"ms";
/*
        qDebug()<<"timebase:"<<pAVFormatContext->streams[videoStreamIndex]->time_base.num<<"/"
                <<pAVFormatContext->streams[videoStreamIndex]->time_base.den;
        qDebug()<<"r_frame_rate:"<<pAVFormatContext->streams[videoStreamIndex]->r_frame_rate.num<<"/"
                        <<pAVFormatContext->streams[videoStreamIndex]->r_frame_rate.den;
        qDebug()<<"avg_frame_rate:"<<pAVFormatContext->streams[videoStreamIndex]->avg_frame_rate.num<<"/"
                        <<pAVFormatContext->streams[videoStreamIndex]->avg_frame_rate.den;
*/
        if (!isNetStream)       //一般文件.nb_frames=0，.duration含有总播放时间
        {
            qDebug()<<"number of frames:"<<pAVFormatContext->streams[videoStreamIndex]->nb_frames<<"duration:"
        <<pAVFormatContext->streams[videoStreamIndex]->duration*1000*av_q2d(pAVFormatContext->streams[videoStreamIndex]->time_base);
        }

        renderSize = QSize(pictureWidth, pictureHeight);

        //获取视频流解码器
        if (avcodec_open2(pAVCodecContext,pAVCodec,NULL)<0)     //avcodec.h
        {
            qDebug()<<"fault to open video codec";
            return false;
        }
    }

//    _close_step = 0;
    qDebug()<<"open file ok. thread:"<<QThread::currentThread();
    return true;
}


void Ffvideo::closeContext()
{
    _state = StoppedState;

    sws_freeContext(pSwsContext);

    if (pAVCodecContext != NULL)
    {
        avcodec_close(pAVCodecContext);
        pAVCodecContext = NULL;
    }
    if (audCodecContext != NULL)
    {
        avcodec_close(audCodecContext);
        audCodecContext = NULL;
    }
    if (pAVFormatContext != NULL)
    {
        avformat_close_input(&pAVFormatContext);
    }
}

void Ffvideo::pause()
{
    if (_state != PlayingState)
        return;
    _state = PausedState;
    videoDecoder.setPlayState(PausedState);
    videoRender.setPlayState(PausedState);
    emit stateChanged(PausedState);
    qDebug()<<"ffvideo Paused.";
}

void Ffvideo::resume()
{
    if (_state != PausedState)
        return;
    qDebug()    <<"ffvideo resume.";
    if (!isNetStream)
    {
        videoRender.sync_req = true;
    }
#ifdef SEM_CHAIN_ToFB
    if (sem_RenderFbValid.available()==0)
    {
        sem_RenderFbValid.release(1);
    }
#endif

    videoRender.setPlayState(PlayingState);
    videoDecoder.setPlayState(PlayingState);
    _state = PlayingState;
    emit stateChanged(PlayingState);
}

bool Ffvideo::close()
{
//    if (_state == StoppedState || _state == OfflineState || _close_step !=0)
    if (_state == StoppedState || _state == OfflineState)
    {
//        qDebug()<<"err: ffvideo state busy.";
        return (false);
    }
/*    if (videoRender.db_step ==4)
    {
        qDebug()<<"painter busy..";
        return (false);
    }
*/
//    _process_closed = false;
//    _close_step = 1;
    QMutexLocker locker(&mutex);
    _state = StoppedState;
//    while(!_process_closed)      //确保退出thread
/*    while(_close_step<2)      //确保退出thread
    {
        QCoreApplication::processEvents();
    }
*/
//    return(_close_step>=2);
    return(true);
}

