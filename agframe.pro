#-------------------------------------------------
#
# Project created by QtCreator 2018-06-18T10:52:48
#
#-------------------------------------------------

#QT       += core gui multimedia

QT       += core gui
QT      += sql
QT      += opengl
QT      += charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = agframe
TEMPLATE = app

#CONFIG += object_with_source
MOC_DIR = ./moc
UI_DIR = ./UI

target.path=/usr/archery
INSTALLS += target

if(contains(DEFINES,ARM))   {
#    FFMpeg_Lib = /root/nfs/arm_local/lib
    FFMpeg_Lib = $$[QT_SYSROOT]/usr/lib
} else {
    FFMpeg_Lib = /usr/local/lib
    INCLUDEPATH +=  /usr/local/include
}


SOURCES += main.cpp\
        wallpaper.cpp \
        archerview.cpp \
    configdlg.cpp \
    ffvideo.cpp \
    devinterface.cpp \
    archeryscore.cpp \
    videodrawer.cpp \
    dbview.cpp \
    signin.cpp \
    softkeyboarddlg.cpp \
    fontlib_zh.cpp

HEADERS  += wallpaper.h \
        archerview.h \
    configdlg.h \
    ffvideo.h \
    devinterface.h \
    videodrawer.h \
    dbview.h \
    signin.h \
    softkeyboarddlg.h

FORMS += archerview.ui \
    configdlg.ui \
    dbview.ui \
    signin.ui \
    softkeyboarddlg.ui

# INCLUDEPATH +=  /usr/local/include

LIBS += -L$${FFMpeg_Lib} -lavcodec \
        -L$${FFMpeg_Lib} -lavdevice \
        -L$${FFMpeg_Lib} -lavfilter \
        -L$${FFMpeg_Lib} -lavformat \
        -L$${FFMpeg_Lib} -lavutil \
        -L$${FFMpeg_Lib} -lswscale \
        -L$${FFMpeg_Lib} -lswresample
#        -L$${FFMpeg_Lib} -lpostproc

QMAKE_CFLAGS += -DWITH_NOIDREF

RESOURCES += \
    resources.qrc

DISTFILES += \
    gameview.json \
    version.txt


