/**
  *
  *     游戏选手基本信息录入，手动开局时用, 游戏中间也要增加/修改
  *
  *     通过SigninSheet结构体来管理(交换)配置游戏局数据，其实这就是编辑SigninSheet的一个UI界面
  *
  */
#include "signin.h"
#include "ui_signin.h"
#include <QMouseEvent>
#include <QDebug>

SignIn::SignIn(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SignIn)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);
    //
    ui->cmArcherName->addItem("新手");
    ui->cmArcherName->addItem("箭师");
    ui->cmArcherName->setCurrentIndex(-1);

    QSettings ini("./config.ini",QSettings::IniFormat);
    ini.setIniCodec(QTextCodec::codecForName("UTF-8"));     //解决读出的中文字符串乱码
    ui->cmGameType->clear();
    for(int i=1; i<=3; i++)
    {
        QString s1 = ini.value(QString("/scheme/GameType-%1").arg(i)).toString();
        ui->cmGameType->addItem(s1);
    }
    min_shots = ini.value("/scheme/MinGameShots").toInt();
    if (min_shots <3)
        min_shots = 3;
    min_times = ini.value("/scheme/MinGameTimes").toInt();
    if (min_times <1)
        min_times = 1;
    //
    softKb = SoftKeyBoardDlg::GetInstance();
    softKb->move(this->geometry().right(), this->geometry().top());

    ui->cmArcherName->lineEdit()->installEventFilter(this);     //combobox包含一个QLineEdit对象，要针对这个对象!
    ui->edPresetPlayVal->installEventFilter(this);
    ui->edPaidVal->installEventFilter(this);
}

SignIn::~SignIn()
{
    delete ui;
}

void SignIn::on_btnCancel_clicked()
{
    this->reject();
}

bool SignIn::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouseEvent=static_cast<QMouseEvent *>(event);
        if(mouseEvent->buttons() & Qt::LeftButton)
        {
            QString value;
            if (obj ==ui->cmArcherName->lineEdit())
            {
//                softKb->startExec(&value, SKB_Page_EnglishChar);
                softKb->startExec(&value, SKB_Page_ChinesePy);
                if(!value.isEmpty())
                {
                    ui->cmArcherName->setEditText(value);
                    ui->cmArcherName->insertItem(0, value);
                }
            }
            else if (obj==ui->edPresetPlayVal || obj==ui->edPaidVal)
            {
                softKb->startExec(&value, SKB_Page_Digit);
                if(!value.isEmpty())
                {
                    ((QLineEdit *)obj)->setText(value);
                }
            }
        }
    }
    return QObject::eventFilter(obj,event);
}

void SignIn::on_cmPayMode_currentIndexChanged(int index)
{
    if (index==0)
        ui->labPlayUnit->setText("分钟");
    else
        ui->labPlayUnit->setText("箭");
}


void SignIn::on_btnOk_clicked()
{
    if (ui->edPresetPlayVal->text().trimmed().length()>0)
    {
        int lim_val = ui->edPresetPlayVal->text().toInt();
        if (((BillingMethod)(ui->cmPayMode->currentIndex()) == Billing_ByTime && lim_val < min_times)||
            ((BillingMethod)(ui->cmPayMode->currentIndex()) == Billing_ByShots && lim_val < min_shots))
        {
            ui->edPresetPlayVal->setFocus();
            return;
        }
    }
    //
    this->accept();
}

///
/// \brief SignIn::setMode 按开局/修改两种不同模式初始化界面
/// \param pSheet
///
void SignIn::setMode(SigninSheet* pSheet)
{
    if (pSheet == NULL)
    {
        //开局模式
        ui->labTitle->setText(tr("开始"));
        ui->cmGameType->setEnabled(true);
        ui->cmPayMode->setEnabled(true);
    }
    else
    {
        //局中修改模式
        ui->labTitle->setText(tr("设定名字/增加预定值"));
        //游戏类型
        if (pSheet->gameType>=0 && pSheet->gameType < ui->cmGameType->count())
        {
            ui->cmGameType->setCurrentIndex(pSheet->gameType);
        }
        ui->cmGameType->setEnabled(false);
        //计费模式
        if ((int)(pSheet->billing_method) >=0 && (int)(pSheet->billing_method)<ui->cmPayMode->count())
        {
            ui->cmPayMode->setCurrentIndex((int)(pSheet->billing_method));
        }
        ui->cmPayMode->setEnabled(false);
        //选手名字
        if (pSheet->archerName.length()>0)
        {
            ui->cmArcherName->setCurrentText(pSheet->archerName);
            ui->cmArcherName->setEnabled(false);
        }
        ui->edPresetPlayVal->setText("");
        ui->edPaidVal->setText("");
    }
}


///
/// \brief SignIn::getSignInSheet 读出配置内容
/// \param sheet
/// \return
///
bool SignIn::getSignInSheet(SigninSheet& sheet)
{
    sheet.archerName = "";
    sheet.gameType = 0;
    sheet.billing_method = Billing_ByTime;
    sheet.billing_lim = 0;
    sheet.pay_amount = 0;

    sheet.archerName = ui->cmArcherName->currentText();
    sheet.gameType = ui->cmGameType->currentIndex();
    sheet.billing_method = (BillingMethod)(ui->cmPayMode->currentIndex());

    //预定时间/箭数，必须有最小值
    if (ui->edPresetPlayVal->text().trimmed().length()>0)
    {
        sheet.billing_lim = ui->edPresetPlayVal->text().toInt();
    }
    if (sheet.billing_method == BILLING_BYSHOTS)
    {
        if (sheet.billing_lim < min_shots)
            sheet.billing_lim = min_shots;
    }
    else
    {
        if (sheet.billing_lim < min_times)
            sheet.billing_lim = min_times;
    }

    if (ui->edPaidVal->text().trimmed().length()>0)
    {
        sheet.pay_amount = ui->edPaidVal->text().trimmed().toFloat();
    }
    return(true);
}

