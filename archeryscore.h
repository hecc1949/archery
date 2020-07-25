#ifndef ARCHERYSCORE_H
#define ARCHERYSCORE_H

#include <QObject>
#include <QTime>
#include <QSqlDatabase>
#include <QtSql>
#include <QStandardItemModel>

#define DEF_SCOREMARK_None  0
#define DEF_SCOREMARK_Manu  1
#define DEF_SCOREMARK_Auto  2
#define DEF_SCOREMARK_Modify  0x04

#define BILLING_BYTIME      0
#define BILLING_BYSHOTS     1
//#define BILLING_BYROUND     1

enum BillingMethod
{
    Billing_ByTime = 0,
    Billing_ByShots = 1
};

struct SigninSheet
{
    QString archerName;
    int gameType;
//    int archerType;
//    int billing_method;
    BillingMethod billing_method;
    int billing_lim;
    float pay_amount;
};

struct ArcheryGame
{
    int id;
    int gametype;       //reserve?
//    int billing_method;     //按时间计费(0), 按局(round)计费(1)
//    int billing_method;     //按时间计费(0), 按箭数计费(1) （12或更多箭组成一回合，每回合清一次靶）
    BillingMethod billing_method;     //按时间计费(0), 按箭数计费(1) （12或更多箭组成一回合，每回合清一次靶）
    int billing_lim;
    int payment_eid;
    QString archer;
    int rounds;             //总局数
    int shoots;             //总箭数
    int points;             //总得分。这个不需要，只是为了将来统计方便
    QDateTime begtime;
    QDateTime endtime;
};

struct ArcheryRounds
{
    int id;
    int game_id;
    int range;
    int shoots;
    int points;
    QDateTime begtime;
    QDateTime endtime;
};

struct Shooting
{
    int id;
    int round_id;
    int arrow_id;
    int phase;              //段号。标准计分法应该3箭为一组(phase), 一轮射4组，即分4段。暂时保留，不用。
    int points;
    QDateTime shotTime;
    int shotTime_sec;
    int scoreMark;          //计分方式： 测试/人工/自动， 改分标记，对应DEF_SCOREMARK_xxx常数
};

struct Payment
{
    int id;
    int recordtype;         //0-单笔付款，1-汇总; +80:已结算(导出)
    int paymode;        //保留
    float amount;           //正值付款数，负值消费数 (?)
    float remain;       //用途待明确
    QString vip_cardno; //用途待明确
    QDateTime paytime;
};

class ArcheryScore
{
public:
    ArcheryScore();
    ~ArcheryScore();

    ArcheryGame gameRecord;
    ArcheryRounds roundRecord;
    Shooting shootRecord;
    QList <Shooting> shotList;      //for a round, 改分用
    int arrows_per_phase;

//    void newGame(int billMethod =0,int billLim = 60, int pay_eid=-1,int gametype =0);
    void newGame(BillingMethod billMethod =Billing_ByTime, int billLim = 60, int pay_eid=-1,QString archerName="", int gametype =0);
    void newRound();
    bool saveRound();
    bool saveGame();
    void closeGame();
    int checkoutGame();
    int savePayment(int linked_id, float amount, QString vip_cardno="");
//    bool resetDbase(int mode);

    int newShooting(int points, int scoringMode = 0, int shootsPerRound = 0, int prepareTime = 0);
    bool modifyShootingPoints(int arrow_id, int points);
    QStandardItemModel *shootingModel;

    void countForPoints(int *counts, QString archer, int dateDelta);

private:
    QSqlDatabase _db;
    int opendb();
    int hot_payment_id = -1;    //dbase table index

    void shootingToModal(QStandardItem *item, Shooting& shootRec);
};

#endif // ARCHERYSCORE_H
