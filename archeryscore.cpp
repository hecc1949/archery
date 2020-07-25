#include "archeryscore.h"
//#include <QSqlDatabase>
//#include <QtSql>
#include <QDebug>

ArcheryScore::ArcheryScore()
{
    gameRecord.id = -1;
    roundRecord.id = -1;
    shootRecord.id = -1;
    arrows_per_phase = 3;
    shootingModel = new QStandardItemModel();

    opendb();
//    newGame();
}

ArcheryScore::~ArcheryScore()
{
    _db.close();
    QSqlDatabase::removeDatabase("QSQLITE");
}

int ArcheryScore::opendb()
{
    if (QSqlDatabase::contains("qt_sql_default_connection"))
        _db = QSqlDatabase::database("qt_sql_default_connection");
    else
        _db = QSqlDatabase::addDatabase("QSQLITE");
    _db.setDatabaseName("archeryscore.db");
    if (!_db.open())
    {
        qDebug()<<"database open fault.";
        return(-1);
    }
    if ( !(_db.tables().contains("games") &&  _db.tables().contains("rounds") && _db.tables().contains("shootings") && _db.tables().contains("payment")))
    {
        QSqlQuery query(_db);
        bool res = query.exec("create table games (id int primary key, gametype int, billing_method int, billing_lim int , payment_eid int,"
                              " archer varchar(40), rounds int, shoots int, points int, begtime datetime, endtime datetime)");
        res &= query.exec("create table rounds (id int primary key, game_id, range int, shoots int, points int, "
                          "begtime datetime, endtime datetime, foreign key(game_id) references games)");
        res &= query.exec("create table if not exists shootings (id int primary key, round_id int, arrow_id int, \
                    phase int, points int, shotTime datetime, shotTime_sec int, scoreMark int, foreign key(round_id) references rounds)");
        res &= query.exec("create table if not exists payment (id int primary key, recordtype int, paymode int, \
                    amount float, remain float, vip_cardno varchar(40), paytime datetime)");
        if (!res)
        {
            qDebug()<<"Create tables fault"<<query.lastError();
            return(-1);
        }
        qDebug()<<"tables create ok";
        return(1);
    }
//    qDebug()<<"database open ok";
    return(0);
}

/**
 * @brief ArcheryScore::newGame 开始游戏。
 *      建立了gameRecor，roundRecord但并未存入数据库
 * @param billMethod - 计费方式， 按时间计费(0), 按箭数计费(1)
 * @param billLim
 * @param pay_eid
 * @param gametype
 */
void ArcheryScore::newGame(BillingMethod billMethod,int billLim, int pay_eid,QString archerName, int gametype)
//void ArcheryScore::newGame(int billMethod,int billLim, int pay_eid,int gametype)
{
//    gameRecord.archer = "";
    gameRecord.archer = archerName;
    gameRecord.begtime = QDateTime::currentDateTime();
    gameRecord.endtime = gameRecord.begtime;
    gameRecord.points = 0;
    gameRecord.rounds = 0;
    gameRecord.shoots = 0;

    gameRecord.billing_method = billMethod;
    gameRecord.billing_lim = billLim;
    gameRecord.gametype = gametype;
    gameRecord.payment_eid = pay_eid;
    if (gameRecord.id <0)
    {
        QSqlQuery query(_db);

        query.exec("Select MAX(id) AS Max_id from games");
        int ser_no = 0;
        if (query.next())
        {
            ser_no = query.value(0).toInt();
        }

        query.exec(("Select MAX(game_id) AS Max_id from rounds"));
        int ser_no2 = 0;
        if (query.next())
        {
            ser_no2 = query.value(0).toInt();
        }
        if (ser_no2 > ser_no)
            ser_no = ser_no2;

        gameRecord.id = ser_no +1;
    }
    else
    {
        gameRecord.id++;
    }
    //
    newRound();
}

/**
 * @brief ArcheryScore::saveGame   当前gameRecord存入数据库
 * @return
 */
bool ArcheryScore::saveGame()
{
    if (gameRecord.shoots <=0 && gameRecord.rounds<=0)
    {
        return(false);
    }
    gameRecord.endtime = QDateTime::currentDateTime();

    QSqlQuery query(_db);
    bool saved = false;
    query.prepare("Select * from games Where id = :id");
    query.bindValue(":id",gameRecord.id);
    if (query.exec())
    {
        if (query.next())
            saved = true;
    }
    //
    if (!saved)
    {
        query.prepare("INSERT INTO games(id, gametype, billing_method, billing_lim, payment_eid, archer, rounds, shoots, points, begtime, endtime) "
                      " VALUES(:id, :gametype, :bill_method, :bill_lim, :pay_id, :archer, :rounds, :shoots, :points, :begtime, :endtime)");
    }
    else
    {
        query.prepare("Update games SET gametype=:gametype, billing_method=:bill_method, billing_lim=:bill_lim, payment_eid=:pay_id, "
                " archer=:archer, rounds=:rounds, shoots=:shoots, points=:points, begtime=:begtime, endtime=:endtime "
                "Where (id = :id)");
    }
    query.bindValue(":id",gameRecord.id);
    query.bindValue(":gametype",gameRecord.gametype);
    query.bindValue(":bill_method",gameRecord.billing_method);
    query.bindValue(":bill_lim",gameRecord.billing_lim);
    query.bindValue(":pay_id",gameRecord.payment_eid);
    query.bindValue(":archer",gameRecord.archer);

    query.bindValue(":rounds",gameRecord.rounds);
    query.bindValue(":shoots",gameRecord.shoots);
    query.bindValue(":points",gameRecord.points);
    query.bindValue(":begtime",gameRecord.begtime);
    query.bindValue(":endtime",gameRecord.endtime);

    if (!query.exec())
    {
        qDebug()<<"Save Games fault:"<<query.lastError();
        return(false);
    }
    return(true);
}

void ArcheryScore::closeGame()
{
    if (saveRound())
    {
        saveGame();
        gameRecord.shoots = 0;      //防止重复调用
        roundRecord.shoots = 0;
    }
}

void ArcheryScore::newRound()
{
    QSettings ini("./config.ini",QSettings::IniFormat);

    roundRecord.range = ini.value("/BaseInfo/RangeId").toInt();
    roundRecord.game_id = gameRecord.id;
    roundRecord.points = 0;
    roundRecord.shoots = 0;
    roundRecord.begtime = QDateTime::currentDateTime();
    roundRecord.endtime = roundRecord.begtime;

    if (roundRecord.id <0)
    {
        QSqlQuery query(_db);

        query.exec("Select MAX(id) AS Max_id from rounds");
        int ser_no = 0;
        if (query.next())
        {
            ser_no = query.value(0).toInt();
//            qDebug()<<"round serial is:"<<ser_no;
        }
        roundRecord.id = ser_no +1;
//        qDebug()<<"New round for:"<<roundRecord.id;
        //
        query.prepare("delete from shootings where round_id<0 or round_id>= :current_id");
        query.bindValue(":current_id", roundRecord.id);
        if (!query.exec())
        {
            qDebug()<<"clear error shooting fault:"<<query.lastError();
        }
    }
    else
    {
        roundRecord.id++;
    }
    //
    shootRecord.round_id = roundRecord.id;
    shootRecord.phase = 0;
    shootRecord.arrow_id = 0;
    shootRecord.shotTime = QDateTime::currentDateTime();

    shotList.clear();
    shootingModel->clear();
}

bool ArcheryScore::saveRound()
{
    if (roundRecord.shoots <=0)
    {
        qDebug()<<"Empty round:"<<roundRecord.shoots;
        return(false);
    }
    gameRecord.rounds++;
    roundRecord.endtime = QDateTime::currentDateTime();

//    QSqlQuery query;
    QSqlQuery query(_db);
    query.prepare("INSERT INTO rounds(id, game_id, range, shoots,points,begtime,endtime)"
        "VALUES(?,?,?,?,?,?,?)");
    query.bindValue(0, roundRecord.id);
    query.bindValue(1, roundRecord.game_id);
    query.bindValue(2, roundRecord.range);
    query.bindValue(3, roundRecord.shoots);
    query.bindValue(4, roundRecord.points);
    query.bindValue(5, roundRecord.begtime);
    query.bindValue(6, roundRecord.endtime);

    if (!query.exec())
    {
        qDebug()<<"Save round fault:"<<query.lastError();
        return(false);
    }
//    qDebug()<<"Save round for:"<<roundRecord.id;
    return(true);
}

int ArcheryScore::newShooting(int points, int scoringMode, int shootsPerRound, int prepareTime)
{
    //
    int result = 0;
    if (shootsPerRound >0 && roundRecord.shoots >= shootsPerRound)
    {
        if (saveRound())
        {
            saveGame();
        }
        newRound();
        result = 1;
    }

    //shooting记录
    if (shootRecord.id <0)
    {
        QSqlQuery query0("Select MAX(id) AS Max_id from shootings");
        if (query0.next())
        {
            shootRecord.id = query0.value(0).toInt() + 1;
//            qDebug()<<"Max shooting ID:"<<shootRecord.id;
        }
        else
            shootRecord.id = 1;
    }
    else
    {
        shootRecord.id++;
    }
    shootRecord.phase = shootRecord.arrow_id / arrows_per_phase;
    shootRecord.arrow_id++;

    shootRecord.points = points;
    shootRecord.scoreMark = scoringMode;
    int pre_time;
    if (prepareTime <= 0)
        pre_time = -QDateTime::currentDateTime().secsTo(shootRecord.shotTime);
    else
        pre_time = prepareTime;
    if (pre_time <0)
        pre_time = 0;
    shootRecord.shotTime = QDateTime::currentDateTime();
    shootRecord.shotTime_sec = pre_time;

    //save to shootings table
//    QSqlQuery query;
    QSqlQuery query(_db);
    query.prepare("INSERT INTO shootings(id, round_id, arrow_id, phase, points, shotTime,shotTime_sec,scoreMark) "
        "VALUES(?,?,?,?,?,?,?,?)");
    query.bindValue(0, shootRecord.id);
    query.bindValue(1, shootRecord.round_id);
    query.bindValue(2, shootRecord.arrow_id);
    query.bindValue(3, shootRecord.phase);
    query.bindValue(4, shootRecord.points);

    query.bindValue(5, shootRecord.shotTime);
    query.bindValue(6, shootRecord.shotTime_sec);
    query.bindValue(7, shootRecord.scoreMark);

    if (!query.exec())
    {
        qDebug()<<"save shootingRecord fault:"<<query.lastError();
        return(-1);
    }
    //
    shotList.append(shootRecord);
    QStandardItem *item = new QStandardItem();
    shootingToModal(item, shootRecord);
    shootingModel->appendRow(item);

//    qDebug()<<"save shootingRecord ok:"<<shootRecord.id<<"round:"<<shootRecord.round_id;
    gameRecord.shoots++;
    gameRecord.points += points;

    //up link to rounds
    roundRecord.shoots++;
    roundRecord.points += points;
    roundRecord.endtime = QDateTime::currentDateTime();
    return(result);
}

bool ArcheryScore::modifyShootingPoints(int arrow_id, int points)
{
//    qDebug()<<"Modify points for:"<<arrow_id<<"To:"<<points;
    int delta = points - shotList[arrow_id].points;

    shotList[arrow_id].points = points;
    shotList[arrow_id].scoreMark = DEF_SCOREMARK_Modify;
    shootingToModal(shootingModel->item(arrow_id), shotList[arrow_id]);

//    QSqlQuery query;
    QSqlQuery query(_db);
    query.prepare("UPDATE shootings set points=:points, shotTime=:shotTime, scoreMark=:scoreMark "
                  "Where(round_id=:round_id AND arrow_id=:arrow_id)");
    query.bindValue(":points", points);
//    query.bindValue(":shotTime", QTime::currentTime());
    query.bindValue(":shotTime", QDateTime::currentDateTime());
    query.bindValue(":scoreMark",DEF_SCOREMARK_Modify);
    query.bindValue(":round_id",roundRecord.id);
    query.bindValue(":arrow_id",arrow_id);
    if (query.exec())
    {
        roundRecord.points += delta;
        gameRecord.points += delta;
        return(true);
    }
    return (false);
}


void ArcheryScore::shootingToModal(QStandardItem *item, Shooting& shootRec)
{
    QString title;
    title = QString("#%1: %2分\n").arg(shootRec.arrow_id+1).arg(shootRec.points);
    title += shootRecord.shotTime.toString("hh:mm:ss");
    item->setText(title);
    if (shootRec.points <3)
    {
        item->setIcon(QIcon(":/shotview/res/m02.ICO"));
    }
    else if (shootRec.points >7)
    {
        item->setIcon(QIcon(":/shotview/res/Ic23.ico"));
    }
    else
    {
        item->setIcon(QIcon(":/shotview/res/m04.ICO"));
    }
    item->setTextAlignment(Qt::AlignLeft);
}

int ArcheryScore::checkoutGame()
{
    if (gameRecord.billing_method == BILLING_BYTIME)
    {
        int times = gameRecord.begtime.secsTo(QDateTime::currentDateTime());
        int over = times - gameRecord.billing_lim*60;
        if (over >0)
        {
            return(over/60/5 +1);
        }
    }
    else
    {
        if (gameRecord.shoots > gameRecord.billing_lim)
            return(1);
    }
    return(0);
}


void ArcheryScore::countForPoints(int *counts, QString archer, int dateDelta)
{
    QString subsql;

    if (dateDelta <0)
    {
        if (dateDelta>-1024)
            subsql = "Select id from rounds Where game_id =" + QString::number(gameRecord.id);  //当前局
        else
            subsql = "";
    }
    else
    {
        subsql = "Select id from rounds Where game_id IN (Select id from games Where (games.archer = '" + archer +"' AND "
                "julianday('now')-julianday(begtime)<=" + QString::number(dateDelta) + "))";        //精确到时间值的日期比较
//#                "date('now', '-" + QString::number(dateDelta) + " day') <=date(begtime)))";      //不计时间值的日期比较
    }

//    subsql = "1, 2, 3, 4";

//    QString sql;
    int p;
    for(p=0; p<=10; p++)
    {
        QSqlQuery query(_db);
        if (subsql.length() >0)
            query.prepare("Select COUNT(*) From shootings Where(shootings.points=:point AND shootings.round_id IN (" +subsql + "))");
        else
            query.prepare("Select COUNT(*) From shootings Where(shootings.points=:point)");

        query.bindValue(":point", p);
        if (query.exec())
        {
            if (query.next())
            {
                counts[p] = query.value(0).toInt();
//                qDebug()<<"count:"<<counts[p]<<p;
            }
        }
 //       else
 //           qDebug()<<"query fault:"<<query.lastError();

    }
}

//  参数amount >0为客户付款，收入; <0为消费,应收.
int ArcheryScore::savePayment(int linked_id, float amount, QString vip_cardno)
{
    int ser_id;
    float remain_v = 0;

    QSqlQuery query(_db);
    //准备
    if (linked_id >=0)
    {
        query.exec(QString("Select amount,remain,vip_cardno from payment where (id = %1)").arg(linked_id));
        if (query.next())
        {
            if (query.value(2).toString()==vip_cardno)
            {
                remain_v = query.value(1).toFloat() + amount;
                if (remain_v <0)
                    remain_v = 0;
            }
            ser_id = linked_id;
        }
        else
        {
//            qDebug()<<"input pay_eid err:"<<linked_id;
            linked_id = -1;
        }
    }
    if (linked_id <0)
    {
        if (hot_payment_id <0)
        {
            query.exec("Select MAX(id) AS Max_id from payment");
            ser_id = 0;
            if (query.next())
            {
                ser_id = query.value(0).toInt();
            }
        }
        else
        {
            ser_id = hot_payment_id;
        }
        ser_id++;
        hot_payment_id = ser_id;
        remain_v = amount;
    }
    //写入
    if (linked_id <0)
    {
        query.prepare("INSERT INTO payment(id, recordtype, paymode, amount, remain, vip_cardno, paytime) "
                      " VALUES(:id, :recordtype, :paymode, :amount, :remain, :vip_cardno, :paytime)");
        query.bindValue(":recordtype", 0);
    }
    else
    {
        query.prepare("Update payment SET recordtype=:recordtype, paymode=:paymode, amount=amount+:amount, remain=:remain, "
                "vip_cardno=:vip_cardno, paytime=:paytime Where (id = :id)");
        query.bindValue(":recordtype", 1);
    }

    query.bindValue(":paytime", QDateTime::currentDateTime());
    query.bindValue(":paymode", 0);
    query.bindValue(":vip_cardno", vip_cardno);
    query.bindValue(":id", ser_id);
    query.bindValue(":amount", amount);
    query.bindValue(":remain", remain_v);
    if (!query.exec())
    {
        qDebug()<<"save to payment table fault:"<<query.lastError();
        return(-1);
    }
//    qDebug()<<"pay_eid="<<ser_id;

    return(ser_id);
}

/*
bool ArcheryScore::resetDbase(int mode)
{
    Q_UNUSED(mode);

    QSqlQuery query(_db);
    bool res = query.exec("Delete From payment");
    res &= query.exec("Delete From shootings");
    res &= query.exec("Delete From rounds");
    res &= query.exec("Delete From games");
    gameRecord.id = 0;
    roundRecord.id = 0;
    shootRecord.id = 0;
    return(res);
}
*/
