#ifndef ARCHERVIEW_H
#define ARCHERVIEW_H

#include <QDialog>
#include <QWidget>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
/*
#include <QtCharts/QChartView>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QLegend>
#include <QtCharts/QBarCategoryAxis>
*/
#include <QtCharts>

QT_CHARTS_USE_NAMESPACE

#include "ffvideo.h"
#include "devinterface.h"

#include "archeryscore.h"


#define DEF_LANE_LEFT       1
#define DEF_LANE_RIGHT      2
#define DEF_LANE_DOUBLE     3

namespace Ui {
class ArcherView;
}



class ArcherView : public QWidget
//class ArcherView : public QDialog
{
    Q_OBJECT

enum tagCameraRunFsr {
    CAM_Offline,
    CAM_SearchURL,
    CAM_Opening,
    CAM_Playing,
    CAM_Paused,
    CAM_Stopped
};


enum tagScoreState  {
    ST_Prepare,             //after shooting
    ST_Shooting,
    ST_ModifyPoints,
    ST_RoundEnd,
    ST_GameOver
//    ST_Closed
};

/*
#define _SHOTMODE_MANUALCLICK   0
#define _SHOTMODE_TIMER         1
#define _SHOTMODE_MANUCHECK     2
#define _SHOTMODE_VIDEOCAP      3
*/

public:
    explicit ArcherView(QWidget *parent = 0);
    ~ArcherView();

//private slots:
    //

//    void on_btnClose_clicked();
    void setSignIn(SigninSheet& sheet);

protected:
    void showEvent(QShowEvent *);
    void resizeEvent(QResizeEvent* event);
    void paintEvent(QPaintEvent* event);
    void closeEvent(QCloseEvent *event);
private:
    Ui::ArcherView *ui;
    QMovie *movie;

    Ffvideo *ffvideo;
    QThread videoThread;

    int _laneMode;
//    int runtimes;       //second
    int rawVideo_width;
    int rawVideo_height;

    int startCamera();

    DevInterface *devIntf;
    tagCameraRunFsr cameraState;

    int shotLimitTime;
    int shotIntfTime;   //run

    ArcheryScore *scoreing;
    tagScoreState scoreState;
    int scoreStateTimer;
    int gameOverDelay_sec;
    bool _closingCamera;

    QJsonObject imglist_bulletin;
    SigninSheet _signSheet;

private:
    void setupFaces();
    void signIn2Faces();
    void doShotingView(int shotMode, int points);
    void setScoreState(tagScoreState newstate);
//    void lightForScoreState();
    void displayGameProgress(int timerCall);
    void do_modifyArcher();

    void  doClose();

//    void loadConfig();
    void loadGameImages();
    QString getBulletinImgfile();
    QMovie *gif_movie;

    void drawBarchart(int selectMode);

public slots:
    bool eventFilter(QObject *, QEvent *);
private slots:
    void runTimer();
    void playStateChanged(PlayState state);

    void on_btnVideoCtrl_clicked();
    void on_cmShotTime_currentIndexChanged(int index);
    void on_sldDistance_sliderMoved(int position);
    void on_lvShooting_doubleClicked(const QModelIndex &index);
    void on_cmModifyPoint_currentIndexChanged(int index);
    void on_toolbtnClose_clicked();
    void on_cmScoreViewMode_currentIndexChanged(int index);    
    void on_rdbChartMode4_toggled(bool checked);

};

#endif // ARCHERVIEW_H
