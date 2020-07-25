#include "wallpaper.h"
#include <QApplication>
#include <QTextCodec>
#include <QFrame>
#include <QDir>
#include "devinterface.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());   //移动到exe文件路径,避免找不到外置文件
/* for QT4
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF8"));
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));    //可不用
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF8"));
*/
//    devIntf = new DevInterface();
    Wallpaper w;
    w.show();

    return a.exec();
//    int ret = a.exec();
//    system("dd if=/dev/zero of=/dev/fb0");
//    return(ret);
}
