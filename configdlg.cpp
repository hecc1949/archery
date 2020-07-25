#include "configdlg.h"
#include "ui_configdlg.h"
//#include <QMessageBox>
#include <QDebug>

#include <QSettings>        //读写ini配置文件

//#include <QStandardItemModel>   //QListView
//#include <QModelIndex>


ConfigDlg::ConfigDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigDlg)
{
    ui->setupUi(this);
    connect(ui->listPageSel,SIGNAL(currentRowChanged(int)), ui->stackContent,SLOT(setCurrentIndex(int)));
//    ui->btnClose->setShortcut(Qt::Key_Escape);

    LoadBaseInfo();

    onvif_dev = DevInterface::GetInstance();

    //动态摄像机信息列表(QTableWidget)表头和属性设置
    ui->tabStrobeIPC->setColumnCount(5);
//    ui->tabStrobeIPC->horizontalHeader()->setClickable(false); //设置表头不可点击（默认点击后进行排序）
//    ui->tabStrobeIPC->horizontalHeader()->setSectionsClickable(false); //设置表头不可点击（默认点击后进行排序）
    QStringList header;
    header<<tr("名称")<<tr("地址")<<tr("序列号")<<tr("类型")<<tr("分辨率");
    ui->tabStrobeIPC->setHorizontalHeaderLabels(header);

    ui->tabStrobeIPC->setColumnWidth(0, 60);
    ui->tabStrobeIPC->setColumnWidth(1, 400);
    ui->tabStrobeIPC->setColumnWidth(2, 150);
    ui->tabStrobeIPC->setColumnWidth(3, 150);
    ui->tabStrobeIPC->setColumnWidth(4, 200);

    QFont font = ui->tabStrobeIPC->horizontalHeader()->font();
    font.setPointSize(12);
    ui->tabStrobeIPC->horizontalHeader()->setFont(font);

    ui->tabStrobeIPC->setEditTriggers(QAbstractItemView::NoEditTriggers);   //禁止编辑
    ui->tabStrobeIPC->setSelectionBehavior(QAbstractItemView::SelectRows); //整行选中的方式
    ui->tabStrobeIPC->setSelectionMode(QAbstractItemView::SingleSelection);    //只能选择一行，不能选择多行

    //ui->tabStrobeIPC->resizeColumnsToContents();    //列宽度与内容匹配
    ui->tabStrobeIPC->resizeRowsToContents();   //行高度与内容匹配

//    connect(ui->tabStrobeIPC,SIGNAL(cellDoubleClicked(int,int)),this,SLOT(on_tabStrobeIPC_cellDoubleClicked(int, int)));
    connect(ui->tabStrobeIPC,SIGNAL(currentItemChanged(QTableWidgetItem *,QTableWidgetItem *)),this,
            SLOT(on_tabStrobeIPC_currentItemChanged(QTableWidgetItem *, QTableWidgetItem *)));

    //others
    ui->labProcHint->setVisible(false);
    ui->btnLinkCamera->setEnabled(false);

    if (onvif_dev->cameraLinkStatus !=0)
    {
        drawIPCameraInfos(&onvif_dev->DroneCameras);
    }

    //about, scheme页
    ui->labVersions->setText(strVersion);

}

ConfigDlg::~ConfigDlg()
{
    delete ui;
}

void ConfigDlg::on_btnClose_clicked()
{
    this->close();
}

/**
 * @brief x显示摄像头搜索得到的摄像头信息列表
 * @param ipcInfo
 */
//void ConfigDlg::drawOnvifStrobResult(IPCameraInfos *ipcInfo)
void ConfigDlg::drawIPCameraInfos(IPCameraInfos *ipcInfo)
{
    QString s1;
    //append新行
    int rowId = ui->tabStrobeIPC->rowCount();
    ui->tabStrobeIPC->setRowCount(rowId+1);

    int linked_id = onvif_dev->isLinkedCamera(ipcInfo);
    if (linked_id !=0)
        s1 = "*";
    else
        s1 = ipcInfo->name;
    QTableWidgetItem *item1 = new QTableWidgetItem(s1);
    ui->tabStrobeIPC->setItem(rowId, 0, item1);

    s1 = ipcInfo->stream_url[0];
    s1.remove(0, s1.indexOf("@")+1);
    s1 = s1.left(s1.indexOf(':'));

    QTableWidgetItem *item2 = new QTableWidgetItem(s1);
    ui->tabStrobeIPC->setItem(rowId, 1, item2);

    QTableWidgetItem *item3 = new QTableWidgetItem(ipcInfo->serial_no);
    ui->tabStrobeIPC->setItem(rowId, 2, item3);

    QTableWidgetItem *item4 = new QTableWidgetItem(ipcInfo->model);
    ui->tabStrobeIPC->setItem(rowId, 3, item4);

    s1 = QString("%1x%2").arg(ipcInfo->pixelFormat[0].width()).arg(ipcInfo->pixelFormat[0].height());
    QTableWidgetItem *item5 = new QTableWidgetItem(s1);
    ui->tabStrobeIPC->setItem(rowId, 4, item5);

    ui->tabStrobeIPC->resizeColumnsToContents();    //列宽度与内容匹配

    ui->chkUseStream2->setChecked((ipcInfo->usingSteamId !=0));
    ui->chkTcptransport->setChecked((ipcInfo->tcp_transport !=0));
}


void ConfigDlg::onvif_strobeEnd(int exitCode)   //slot
{
    Q_UNUSED(exitCode);

    ui->tabStrobeIPC->setRowCount(0);
    for(int i=0; i<onvif_dev->strobCameras.count(); i++)
    {
//        drawOnvifStrobResult(&(onvif_dev->strobCameras[i]));
        drawIPCameraInfos(&(onvif_dev->strobCameras[i]));
    }
    ui->tabStrobeIPC->selectRow(onvif_dev->strobCameras.count()-1);

    ui->btnFindCamera->setEnabled(true);
    ui->labProcHint->setVisible(false);
    if (onvif_dev->strobCameras.count()>0)
    {
        ui->btnLinkCamera->setEnabled(true);
    }
    disconnect(onvif_dev, SIGNAL(externProcDone(int)), this, SLOT(onvif_strobeEnd(int)));
}

void ConfigDlg::on_btnFindCamera_clicked()
{
    connect(onvif_dev, SIGNAL(externProcDone(int)), this, SLOT(onvif_strobeEnd(int)));
    onvif_dev->startOnvifStrobe();
    ui->btnFindCamera->setEnabled(false);

    ui->labProcHint->setText(tr("正在查找摄像头，请稍候..."));
    ui->labProcHint->setVisible(true);
}

/**
 * @brief ConfigDlg::on_btnLinkCamera_clicked
 *  从列表中选择摄像头，建立连接关系
 */
void ConfigDlg::on_btnLinkCamera_clicked()
{
    if (ui->tabStrobeIPC->rowCount()<1)
        return;
    int id = ui->tabStrobeIPC->currentRow();
    if (id<0 || id>=onvif_dev->strobCameras.count())
        return;
    ui->btnLinkCamera->setEnabled(false);
    //获得并显示snapshot图像
    onvif_dev->ipcSnapshot(&(onvif_dev->strobCameras[id]));
    char filename[128] = "/tmp/snapshot0.jpg";
    QImage img;
    if (img.load(filename))
    {
        QPixmap pixmap = QPixmap::fromImage(img.scaled(ui->labCameraImg->size(), Qt::IgnoreAspectRatio));
        ui->labCameraImg->setPixmap(pixmap);
    }
    //
    if ( ui->chkUseStream2->isChecked())
        onvif_dev->strobCameras[id].usingSteamId =1;
    else
        onvif_dev->strobCameras[id].usingSteamId =0;
    if (ui->chkTcptransport->isChecked())
        onvif_dev->strobCameras[id].tcp_transport = 1;
    else
        onvif_dev->strobCameras[id].tcp_transport = 0;

    onvif_dev->LinkCameraToDrone(id);
    ui->tabStrobeIPC->setRowCount(0);
    for(int i=0; i<onvif_dev->strobCameras.count(); i++)
    {
//        drawOnvifStrobResult(&(onvif_dev->strobCameras[i]));
        drawIPCameraInfos(&(onvif_dev->strobCameras[i]));
    }
    ui->tabStrobeIPC->selectRow(id);

    ui->btnLinkCamera->setEnabled(true);
}

void ConfigDlg::SaveBaseInfo()
{
    QSettings ini("./config.ini",QSettings::IniFormat);

    ini.setValue(tr("/BaseInfo/Title"), ui->edTitle->text());
    ini.setValue(tr("/BaseInfo/RangeId"), ui->spRangeId->value());

}

void ConfigDlg::LoadBaseInfo()
{
    QSettings ini("./config.ini",QSettings::IniFormat);

    ui->edTitle->setText(ini.value("/BaseInfo/Title").toString());
    ui->spRangeId->setValue(ini.value("/BaseInfo/RangeId").toInt());
}

void ConfigDlg::on_btnApply_clicked()
{
    if (ui->listPageSel->currentRow() ==0)
    {
        SaveBaseInfo();
    }
}

/**
 * @brief ConfigDlg::on_tabStrobeIPC_currentItemChanged 摄像机Table选择不同行时，同步显示附加参数。slot函数
 * @param current
 * @param previous
 */
void ConfigDlg::on_tabStrobeIPC_currentItemChanged(QTableWidgetItem *current, QTableWidgetItem *previous)
{
    //注意：这里有小Bug，一是传来的current/previous指针可能是NUL，（对应无焦点的情况？)，二是每次点击选择行，会触发进入
    //  此函数两次，且current/previous参数是相同的，没法消除冗余.
    Q_UNUSED(current);
    Q_UNUSED(previous);

    if (onvif_dev->strobCameras.count()<=0)
        return;
    int id = ui->tabStrobeIPC->currentRow();
    if (id>=0 && id <onvif_dev->strobCameras.count())
    {
        IPCameraInfos *ipcInfo = &onvif_dev->strobCameras[id];
        ui->chkUseStream2->setChecked((ipcInfo->usingSteamId !=0));
        ui->chkTcptransport->setChecked((ipcInfo->tcp_transport !=0));
    }
}

