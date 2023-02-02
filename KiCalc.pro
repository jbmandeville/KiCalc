QT += core gui concurrent widgets printsupport

#DEFINES += QT_NO_DEBUG_OUTPUT

#CONFIG += c++11 console
#CONFIG -= app_bundle

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ../qcustomplot/qcustomplot.cpp \
        generalglm.cpp \
        main.cpp \
        mainwindow.cpp \
        plot.cpp \
        utilio.cpp

HEADERS += \
    ../qcustomplot/qcustomplot.h \
    generalglm.h \
    io.h \
    mainwindow.h \
    plot.h
