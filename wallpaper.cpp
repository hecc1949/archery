#include "wallpaper.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QtGui>
//#include <QToolBox>
#include <QGridLayout>
//#include <QHBoxLayout>
//#include <QVBoxLayout>
#include <QTimer>
#include <QTime>
#include <QDockWidget>

//#include <QWSServer>
#include <QScreen>

#include "archerview.h"
#include "configdlg.h"
#include "dbview.h"

Wallpaper::Wallpaper(QWidget *parent)
          : QMainWindow(parent)
{
    /*
#ifdef ARM
    QWSServer::setBackground(QColor(71,138,205,255));   //黑色RGBA=QColor(0,0,0,0)
#endif
*/
//    setAttribute( Qt::WA_TranslucentBackground,true );

#ifdef ARM
    QDesktopWidget* desktop = QApplication::desktop();
    resize(desktop->width(),desktop->height());
#else
    resize(1280, 720);
#endif
    setWindowFlags(Qt::FramelessWindowHint);

    //
    setAutoFillBackground(true);
    QPixmap pixmap("image/img02.jpg");
    QPalette   palette;
    palette.setBrush(QPalette::Background,QBrush(pixmap));
    setPalette(palette);
    QRegion maskedRegion(0, 0, width(), height(), QRegion::Rectangle);
    setMask(maskedRegion);

    //
    QWidget *toolbox = new QWidget(this);
    QGridLayout *gridlayout = new QGridLayout(toolbox);

    labelTitle = new QLabel(this);
    labelTitle->setText("欢迎光临");
    labelTitle->setAlignment(Qt::AlignCenter);
//    labelTitle->setStyleSheet("background-color:#939299; color:rgb(5,255,5); font-size:32px;
    labelTitle->setStyleSheet("background-color:#939299; color:rgb(5,255,5); font-size:22px; \
            border: 2px groove gray; border-top-left-radius:6px;border-top-right-radius:6px; \
            border-bottom-left-radius:6px;border-bottom-right-radius:6px;   \
            padding-left:0px;padding-right:0px;");
    gridlayout->addWidget(labelTitle, 0, 0, 1, 3);

    //统一风格设置
//    int w1 = 64, h1 = 64;
    int w1 = 36, h1 = 36;
    Qt::ToolButtonStyle style_btn = Qt::ToolButtonTextBesideIcon;     //Qt::ToolButtonTextUnderIcon;
//    QString styles = "background-color:#93A2B9; color:#0000ff; font-size:24px";
    QString styles = "background-color:#93A2B9; color:#0000ff; font-size:16px";

    //第1行的Button
    btnExit = new QToolButton(this);
    btnExit->setIconSize(QSize(w1,h1));
    btnExit->setToolButtonStyle(style_btn);
    btnExit->setStyleSheet(styles);
    gridlayout->addWidget(btnExit, 0, 3);
    QAction *exitAction = new QAction(tr("关机"), this);
    exitAction->setIcon(QPixmap("icon/stop.png"));
    exitAction->setShortcut(tr("Ctrl+Q"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(exitApp()));
    btnExit->setDefaultAction(exitAction);

    //第2行各个Button
    btnStart = new QToolButton(this);
    btnStart->setIconSize(QSize(w1,h1));
    btnStart->setMaximumSize(2*w1+w1/2,h1*2);       //
    btnStart->setToolButtonStyle(style_btn);
    btnStart->setStyleSheet(styles);
//    btnStart->setIcon(QPixmap("icon/play.png"));
    gridlayout->addWidget(btnStart, 1, 0);
//    QAction *startAction = new QAction(tr("点击开始，右键切换"), this);
    QAction *startAction = new QAction(tr("开始"), this);
    startAction->setIcon(QPixmap("icon/play.png"));
    connect(startAction, SIGNAL(triggered()), this, SLOT(startgame()));
    btnStart->setDefaultAction(startAction);

//    btnStart->setContextMenuPolicy (Qt::CustomContextMenu);
//    connect(btnStart, SIGNAL(customContextMenuRequested(QPoint)),this, SLOT(startBtnRightClick(QPoint)));   //鼠标右键

    btnReport = new QToolButton(this);
    btnReport->setIconSize(QSize(w1,h1));
    btnReport->setToolButtonStyle(style_btn);
    btnReport->setStyleSheet(styles);
    gridlayout->addWidget(btnReport, 1, 1);
    QAction *reportAction = new QAction(tr("报告"), this);
    reportAction->setIcon(QPixmap("icon/play.png"));
    connect(reportAction, SIGNAL(triggered()), this, SLOT(report()));
    btnReport->setDefaultAction(reportAction);
//    btnReport->setEnabled(false);

    btnConfig = new QToolButton(this);
    btnConfig->setIconSize(QSize(w1,h1));
    btnConfig->setToolButtonStyle(style_btn);
    btnConfig->setStyleSheet(styles);
    gridlayout->addWidget(btnConfig, 1, 2);
    QAction *configAction = new QAction(tr("配置"), this);
    configAction->setIcon(QPixmap("icon/enlarge.png"));
    connect(configAction, SIGNAL(triggered()), this, SLOT(config_dlg()));
    btnConfig->setDefaultAction(configAction);

    btnResource = new QToolButton(this);
    btnResource->setIconSize(QSize(w1,h1));
    btnResource->setToolButtonStyle(style_btn);
    btnResource->setStyleSheet(styles);
    gridlayout->addWidget(btnResource, 1, 3);
    QAction *resourceAction = new QAction(tr("资源"), this);
    resourceAction->setIcon(QPixmap("icon/enlarge.png"));
    connect(resourceAction, SIGNAL(triggered()), this, SLOT(resource_cfg()));
    btnResource->setDefaultAction(resourceAction);
    btnResource->setEnabled(false);

    //
    dockPan = new QDockWidget(tr("射箭游戏"),this);
    dockPan->setStyleSheet("background-color:#93A2B9; color:#0000ff");
    dockPan->setAutoFillBackground(true);

    dockPan->setWidget(toolbox);
    dockPan->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    dockPan->move(width() - 700, height() - 300);     //设置位置

    QTimer *idle_timer = new QTimer(this);
    connect(idle_timer,SIGNAL(timeout()), this, SLOT(idleTimer()));
    idle_times = 0;
    idle_timer->start(1000);

    //
//    laneMode = DEF_LANE_LEFT;
//    SelectLaneMode(false);

    devIntf = DevInterface::GetInstance();
//    cameraLinkStep = 0;

//    dialogRunings = 0;
    isBackgroup = false;

}

Wallpaper::~Wallpaper()
{
}

void Wallpaper::closeEvent(QCloseEvent *)
{
}

void Wallpaper::clearAndClose()
{
    dockPan->setHidden(true);
    this->clearMask();
    QPalette palette;
    palette.setColor(QPalette::Background, Qt::lightGray);
    setPalette(palette);
    repaint();

    QTimer *closeTimer = new QTimer(this);
    closeTimer->setSingleShot(true);
    connect(closeTimer, &QTimer::timeout, this,[=](){this->close();});
    closeTimer->start(50);
}

void Wallpaper::exitApp()   //slots
{
    dockPan->hide();
    this->repaint();

//    qDebug()<<"Application close.";
    devIntf->closeProcs();

    clearAndClose();
//    system("dd if=/dev/zero of=/dev/fb0");    //eglfs清屏，可以，但不规范
//    this->close();
//    QApplication::exit(0);
//#    system("poweroff");
}

void Wallpaper::idleTimer()     //slots
{    
    idle_times++;
    if (isBackgroup)
        return;
    if (idle_times ==1)
    {
        devIntf->setBuzzle(1);
    }
    devIntf->setGpio(GPIO_LED_0, 3);

    //时间显示
    if (idle_times >5 && devIntf->cameraLinkStatus!=0)
    {
        QTime curtime = QTime::currentTime();
        if (curtime.second() <10 || curtime.second()>50)
        {
            labelTitle->setText(curtime.toString("hh:mm:ss"));
        }
        else
        {
            labelTitle->setText(curtime.toString("hh:mm"));
        }
    }

    //
    if ((idle_times % 10)==1 && devIntf->cameraLinkStatus==0)
    {
        CheckCameraLinks();
    }

}

void Wallpaper::startgame()     //slots
{
    SignIn *signIn = new SignIn();
//    signIn->setWindowModality(Qt::ApplicationModal);
//    signIn->setMode(0);
    signIn->setMode(NULL);
    if (!signIn->exec())
        return;
    SigninSheet sheet;
    if (!signIn->getSignInSheet(sheet))
        return;

    ArcherView *arview = new ArcherView(NULL);
    arview->setSignIn(sheet);

    arview->setWindowModality(Qt::ApplicationModal);    //和setAttribute(Qt::WA_ShowModal, true)两选一，都可
    arview->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);   //不允许TAB切换

    //QT5 for eglfs下showFullScreen()/showMaximized()都不能使窗口运行在全屏模式，只能Resize()来设置。
    arview->setGeometry(0,0, this->width(), this->height());
    arview->show();
    isBackgroup = true;
}

void Wallpaper::report()        //slots
{
    DbView *dbview = new DbView(this);
    dbview->exec();
}

void Wallpaper::config_dlg()    //slots
{
    ConfigDlg *cfgdlg = new ConfigDlg(this);

    cfgdlg->setAttribute(Qt::WA_ShowModal, true);
    devIntf->setGpio(GPIO_Relay,1);
//    dialogRunings = 1;
    isBackgroup = true;
    cfgdlg->exec();     //模式对话
    isBackgroup = false;
//    dialogRunings = 0;
    devIntf->setGpio(GPIO_Relay,0);
//    devIntf->setGpio(GPIO_LED_CameraVideo,0);
}

void Wallpaper::resource_cfg()
{

}


void Wallpaper::CheckCameraLinks()
{
    if (devIntf->cameraLinkStatus ==0)
    {
        connect(devIntf, SIGNAL(externProcDone(int)), this, SLOT(devIntf_exProcEnd(int)));
        devIntf->startOnvifStrobe();
        labelTitle->setText(tr("连接摄像头..."));
    }
    else
    {
        labelTitle->setText(tr("摄像头已连接"));
    }

}

void Wallpaper::devIntf_exProcEnd(int exitCode)    //slot
{
    Q_UNUSED(exitCode);
    if (devIntf->cameraLinkStatus !=0)
    {
        labelTitle->setText(tr("摄像头已连接"));
    }
    else
    {
        labelTitle->setText(tr("没有找到摄像头"));
    }
    disconnect(devIntf, SIGNAL(externProcDone(int)), this, SLOT(devIntf_exProcEnd(int)));
//    qDebug() <<"Camera Check done." <<laneDisMark << "is:"<<devIntf->cameraLinkStatus;
}

