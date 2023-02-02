#include <QtWidgets>
#include <QFrame>
#include <QDebug>
#include <QVector>
#include <QObject>
#include <QFileDialog>
#include <QString>
#include <QPen>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSize>

#include "plot.h"

/*
plotData::~plotData()
{
}
*/

plotData::plotData(int plotID)
{
    FUNC_ENTER;
//    _parentPage = containingPage;

    _plotID = plotID;

    _qcplot = new QCustomPlot();
    _qcplot->setObjectName(QStringLiteral("plot"));
    _qcplot->xAxis->setLabel("time");
    _qcplot->yAxis->setLabel("y");
    _qcplot->xAxis2->setLabel("");
    _qcplot->yAxis2->setLabel("");
    _qcplot->xAxis->setLabelFont(QFont("Arial", 20));
    _qcplot->yAxis->setLabelFont(QFont("Helvetica", 20));
    _qcplot->xAxis->setTickLabelFont(QFont("Arial", 16));
    _qcplot->yAxis->setTickLabelFont(QFont("Helvetica",16));
    _qcplot->xAxis2->setVisible(true);
    _qcplot->xAxis2->setTickLabels(false);
    _qcplot->yAxis2->setVisible(true);
    _qcplot->yAxis2->setTickLabels(false);
//    _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectAxes);
//    _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    _qcplot->setAutoAddPlottableToLegend(true);
    _qcplot->legend->setVisible(false);
    _qcplot->legend->setFont(QFont("Arial", 20));

    // add the phase tracer (red circle) which sticks to the graph data (and gets updated in bracketDataSlot by timer event):
    _positionTracer = new QCPItemTracer(_qcplot);
//    phaseTracer->setInterpolating(true);
    _positionTracer->setStyle(QCPItemTracer::tsCircle);
    _positionTracer->setPen(QPen(Qt::red));
    _positionTracer->setBrush(Qt::cyan);
    _positionTracer->setSize(16);
//    _positionTracer->setVisible(false);
    _positionTracer->setGraphKey(0);
    _positionTracer->setVisible(true);

    setSelectPoints(true);

    connect(_qcplot, SIGNAL(axisClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this,
            SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
//    connect(_qcplot, SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)), this,
//            SLOT(axisLabelDoubleClick(QCPAxis*,QCPAxis::SelectablePart)));
    connect(_qcplot, SIGNAL(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this,
            SLOT(legendDoubleClick(QCPLegend*,QCPAbstractLegendItem*)));
    connect(_qcplot, SIGNAL(mousePress(QMouseEvent*)),this,SLOT(plotMousePress(QMouseEvent*)));
    connect(_qcplot, SIGNAL(mouseMove(QMouseEvent*)), this,SLOT(plotMouseMove(QMouseEvent*)));

    connect(_qcplot->xAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(setXAxis2Range(QCPRange)));
    connect(_qcplot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(setYAxis2Range(QCPRange)));

    // setup policy and connect slot for context menu popup:
    _qcplot->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_qcplot, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequest(QPoint)));

    if ( _plotID == 0 )
    { // for the time-series plot only
        QAction *plusAction = new QAction(this);
        plusAction->setShortcut(Qt::Key_Equal);
        connect(plusAction, SIGNAL(triggered()), this, SLOT(keyboardPlus()));
        QAction *minusAction = new QAction(this);
        minusAction->setShortcut(Qt::Key_Minus);
        connect(minusAction, SIGNAL(triggered()), this, SLOT(keyboardMinus()));

        _qcplot->addAction(plusAction);
        _qcplot->addAction(minusAction);
    }

    FUNC_EXIT;
}

void plotData::keyboardMinus()
{
    int iCurve = _iCurvePosition;
    int iPoint = _iPointPosition - 1;
    if ( iPoint < 0 )
    { // go to the previous data curve
        iCurve--;
        int iDataCurve = getDataCurveIndex(iCurve);   // data points should be 1st curve in iCurve
        if ( iDataCurve < 0 )
        {
            iDataCurve = getDataCurveIndex(_listOfCurves.size()-1);
            iPoint = _listOfCurves[iDataCurve].yData.size() - 1; // last time point in last file
        }
    }
    setPositionTracer(iCurve, iPoint);
    emit changedPointFromGraph(_iCurvePosition,_iPointPosition);
}

void plotData::keyboardPlus()
{
    FUNC_ENTER << _iCurvePosition << _iPointPosition;
    int iCurve = _iCurvePosition;
    // Go to the next time point on the same curve, if it exists
    int iDataCurve = getDataCurveIndex(iCurve);
    int iPoint = _iPointPosition + 1;
    if ( iPoint >= _listOfCurves[iDataCurve].yData.size() )
    { // go to the next data curve
        iCurve++;  iPoint=0;
        int iDataCurve = getDataCurveIndex(iCurve);
        if ( iDataCurve >= _listOfCurves.size() )
            iCurve = iPoint = 0; // wrap-around to the 1st data curve and point
    }
    FUNC_INFO << "setPositionTracer" << iCurve << iPoint;
    setPositionTracer(iCurve, iPoint);
    emit changedPointFromGraph(_iCurvePosition,_iPointPosition);
}

///////////////////////////////////////////////////////////////////
///////////////////////// slots ///////////////////////////////////
///////////////////////////////////////////////////////////////////
void plotData::setXZoom()
{
    _qcplot->setCursor(Qt::OpenHandCursor);
    _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    _qcplot->axisRect(0)->setRangeDrag(Qt::Horizontal);
    _qcplot->axisRect(0)->setRangeZoom(Qt::Horizontal);
    autoScale(false);
}
void plotData::setYZoom()
{
    _qcplot->setCursor(Qt::OpenHandCursor);
    _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    _qcplot->axisRect(0)->setRangeDrag(Qt::Vertical);
    _qcplot->axisRect(0)->setRangeZoom(Qt::Vertical);
    autoScale(false);
}
void plotData::setSelectPoints(bool pointNotCurve)
{
    if ( pointNotCurve )
    {
        _pressToMoveTracer = true;
        _qcplot->setCursor(Qt::CrossCursor);
        if ( _autoScale )
            _qcplot->setInteractions(nullptr);
        else
            _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    }
    else
    {
        _pressToMoveTracer = false;
        _qcplot->setCursor(Qt::ArrowCursor);
        if ( _autoScale )
            _qcplot->setInteractions(QCP::iSelectPlottables);
        else
            _qcplot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    }
}
void plotData::lastTimePoint()
{
    setPositionTracer(_iCurvePositionLast, _iPointPositionLast);
    emit changedPointFromGraph(_iCurvePosition,_iPointPosition);
}

void plotData::setXAxis2Range( QCPRange Range1 )
{
    QCPRange Range2;
    Range2.lower = Range1.lower * _xAxis2Ratio;
    Range2.upper = Range1.upper * _xAxis2Ratio;
    FUNC_INFO << "Range2" << _xAxis2Ratio << Range1 << Range2;
    _qcplot->xAxis2->setRange(Range2);
}
void plotData::setYAxis2Range( QCPRange Range1 )
{
    QCPRange Range2;
    Range2.lower = Range1.lower * _yAxis2Ratio;
    Range2.upper = Range1.upper * _yAxis2Ratio;
    _qcplot->yAxis2->setRange(Range2);
}
void plotData::axisLabelDoubleClick(QCPAxis *axis, QCPAxis::SelectablePart part)
{
  // Set an axis label by double clicking on it
  if (part == QCPAxis::spAxisLabel) // only react when the actual axis label is clicked, not tick label or axis backbone
  {
    bool ok;
    QString newLabel = QInputDialog::getText(this, "QCustomPlot example", "New axis label:", QLineEdit::Normal, axis->label(), &ok);
    if (ok)
    {
      axis->setLabel(newLabel);
      _qcplot->replot();
    }
  }
}
void plotData::legendDoubleClick(QCPLegend *legend, QCPAbstractLegendItem *item)
{
  // Rename a graph by double clicking on its legend item
  Q_UNUSED(legend)
  if (item) // only react if item was clicked (user could have clicked on border padding of legend where there is no item, then item is 0)
  {
      QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
      bool ok;
      QString newName = QInputDialog::getText(this, "QCustomPlot example", "New graph name:", QLineEdit::Normal, plItem->plottable()->name(), &ok);
      if (ok)
      {
          plItem->plottable()->setName(newName);
          _qcplot->replot();
      }
  }
}
void plotData::contextMenuRequest(QPoint pos)
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (_qcplot->legend->selectTest(pos, false) >= 0 ) // context menu on legend requested
        //  if (_qcplot->legend->selectTest(pos, false) >= 0 && _qcplot->legend->visible() ) // context menu on legend requested
    {
        if  ( _qcplot->legend->visible() )
            menu->addAction("Hide legend", this, SLOT(hideLegend()));
        else
            menu->addAction("Show legend", this, SLOT(showLegend()));

        menu->addAction("Move to top left", this, SLOT(moveLegend()))->setData(static_cast<int>((Qt::AlignTop|Qt::AlignLeft)));
        menu->addAction("Move to top center", this, SLOT(moveLegend()))->setData(static_cast<int>((Qt::AlignTop|Qt::AlignHCenter)));
        menu->addAction("Move to top right", this, SLOT(moveLegend()))->setData(static_cast<int>((Qt::AlignTop|Qt::AlignRight)));
        menu->addAction("Move to bottom right", this, SLOT(moveLegend()))->setData(static_cast<int>((Qt::AlignBottom|Qt::AlignRight)));
        menu->addAction("Move to bottom left", this, SLOT(moveLegend()))->setData(static_cast<int>((Qt::AlignBottom|Qt::AlignLeft)));
    }
    else if (_qcplot->selectedGraphs().size() > 0)
    {
        menu->addAction("Write selected graph", this, SLOT(writeSelectedGraph()));
        menu->addAction("Remove selected graph", this, SLOT(removeSelectedGraph()));
        menu->addAction("LineStyle dotted", this, SLOT(makeSelectedGraphDotted()));
        if ( _qcplot->selectedGraphs().first()->scatterStyle().size() != 0. )
        {
            menu->addAction("Bigger points", this, SLOT(makeSelectedGraphBiggerPoints()));
            menu->addAction("Smaller points", this, SLOT(makeSelectedGraphSmallerPoints()));
        }
    }
    else
    {
        menu->addAction("Change x range", this, SLOT(popUpChangeXRange()));
        menu->addAction("Change y range", this, SLOT(popUpChangeYRange()));
        if ( _pressToMoveTracer )
            menu->addAction("Select one curve", this, SLOT(changeSelectMethod()));
        else
            menu->addAction("Select one point", this, SLOT(changeSelectMethod()));
        menu->addAction("Write all curves to table file", this, SLOT(writeOneGraph()));
    }
    /*
  if (_qcplot->selectedGraphs().size() > 0)
  {
      menu->addAction("Remove selected graph", this, SLOT(removeSelectedGraph()));
      menu->addAction("LineStyle dotted", this, SLOT(makeSelectedGraphDotted()));
      menu->addAction("Bigger points", this, SLOT(makeSelectedGraphBiggerPoints()));
      menu->addAction("Smaller points", this, SLOT(makeSelectedGraphSmallerPoints()));
  }
*/
    menu->popup(_qcplot->mapToGlobal(pos));
}

void plotData::popUpChangeXRange()
{
    QCPRange xRange = _qcplot->xAxis->range();
    bool ok;
    QString lower, upper, range;
    range = lower.setNum(xRange.lower) + " " + upper.setNum(xRange.upper);
    QString rangeString = QInputDialog::getText(this, "X Range:", "min,max (use comma/space)", QLineEdit::Normal, range, &ok);
    if ( ok )
    {
        QRegExp rx("[,\\s]");// match a comma or a space
        QStringList valueList = rangeString.split(rx, QString::SkipEmptyParts);
        if ( valueList.size() != 2 ) return;
        lower = valueList[0];  upper = valueList[1];
        bool ok1, ok2;
        double lowerValue = lower.toDouble(&ok1);
        double upperValue = upper.toDouble(&ok2);
        if ( ok1 && ok2 )
        {
            xRange.lower = qMin(lowerValue,upperValue);
            xRange.upper = qMax(lowerValue,upperValue);
            _qcplot->xAxis->setRange(xRange);
            _qcplot->replot();
        }
        _autoScale = false;
        emit autoScaleRanges(_autoScale);
    }
}
void plotData::popUpChangeYRange()
{
    QCPRange yRange = _qcplot->yAxis->range();
    bool ok;
    QString lower, upper, range;
    range = lower.setNum(yRange.lower) + " " + upper.setNum(yRange.upper);
    QString rangeString = QInputDialog::getText(this, "Y Range:", "min,max (use comma/space)", QLineEdit::Normal, range, &ok);
    if ( ok )
    {
        QRegExp rx("[,\\s]");// match a comma or a space
        QStringList valueList = rangeString.split(rx, QString::SkipEmptyParts);
        if ( valueList.size() != 2 ) return;
        lower = valueList[0];  upper = valueList[1];
        bool ok1, ok2;
        double lowerValue = lower.toDouble(&ok1);
        double upperValue = upper.toDouble(&ok2);
        if ( ok1 && ok2 )
        {
            yRange.lower = qMin(lowerValue,upperValue);
            yRange.upper = qMax(lowerValue,upperValue);
            _qcplot->yAxis->setRange(yRange);
            _qcplot->replot();
        }
        _autoScale = false;
        emit autoScaleRanges(_autoScale);
    }
}

void plotData::changeSelectMethod()
{
    setSelectPoints(!_pressToMoveTracer);
}

void plotData::makeSelectedGraphDotted()
{
  if (_qcplot->selectedGraphs().size() > 0)
  {
      QPen currentPen = _qcplot->selectedGraphs().at(0)->pen();
      QCPScatterStyle currentScatter = _qcplot->selectedGraphs().first()->scatterStyle();
      QBrush currentBrush = _qcplot->selectedGraphs().first()->brush();
      currentPen.setStyle(Qt::DotLine);
//      currentPen.setColor(Qt::green);
//      currentScatter.setBrush(Qt::green);
//      currentScatter.setPen(currentPen);
//      currentScatter.setSize(5);
//      currentScatter.setShape(QCPScatterStyle::ssCircle);
      _qcplot->selectedGraphs().first()->setScatterStyle(currentScatter);
      _qcplot->selectedGraphs().first()->setPen(currentPen);
      _qcplot->replot();
  }
}
void plotData::makeSelectedGraphBiggerPoints()
{
  if (_qcplot->selectedGraphs().size() > 0)
  {
      QCPScatterStyle currentScatter = _qcplot->selectedGraphs().first()->scatterStyle();
      currentScatter.setSize(1.5*currentScatter.size());
      _qcplot->selectedGraphs().first()->setScatterStyle(currentScatter);
      _qcplot->replot();
  }
}
void plotData::makeSelectedGraphSmallerPoints()
{
  if (_qcplot->selectedGraphs().size() > 0)
  {
      QCPScatterStyle currentScatter = _qcplot->selectedGraphs().first()->scatterStyle();
      currentScatter.setSize(0.75*currentScatter.size());
      _qcplot->selectedGraphs().first()->setScatterStyle(currentScatter);
      _qcplot->replot();
  }
}
void plotData::removeSelectedGraph()
{
  if (_qcplot->selectedGraphs().size() > 0)
  {
    _qcplot->removeGraph(_qcplot->selectedGraphs().first());
    _qcplot->replot();
  }
}

void plotData::writeOneGraph()
{
    QString fileName = "/graph.dat";
    QFileDialog fileDialog;
    QString fullFileName;
    fullFileName = fileDialog.getSaveFileName(this,
                                              "Name of file",
                                              QDir::currentPath()+fileName,
                                              tr("Text files (*.roi *.dat *.txt)"));
    if ( fullFileName.isEmpty() ) return;
    writeGraphAllCurves(true,true,fullFileName,"");
}

void plotData::writeTemporaryStream(bool newDirectory, QString regionName, QTextStream &tempStream)
{
    if ( newDirectory ) tempStream << "directory " << QDir::currentPath() << "\n";
    // Write the headers; assume all files have the same curves to be exported
    FUNC_INFO << "nfiles" << _nFiles;
    for (int jFile=0; jFile<_nFiles; jFile++)
    {
        int iDataCurve = getDataCurveIndex(jFile);  // points to 1st curve for this file
        tempStream << "new File " << _listOfCurves[iDataCurve].fileName << "\n";
        // Write headers
        tempStream << "index time ";
        FUNC_INFO << "_nCurvesPerFile" << _nCurvesPerFile;
        for (int jCurve=0; jCurve<_nCurvesPerFile; jCurve++)
        {
            plotCurve currentCurve = _listOfCurves[jCurve];
            if ( currentCurve.iDataCurve == jFile && currentCurve.exportCurve )
            {
                if ( currentCurve.legend == "data" && !regionName.isEmpty() )
                    tempStream << regionName << " ";
                else
                    tempStream << currentCurve.legend << " ";
                if ( currentCurve.errorBars != 0 )
                    tempStream << currentCurve.legend << "_err ";
                if ( currentCurve.scaleFactorY != 1. )
                    tempStream << currentCurve.legend << "_scaled ";
            }
        }
        tempStream << "\n";
        // loop over time points and write all curves for this file in columns
        for (int jt=0; jt<_listOfCurves[iDataCurve].yData.size(); jt++)
        {
            tempStream << jt << " " << _listOfCurves[iDataCurve].xPlot[jt] << " ";
            for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
            {
                plotCurve currentCurve = _listOfCurves[jCurve];
                if ( currentCurve.iDataCurve == jFile && currentCurve.exportCurve ) // if correct file and export
                {
//                    if ( jt==0) FUNC_INFO << "output" << currentCurve.legend << currentCurve.yData[jt];
                    if ( currentCurve.scaleFactorY != 1. )
                        tempStream << currentCurve.yData[jt] / currentCurve.scaleFactorY << " ";
                    else
                        tempStream << currentCurve.yData[jt] << " ";
                    if ( currentCurve.errorBars != 0 )
                        tempStream << currentCurve.yError[jt] << " ";
                } // write curve
            } // jCurve
            tempStream << "\n";
        } // jt
        tempStream << "\n";
    } // jFile
    tempStream.flush();//flush the stream into the file
}


void plotData::writeGraphAllCurves(bool newDirectory, bool newFile, QString fileName, QString regionName)
{
    FUNC_ENTER << newDirectory << newFile << fileName << regionName;
    QTemporaryFile tempFile;
    if (!tempFile.open()) return;
    QTextStream tempStream(&tempFile);
    writeTemporaryStream(newDirectory,  regionName, tempStream);
    tempStream.seek(0);

    QFile file(fileName);
    QTextStream outStream(&file);

    if ( newFile )
    { // open the output file as write only and do a copy/paste
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    }
    else
    { // not new file: append
        if (!file.open(QIODevice::Append | QIODevice::Text)) return;
        if ( !newDirectory ) outStream << "new ROI\n";
    }
    outStream << tempFile.readAll(); // just copy/paste
    file.close();
    tempFile.close();
}

void plotData::writeGraphJustData(bool newDirectory, bool newFile, QString fileName, QString regionName)
{
    FUNC_ENTER << newDirectory << newFile << fileName << regionName;
    QTemporaryFile tempFile;
    if (!tempFile.open()) return;
    QTextStream tempStream(&tempFile);
    writeTemporaryStream(newDirectory,  regionName, tempStream);
    tempStream.seek(0);

    if ( newFile ) // straightforward: just take 3 first words
    { // open the output file as write only and take the 1st 3 words (index time region)
        QFile file(fileName);
        QTextStream outStream(&file);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
         // just take the 1st 3 words of the temporary file (index time region)
        QString line = tempStream.readLine();
        while (!tempStream.atEnd() )
        {
            QRegExp rx("[,\\s]");// match a comma or a space
            QStringList stringList = line.split(rx, QString::SkipEmptyParts);
            FUNC_INFO << "lineOrig" << line << "stringList" << stringList;
            if ( stringList.count() < 3 ) // blank line or "new file"
                outStream << line << "\n";
            else // index time region
                outStream << stringList.at(0) << " " << stringList.at(1) << " " << stringList.at(2) << "\n";
            line = tempStream.readLine();
        }
        file.close();
    }
    else // take all words and append the data column
    { // not new file: append
        // open the output file 1st as read and then as write
        QFile file(fileName);
        QTextStream outStream(&file);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QTemporaryFile tempFileOriginal;
        QTextStream tempStreamOriginal(&tempFileOriginal);
        if (!tempFileOriginal.open()) return;
        tempStreamOriginal << outStream.readAll(); // first make a copy of the existing data.
        tempStreamOriginal.seek(0);
        file.close();

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        outStream.setDevice(&file);  outStream.seek(0);
        if ( !newDirectory )
        { // only the 1st/original has the directory info
            QString lineOriginal = tempStreamOriginal.readLine();  // to match prior write
            outStream << lineOriginal << "\n";
        }
        while (!tempStreamOriginal.atEnd())
        {
            QString lineOriginal = tempStreamOriginal.readLine();
            QString lineNew      = tempStream.readLine();
            FUNC_INFO << "original" << lineOriginal;
            FUNC_INFO << "new line" << lineNew;
            QRegExp rx("[,\\s]");// match a comma or a space
            QStringList stringListOrig = lineOriginal.split(rx, QString::SkipEmptyParts);
            QStringList stringListNew  = lineNew.split(rx, QString::SkipEmptyParts);
            if ( stringListNew.count() < 3 ) // blank line or "new file"
                outStream << lineNew << "\n";
            else
            {
                // index time region, but use only region
                //                    FUNC_INFO << "new line:" << lineOriginal << " " << stringList.at(2) << "\n";
                if ( !stringListOrig.at(0).compare("directory") )
                    outStream << lineOriginal << "\n";
                else
                    outStream << lineOriginal << " " << stringListNew.at(2) << "\n";
            }
            //                FUNC_INFO << "end it?" << tempStreamOriginal.atEnd();
        }
        file.close();
        tempFileOriginal.close();

    }
    tempFile.close();
}

void plotData::writeSelectedGraph()
{
    if (_qcplot->selectedGraphs().size() == 1)
    {
        QString fileName;
        QFileDialog fileDialog;
        fileName = fileDialog.getSaveFileName(this,
                                              "Name of file",
                                              QDir::currentPath(),
                                              "(*.dat)");
        if ( fileName.isEmpty() ) return;
        QFile file(fileName);
        QTextStream out(&file);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return;
        out << "x y\n";
        int nTime = _qcplot->selectedGraphs().at(0)->dataCount();
        for (int jt=0; jt<nTime; jt++)
            out << _qcplot->selectedGraphs().at(0)->dataMainKey(jt)   << " "
                << _qcplot->selectedGraphs().at(0)->dataMainValue(jt) << "\n";
        file.close();
    }
}
void plotData::moveLegend()
{
    if (QAction* contextAction = qobject_cast<QAction*>(sender())) // make sure this slot is really called by a context menu action, so it carries the data we need
    {
        bool ok;
        int dataInt = contextAction->data().toInt(&ok);
        if (ok)
        {
            _qcplot->axisRect()->insetLayout()->setInsetAlignment(0, static_cast<Qt::Alignment>(dataInt));
            _qcplot->replot();
        }
    }
}

void plotData::setPositionTracer(int iDataCurve, int iPoint)
{
    FUNC_ENTER << iDataCurve << iPoint << _listOfCurves.size();
    if ( iDataCurve >= _listOfCurves.size() ) return;

    FUNC_INFO << "step 2";
    static bool concatenateLast=false;
    if ( iDataCurve == _iCurvePosition && iPoint == _iPointPosition && concatenateLast == _concatenateRuns )
        return;
    FUNC_INFO << "step 3";
    concatenateLast = _concatenateRuns;
    double x = setXPosition(iDataCurve, iPoint);
    FUNC_INFO << 2 << x;
    if ( _iCurvePositionLast != _iCurvePosition ) _iCurvePositionLast = _iCurvePosition;
    if ( _iPointPositionLast != _iPointPosition ) _iPointPositionLast = _iPointPosition;
    _iCurvePosition = iDataCurve;
    _iPointPosition = iPoint;
    FUNC_INFO << iDataCurve << iPoint << x;
    setPositionTracer(iDataCurve,x);
    FUNC_EXIT;
}

void plotData::setPositionTracer(int iDataCurve, double xPosition)
{ // xPosition is actual x value on axis, with or without concatenation
    FUNC_ENTER << "***" << xPosition;
    _positionTracer->setGraphKey(xPosition);
    _positionTracer->setVisible(true);

    FUNC_INFO << "getDataCurveIndex" << getDataCurveIndex(iDataCurve);
    for ( int jCurve=0; jCurve<_listOfCurves.size(); jCurve++ )
    {
        _listOfCurves[jCurve].isCurrentCurve = (jCurve == getDataCurveIndex(iDataCurve));
//        FUNC_INFO << jCurve << "getDataCurveIndex?" << _listOfCurves[jCurve].isCurrentCurve;
    }

    plotDataAndFit(false);

    _positionTracer->updatePosition();
    QCPItemPosition *position = _positionTracer->position;
    QString message;
    message.sprintf("(scan,pt) = (%d , %d)    (x,y) = (%g , %g)",_iCurvePosition+1,_iPointPosition+1,
                    position->coords().x(),position->coords().y());
    FUNC_INFO << "position" << message;
    if ( _statusBar ) _statusBar->showMessage(message);
    FUNC_EXIT << _positionTracer->graphKey() << position->coords().x();

}

void plotData::plotMousePress(QMouseEvent *event)
{
    double x = _qcplot->xAxis->pixelToCoord(event->pos().x());
    // double y = _qcplot->yAxis->pixelToCoord(event->pos().y());
    if ( event->buttons() == Qt::LeftButton && _pressToMoveTracer )
    {
        if ( !_concatenateRuns )
        {
            // then don't identify the curve; keep the same one
            setPositionTracer(_iCurvePosition,x);
            int iPoint = whichTracerTimePoint(_iCurvePosition);
            setPositionTracer(_iCurvePosition,iPoint);  // store last time point info
            emit changedPointFromGraph(_iCurvePosition,iPoint);
        }
        else
        {
            // Need to indentify which curve and which time point
            int iCurve = whichConcatenatedTracerFile(x);
//            FUNC_INFO << "plotData::plotMousePress iCurve" << iCurve;
            if ( iCurve < 0 ) return;
            setPositionTracer(iCurve,x);
            int iPoint = whichTracerTimePoint(iCurve);
            setPositionTracer(iCurve,iPoint);   // store last time point info
            emit changedPointFromGraph(iCurve,iPoint);
        }
        _qcplot->replot();
    }
}

double plotData::setXPosition( int iCurve, int iPoint)
{   // This function assumes all curves for a file have the same x values
//    FUNC_INFO << "plotData::setXPosition" << iCurve << iPoint;
//    FUNC_INFO << "plotData::setXPosition index" << getDataCurveIndex(iCurve);
    double xValue = _listOfCurves[getDataCurveIndex(iCurve)].xData[iPoint];
//    FUNC_INFO << "plotData::setXPosition1";
    if ( iCurve >= 0 && _concatenateRuns )
    {
        for (int jFile=0; jFile<iCurve; jFile++)
        {
            int iDataCurve = getDataCurveIndex(jFile);
            int iLastTimeIndexLastFile = _listOfCurves[iDataCurve].xData.size() - 1;
            double deltaTime = 1.;
            if ( iLastTimeIndexLastFile != 0 )
                deltaTime = _listOfCurves[iDataCurve].xData[iLastTimeIndexLastFile] - _listOfCurves[iDataCurve].xData[iLastTimeIndexLastFile-1];
            double startTime = _listOfCurves[iDataCurve].xData[iLastTimeIndexLastFile];
            xValue += startTime + deltaTime;  // last run + 1 time step
        }
    }
//    FUNC_INFO << "plotData::setXPosition exit" << xValue;
    return xValue;
}

int plotData::getDataCurveIndex(int iDataCurve)
{ // data should be 1st curve in set with index "iDataCurve"
    int iCurve=0;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        if ( _listOfCurves[jCurve].iDataCurve == iDataCurve )
        {
            iCurve = jCurve;
            break;
        }
    }
    return iCurve;
}

int plotData::whichConcatenatedTracerFile( double x )
{
    double firstX, lastX;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
//        FUNC_INFO << "for curve" << jCurve << "iCurve=" << _listOfCurves[jCurve].iDataCurve << "nTime" << _listOfCurves[jCurve].xPlot.size();
        firstX = _listOfCurves[jCurve].xPlot[0];
        lastX  = _listOfCurves[jCurve].xPlot[_listOfCurves[jCurve].xPlot.size()-1];
        if ( firstX == lastX )
        {
            firstX -= 0.5;  lastX += 0.5;
        }
//        FUNC_INFO << "x, firstX, lastX" << x << firstX << lastX << _listOfCurves[jCurve].xPlot.size();
//        if ( _listOfCurves[jCurve].xPlot.size() == 1 || (x >= firstX && x<= lastX) )
        if ( x >= firstX && x<= lastX )
            return _listOfCurves[jCurve].iDataCurve;
    }
    return -1;
}

int plotData::whichTracerTimePoint(int iDataCurve )
{
    _positionTracer->updatePosition();
    double xTracer = _positionTracer->position->key();
    int iCurve = getDataCurveIndex(iDataCurve);
    for (int jT=0; jT<_listOfCurves[iDataCurve].xPlot.size(); jT++)
    {
        if ( xTracer == _listOfCurves[iCurve].xPlot[jT] )
            return jT;
    }
    qWarning() << "Programming error: time point not found for curve " << iDataCurve << " in function whichTracerTimePoint";
    exit(1);
}

void plotData::plotMouseMove(QMouseEvent *event)
{
    interpretMousePosition(event);
    // bool leftButtonMove = (event->type() == QEvent::MouseMove) && (event->buttons() == Qt::LeftButton);

    if ( _cursorOnPlot )
    {
        double x = _qcplot->xAxis->pixelToCoord(event->pos().x());
        double y = _qcplot->yAxis->pixelToCoord(event->pos().y());
//        double y2 = _qcplot->yAxis2->pixelToCoord(event->pos().y());
        QString message = QString("%1 , %2").arg(x).arg(y);
        _qcplot->setToolTip(message);
        if ( _statusBar )
            _statusBar->showMessage(message);
    }
    else
    {
        QCPItemPosition *position = _positionTracer->position;
        QString message;
        message.sprintf("(scan,pt) = (%d , %d)    (x,y) = (%g , %g)",_iCurvePosition+1,_iPointPosition+1,
                        position->coords().x(),position->coords().y());
        if ( _statusBar ) _statusBar->showMessage(message);
    }
}

void plotData::interpretMousePosition(QMouseEvent *event)
{
    qreal x = event->localPos().x();
    qreal y = event->localPos().y();
    _cursorOnPlot = false;

    if ( x >= _qcplot->axisRect()->left() && x <=_qcplot->axisRect()->rect().right() &&
         y >= _qcplot->axisRect()->top()  && y <=_qcplot->axisRect()->rect().bottom() )
        _cursorOnPlot = true;
}

double plotData::getMaxYAbs(int iGraph)
{
    plotCurve *curve = &(_listOfCurves[iGraph]);
    return getMaxYAbs(curve);
}
double plotData::getMaxYAbs(plotCurve *ptrCurve)
{
    double dMax = 0.;
    for ( int jPoint=0; jPoint<ptrCurve->yData.size(); jPoint++ )
    {
        if ( qAbs(ptrCurve->yData[jPoint]) > dMax ) dMax = qAbs(ptrCurve->yData[jPoint]);
    }
    return dMax;
}

void plotData::init()
{
    _listOfCurves.resize(0);
    setLegendOn(false);
    setLabelXAxis("time");
    setLabelYAxis("signal");
    _qcplot->xAxis2->setLabel("");
    _qcplot->yAxis2->setLabel("");
    _xAxis2Ratio = _yAxis2Ratio = 0.;
}
void plotData::conclude(int iCurrentFile, bool singleRun)
{
    FUNC_ENTER;
    // Find current curve as first instance of current file counter
    bool found=false;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        if ( _listOfCurves[jCurve].isCurrentCurve ) found=true;
        FUNC_INFO << "scale factor" << jCurve << _listOfCurves[jCurve].scaleFactorY << _yAxis2Ratio;
        if ( _listOfCurves[jCurve].scaleFactorX != 1. )
        {
            for (int jt=0; jt<_listOfCurves[jCurve].xData.size(); jt++)
                _listOfCurves[jCurve].xData[jt] *= _listOfCurves[jCurve].scaleFactorX;
        }
        if ( _listOfCurves[jCurve].scaleFactorY != 1. )
        {
            for (int jt=0; jt<_listOfCurves[jCurve].yData.size(); jt++)
                _listOfCurves[jCurve].yData[jt] *= _listOfCurves[jCurve].scaleFactorY;
            for (int jt=0; jt<_listOfCurves[jCurve].yError.size(); jt++)
                _listOfCurves[jCurve].yError[jt] *= _listOfCurves[jCurve].scaleFactorY;
        }
    }

    int iCurveCurrent=0;
    if ( !found )
    {
        for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
            _listOfCurves[jCurve].isCurrentCurve = false;
        for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
        {
            if ( _listOfCurves[jCurve].iDataCurve == iCurrentFile )
            {  // set this as the current curve with the tracer
                FUNC_INFO << "***setDefaultCurrentCurve" << jCurve;
                _listOfCurves[jCurve].isCurrentCurve = true;
                iCurveCurrent = _listOfCurves[jCurve].iDataCurve;
                break;
            }
        }
    }

    _nFiles = 0;
    FUNC_INFO << _listOfCurves.size() << iCurrentFile;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        FUNC_INFO << "visible" << _listOfCurves[jCurve].visible;
        if ( _listOfCurves[jCurve].isCurrentCurve )
            _listOfCurves[jCurve].visible = true;
        else
            _listOfCurves[jCurve].visible = !singleRun || (_listOfCurves[jCurve].iDataCurve == iCurveCurrent);

        if ( _listOfCurves[jCurve].iDataCurve+1 > _nFiles ) _nFiles = _listOfCurves[jCurve].iDataCurve+1;

        // Set the color for iCurve=0 only (generally data)
        FUNC_INFO << _listOfCurves[jCurve].legend << _listOfCurves[jCurve].isCurrentCurve << _listOfCurves[jCurve].iDataCurve;
    }
    FUNC_INFO << "nFiles" << _nFiles;

    // Find the number entries per file, assuming the 1st set is representative
    _nTimePerFile.resize(_nFiles);
    _nCurvesPerFile=0;
    int iCurve=0;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        if ( _listOfCurves[jCurve].iDataCurve == _listOfCurves[0].iDataCurve ) // presumably = 0
            _nCurvesPerFile++;
        if ( jCurve == getDataCurveIndex(_listOfCurves[jCurve].iDataCurve) )
        {
            _nTimePerFile[iCurve] = _listOfCurves[jCurve].yData.size();
            FUNC_INFO << "nTimePerFile" << iCurve << _nTimePerFile[iCurve];
            iCurve++;
        }
    }

    // concatenate runs (or not)
    concatenateRuns();
    FUNC_EXIT;
}

void plotData::concatenateRuns()
{
    FUNC_ENTER << _listOfCurves.size();
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        int iCurve = _listOfCurves[jCurve].iDataCurve;
        if ( iCurve == 0 || !_concatenateRuns ) // for the 1st file, use the original xData (no offset due to concatenation)
            _listOfCurves[jCurve].xPlot = _listOfCurves[jCurve].xData;
        else
        {
            int iDataLast = getDataCurveIndex(iCurve-1);
            FUNC_INFO << "jCurve iCurve iDataLast" << jCurve << iCurve << iDataLast;
            int nTime = _listOfCurves[jCurve].xData.size();
            double offset;
            if ( nTime == 1 )
                offset = 0.;
            else
            {
                FUNC_INFO << "here 0.1" << nTime;
                double delta = _listOfCurves[iDataLast].xPlot[nTime-1] - _listOfCurves[iDataLast].xPlot[nTime-2];
                int iLast = _listOfCurves[iDataLast].xPlot.size()-1;
                offset = _listOfCurves[iDataLast].xPlot[iLast] + delta;
            }
            FUNC_INFO << "here 1";
            _listOfCurves[jCurve].xPlot.resize(nTime);
            for ( int jt=0; jt<nTime; jt++ )
                _listOfCurves[jCurve].xPlot[jt] = _listOfCurves[jCurve].xData[jt] + offset;
            FUNC_INFO << "here 2";
        }
        FUNC_INFO << "xplot size for" << _listOfCurves[jCurve].legend << " = " << _listOfCurves[jCurve].xPlot.size();
    }
    FUNC_EXIT << _listOfCurves.size();
}

void plotData::addCurve(int iDataCurve, QString fileName, QString legend)
{
    plotCurve newCurve;
    newCurve.fileName = fileName;
    newCurve.iDataCurve = iDataCurve;
    newCurve.lineThickness = 1;
    newCurve.pointSize = 0;
    newCurve.pointStyle = QCPScatterStyle::ssNone;
    newCurve.color = Qt::black;
    newCurve.visible = true;
    newCurve.enabled = true;
    newCurve.exportCurve = true;
    newCurve.isCurrentCurve = false;
    newCurve.histogram = false;
    newCurve.errorBars = false;
    newCurve.scaleFactorX = newCurve.scaleFactorY = 1.;
    _listOfCurves.append(newCurve);
    setLegend(legend);
}

void plotData::setScaleFactorYRelative(QString referenceLegend)
{
    int iCurveRef = -1;
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        if ( !referenceLegend.compare(_listOfCurves[jCurve].legend) )
            iCurveRef = jCurve;
    }
    if ( iCurveRef < 0 ) qFatal("Programming error in plotData::setScaleFactorYRelative");
    plotCurve refCurve = _listOfCurves[iCurveRef];
    plotCurve currentCurve = _listOfCurves.last();

    double maxRef = 0.;
    for (int j=0; j< refCurve.yData.size(); j++)
        maxRef = qMax(maxRef,refCurve.yData[j]);
    double maxThis = 0.;
    for (int j=0; j< currentCurve.yData.size(); j++)
        maxThis = qMax(maxThis,currentCurve.yData[j]);

    double scaleFactorY = 1.;
    if ( maxThis != 0. ) scaleFactorY = maxRef / maxThis;
    setScaleFactorY(scaleFactorY);

}

void plotData::setData(dVector xData, dVector yData)
{
    bVector ignoreRescale;
    ignoreRescale.fill(false,xData.size());
    dVector yError;
    yError.fill(0.,xData.size());
    setData(xData, yData, yError, ignoreRescale);
}
void plotData::setData(dVector xData, dVector yData, dVector yError)
{
    bVector ignoreRescale;
    ignoreRescale.fill(false,xData.size());
    setData(xData, yData, yError, ignoreRescale);
}
void plotData::setData(dVector xData, dVector yData, bVector ignoreRescale)
{
    dVector yError;
    yError.fill(0.,xData.size());
    setData(xData, yData, yError, ignoreRescale);
}
void plotData::setData(dVector xData, dVector yData, dVector yError, bVector ignoreRescale)
{
    _listOfCurves.last().xData         = xData;
    _listOfCurves.last().yData         = yData;
    _listOfCurves.last().yError        = yError;
    _listOfCurves.last().ignoreRescale = ignoreRescale;
}

void plotData::plotDataAndFit(bool newData)
{
    plotCurve currentCurve;
    QPen myPen;

    FUNC_ENTER << _listOfCurves.size();

    // Reset x position based upon _concatenateRuns
    /*
    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    {
        currentCurve = _listOfCurves[jCurve];
        for (int jTime=0; jTime<_listOfCurves[jCurve].xData.size(); jTime++)
            currentCurve.xPlot[jTime] = setXPosition(currentCurve.iDataCurve,jTime);
    }
    */

    // clear the graph (the following can return the number cleared)
    _qcplot->clearGraphs();
    QCPRange Range1 = _qcplot->yAxis->range();
    _qcplot->yAxis2->setRange(Range1);
    _qcplot->yAxis2->setTickLabels(false);

    // plot curves in reverse order so that the first curves (e.g., jCurve=0) are not occluded by later curves
//    for (int jCurve=0; jCurve<_listOfCurves.size(); jCurve++)
    for (int jCurve=_listOfCurves.size()-1; jCurve>=0; jCurve--)
    {
        currentCurve = _listOfCurves[jCurve];
        if ( currentCurve.visible && currentCurve.enabled )
        {
            // Fill color for error envelopes
            QColor fillColor;
            int hue; int saturation; int value;
            currentCurve.color.getHsv(&hue, &saturation, &value);
            fillColor.setHsv(hue,saturation/4,value);

            // Pen for lines & points
            if ( currentCurve.errorBars == 2 )
                myPen.setColor(fillColor);
            else
                myPen.setColor(currentCurve.color);
            myPen.setWidth(currentCurve.lineThickness);
            // Points
            QCPScatterStyle myScatter;
            myScatter.setShape(currentCurve.pointStyle);
            myScatter.setPen(myPen);
            myScatter.setBrush(currentCurve.color);
            myScatter.setSize(currentCurve.pointSize);

            _qcplot->addGraph();

            // set the curve name and color
            _qcplot->graph()->setName(currentCurve.legend);
            if (currentCurve.legend.compare("none") == 0  )
            {
                int nLegendItems = _qcplot->legend->itemCount();
                _qcplot->legend->removeItem(nLegendItems-1);
            }
            // Points
            _qcplot->graph()->setPen(myPen);
            _qcplot->graph()->setScatterStyle(myScatter);
            // lines
            if ( currentCurve.lineThickness == 0 )
                _qcplot->graph()->setLineStyle(QCPGraph::lsNone);
            if ( currentCurve.histogram )
                _qcplot->graph()->setLineStyle(QCPGraph::lsStepCenter);
            // Set the data
            FUNC_INFO << "set data" << currentCurve.legend << currentCurve.xPlot.size() << currentCurve.yData.size();
            _qcplot->graph()->setData(currentCurve.xPlot,currentCurve.yData);
            if ( _listOfCurves[jCurve].isCurrentCurve && currentCurve.yData.size() != 0 )
            {
                FUNC_INFO << "***setGraph" << jCurve;
                _positionTracer->setGraph(_qcplot->graph());  // set this when graphs are defined
            }
            //                if ( currentCurve.errorBars == 2 && jCurve != 0 )
            if ( currentCurve.errorBars == 1 )
            {
                // error bars
                QCPErrorBars *errorBars = new QCPErrorBars(_qcplot->xAxis, _qcplot->yAxis);
                errorBars->removeFromLegend();
                errorBars->setAntialiased(false);
                // errorBars->setDataPlottable(_qcplot->graph());
                int iGraph = _qcplot->graphCount() - 1; // iGraph-1 is  current graph
                errorBars->setDataPlottable(_qcplot->graph(iGraph));
                errorBars->setPen(myPen);
                // Set the data
                errorBars->setData(currentCurve.yError);
            }
            else if ( currentCurve.errorBars == 2 )
            {
                if ( _listOfCurves[jCurve+1].errorBars == 2 ) // jCurve+1 due to reverse order plotting; otherwise jCurve-1
                {
                    int iGraph = _qcplot->graphCount() - 2; // iGraph-1 is  current graph
                    _qcplot->graph()->setBrush(QBrush(QBrush(fillColor)));
                    _qcplot->graph()->setChannelFillGraph(_qcplot->graph(iGraph));
                }
            } // error bars
            if ( currentCurve.scaleFactorX != 1. )
            {
                FUNC_INFO << "scaleFactorX";
                _qcplot->xAxis2->setVisible(true);
                _qcplot->xAxis2->setTickLabels(true);
                _qcplot->xAxis2->setTickLabelColor(currentCurve.color);
                _qcplot->xAxis2->setTickLabelFont(QFont("Helvetica",16));
                Range1 = _qcplot->xAxis->range();
                FUNC_INFO << "Range1" << Range1;
                setXAxis2Range(Range1);
//                _qcplot->xAxis2->setLabel(currentCurve.legend);
                _qcplot->xAxis2->setLabelFont(QFont("Helvetica", 20));
                _qcplot->xAxis2->setLabelColor((currentCurve.color));
            }
            if ( currentCurve.scaleFactorY != 1. )
            {
                FUNC_INFO << "scaleFactorY";
                _qcplot->yAxis2->setVisible(true);
                _qcplot->yAxis2->setTickLabels(true);
                _qcplot->yAxis2->setTickLabelColor(currentCurve.color);
                _qcplot->yAxis2->setTickLabelFont(QFont("Helvetica",16));
                Range1 = _qcplot->yAxis->range();
                setYAxis2Range(Range1);
//                _qcplot->yAxis2->setLabel(currentCurve.legend);
                _qcplot->yAxis2->setLabelFont(QFont("Helvetica", 20));
                _qcplot->yAxis2->setLabelColor((currentCurve.color));
            }
        } // visible
    } //jcurve
    if ( _autoScale && newData )
        autoScale(true);
    else
        _qcplot->replot();

    FUNC_EXIT;
}

void plotData::autoScale(bool state)
{
//    FUNC_INFO << "plotData::autoScale" << state;
    _autoScale = state;
    if ( _autoScale )
    {
        QCPRange xRange, yRange;
        reScaleAxes(&xRange, &yRange);
//        FUNC_INFO << "plotData::autoScale yRange" << yRange.lower << yRange.upper;
        _qcplot->yAxis->setRange(yRange);
        setXAxis2Range(xRange);
        setYAxis2Range(yRange);
        if ( _autoScaleXRange.lower == _autoScaleXRange.upper )
            _qcplot->xAxis->setRange(xRange);
        else
            _qcplot->xAxis->setRange(_autoScaleXRange);
        _qcplot->replot();
        setSelectPoints(_pressToMoveTracer);
    }
}

void plotData::reScaleAxes(QCPRange *xRange, QCPRange *yRange)
{ // don't use the QCustomPlot function rescaleAxes (_qcplot->rescaleAxes()) so we can flag ignored points in the re-scaling
    xRange->lower=1.e10;  xRange->upper=-1.e10;
    yRange->lower=1.e10;  yRange->upper=-1.e10;
//    FUNC_INFO << "Data::reScaleAxes enter";

    for ( int jGraph=0; jGraph<_listOfCurves.size(); jGraph++ )
    {
//        FUNC_INFO << "Data::reScaleAxes jGraph" << jGraph;
        if ( _listOfCurves[jGraph].visible && _listOfCurves[jGraph].enabled )
        {
//            FUNC_INFO << "Data::reScaleAxes 2";
            int nPoints = _listOfCurves[jGraph].yData.size();
            for ( int jPoint=0; jPoint<nPoints; jPoint++ )
            {
//                FUNC_INFO << "Data::reScaleAxes 3" << jPoint << _listOfCurves[jGraph].ignoreRescale[jPoint];
                bool errorBars = _listOfCurves[jGraph].errorBars == 1; // for errorBars=2 (envelop, data are passed as two line curves)
                if ( !_listOfCurves[jGraph].ignoreRescale[jPoint] )
                {  // Ignore the y value of "ignored" points
//                    FUNC_INFO << "Data::reScaleAxes 4";
                    double yMin, yMax;
                    if ( errorBars )
                    {
                        yMin = _listOfCurves[jGraph].yData[jPoint] - _listOfCurves[jGraph].yError[jPoint];
                        yMax = _listOfCurves[jGraph].yData[jPoint] + _listOfCurves[jGraph].yError[jPoint];
                    }
                    else
                        yMin = yMax = _listOfCurves[jGraph].yData[jPoint];
//                    FUNC_INFO << "graph" << jGraph << "point" << jPoint << "yMin, yMax" << yMin << yMax;
                    if ( yMin < yRange->lower ) yRange->lower = yMin;
                    if ( yMax > yRange->upper ) yRange->upper = yMax;
                }
                // Do not ignore the x value of "ignored" points.
                if ( _listOfCurves[jGraph].xPlot[jPoint] < xRange->lower ) xRange->lower = _listOfCurves[jGraph].xPlot[jPoint];
                if ( _listOfCurves[jGraph].xPlot[jPoint] > xRange->upper ) xRange->upper = _listOfCurves[jGraph].xPlot[jPoint];
            }
        }
    }
    if ( xRange->lower ==  1.e10 ) xRange->lower = -10.;
    if ( xRange->upper == -1.e10 ) xRange->upper =  10.;
    // expland the x and y ranges by 10% total
    double extra = (xRange->upper - xRange->lower) * 0.05;
    xRange->lower -= extra;
    xRange->upper += extra;

    if ( yRange->lower ==  1.e10 ) yRange->lower = -10.;
    if ( yRange->upper == -1.e10 ) yRange->upper =  10.;
    extra = (yRange->upper - yRange->lower) * 0.05;
//    FUNC_INFO << "rescale" << yRange->lower << yRange->upper << extra;
    yRange->lower -= extra;
    yRange->upper += extra;
//    FUNC_INFO << "Data::reScaleAxes exit";
}
