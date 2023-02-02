#ifndef plotData_H
#define plotData_H

#include <QMainWindow>
#include <QWidget>
#include <QVector>
#include "qcustomplot.h"
#include "io.h"

QT_BEGIN_NAMESPACE
class QAction;
class QGroupBox;
class QLabel;
class QMenu;
class QMenuBar;
class QPushButton;
class QFileDialog;
class QListWidget;
class QRadioButton;
class QString;
class QCPGraph;
QT_END_NAMESPACE

struct plotCurve
{
    dVector xData;        // original (non-concatenated x values); this should be removed from here and put into graphWindow
    dVector yData;        // data points
    bVector ignoreRescale;// if true, ignore the point when auto-scaling
    dVector yError;       // error bars (0=none, 1=bars, 2=envelope)
    dVector xPlot;        // potentially concatenated across files
    bool visible=true;    // allows curves to be turned off (e.g., single run mode)
    bool enabled=true;    // allows curves to be always invisible (but potentially available for export)
    bool histogram=false;
    bool exportCurve=true; // export curve to file? (e.g., ROIs)
    int iDataCurve=0;      // keep track of which data curve (generally file index); concatenated or parallel can change this
    double scaleFactorX=1;
    double scaleFactorY=1;    // this is a property of a single curve
    bool isCurrentCurve=true; // for interactive cooperation between plotting and image display (curve = file, time point = tracer position)
    QString fileName;      // file source for this curve
    QString legend;
    QColor color=Qt::black;
    int lineThickness=1;
    int pointSize=5;
    QCPScatterStyle::ScatterShape pointStyle;
    int errorBars=0;      // 0=none, 1=bars, 2=envelop
};

class plotData : public QMainWindow
{
    Q_OBJECT
signals:
    void changedPointFromGraph(int iDataCurve, int iTime);

private:
    QCustomPlot *_qcplot;              // the QCustomPlot plotting class object
    QWidget *_parentPage;
    QStatusBar *_statusBar=nullptr;
    // Plotting mode and style
    int _plotID;                       // ID different plot surfacers (only the time plot gets some connections)
    bool _singleROIMode=true;          // if true, show only 1 ROI and potentially also the fit
    int _iFitType=0;                   // different fit type that indicate different plotting styles (data/fit, PET RTM with BP, .. etc)
    bool _autoScale=true;
    QCPRange _autoScaleXRange={0.,0.};
    bool _pressToMoveTracer=true;
    QCPItemTracer *_positionTracer;
    int _iCurvePosition=0;
    int _iCurvePositionLast=0;
    int _iPointPosition=0;
    int _iPointPositionLast=0;
    bool _concatenateRuns;
    bool _cursorOnPlot=false;
    double _xAxis2Ratio;    // ratio of yAxis2 max to yAxis1 max
    double _yAxis2Ratio;    // ratio of yAxis2 max to yAxis1 max

    iVector _nTimePerFile;  // [_nFiles]

    int whichConcatenatedTracerFile(double x);
    int whichTracerTimePoint(int iDataCurve);
    void interpretMousePosition(QMouseEvent *event);
    int getDataCurveIndex(int iDataCurve);
    void concatenateRuns();
    void reScaleAxes(QCPRange *xRange, QCPRange *yRange);
    double setXPosition(int iDataCurve, int iTime);
    void writeTemporaryStream(bool newDirectory, QString regionName, QTextStream &tempStream);

public:
    int _nFiles=0;
    int _nCurvesPerFile=3;  // often data + fit + weights

    plotData(int plotID);
//    virtual ~plotData() {};
    QVector<plotCurve> _listOfCurves;
    // total number of curves = _listOfCurves.size() = _nFiles * _nCurvesPerFile + numberExtra

    void plotDataAndFit(bool newData);
    inline void setTracerDisplay(bool state) {_positionTracer->setVisible(state);}
    void writeGraphAllCurves(bool newDirectory, bool newFile, QString fileName, QString regionName);
    void writeGraphJustData(bool newDirectory, bool newGraph, QString fileName, QString regionName);

    inline QCustomPlot *getPlotSurface() const {return _qcplot;}
    inline int getFitType() const {return _iFitType;}
    inline bool getSingleROIMode() const {return _singleROIMode;}
    inline double getXPosition(int iCurve, int iTime) {return _listOfCurves[iCurve].xPlot[iTime];}
    void setPositionTracer(int iDataCurve, double xPosition);
    void setPositionTracer(int iDataCurve, int iTime);
    inline int getNumberCurves() {return _listOfCurves.size();}

    // setting up a series of curves
    void init();
    inline void addCurve(int iDataCurve, QString legend) {addCurve(iDataCurve, "", legend);}
    void addCurve(int iDataCurve, QString fileName, QString legend);
    void conclude(int iCurrentFile, bool singleRun);
    void setData(dVector xData, dVector yData);
    void setData(dVector xData, dVector yData, dVector yError);
    void setData(dVector xData, dVector yData, bVector ignoreRescale);
    void setData(dVector xData, dVector yData, dVector yError, bVector ignoreRescale);
    inline void setLineThickness(int thickness) {_listOfCurves.last().lineThickness=thickness;}
    inline void setPointSize(int pointSize) {_listOfCurves.last().pointSize=pointSize;
                                            if (_listOfCurves.last().pointStyle == QCPScatterStyle::ssNone)
                                                _listOfCurves.last().pointStyle = QCPScatterStyle::ssDisc;}
    inline void setPointStyle(QCPScatterStyle::ScatterShape pointStyle) {_listOfCurves.last().pointStyle=pointStyle;}
    inline void setErrorBars(int errorBars) {_listOfCurves.last().errorBars=errorBars;}
    inline void setColor(QColor color) {_listOfCurves.last().color=color;}
    inline void setVisible(bool visible) {_listOfCurves.last().visible=visible;}
    inline void setEnabled(bool enabled) {_listOfCurves.last().enabled=enabled;}
    inline void setHistogram(bool histogram) {_listOfCurves.last().histogram=histogram;}
    inline void setExport(bool exportCurve) {_listOfCurves.last().exportCurve=exportCurve;}
    inline void setCurrentCurve(bool isCurrentCurve) {_listOfCurves.last().isCurrentCurve=isCurrentCurve;}
    inline void setLegend(QString legend) {_listOfCurves.last().legend=legend;}
    // set scaleFactor for single curve; set _xAxis2Ratio; if necessary, this can be overridden by a later call of setYAxisRatio
    inline void setScaleFactorX(int iCurve, double scaleFactor) {_listOfCurves[iCurve].scaleFactorX=scaleFactor; _xAxis2Ratio=1./scaleFactor;}
    inline void setScaleFactorX(double scaleFactor) {_listOfCurves.last().scaleFactorX=scaleFactor; _xAxis2Ratio=1./scaleFactor;}
    // set scaleFactor for single curve; set _yAxis2Ratio; if necessary, this can be overridden by a later call of setYAxisRatio
    inline void setScaleFactorY(int iCurve, double scaleFactor) {_listOfCurves[iCurve].scaleFactorY=scaleFactor; _yAxis2Ratio=1./scaleFactor;}
    inline void setScaleFactorY(double scaleFactor) {_listOfCurves.last().scaleFactorY=scaleFactor; _yAxis2Ratio=1./scaleFactor;}
    void setScaleFactorYRelative(QString referenceLegend);
    inline void setXAxisRatio(double ratio) {_xAxis2Ratio = ratio;}
    inline void setYAxisRatio(double ratio) {_yAxis2Ratio = ratio;}

    inline void setQCStatusBar(QStatusBar *statusBar ) {_statusBar = statusBar;}
    inline void setAutoScale( bool value ) {_autoScale = value;}
    inline void setSingleROIMode( bool mode ) {_singleROIMode = mode;}
    inline void setVisibleXAxis(bool visible) {_qcplot->xAxis->setVisible(visible);}
    inline void setLabelXAxis(QString label) {_qcplot->xAxis->setLabel(label);}
    inline void setLabelYAxis(QString label) {_qcplot->yAxis->setLabel(label);}
    inline void setLabelXAxis2(QString label) {_qcplot->xAxis2->setLabel(label);}
    inline void setLabelYAxis2(QString label) {_qcplot->yAxis2->setLabel(label);}
    inline void setFitType(int iFit) {_iFitType = iFit;}
    inline void setMainPage(QWidget *main) {_parentPage = main;}
    inline void setAutoScaleXRange(QCPRange range) {_autoScaleXRange = range;}
    inline void setLegendOn(bool state) {_qcplot->legend->setVisible(state);}
    inline void setConcatenatedRuns(bool state) {_concatenateRuns = state; concatenateRuns();}

    // getters
    inline double getTracerCurve() {return _iCurvePosition;}
    inline double getTracerPoint() {return _iPointPosition;}
    inline double getTracerXPosition() {return _positionTracer->graphKey();}
    inline QString getLabelXAxis() {return _qcplot->xAxis->label();}
    double getMaxYAbs(int iGraph);
    double getMaxYAbs(plotCurve *ptrCurve);
    inline void setXRange(QCPRange range) {_qcplot->xAxis->setRange(range); _qcplot->replot();}
    inline void setYRange(QCPRange range) {_qcplot->yAxis->setRange(range); _qcplot->replot();}
    inline bool isAutoScale() {return _autoScale;}
    inline plotCurve *getThisCurvePointer() {return &_listOfCurves.last();}
    inline double getXAxisRatio() {return _xAxis2Ratio;}
    inline double getYAxisRatio() {return _yAxis2Ratio;}
    inline int getNumberPoints(int iGraph) {return _listOfCurves[iGraph].xData.size();}
    inline double getXValue(int iGraph, int iPoint) {return _listOfCurves[iGraph].xData[iPoint];}
    inline double getYValue(int iGraph, int iPoint) {return _listOfCurves[iGraph].yData[iPoint];}
    inline dVector getXData(int iGraph)             {return _listOfCurves[iGraph].xData;}
    inline dVector getYData(int iGraph)             {return _listOfCurves[iGraph].yData;}

private slots:
    void axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part);
    void legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item);
    void contextMenuRequest(QPoint pos);
    void makeSelectedGraphDotted();
    void makeSelectedGraphBiggerPoints();
    void makeSelectedGraphSmallerPoints();
    void removeSelectedGraph();
    void writeSelectedGraph();
    void moveLegend();
    void plotMousePress(QMouseEvent *event);
    void plotMouseMove(QMouseEvent *event);
    void writeOneGraph();
    void changeSelectMethod();
    void popUpChangeXRange();
    void popUpChangeYRange();
    inline void showLegend() {_qcplot->legend->setVisible(true); _qcplot->replot();}
    inline void hideLegend() {_qcplot->legend->setVisible(false); _qcplot->replot();}

    void lastTimePoint();
    void setXAxis2Range(QCPRange Range);
    void setYAxis2Range(QCPRange Range);

    void keyboardPlus();
    void keyboardMinus();

public slots:
    void setXZoom();
    void setYZoom();
    void autoScale(bool state);
    void setSelectPoints(bool pointNotCurve);
signals:
    void autoScaleRanges(bool state);
};

#endif // plotData_H
