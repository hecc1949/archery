#ifndef SIGNIN_H
#define SIGNIN_H

#include <QDialog>
#include "softkeyboarddlg.h"
#include "archeryscore.h"

namespace Ui {
class SignIn;
}

class SignIn : public QDialog
{
    Q_OBJECT

public:
    explicit SignIn(QWidget *parent = 0);
    ~SignIn();

    void setMode(SigninSheet* pSheet);
    bool getSignInSheet(SigninSheet& sheet);
protected:
    bool eventFilter(QObject *, QEvent *);

private slots:
    void on_btnCancel_clicked();
    void on_btnOk_clicked();
    void on_cmPayMode_currentIndexChanged(int index);
private:
    Ui::SignIn *ui;
    SoftKeyBoardDlg *softKb;

    int min_shots;
    int min_times;
};

#endif // SIGNIN_H
