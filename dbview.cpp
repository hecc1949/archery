#include "dbview.h"
#include "ui_dbview.h"
#include "archeryscore.h"
#include <QComboBox>
#include <QMessageBox>

ExRoundsTableModel::ExRoundsTableModel(QWidget *parent):QSqlTableModel(parent)
{
}

QVariant ExRoundsTableModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    QVariant vt = QSqlTableModel::data(index, role);
//    if (vt.type() == QVariant::DateTime)
    if (!vt.Invalid && (index.column()==3 || index.column()==4))     //这里的colum index是removeColum()后重新编的自然顺序，而不是数据库原有column index
    {
//        return(vt.toDateTime().toString("hh:mm:ss"));
        return(vt.toDateTime().toString("yyyy-M-d, hh:mm:ss"));
    }
    return(vt);
}

ExShootingsTableModel::ExShootingsTableModel(QWidget *parent):QSqlTableModel(parent)
{
}

QVariant ExShootingsTableModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    QVariant vt = QSqlTableModel::data(index, role);
    if (!vt.Invalid && role==Qt::DisplayRole && index.column()==4)
    {
        QString s1 ="";
        switch(vt.toInt())
        {
        case DEF_SCOREMARK_None:
            s1 = "未定义";
            break;
        case DEF_SCOREMARK_Manu:
            s1 = "人工";
            break;
        case DEF_SCOREMARK_Auto:
            s1 = "自动计分";
        case DEF_SCOREMARK_Modify:
            s1 = "改分";
            break;
        default:
            break;
        }
        return(QVariant(s1));
    }
    return(vt);
}

ExGamesTableModel::ExGamesTableModel(QWidget *parent):QSqlTableModel(parent)
{
}

QVariant ExGamesTableModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }
    QVariant vt = QSqlTableModel::data(index, role);
    if (vt.Invalid)
        return QVariant();
    if (index.column()==3)        //
    {
        if (role == Qt::CheckStateRole)
        {
            if (vt.toInt() <=0)
                return(Qt::Unchecked);
            else
                return(Qt::Checked);
        }
        else if (role==Qt::DisplayRole)
        {
            if (vt.toInt() <=0)
                return QVariant(QString(""));
            else
                return QVariant(vt.toString());
        }
    }
    else if (index.column() ==1)
    {
        if (role==Qt::DisplayRole)
        {
            QString s1 ="";
            switch(vt.toInt())
            {
            case 0:
                s1 = "按时间计费";
                break;
            case 1:
/*                s1 = "按回合数计费";
                break;
            case 2: */
                s1 = "按箭数计费";
                break;
            default:
                break;
            }
            return(QVariant(s1));
        }
    }
    return(vt);
}

ExPaymentTableModel::ExPaymentTableModel(QWidget *parent):QSqlTableModel(parent)
{
}

QVariant ExPaymentTableModel::data(const QModelIndex & index, int role) const
{
    if (role==Qt::DisplayRole)
    {
        QVariant vt = QSqlTableModel::data(index, role);
        if (vt.Invalid)
            return QVariant();
        if (index.column()==6)
        {
            return(vt.toDateTime().toString("yyyy-M-d hh:mm"));
        }
        return(vt);
    }

    return(QSqlTableModel::data(index, role));
}


#if 0
GamesDelegate::GamesDelegate(QObject *parent) :    QItemDelegate(parent)
{
}

void GamesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    drawDisplay(painter, option, option.rect,index.model()->data(index, Qt::DisplayRole).toString());   //??
    drawFocus(painter, option, option.rect);
}

QWidget *GamesDelegate::createEditor(QWidget *parent,const QStyleOptionViewItem &/*option*/,const QModelIndex &/*index*/) const
{
    QComboBox *editor = new QComboBox(parent);
    editor->addItem(tr("按时间计费"));
    editor->addItem(tr("按箭数计费"));
    editor->addItem(tr("按回合数计费"));
//    editor->installEventFilter(const_cast<GamesDelegate*>(this));
//    qDebug()<<"data:";
    return editor;
}

void GamesDelegate::setEditorData(QWidget *editor,const QModelIndex &index) const
{
    int id = index.model()->data(index).toInt();
    qDebug()<<"data:"<<id;
    QComboBox *box = static_cast<QComboBox*>(editor);       //!
    box->setCurrentIndex(id);
}

void GamesDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    QComboBox *box = static_cast<QComboBox*>(editor);
    int id = box->currentIndex();
    model->setData(index, id);
}

void GamesDelegate::updateEditorGeometry(QWidget *editor,const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    editor->setGeometry(option.rect);
}
#endif



//-----------------------------------------------------------------------------------------------

DbView::DbView(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DbView)
{
    ui->setupUi(this);
    createScoreView();

    ui->btnExport->setEnabled(false);
//    ui->btnDbReset->setEnabled(false);
}

DbView::~DbView()
{
    delete ui;
}

void DbView::closeEvent(QCloseEvent *)
{
    db.close();
    QSqlDatabase::removeDatabase("QSQLITE");
}

void DbView::on_btnClose_clicked()
{
    this->close();
}

void DbView::createScoreView()
{
    if (QSqlDatabase::contains("qt_sql_default_connection"))
        db = QSqlDatabase::database("qt_sql_default_connection");
    else
        db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("archeryscore.db");
    if (!db.open())
    {
        qDebug()<<"database open fault.";
        return;
    }

    gamesModal = new ExGamesTableModel(this);
    gamesModal->setTable("games");
    gamesModal->select();
    ui->tvGames->setModel(gamesModal);
#if 0
    ui->tvGames->setItemDelegateForColumn(2, new GamesDelegate(ui->tvGames));
#endif

    ui->tvGames->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tvGames->setAlternatingRowColors(true);
    ui->tvGames->verticalHeader()->setVisible(false);
    ui->tvGames->setSelectionBehavior(QAbstractItemView::SelectRows); //整行选中的方式
    ui->tvGames->setSelectionMode(QAbstractItemView::SingleSelection);    //只能选择一行，不能选择多行
    gamesModal->removeColumn(10);       //endtime
    gamesModal->removeColumn(9);        //begtime
    gamesModal->removeColumn(8);        //points 总分
//    gamesModal->removeColumn(7);        //shots
    gamesModal->removeColumn(6);        //rounds
//    gamesModal->removeColumn(4);        //payment_eid
//    gamesModal->removeColumn(3);        //billing_lim
    gamesModal->removeColumn(1);        //gametype
    gamesModal->setHeaderData(0, Qt::Horizontal, tr("编号"));
    gamesModal->setHeaderData(1, Qt::Horizontal, tr("计费方式"));
    gamesModal->setHeaderData(2, Qt::Horizontal, tr("时间/局数"));
    gamesModal->setHeaderData(3, Qt::Horizontal, tr("已结帐"));
    gamesModal->setHeaderData(4, Qt::Horizontal, tr("射手"));
    gamesModal->setHeaderData(5, Qt::Horizontal, tr("总箭数"));

    ui->tvGames->resizeColumnsToContents();    //列宽度与内容匹配

    roundsModal = new ExRoundsTableModel(this);
    roundsModal->setTable("rounds");
    //#主从表的.setRelation:
    //  .setRelation(column_id_in_slaveTab, QSqlRelation("masterTab_name","masterTab_key_column_name","masterTab_val_column_name"))
    // column_id_in_slaveTab指定要建立关联的slave字段index, base-0. 在games-rounds,是rounds表的game_id字段，id=1
    // masterTab_key_column_name指定(master表中)要和slave表中建立关联的字段名， (column_id_in_slaveTab作为masterTab_key_column_name的外键),
    //在games-rounds表, 就是games的id字段。
    // 在tableview将显示masterTab_val_column_name指定字段的内容来替代，而不是master表或slave中互为Key关联字段的内容(games.id / rounds.game_id).
    //在games-rounds表，没有好的替代。
    roundsModal->select();
    ui->tvRounds->setModel(roundsModal);
    ui->tvRounds->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tvRounds->setAlternatingRowColors(true);
    ui->tvRounds->verticalHeader()->setVisible(false);
    ui->tvRounds->setSelectionBehavior(QAbstractItemView::SelectRows); //整行选中的方式
    ui->tvRounds->setSelectionMode(QAbstractItemView::SingleSelection);    //只能选择一行，不能选择多行
    //隐藏不用显示的列。注意：a. 从大到小，removeColum()时会重新编号; b. 作为从表索引的id列不能隐藏，否则从表不显示。
    roundsModal->removeColumn(2);
    roundsModal->removeColumn(1);
    //设置各列标题(表头)。注意，这里的colum index是removeColum()后重新编的自然顺序，而不是数据库table原有colum index.
    roundsModal->setHeaderData(0, Qt::Horizontal, tr("编号"));
    roundsModal->setHeaderData(1, Qt::Horizontal, tr("箭数"));
    roundsModal->setHeaderData(2, Qt::Horizontal, tr("总分"));
    roundsModal->setHeaderData(3, Qt::Horizontal, tr("开始时间"));
    roundsModal->setHeaderData(4, Qt::Horizontal, tr("完成时间"));
    ui->tvRounds->setColumnHidden(0, true);
    ui->tvRounds->setColumnWidth(1, 40);
    ui->tvRounds->setColumnWidth(2, 50);
    ui->tvRounds->setColumnWidth(3, 150);
    ui->tvRounds->setColumnWidth(4, 150);
//    ui->tvRounds->resizeColumnsToContents();    //列宽度与内容匹配

    shootingsModal = new ExShootingsTableModel(this);
    shootingsModal->setTable("shootings");
    shootingsModal->select();
    ui->tvShootings->setModel(shootingsModal);
    ui->tvShootings->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tvShootings->setAlternatingRowColors(true);
    ui->tvShootings->verticalHeader()->setVisible(false);
    shootingsModal->removeColumn(5);        //shotTime
    shootingsModal->removeColumn(3);          //phase
    shootingsModal->removeColumn(0);          //id
    shootingsModal->setHeaderData(1, Qt::Horizontal, tr("序号"));     //raw-2, arrow-id
    shootingsModal->setHeaderData(2, Qt::Horizontal, tr("得分"));
    shootingsModal->setHeaderData(3, Qt::Horizontal, tr("用时(s)"));
    shootingsModal->setHeaderData(4, Qt::Horizontal, tr("标记"));     //scoreMark

    ui->tvShootings->setColumnHidden(0, true);      //rwa-1, round_id
    ui->tvShootings->resizeColumnsToContents();    //列宽度与内容匹配

    connect(ui->tvGames->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(tvGames_currentRowChanged(const QModelIndex&, const QModelIndex&)));
    connect(ui->tvRounds->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
            this, SLOT(tvRounds_currentRowChanged(const QModelIndex&, const QModelIndex&)));

    ui->tvGames->selectRow(gamesModal->rowCount() -1);

    //--- 第2页
    paymentModal = new ExPaymentTableModel(this);
    paymentModal->setTable("payment");
    paymentModal->select();
    ui->tvPayment->setModel(paymentModal);
    ui->tvPayment->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tvPayment->setAlternatingRowColors(true);
    ui->tvPayment->verticalHeader()->setVisible(false);
    //这种模式设置column显示无需修改column_id, 用自然序号即可
    ui->tvPayment->setColumnHidden(2, true);        //paymode
    ui->tvPayment->setColumnHidden(4, true);        //remain
    paymentModal->setHeaderData(0, Qt::Horizontal, "编号");
    paymentModal->setHeaderData(1, Qt::Horizontal, "记录类型");
    paymentModal->setHeaderData(3, Qt::Horizontal, "数额");
    paymentModal->setHeaderData(5, Qt::Horizontal, "VIP卡号");
    paymentModal->setHeaderData(6, Qt::Horizontal, "时间");

    ui->tvPayment->resizeColumnsToContents();    //列宽度与内容匹配

}

#if 0
void DbView::on_gamesTableViewClicked(QModelIndex index)
{
    QSqlRecord record = gamesModal->record(index.row());
    QString id_str = record.value("id").toString();
//    roundsModal->setFilter("game_id= '" + id_str +"' ");
    roundsModal->setFilter("game_id=" + id_str);
//    qDebug()<<"games change:"<<id_str;
}
#endif


void DbView::tvGames_currentRowChanged(const QModelIndex& current,  const QModelIndex&)
{
    QSqlRecord record = gamesModal->record(current.row());
    QString id_str = record.value("id").toString();
    roundsModal->setFilter("game_id=" + id_str);
    ui->tvRounds->selectRow(0);

}


void DbView::tvRounds_currentRowChanged(const QModelIndex& current, const QModelIndex&)
{
    QSqlRecord record = roundsModal->record(current.row());
    int round_id = record.value("id").toInt();
    shootingsModal->setFilter(QString("round_id=%1").arg(round_id));

}

void DbView::on_btnDbReset_clicked()
{
 //   if (QMessageBox::question(this, "清空数据库"，”确定要清除所有数据记录？", ))
    if (QMessageBox::warning(this, "清空数据库", "确定要清除所有数据记录？", QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) ==QMessageBox::Ok)
    {
        QSqlQuery query(db);
        bool res = query.exec("Delete From payment");
        res &= query.exec("Delete From shootings");
        res &= query.exec("Delete From rounds");
        res &= query.exec("Delete From games");
        if (res)
        {
            QMessageBox::information(this, tr("清空数据库"), tr("所有数据记录已清除。"));
            ui->btnDbReset->setEnabled(false);
            ui->btnClose->setFocus();
        }
        else
        {
            QMessageBox::warning(this, tr("错误"), tr("清除数据时出错！"), QMessageBox::Discard);
        }
    }
}
