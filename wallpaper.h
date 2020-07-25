#ifndef WALLPAPER_H
#define WALLPAPER_H

#include <QMainWindow>
#include <QWidget>
//#include <QFrame>
//#include <QGroupBox>
#include <QLabel>
#include <QToolButton>
//#include <QPushButton>
#include <QAction>
//#include <QToolBar>

#include "devinterface.h"
#include "signin.h"

class Wallpaper : public QMainWindow
{
    Q_OBJECT

public:
    Wallpaper(QWidget *parent = 0);
    ~Wallpaper();
    bool isBackgroup;

private:
    QLabel *labelTitle;

    QToolButton *btnExit;
    QToolButton *btnStart;
    QToolButton *btnReport;
    QToolButton *btnConfig;
    QToolButton *btnResource;
//    QToolButton *btnLaneModeSel;

    QDockWidget *dockPan;
    int idle_times;
//    int laneMode;

    DevInterface *devIntf;
//    int cameraLinkState;
//    int cameraLinkStep;
//    int dialogRunings;

//    void SelectLaneMode(bool up);
    void CheckCameraLinks();
    void clearAndClose();

protected:
//    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);

protected slots:
    void exitApp();
    void startgame();
    void report();
    void config_dlg();
    void resource_cfg();
    void idleTimer();

//    void startBtnRightClick(QPoint);
    void devIntf_exProcEnd(int exitCode);

};

#endif // WALLPAPER_H
