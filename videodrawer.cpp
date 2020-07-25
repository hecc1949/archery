#include "videodrawer.h"
#include <qpainter.h>
#include <QDebug>
#include <QSemaphore>
#include <QCoreApplication>
#include "ffvideo.h"
//#include <QDirectPainter>

//extern QSemaphore sem_RenderFbValid;

#ifdef RENDER_OPENGL
//VideoDrawer::VideoDrawer(QWidget *parent)    :QGLWidget(parent)
VideoDrawer::VideoDrawer(QWidget *parent)    :QOpenGLWidget(parent)
#else
VideoDrawer::VideoDrawer(QWidget *parent)    :QWidget(parent)
#endif
{
//    setAttribute(Qt::WA_OpaquePaintEvent);      //每次Paint不擦除旧的，直接画。repaint()获得和repaint(&rect)一样的效果

    setAutoFillBackground(false);   //for QGLWidget, 每次Paint不擦除旧的

    rawVideoSize = QSize(0,0);
    frameCount = 0;
    frameValid = false;
}

VideoDrawer::~VideoDrawer()
{
}

void VideoDrawer::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (frameValid)
    {
        painter.drawImage(renderRect, videoImg);
        frameValid = false;
        //
        int x1 = renderRect.width()/2 + renderRect.x();
        int y1 = renderRect.height()/2 + renderRect.y();
        int r1 = 100;
//        painter.setPen(QPen(Qt::magenta,8));
        painter.setPen(QPen(Qt::darkGreen,8));
//        painter.setPen(QPen(Qt::yellow,8));
        painter.drawEllipse(QPoint(x1,y1),r1,r1);
        painter.drawLine(QPoint(x1-r1-20,y1),QPoint(x1+r1+20,y1));
        painter.drawLine(QPoint(x1,y1-r1-20),QPoint(x1,y1+r1+20));

#ifdef SEM_CHAIN_ToFB
        sem_RenderFbValid.release(1);
#endif
    }
    else
    {
        QPoint p1 = QPoint(0,0);
        QLinearGradient linearGradient(p1.x(), p1.y(), p1.x()+800, p1.y()+800);     //方向线，覆盖绘图范围
        linearGradient.setColorAt(0.1, QColor(76,114,237,255));  //起点颜色，坐标是0~1区间值
        linearGradient.setColorAt(1, QColor(255, 255, 255, 255));       //终点颜色，坐标是0~1区间值

        painter.setBrush(linearGradient);
        painter.drawRect(QRect(p1.x(),p1.y(),this->width(),this->height()));
    }    
//    qDebug()<<"draw frame:"<<frameCount;
}

void VideoDrawer::setVideoSize(int rawVideo_width, int rawVideo_height)
{
    int x1,y1,w1,h1;
    int border = 0;

    rawVideoSize = QSize(rawVideo_width, rawVideo_height);
    if (rawVideo_width <=0 || rawVideo_height <=0)
        return;
    w1 = (this->width() - border) & (~0x0F);      //client size
    h1 = (this->height() - border)& (~0x0F);
    if (rawVideo_width <=w1 && rawVideo_height <=h1 && rawVideo_width>w1/3)
    {
        w1 = rawVideo_width;    //尽量保持原尺寸，不放大
        h1 = rawVideo_height;
    }
    else
    {
        if (rawVideo_width*h1 > rawVideo_height*w1)
        {
            //Y截短
            h1 = (rawVideo_height*w1/rawVideo_width) & (~0x0F);
        }
        else
        {
            //X截短
            w1 = (rawVideo_width*h1/rawVideo_height) & (~0x0F);
        }
    }

    x1 = (this->width()-w1)/2;
    y1 = (this->height()-h1)/2;   //0 or border/2

    renderRect = QRect(x1,y1,w1,h1);
    videoImg = QImage(rawVideo_width,rawVideo_height, QImage::Format_RGB888);
}

void VideoDrawer::setFrame(QImage frame, long playTime)     //slot
{
    Q_UNUSED(playTime);

    videoImg = frame;
    frameValid = true;
//    repaint();
    repaint(renderRect);
    QCoreApplication::processEvents();      //!
    frameCount++;
}

/*
void VideoDrawer::stopPainter()
{
    qDebug()<<"painter stop";
//    QCoreApplication::flush();
    QCoreApplication::removePostedEvents(NULL);

}
*/
