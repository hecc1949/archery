#include "archerview.h"
#include "ui_archerview.h"
#include <QDesktopWidget>
#include <QTimer>
#include <QTime>
#include <QMovie>
//#include <QMessageBox>
#include <qpainter.h>
#include <QMouseEvent>

#include <QSettings>        //读写ini配置文件

#include <QFileDialog>
#include <QDebug>
#include "wallpaper.h"

ArcherView::ArcherView(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ArcherView)
{
    loadGameImages();

    ui->setupUi(this);


    QTimer *timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()), this, SLOT(runTimer()));
    timer->start(1000);
    _closingCamera = false;

    rawVideo_width = 1280;
    rawVideo_height = 720;

//    ui->btnClose->setShortcut(Qt::Key_Escape);

    ffvideo = new Ffvideo();
    /** A.视频播放的信号/槽连接方式很重要：
     *  DirectConnection方式，信号emit后会直接执行slot函数，执行完才继续，slot函数实际是在emit信号的线程中执行的。
     *  BlockingQueuedConnection方式，信号emit的线程在emit后被阻塞，按系统的线程调度，执行slot函数所在的线程，slot函数执行完后阻塞才解除，即
     *          slot函数是接收信号的（主）线程中执行的。
     *
     *  default的AutoConnection方式等价于QueuedConnection，完全按系统线程调度queue执行，这样会导致源视频帧emit送过来的速度很快时，
     * VideoRender线程运行一次emit多个frame，再切换到QT界面线程执行slot函数，QT主线程也是运行一次执行多次slot函数（paint显示），而实际的frame
     * 缓存只有一个，图像被破坏，解码虽然都顺序完成了，显示却是跳动的。
     *
     */
    connect(ffvideo, SIGNAL(GetImage(QImage, long)), ui->widCamera,SLOT(setFrame(QImage,long)),Qt::BlockingQueuedConnection);

//    connect(ffvideo, SIGNAL(stopPainter()), ui->widCamera,SLOT(stopPainter()));
    connect(ffvideo, SIGNAL(stateChanged(PlayState)), this,SLOT(playStateChanged(PlayState)));

    ffvideo->moveToThread(&videoThread);
    QObject::connect(&videoThread, SIGNAL(started()), ffvideo, SLOT(proccess()));
//    QObject::connect(&vidThread, SIGNAL(finished()), ffvideo, SLOT(deleteLater()));   //不能要

    devIntf = DevInterface::GetInstance();
    cameraState = CAM_Offline;
/*
    scoreing = new ArcheryScore();
    scoreing->newGame(BILLING_BYTIME, 10, -1);

    setupFaces();
    setScoreState(ST_Prepare);
*/
    qsrand(time(NULL));     //随机数初始化
    gif_movie = new QMovie();
    ui->labIcon->setScaledContents(true);       //图片大小自适应

    //radioButton组的手工连接
    ui->buttonGroup_2->setId(ui->rdbChartMode1, 0);
    ui->buttonGroup_2->setId(ui->rdbChartMode2, 1);
    ui->buttonGroup_2->setId(ui->rdbChartMode3, 2);
    ui->buttonGroup_2->setId(ui->rdbChartMode4, 3);
    connect(ui->rdbChartMode1,SIGNAL(toggled(bool)), this, SLOT(on_rdbChartMode4_toggled(bool)));
    connect(ui->rdbChartMode2,SIGNAL(toggled(bool)), this, SLOT(on_rdbChartMode4_toggled(bool)));
    connect(ui->rdbChartMode3,SIGNAL(toggled(bool)), this, SLOT(on_rdbChartMode4_toggled(bool)));

//    drawBarchart();
    //
//    scoreing = new ArcheryScore();
//    scoreing->newGame(BILLING_BYTIME, 10, -1);

//    setupFaces();
//    setScoreState(ST_Prepare);

}

ArcherView::~ArcherView()
{     
    delete ui;
}


void ArcherView::showEvent(QShowEvent *)
{
}

void ArcherView::resizeEvent(QResizeEvent*)
{
}

void ArcherView::paintEvent(QPaintEvent*)
{
}

void ArcherView::closeEvent(QCloseEvent *)
{
    if (this->parent() != NULL)
    {
        ((Wallpaper *)this->parent())->isBackgroup = false;     //通知主控窗口
    }

    // !在PC平台下，在退出时会出现QGLWidget未安全释放的错误，出现一大串错误提示：
    //  QGLContext::makeCurrent(): Cannot make invalid context current.
    // 解决的方法是在这里强行释放QGLWidget (--QT4)
#ifdef RENDER_OPENGL
    ui->widCamera->setParent(NULL); //for QT4
    delete ui->widCamera;    
    this->setParent(NULL);  //for QT5可以消除程序结束时的错误提示：QEGLPlatformContext: eglMakeCurrent failed: 3009
    this->deleteLater();
#endif
    while(videoThread.isRunning())
    {
        videoThread.quit();     //对stopped后重新打开摄像头，这是必须的，ffvideo线程可能还没退出.
        videoThread.wait();
    }
}

void ArcherView::playStateChanged(PlayState state)      //slot
{
    if (state == PlayingState)
    {
        cameraState = CAM_Playing;
        ui->btnVideoCtrl->setText(tr("冻结图像"));
    }
    else if (state == StoppedState)
    {
        cameraState = CAM_Stopped;
        ui->btnVideoCtrl->setText(tr("重新连接"));
        if (_closingCamera)
            this->close();
    }
    else
    {
        cameraState = CAM_Paused;
        ui->btnVideoCtrl->setText(tr("恢复"));
    }
}

/**
 * @brief ArcherView::startCamera
 * @return
 */
int ArcherView::startCamera()
{
    if (devIntf->cameraLinkStatus ==0)
        return(-1);
    if (cameraState == CAM_Playing || cameraState == CAM_Paused)
        return(-2);
    while(videoThread.isRunning())
    {
        videoThread.quit();     //对stopped后重新打开摄像头，这是必须的，ffvideo线程可能还没退出.
        videoThread.wait();
    }    

    QString filename;
    if (devIntf->DroneCameras.usingSteamId>0 && devIntf->DroneCameras.stream_url[1].length()>0)
        filename = devIntf->DroneCameras.stream_url[1];
    else
        filename = devIntf->DroneCameras.stream_url[0];
/*
    //for test file-Mode
    filename = QFileDialog::getOpenFileName(this, tr("Open Image"), ".",
                        tr("Video Files (*.avi *.mpg *.mp4 *.wmv *.h264 *.h265 *.rm *.rmvb)"));
*/
    if (filename.length()==0)
        return(-3);
    if (!ffvideo->open(filename, devIntf->DroneCameras.tcp_transport))
        return(-2);


    rawVideo_width = ffvideo->getPictureWidth();
    rawVideo_height = ffvideo->getPictureHeigth();
    ui->widCamera->setVideoSize(rawVideo_width,rawVideo_height);

//    connect(ffvideo, SIGNAL(GetImage(QImage, long)), ui->widCamera,SLOT(setFrame(QImage,long)),Qt::BlockingQueuedConnection);
    videoThread.start();

    return(0);
}

void ArcherView::on_btnVideoCtrl_clicked()
{
    if (cameraState == CAM_Playing)
    {
        ffvideo->pause();
    }
    else if (cameraState == CAM_Paused)
    {
        ffvideo->resume();
    }
    else if (cameraState == CAM_Stopped)
    {
        startCamera();
    }

}

void ArcherView::on_toolbtnClose_clicked()
{
    doClose();
}

void ArcherView::doClose()
{
    if (_closingCamera)
        return;
    _closingCamera = ffvideo->close();

    QSettings ini("./config.ini",QSettings::IniFormat);
    ini.setValue("/scheme/shotTimeIndex", ui->cmShotTime->currentIndex());

    scoreing->closeGame();

    if (!_closingCamera)
    {
        if (!(cameraState == CAM_Playing || cameraState == CAM_Paused))
        {
            this->close();
        }
    }
}

//======================================================================================================================================
//
//  射箭业务相关
//
//======================================================================================================================================

//------------------------------------------- A. 辅助功能 -------------------------------------------------------------------------------
/**
 * @brief ArcherView::on_cmShotTime_currentIndexChanged 不同的"每箭限时"模式选择
 * @param index
 */
void ArcherView::on_cmShotTime_currentIndexChanged(int index)
{
    int id = index;
    if (id ==0)
    {
        shotLimitTime = 0;
        ui->lcdnumCurrentTime->setDigitCount(9);
    }
    else
    {
        shotLimitTime = ui->cmShotTime->itemData(id).toInt();
        if (shotLimitTime >99)
            ui->lcdnumCurrentTime->setDigitCount(3);
        else
            ui->lcdnumCurrentTime->setDigitCount(2);
    }
    shotIntfTime = shotLimitTime;
    //！不能在这里改写ini保存shotTimeIndex(=id), 因为初始化时有设置index=0的default动作。
}


void ArcherView::on_sldDistance_sliderMoved(int position)
{
    Q_UNUSED(position);

    ui->labDistance->setText(QString("%1m").arg(ui->sldDistance->value()));
}

/**
 * @brief 双击记分表进入改分界面  ArcherView::on_lvShooting_doubleClicked
 *   * @param index
 */
void ArcherView::on_lvShooting_doubleClicked(const QModelIndex &index)
{
    if ((index.row() >=0)&& scoreState == ST_Shooting)
    {
        int points = scoreing->shotList[index.row()].points;
        ui->cmModifyPoint->setCurrentIndex(points);
        ui->stackScoreEx->setCurrentIndex(1);
        setScoreState(ST_ModifyPoints);
    }
}

/**
 * @brief 改分动作 ArcherView::on_cmModifyPoint_currentIndexChanged
 * @param index
 */
void ArcherView::on_cmModifyPoint_currentIndexChanged(int index)
{
    if (scoreState == ST_ModifyPoints)
    {
        if (scoreing->modifyShootingPoints(ui->lvShooting->currentIndex().row(), index))
        {
            setScoreState(ST_Shooting);
            ui->stackScoreEx->setCurrentIndex(0);
        }
    }
}


void ArcherView::loadGameImages()
{
    QFile loadFile("gameview.json");
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        qWarning("Load json file failure");
        return;
    }
    QByteArray jsonDat = loadFile.readAll();
    loadFile.close();
    //
    QJsonParseError json_err;
    QJsonDocument jsonCfgDoc = QJsonDocument::fromJson(jsonDat, &json_err);
    if (json_err.error != QJsonParseError::NoError)
    {
        return;
    }
    if (!jsonCfgDoc.isObject())
    {
        return;
    }

    QJsonObject cfgObj = jsonCfgDoc.object();
    QJsonObject imglistObj = cfgObj.find("imagelist")->toObject();
    imglist_bulletin = imglistObj.find("bulletin-pane")->toObject();

#if 0
    int size;
    QString s1;

//    QJsonValue idle_images = bulletinObj.value("idle");
//    QJsonValue idle_images = bulletinObj.value("gameover");
    QJsonValue idle_images = bulletinObj.value("runing");
    if (idle_images.isArray())
    {
        QJsonArray filenames = idle_images.toArray();
        size = filenames.size();
        for(int i=0; i<size; i++)
        {
            s1 = filenames.at(i).toString();
            qDebug()<<s1;
        }
    }
#endif
    //
}

///
/// \brief ArcherView::getBulletinImgfile 随机选取展板显示图片
/// \return
///
QString ArcherView::getBulletinImgfile()
{
    QString res = "";
    if (imglist_bulletin.isEmpty())
        return(res);
    QJsonValue jsonval;

    if (scoreState == ST_GameOver)
    {
        jsonval = imglist_bulletin.value("gameover");
    }
    else if (scoreState == ST_Prepare)
    {
        jsonval = imglist_bulletin.value("idle");
    }
    else
    {
        jsonval = imglist_bulletin.value("runing");
    }
    if (!jsonval.isArray())
        return(res);
    QJsonArray imgfiles = jsonval.toArray();

    int size = imgfiles.size();
    int id = qrand() % size;
    res = imgfiles.at(id).toString();

    return(res);
}

//------------------------------------------- B. 主功能 -------------------------------------------------------------------------------


/**
 * @brief ArcherView::runTimer
 */
void ArcherView::runTimer()     //slot
{
    scoreStateTimer++;
    //时间提示
    QTime curtime = QTime::currentTime();
    if (shotLimitTime ==0)
    {
        ui->lcdnumCurrentTime->display(curtime.toString("hh:mm:ss"));
        //不限时模式，直接显示已用时间。
        //注意这里实际两个DateTime值相减得到一个时间值的方法。QDateTime/QTime不能直接相减.
        QTime _time;
        _time.setHMS(0,0,0);
        ui->lcdRunedTimes->display(_time.addSecs(scoreing->gameRecord.begtime.secsTo(QDateTime::currentDateTime())).toString("hh:mm"));
    }
    else
    {
        shotIntfTime--;
        if (shotIntfTime<=0)
        {
            shotIntfTime = shotLimitTime;
        }
        ui->lcdnumCurrentTime->display(QString("%1").arg(shotIntfTime));
    }
    if (curtime.second() %10 ==0)
    {
        //进度显示和控制
        displayGameProgress(1);
        if (scoreState == ST_GameOver)
        {
            if (scoreStateTimer >= gameOverDelay_sec)
            {
                doClose();
            }
        }
        else
        {
            if (scoreing->checkoutGame())
            {
                setScoreState(ST_GameOver);
            }
        }
    }
    //

    //自动查找/更新摄像头的IP地址，启动摄像头
    if (cameraState == CAM_Offline)
    {
        if (devIntf->cameraLinkStatus ==0)
        {
            devIntf->startOnvifStrobe();
            ui->btnVideoCtrl->setText(tr("连接摄像头.."));
        }
        cameraState = CAM_SearchURL;
    }
    else if (cameraState == CAM_SearchURL)
    {
        if (devIntf->cameraLinkStatus !=0)
        {
            cameraState = CAM_Opening;
            startCamera();
        }
    }

    //切换自动/人工状态
    if (scoreState == ST_Prepare)
    {
        if (scoreStateTimer >2)
        {
            setScoreState(ST_Shooting);
        }
    }
    else if (scoreState == ST_ModifyPoints)
    {
        if (scoreStateTimer >10)        //自动结束改分状态
        {
            setScoreState(ST_Shooting);
            ui->stackScoreEx->setCurrentIndex(0);
        }
    }
}

///
/// \brief ArcherView::setSignIn 游戏局开启的注册入口
/// \param sheet
///
void ArcherView::setSignIn(SigninSheet& sheet)          //export
{
    int pay_eid = -1;
    scoreing = new ArcheryScore();
    if (sheet.pay_amount >0)
    {
        pay_eid = scoreing->savePayment(-1, sheet.pay_amount,"");
    }
    scoreing->newGame(sheet.billing_method, sheet.billing_lim, pay_eid, sheet.archerName, sheet.gameType);
    setupFaces();
    setScoreState(ST_Prepare);

    _signSheet = sheet;
}

///
/// \brief ArcherView::setupFaces 游戏界面初始化(和局设置无关部分)
///
void ArcherView::setupFaces()
{
    int i;
    QSettings ini("./config.ini",QSettings::IniFormat);
    gameOverDelay_sec = ini.value("/scheme/GameOverDelay_Sec").toInt();
    if (gameOverDelay_sec <3)
        gameOverDelay_sec = 3;

    //场馆名称
    QString title = ini.value("/BaseInfo/Title").toString();
    ui->labTitleA->setText(title);

    //
    ui->edArcherName->installEventFilter(this);
    ui->btnVideoCtrl->setText(tr("..."));


    //每箭限时模式
    ui->cmShotTime->clear();
    ui->cmShotTime->addItem(tr("不限时"));
    int shotTimeBase = ini.value("/scheme/shotTimeBase").toInt();
    if (shotTimeBase<30)
        shotTimeBase = 30;
    int shotTimeStep = ini.value("/scheme/shotTimeStep").toInt();
    if (shotTimeStep<10)
        shotTimeStep = 10;
    for(i=0; i<3; i++)
    {
        int v1 = shotTimeBase+i*shotTimeStep;
        ui->cmShotTime->addItem(QString("每箭限%1秒").arg(v1), v1);
    }
    int shotTimeId = ini.value("/scheme/shotTimeIndex").toInt();
    if (shotTimeId <0 || shotTimeId>3)
        shotTimeId = 0;
    ui->cmShotTime->setCurrentIndex(shotTimeId);
    ui->lcdnumCurrentTime->display("");

    //每局预定箭数
    int ar_num = 12;
    int ar_add = 10;
    ui->cmArrowsforgame->clear();
    for(i=0; i<5; i++)
    {
        ui->cmArrowsforgame->addItem(QString("%1箭").arg(ar_num),ar_num);
        ar_num += ar_add;
    }
    ui->cmArrowsforgame->setCurrentIndex(0);

    //数据视图模式
    ui->cmScoreViewMode->clear();
    ui->cmScoreViewMode->addItem(tr("本次成绩"));
    ui->cmScoreViewMode->addItem(tr("历史成绩"));
    ui->cmScoreViewMode->setCurrentIndex(0);
    //
    ui->chkMute->setText("");
    ui->chkMute->setStyleSheet("QCheckBox{ spacing: 5px; }"
        "QCheckBox::indicator{ width: 32px; height: 32px;  }"
        "QCheckBox::indicator:unchecked{ image:url(:/checkboxs/res/Mute_E.png) }"
        "QCheckBox::indicator:checked{ image:url(:/checkboxs/res/Mute_D.png); }"
        "QCheckBox::indicator:checked:disabled{ image:url(:/checkboxs/res/Mute_D.png);  }");

    //箭靶距离
    ui->sldDistance->setValue(10);
    ui->labDistance->setText(QString("%1m").arg(ui->sldDistance->value()));
    ui->sldDistance->installEventFilter(this);

    //
    ui->widCamera->installEventFilter(this);
    ui->widCamera->setMouseTracking(true);

    ui->stackScoreEx->setCurrentIndex(0);

    //得分记录显示
    ui->lvShooting->setViewMode(QListView::IconMode);
    ui->lvShooting->setMovement(QListView::Static);
    ui->lvShooting->setFlow(QListView::LeftToRight);    //
    ui->lvShooting->setEditTriggers(QAbstractItemView::NoEditTriggers);     //不许编辑
    ui->lvShooting->setSpacing(0);
    ui->lvShooting->setIconSize(QSize(48,48));
    ui->lvShooting->setGridSize(QSize(100,80));     //这个尺寸，特别是高度，影响换行
    ui->lvShooting->setResizeMode(QListView::Adjust);
    ui->lvShooting->setWrapping(true);
    ui->lvShooting->setSpacing(1);
    ui->lvShooting->setModel(scoreing->shootingModel);
    //进度条和限制值
    ui->progGameStep->setMinimum(0);
    ui->progGameStep->setValue(0);

    //
    signIn2Faces();
}

///
/// \brief ArcherView::signIn2Faces 游戏界面初始，动态部分
///
void ArcherView::signIn2Faces()
{
    QSettings ini("./config.ini",QSettings::IniFormat);

    QString s1 = ini.value(QString("/scheme/GameType-%1").arg(scoreing->gameRecord.gametype+1)).toString();
    s1 = s1 +" : " + scoreing->gameRecord.archer;
    ui->edArcherName->setText(s1);

    if (scoreing->gameRecord.billing_method == Billing_ByTime)
        s1 = QString("%1分钟").arg(scoreing->gameRecord.billing_lim);
    else
        s1 = QString("%1箭").arg(scoreing->gameRecord.billing_lim);
    ui->labBillingMethod->setText(s1);

    if (scoreing->gameRecord.billing_method == BILLING_BYSHOTS)
        ui->progGameStep->setMaximum(scoreing->gameRecord.billing_lim);
    else
        ui->progGameStep->setMaximum(scoreing->gameRecord.billing_lim*60);  //按second显示

}


///
/// \brief ArcherView::do_modifyArcher 修改选手名，增加时间/付款
///
void ArcherView::do_modifyArcher()
{
    SignIn *signIn = new SignIn();
    signIn->setMode(&_signSheet);
    if (!signIn->exec())
        return;
    SigninSheet sheet;
    if (!signIn->getSignInSheet(sheet))
        return;
    if (sheet.archerName.length()>0 && _signSheet.archerName.length()==0)
    {
        _signSheet.archerName = sheet.archerName;
        scoreing->gameRecord.archer = sheet.archerName;
    }
    if (sheet.billing_lim >0)
    {
        _signSheet.billing_lim += sheet.billing_lim;
        scoreing->gameRecord.billing_lim += sheet.billing_lim;
    }

    if (sheet.pay_amount >0)
    {
        _signSheet.pay_amount += sheet.pay_amount;
        scoreing->savePayment(scoreing->gameRecord.payment_eid, sheet.pay_amount,"");
    }
    signIn2Faces();
}

/**
 * @brief ArcherView::eventFilter
 * @param obj
 * @param event
 * @return
 */
bool ArcherView::eventFilter(QObject *obj, QEvent *event)   //event SLOT
{
    if (event->type()==QEvent::MouseButtonPress)           //判断类型
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->buttons() & Qt::LeftButton)
        {
            if(obj==ui->sldDistance)
            {
                int dur = ui->sldDistance->maximum() - ui->sldDistance->minimum();
                int pos = ui->sldDistance->minimum() + round(dur * ((double)mouseEvent->x() / ui->sldDistance->width()));
                if(pos != ui->sldDistance->sliderPosition())
                {
                    ui->sldDistance->setValue(pos);
                    ui->labDistance->setText(QString("%1m").arg(ui->sldDistance->value()));
                }
            }
            else if (obj==ui->edArcherName)
            {
                do_modifyArcher();
            }
            else if (obj==ui->widCamera)
            {
                //模拟计分，调试用
                int points = (mouseEvent->x() + mouseEvent->y()) % 11;
//           ui->labPrompt->setText(QString("%1-%2").arg(mouseEvent->x()).arg(mouseEvent->y()));
                doShotingView(DEF_SCOREMARK_None, points);
            }
        }
    }


    return QObject::eventFilter(obj,event);
}


/**
 * @brief 单箭射出，得到分值后的记分处理 ArcherView::doShotingView
 * @param shotMode
 * @param points
 */
void ArcherView::doShotingView(int shotMode, int points)
{
    if(scoreState != ST_Shooting)
        return;
    //数据库; 第一列，listView显示
    int shoots_lim = ui->cmArrowsforgame->itemData(ui->cmArrowsforgame->currentIndex()).toInt();
    if (scoreing->newShooting(points,shotMode,shoots_lim) >0)
    {
//        displayGameProgress(0);
    }
    //得分明细，第2列显示
    ui->edShoots->setText(QString("%1").arg(scoreing->gameRecord.shoots));
    ui->edPoints->setText(QString("%1").arg(scoreing->gameRecord.points));
    float avg1 = (float)scoreing->gameRecord.points/scoreing->gameRecord.shoots;
    ui->edAvgPoints->setText(QString("%1").arg(avg1, 2,'f',1));

    //时钟和状态灯，第3列
    if (shotLimitTime !=0)
    {
        shotIntfTime = shotLimitTime;
        ui->lcdnumCurrentTime->display(QString("%1").arg(shotIntfTime));
    }

    displayGameProgress(0);
    if (scoreing->checkoutGame())
    {
        setScoreState(ST_GameOver);
    }
    else
    {
        setScoreState(ST_Prepare);
    }
}

/**
 * @brief ArcherView::setScoreState 进入不同计分状态，显示不同的Aux图像
 * @param newstate
 */
void ArcherView::setScoreState(tagScoreState newstate)
{
    scoreStateTimer = 0;
    scoreState = newstate;

    //指示灯Icon显示
    if (scoreState == ST_Shooting)
    {
        ui->labStateLight->setPixmap(QPixmap(":/light/res/light_green.ico"));
    }
    else if (scoreState == ST_Prepare)
    {
        ui->labStateLight->setPixmap(QPixmap(":/light/res/light_yellow.ico"));
    }
    else
    {
        ui->labStateLight->setPixmap(QPixmap(":/light/res/light_red.ico"));
    }

    //图片板显示
    QString imgfile = getBulletinImgfile();
    QFileInfo fileinfo(imgfile);
    QString file_suffix = fileinfo.suffix();
    //# QFileInfo提取文件路径有点特别，不管是.path()还是.canonicalPath()，没有指定目录的文件，返回是"."，不如提取纯文件名
    //  部分来比较是否带路径。
    if (fileinfo.fileName() == imgfile)
    {
        imgfile = "image/" + imgfile;
    }

    if (file_suffix.toLatin1().toLower() == "gif")
    {
        gif_movie->setFileName(imgfile);
        if (gif_movie->isValid())
        {
            ui->labIcon->setMovie(gif_movie);       //要在setFileName(*.gif)后
            gif_movie->start();
        }
        else
        qDebug()<<"gif image invalid:"<<imgfile;
    }
    else
    {
        if (ui->labIcon->movie() != NULL)
        {
            if (gif_movie->state() == QMovie::Running)      //不必
            {
               gif_movie->stop();
               ui->labIcon->setMovie(NULL);
            }
        }
        QPixmap bmp = QPixmap(imgfile);
        if (!bmp.isNull())
            ui->labIcon->setPixmap(QPixmap(imgfile));
        else
            qDebug()<<"image invalid:"<<imgfile;
    }
}


/**
 * @brief ArcherView::displayGameProgress 游戏进度显示, 进度条和时钟两种形式
 * @param timerCall
 */
void ArcherView::displayGameProgress(int timerCall)
{
    Q_UNUSED(timerCall);

    int times = scoreing->gameRecord.begtime.secsTo(QDateTime::currentDateTime());
    if (scoreing->gameRecord.billing_method == BILLING_BYSHOTS)
    {
/*        if (timerCall !=0)
        {
            return;
        } */
        ui->progGameStep->setValue(scoreing->gameRecord.shoots);
    }
    else
    {
        int v1 = times;
        if (v1 > ui->progGameStep->maximum())
            v1 = ui->progGameStep->maximum();
        ui->progGameStep->setValue(v1);
    }
    QTime _time;
    _time.setHMS(0,0,0);
    ui->lcdRunedTimes->display(_time.addSecs(times).toString("hh:mm"));
}


/**
 * @brief ArcherView::on_cmScoreViewMode_currentIndexChanged 切换"当前得分"(Icon+文字)/和"历史记录"(QtChart图)两种显示界面
 * @param index
 */
void ArcherView::on_cmScoreViewMode_currentIndexChanged(int index)
{
    ui->stackScores->setCurrentIndex(index);
    if (index==1)
    {
        drawBarchart(ui->buttonGroup_2->checkedId());
    }
}


/**
 * @brief ArcherView::drawBarchart
 * @param selectMode
 */
void ArcherView::drawBarchart(int selectMode)
{
    int statCounts[12] = { 0 };
    int dateDelta;
    if (selectMode ==0)
        dateDelta = -1;
    else if (selectMode ==1)
        dateDelta = 7;
    else if (selectMode ==2)
        dateDelta = 30;
    else
        dateDelta = -1024;

    scoreing->countForPoints(statCounts, "", dateDelta);

    //QBarSet为条形图中的一个数值组，QBarSet的项数对应X轴长。可以同时显示N个数值组，最好等长。
    QBarSet *set0 = new QBarSet("archer");
    int i;
    for(i=0; i<11; i++)
    {
        set0->append(statCounts[i]);
    }

    QBarSeries *series = new QBarSeries();  //值域
    series->append(set0);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setAnimationOptions(QChart::SeriesAnimations);

    //坐标轴
    QStringList points;
    for(i=0; i<11; i++)
    {
        points.append(QString::number(i));
    }
    QBarCategoryAxis *axis = new QBarCategoryAxis();
    axis->append(points);
    chart->createDefaultAxes();
    chart->setAxisX(axis, series);

    //图例，即标示各QBarSet的"title-颜色"String
    chart->legend()->setVisible(true);
//    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setAlignment(Qt::AlignLeft);

    ui->barChart->setChart(chart);

}

/**
 * @brief ArcherView::on_rdbChartMode4_toggled
 * @param checked
 */
void ArcherView::on_rdbChartMode4_toggled(bool checked)
{
    if (checked)        //这个约束必须要，否则多次重复
    {
        drawBarchart(ui->buttonGroup_2->checkedId());
    }
}

