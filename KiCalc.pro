QT += core gui concurrent widgets printsupport

DEFINES += QT_NO_DEBUG_OUTPUT

#CONFIG += c++11 console
#CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += util

SOURCES += \
        main.cpp \
        mainwindow.cpp \
        util/generalglm.cpp \
        util/plot.cpp \
        util/qcustomplot.cpp \
        util/utilio.cpp

HEADERS += \
    mainwindow.h \
    util/generalglm.h \
    util/io.h \
    util/plot.h \
    util/qcustomplot.h

RESOURCES += \
    MyResources.qrc
