#ifndef DBVIEW_H
#define DBVIEW_H

#include <QDialog>
#include <QSqlDatabase>
#include <QtSql>
#include <QItemDelegate>

namespace Ui {
class DbView;
}

class ExRoundsTableModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit ExRoundsTableModel(QWidget *parent = 0);
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
};

class ExShootingsTableModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit ExShootingsTableModel(QWidget *parent = 0);
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
};

class ExGamesTableModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit ExGamesTableModel(QWidget *parent = 0);
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
};

class ExPaymentTableModel : public QSqlTableModel
{
    Q_OBJECT

public:
    explicit ExPaymentTableModel(QWidget *parent = 0);
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
};

#if 0
class GamesDelegate : public QItemDelegate
{
    Q_OBJECT
public:
    GamesDelegate(QObject *parent = 0);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const  QModelIndex &index) const;
};
#endif

//-------------------------------------------

class DbView : public QDialog
{
    Q_OBJECT

public:
    explicit DbView(QWidget *parent = 0);
    ~DbView();

private slots:
    void on_btnClose_clicked();
    void createScoreView();

//    void on_gamesTableViewClicked(QModelIndex index);
//    void on_tvGames_currentRowChanged(QModelIndex, QModelIndex);
    void tvGames_currentRowChanged(const QModelIndex&, const QModelIndex&);
//    void tvGame_selectionChanged(QItemSelection,QItemSelection);

    void tvRounds_currentRowChanged(const QModelIndex&, const QModelIndex&);

    void on_btnDbReset_clicked();

protected:
    void closeEvent(QCloseEvent *event);

private:
    Ui::DbView *ui;
    QSqlDatabase db;
//    QSqlRelationalTableModel *gamesModal;
//    QSqlTableModel *roundsModal;
//    QSqlTableModel *gamesModal;
    ExGamesTableModel *gamesModal;
    ExRoundsTableModel *roundsModal;
//    QSqlTableModel *shootingsModal;
    ExShootingsTableModel *shootingsModal;
    ExPaymentTableModel *paymentModal;

//    GamesDelegate games_delegate;

};

#endif // DBVIEW_H
