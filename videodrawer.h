#ifndef VIDEODRAWER_H
#define VIDEODRAWER_H

#include <QObject>
#include <QPen>
#include <QWidget>
//#include <QFrame>
#include <QMutex>
#include <QtOpenGL>

#define RENDER_OPENGL

//-----------------------------------------------------------------------

#ifdef RENDER_OPENGL
//class VideoDrawer : public QGLWidget
class VideoDrawer : public QOpenGLWidget
#else
class VideoDrawer : public QWidget
#endif
{
    Q_OBJECT
public:
    explicit VideoDrawer(QWidget *parent=0);
    ~VideoDrawer();

    QImage videoImg;
    QRect renderRect;
    int frameCount;

    void setVideoSize(int rawVideo_width, int rawVideo_height);
protected:
    void paintEvent(QPaintEvent* event);

private:
    QSize rawVideoSize;
    QMutex mutex_draw;
    QPixmap *pixmap;
    bool frameValid;
public slots:
    void setFrame(QImage frame, long playTime);
//    void stopPainter();
};

#endif // VIDEODRAWER_H
