#ifndef CONFIGDLG_H
#define CONFIGDLG_H

#include <QDialog>
#include <QProcess>
#include "devinterface.h"

#include <QTableWidgetItem>

namespace Ui {
class ConfigDlg;
}

#define strVersion  "版本v0.1 2019 测试中"

class ConfigDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigDlg(QWidget *parent = 0);
    ~ConfigDlg();

private slots:
    void on_btnClose_clicked();
    void on_btnApply_clicked();
    //
    void onvif_strobeEnd(int exitCode);

    void on_btnFindCamera_clicked();
    void on_btnLinkCamera_clicked();

//    void on_tabStrobeIPC_currentItemChanged(int, int);
    void on_tabStrobeIPC_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous);

//    void on_tabStrobeIPC_itemSelectionChanged();

private:
    Ui::ConfigDlg *ui;
    DevInterface *onvif_dev;

//    void drawOnvifStrobResult(IPCameraInfos *ipcInfo);
    void drawIPCameraInfos(IPCameraInfos *ipcInfo);

    void SaveBaseInfo();
    void LoadBaseInfo();

};

#endif // CONFIGDLG_H
