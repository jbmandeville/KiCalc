#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>//>
#include <QCoreApplication>
#include "plot.h"
//#include "qcustomplot.h"

struct CommandOptions
{
    QString gadoTableFileName;  // positional argument 1
    QString variableTRFileName; // positional argument 2
    bool batchMode=false;       // -b
};
enum CommandLineParseResult
{
    CommandLineOk,
    CommandLineError,
    CommandLineHelpRequested
};

class MainWindow : public QMainWindow
{
private:
    QWidget *_centralWidget;
    QStatusBar *_statusBar;

    QCheckBox *_checkBoxLesion;
    QCheckBox *_checkBoxContra;
    QCheckBox *_checkBoxSagittal;

    plotData *_plotVariableTr;
    plotData *_plotEchoes;
    plotData *_plotDeltaR1;
    plotData *_plotKTrans;

    QLabel *_dataSetLabel;
    QLabel *_T1Lesion;
    QLabel *_T1Contra;
    QLabel *_T1Sag;
    QLabel *_kTransSlope;
    QLabel *_kTransOffset;

    CommandOptions _inputOptions;

    QHBoxLayout *createTopLayout();
    QHBoxLayout *createBottomLayout();
    QWidget *createPlotWidget();
    CommandLineParseResult parseCommandLine(QStringList commandLine);
    QString reformatStartupHelpText(QString inputText);

public:
    MainWindow();
    void readCommandLine();

private slots:
    void exitApp();

};

#endif // MAINWINDOW_H
