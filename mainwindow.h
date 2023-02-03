#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>//>
#include <QCoreApplication>
#include "plot.h"
//#include "qcustomplot.h"

struct fixedParameters
{
    double TE1=2.;
    double TE2=7.;
    double TRGado=0.5;
    double Hct=0.4;
    double timeStepGd=0.8;  // min
};

struct CommandOptions
{
    QString variableTRFileName;  // positional argument 1
    QString gadoShortTEFileName; // positional argument 2
    QString gadoLongTEFileName;  // positional argument 3
    bool batchMode=false;        // -b
};
enum CommandLineParseResult
{
    CommandLineOk,
    CommandLineError,
    CommandLineHelpRequested
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QWidget *_centralWidget;
    QStatusBar *_statusBar;

    QCheckBox *_checkBoxLesion;
    QCheckBox *_checkBoxContra;
    QCheckBox *_checkBoxSagittal;
    QRadioButton *_radioRaw;
    QRadioButton *_radioNorm;
    QCheckBox *_includeCorrected;

    plotData *_plotVariableTr;
    plotData *_plotEchoes;
    plotData *_plotDeltaR1;
    plotData *_plotKTrans;

    QLabel *_dataSetLabel;
    QLabel *_T1LesionLabel;
    QLabel *_T1ContraLabel;
    QLabel *_T1SagLabel;
    QLabel *_kTransSlopeLesionLabel;
    QLabel *_kTransSlopeContraLabel;
    QLabel *_kTransOffsetLesionLabel;
    QLabel *_kTransLesionLabel;
    QLabel *_kTransContraLabel;

    CommandOptions _inputOptions;
    fixedParameters protocolPars;
    double _R1Lesion;
    double _R1Contra;
    double _R1Sag;
    double _temporaryR1 = 0.33;
    double _kiLesion;
    double _kiContra;
    dVector _fitLesion;
    dVector _fitContra;
    double _S0Iterate;
    double _R1Iterate;

    QStringList _columnNamesVTR;     // column names
    QStringList _columnNamesShortTE; // column names
    QStringList _columnNamesLongTE;  // column names

    dMatrix _tableVTR;     // [time][column]
    dMatrix _tableShortTE; // [time][column]
    dMatrix _tableLongTE;  // [time][column]

    dVector _deltaR1_lesion;
    dVector _deltaR1_contra;
    dVector _deltaR1_sinus;

    dVector _fitR1_lesion;
    dVector _fitR1_contra;
    dVector _fitR1_sinus;

    QHBoxLayout *createTopLayout();
    QWidget     *createTopPlotDuo();
    QHBoxLayout *createMiddleLayout();
    QWidget     *createBottomPlotDuo();
    QHBoxLayout *createBottomLayout();

    CommandLineParseResult parseCommandLine(QStringList commandLine);
    QString reformatStartupHelpText(QString inputText);
    void readDataFiles();
    void addCurveToPlot(plotData *plot, QStringList columnNames, dMatrix table, int iColumn);
    void addCorrectedCurveToPlot(QStringList columnNames, int iColumn);
    void updateDeltaR1Plot();
    void updateKTransPlot();
    void updateGdPlot();
    void updateVariableTRPlot();
    void normalizeToFirstPoint(dVector &vector);
    dVector computeDeltaR1(int iColumn);
    double integrate(dVector vector, int iTime);
    dVector fitVariableTR(int iColumn, int iLevel);
    dVector fitKi(dVector dR1Tissue, dVector dR1Sinus, double &Ki, double &offset);

public:
    MainWindow();
    void readCommandLine();

public slots:
    void updateGraphs();

private slots:
    void exitApp();

};

#endif // MAINWINDOW_H
