#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>//>
#include <QCoreApplication>
#include "plot.h"
#include "io.h"
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
    dVector _fitLesion;
    dVector _fitContra;
    double _S0Iterate;
    double _R1Iterate;

    QStringList _columnNamesVTR;     // column names
    QStringList _columnNamesShortTE; // column names
    QStringList _columnNamesLongTE;  // column names

    dMatrix _tableVTR;     // [columns][time]
    dMatrix _tableShortTE; // [columns][time]
    dMatrix _tableLongTE;  // [columns][time]

    // reformat the table above into (x,y,fit) structs
    GraphVector _lesionVTR,         _contraVTR,         _sinusVTR;
    GraphVector _lesionGdShortTE,   _contraGdShortTE,   _sinusGdShortTE;
    GraphVector _lesionGdLongTE,    _contraGdLongTE,    _sinusGdLongTE;
    GraphVector _lesionDR1,         _contraDR1,         _sinusDR1;

    dVector _deltaR1_lesion;
    dVector _deltaR1_contra;
    dVector _deltaR1_sinus;

    QHBoxLayout *createTopLayout();
    QWidget     *createTopPlotDuo();
    QHBoxLayout *createMiddleLayout();
    QWidget     *createBottomPlotDuo();
    QHBoxLayout *createBottomLayout();

    CommandLineParseResult parseCommandLine(QStringList commandLine);
    QString reformatStartupHelpText(QString inputText);
    void readDataFiles();
    void reformatData();
    void reformatDataVector(dMatrix table, QStringList columnNames, int iColumn, GraphVector &vector);
    void addCurveToPlot(plotData *plot, GraphVector vector, int color, bool addFit, bool normalizeOK);
    void addCorrectedCurveToPlot(GraphVector shortTE, GraphVector longTE, int color);    void updateDeltaR1Plot();
    void updateKTransPlot();
    void updateGdPlot();
    void updateVariableTRPlot();
    void normalizeToFirstPoint(dVector &data);
    void normalizeToFirstPoint(dVector data, dVector &fit);
    dVector computeDeltaR1(GraphVector shortTE, GraphVector longTE, double R1);
    void calculateKi();
    double integrate(dVector vector, int iTime);
    double fitVariableTR(GraphVector &dataVector, int iLevel);
    dVector fitKi(GraphVector tissueVector, GraphVector sinusVector, double &Ki, double &offset);

public:
    MainWindow();
    void readCommandLine();

public slots:
    void updateGraphs();

private slots:
    void exitApp();

};

#endif // MAINWINDOW_H
