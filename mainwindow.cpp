#include <QtWidgets>
#include "mainwindow.h"
#include "generalglm.h"

MainWindow::MainWindow()
{
    _centralWidget = new QWidget(this);
    this->setCentralWidget( _centralWidget );

    auto *mainLayout = new QVBoxLayout( _centralWidget );
    mainLayout->addLayout(createTopLayout());
    mainLayout->addWidget(createPlotWidget());
    mainLayout->addLayout(createBottomLayout());
    mainLayout->setStretch(0,1);
    mainLayout->setStretch(1,20);
    mainLayout->setStretch(0,2);

    // add a status bar
    _statusBar = this->statusBar();
    _statusBar->setStyleSheet("color:Darkred");
    mainLayout->addWidget(_statusBar);

    // add a menu
    auto *menuBar = new QMenuBar;
    mainLayout->setMenuBar(menuBar);
    QMenu *mainMenu  = new QMenu(tr("&Menu"), this);
    auto *helpMenu  = new QMenu(tr("Help"), this);
    menuBar->addMenu(mainMenu);
    menuBar->addMenu(helpMenu);

    QAction *quitAction = mainMenu->addAction(tr("&Quit"));
    // short-cuts and tooltips
    quitAction->setShortcut(Qt::ControlModifier + Qt::Key_Q);
    connect(quitAction, &QAction::triggered, this, &MainWindow::exitApp);
}

QHBoxLayout *MainWindow::createTopLayout()
{
    _dataSetLabel = new QLabel("dataset name");
    auto *dataSetLayout = new QHBoxLayout();
    dataSetLayout->addWidget(_dataSetLabel);
    auto *dataSetBox = new QGroupBox("DataSet");
    dataSetBox->setLayout(dataSetLayout);
    dataSetBox->setStyleSheet("background-color:lightYellow;");

    _checkBoxLesion   = new QCheckBox("lesion");
    _checkBoxContra   = new QCheckBox("contralateral");
    _checkBoxSagittal = new QCheckBox("sag sinus");
    _checkBoxLesion->setChecked(true);
    _checkBoxContra->setChecked(true);
    _checkBoxSagittal->setChecked(true);

    _radioRaw  = new QRadioButton("raw");
    _radioNorm = new QRadioButton("normalized");
    _includeCorrected = new QCheckBox("corrected S(t)");
    _radioNorm->setChecked(true);

    auto *radioLayout = new QGridLayout();
    radioLayout->addWidget(_checkBoxLesion,0,0);
    radioLayout->addWidget(_checkBoxContra,0,1);
    radioLayout->addWidget(_checkBoxSagittal,0,2);
    radioLayout->addWidget(_radioRaw,1,0);
    radioLayout->addWidget(_radioNorm,1,1);
    radioLayout->addWidget(_includeCorrected,1,2);

    connect(_checkBoxLesion,   SIGNAL(toggled(bool)), this, SLOT(updateGraphs()));
    connect(_checkBoxContra,   SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_checkBoxSagittal, SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_radioRaw,         SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_radioNorm,        SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_includeCorrected, SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));

    auto *radioBox = new QGroupBox("Choose curves");
    radioBox->setLayout(radioLayout);
    radioBox->setStyleSheet("background-color:lightYellow;");

    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(dataSetBox);
    topLayout->addWidget(radioBox);

    return topLayout;
}

QHBoxLayout *MainWindow::createBottomLayout()
{
    _T1Lesion = new QLabel("lesion: ?");
    _T1Contra = new QLabel("contra: ?");
    _T1Sag    = new QLabel("sinus : ?");
    auto *T1Layout = new QVBoxLayout();
    T1Layout->addWidget(_T1Lesion);
    T1Layout->addWidget(_T1Contra);
    T1Layout->addWidget(_T1Sag);

    _kTransSlopeLesion  = new QLabel("lesion Ki    : ?");
    _kTransSlopeContra  = new QLabel("contra Ki    : ?");
    _kTransOffsetLesion = new QLabel("lesion offset: ?");
    auto *kTransLayout = new QVBoxLayout();
    kTransLayout->addWidget(_kTransSlopeLesion);
    kTransLayout->addWidget(_kTransSlopeContra);
    kTransLayout->addWidget(_kTransOffsetLesion);

    auto *T1Box = new QGroupBox("T1 values from fit:");
    T1Box->setLayout(T1Layout);
    T1Box->setStyleSheet("background-color:lightBlue;");

    auto *kTransBox = new QGroupBox("values from fit:");
    kTransBox->setLayout(kTransLayout);
    kTransBox->setStyleSheet("background-color:lightBlue;");

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(T1Box);
    bottomLayout->addWidget(spacer);
    bottomLayout->addWidget(kTransBox);

    return bottomLayout;
}

QWidget *MainWindow::createPlotWidget()
{
    _plotVariableTr = new plotData(0);
    _plotEchoes     = new plotData(1);
    _plotDeltaR1    = new plotData(2);
    _plotKTrans     = new plotData(3);

    auto *plotLayout = new QGridLayout();
    plotLayout->addWidget(_plotVariableTr->getPlotSurface(),0,0);
    plotLayout->addWidget(_plotEchoes->getPlotSurface(),0,1);
    plotLayout->addWidget(_plotDeltaR1->getPlotSurface(),1,0);
    plotLayout->addWidget(_plotKTrans->getPlotSurface(),1,1);

    auto *plotWidget = new QWidget();
    plotWidget->setLayout(plotLayout);
    QSize size; size.setWidth(900);  size.setHeight(600);
    plotWidget->setMinimumSize(size);

    return plotWidget;
}

void MainWindow::exitApp()
{
//    writeQSettings();
    QCoreApplication::exit(0);
}

void MainWindow::readCommandLine()
{
    QStringList commandLine = QCoreApplication::arguments();

    switch (parseCommandLine(commandLine))
    {
    case CommandLineOk:
        break;
    case CommandLineError:
        exit(1);
    case CommandLineHelpRequested:
        QCoreApplication::exit(0);
    }
    readDataFiles();
    updateGraphs();
}

CommandLineParseResult MainWindow::parseCommandLine(QStringList commandLine)
{
    FUNC_ENTER << commandLine;
    QCommandLineParser parser;
    QString HelpText = "\nDisplay images (usually 4D) and analyze time series.\n";
    HelpText.append("Syntax:  fastmap [file1] [file2] [...]\n\n");
    HelpText.append("where\n\n");
    HelpText.append("Files are NIFTI or JIP format and optional arguments include:");
    parser.setApplicationDescription(HelpText);

    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    const QCommandLineOption batchOption({"b","batch"}, "Suppress GUI");
    parser.addOption(batchOption);  // should add auto-calculate and auto-quit by default

    parser.addPositionalArgument("[file1] [file2] ...", "A series of files for a time series.");

    const QCommandLineOption helpOption = parser.addHelpOption();

    int numberArguments = commandLine.count();
    if ( numberArguments < 4 )   // executable [VTR file] [shortTE file] [longTE file]
            return CommandLineError;
    else
    {
        if (!parser.parse(commandLine))
        {
            QString outputString = reformatStartupHelpText(parser.helpText());
            QMessageBox::warning(nullptr, QGuiApplication::applicationDisplayName(),
                                 "<html><head/><body><h2>" + parser.errorText() + "</h2><pre>"
                                             + outputString + "</pre></body></html>");
            return CommandLineError;
        }
    }

    if (parser.isSet(helpOption))
    {
        QMessageBox::warning(nullptr, QGuiApplication::applicationDisplayName(),
                             "<html><head/><body><pre>"
                             + parser.helpText() + "</pre></body></html>");
        return CommandLineHelpRequested;
    }

    if ( parser.isSet(batchOption) )
        _inputOptions.batchMode = true;

    // positional arguments: [variable TR table] [gado short TE] [gado long TE]
    if ( parser.positionalArguments().size() < 2 ) return CommandLineError;
    _inputOptions.variableTRFileName  = parser.positionalArguments().at(0);
    _inputOptions.gadoShortTEFileName = parser.positionalArguments().at(1);
    _inputOptions.gadoLongTEFileName  = parser.positionalArguments().at(2);

    return CommandLineOk;
}


QString MainWindow::reformatStartupHelpText(QString inputText)
{
    FUNC_ENTER << inputText;
    QString outputString;
    QRegExp rx("[\r\n]");// match a comma or a space
    QStringList inputList = inputText.split(rx, QString::SkipEmptyParts);
    outputString = inputList.at(0);
    for (int jList=1; jList<inputList.size(); jList++)
    {
        QString newLine = inputList.at(jList);
        QRegExp rx("[,\\s]");// match a comma or a space
        QStringList lineList = newLine.split(rx, QString::SkipEmptyParts);
        QChar firstChar = lineList.at(0)[0];
        if ( firstChar == '-' )
            outputString = outputString + "\n" + newLine; // terminate last line; add new line
        else
        {
            for (int jLineList=0; jLineList<lineList.size(); jLineList++)
                outputString = outputString + " " + lineList.at(jLineList);
        }
    }
    outputString = outputString + "\n"; // terminate last line
    return outputString;
}

void MainWindow::readDataFiles()
{
    QString errorString = utilIO::readTimeTableFile(_inputOptions.variableTRFileName, _columnNamesVTR, _tableVTR);
    if ( !errorString.isEmpty() )
    {
        qInfo() << errorString;
        exit(1);
    }
    FUNC_INFO << "VTR columns" << _columnNamesVTR;
    FUNC_INFO << "nTime" << _tableVTR.size();

    errorString = utilIO::readTimeTableFile(_inputOptions.gadoShortTEFileName, _columnNamesShortTE, _tableShortTE);
    if ( !errorString.isEmpty() )
    {
        qInfo() << errorString;
        exit(1);
    }
    FUNC_INFO << "shortTE columns" << _columnNamesShortTE;
    FUNC_INFO << "nTime" << _tableShortTE.size();

    errorString = utilIO::readTimeTableFile(_inputOptions.gadoLongTEFileName, _columnNamesLongTE, _tableLongTE);
    if ( !errorString.isEmpty() )
    {
        qInfo() << errorString;
        exit(1);
    }
    FUNC_INFO << "longTE columns" << _columnNamesLongTE;
    FUNC_INFO << "nTime" << _tableLongTE.size();
}

void MainWindow::updateGraphs()
{
    FUNC_ENTER << _checkBoxLesion->isChecked() << _checkBoxContra->isChecked() << _checkBoxSagittal->isChecked();

//    fitVariableTR();
    // Variable-TR plot
    _plotVariableTr->init();
    _plotVariableTr->setLabelXAxis("TR (sec)");
    _plotVariableTr->setLabelYAxis("Signal");
    // columns name: x(0), lesion(1), contra(2), sag(3)
    if ( _checkBoxLesion->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 1);
        _plotVariableTr->setPointSize(5);
    }
    if ( _checkBoxContra->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 2);
        _plotVariableTr->setPointSize(5);
    }
    if ( _checkBoxSagittal->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 3);
        _plotVariableTr->setPointSize(5);
    }
    _plotVariableTr->conclude(0,true);
    _plotVariableTr->plotDataAndFit(true);

    // columns: x(0), lesion(1), contra(2), sag(3)
    // Gd-time series
    // columns name: x(0), lesion(1), contra(2), sag(3)
    _plotEchoes->init();
    _plotEchoes->setLabelXAxis("time");
    _plotEchoes->setLabelYAxis("Signal (Gd)");
    if ( _checkBoxLesion->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _columnNamesShortTE, _tableShortTE, 1);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _columnNamesLongTE,  _tableLongTE,  1);
        if ( _includeCorrected->isChecked() )
        {
            addCorrectedCurveToPlot(_columnNamesShortTE, 1);
            _plotEchoes->setPointSize(3);
        }
    }
    if ( _checkBoxContra->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _columnNamesShortTE, _tableShortTE, 2);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _columnNamesLongTE,  _tableLongTE,  2);
        if ( _includeCorrected->isChecked() )
        {
            addCorrectedCurveToPlot(_columnNamesShortTE, 2);
            _plotEchoes->setPointSize(3);
        }
    }
    if ( _checkBoxSagittal->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _columnNamesShortTE, _tableShortTE, 3);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _columnNamesLongTE,  _tableLongTE,  3);
        if ( _includeCorrected->isChecked() )
        {
            addCorrectedCurveToPlot(_columnNamesShortTE, 3);
            _plotEchoes->setPointSize(3);
        }
    }
    _plotEchoes->conclude(0,true);
    _plotEchoes->plotDataAndFit(true);

    // delta_R1 plot
    addDeltaR1Plot();

    // KTrans plot
    addKTransPlot();
}

void MainWindow::addCurveToPlot(plotData *plot, QStringList columnNames, dMatrix table, int iColumn)
{
    QString columnName = columnNames.at(iColumn);
    plot->addCurve(0,columnName);

    int nTime = table.size();
    dVector xTime, ySignal;
    for (int jt=0; jt<nTime; jt++)
    {
        xTime.append(table[jt][0]);
        ySignal.append(table[jt][iColumn]);
    }
    if ( _radioNorm->isChecked() ) normalizeToFirstPoint(ySignal);

    plot->setData(xTime, ySignal);
    if ( iColumn == 1 )
        plot->setColor(Qt::blue);  // lesion
    else if ( iColumn == 2 )
        plot->setColor(Qt::black); // contra
    else
        plot->setColor(Qt::red);   // sag sinus
}

void MainWindow::addCorrectedCurveToPlot(QStringList columnNames, int iColumn)
{
    plotData *plot;
    plot = _plotEchoes;

    QString columnName = columnNames.at(iColumn);
    plot->addCurve(0,columnName);

    int nTime = _tableShortTE.size();
    dVector xTime, yCorrected;
    double exponent = protocolPars.TE1 / (protocolPars.TE2 - protocolPars.TE1);
    for (int jt=0; jt<nTime; jt++)
    {
        xTime.append(_tableShortTE[jt][0]);
        double y = _tableShortTE[jt][iColumn] * qPow(_tableShortTE[jt][iColumn]/_tableLongTE[jt][iColumn],exponent);
        yCorrected.append(y);
    }

    if ( _radioNorm->isChecked() ) normalizeToFirstPoint(yCorrected);

    plot->setData(xTime, yCorrected);
    if ( iColumn == 1 )
        plot->setColor(Qt::blue);  // lesion
    else if ( iColumn == 2 )
        plot->setColor(Qt::black); // contra
    else
        plot->setColor(Qt::red);   // sag sinus
}

void MainWindow::addDeltaR1Plot()
{
    // columns name: x(0), lesion(1), contra(2), sag(3)
    _deltaR1_lesion = computeDeltaR1(1);
    _deltaR1_contra = computeDeltaR1(2);
    _deltaR1_sinus  = computeDeltaR1(3);
    dVector xTime;
    int nTime = _tableShortTE.size();
    for (int jt=0; jt<nTime; jt++)
        xTime.append(_tableShortTE[jt][0]);

    _plotDeltaR1->init();
    _plotDeltaR1->setLabelXAxis("time");
    _plotDeltaR1->setLabelYAxis("Delta_R1 (Gd)");
    if ( _checkBoxLesion->isChecked() )
    {
        _plotDeltaR1->addCurve(0,_columnNamesShortTE.at(1));
        _plotDeltaR1->setData(xTime, _deltaR1_lesion);
        _plotDeltaR1->setColor(Qt::blue);  // lesion
        // fit the lesion
        dVector fit = fitKi(_deltaR1_lesion,_deltaR1_sinus);
        _plotDeltaR1->addCurve(0,"fit");
        _plotDeltaR1->setData(xTime, fit);
        _plotDeltaR1->setColor(Qt::blue);  // lesion
        _plotDeltaR1->setLineThickness(2);
    }
    if ( _checkBoxContra->isChecked() )
    {
        _plotDeltaR1->addCurve(0,_columnNamesShortTE.at(1));
        _plotDeltaR1->setData(xTime, _deltaR1_contra);
        _plotDeltaR1->setColor(Qt::black);  // contra
        // fit the contra side
        dVector fit = fitKi(_deltaR1_contra,_deltaR1_sinus);
        _plotDeltaR1->addCurve(0,"fit");
        _plotDeltaR1->setData(xTime, fit);
        _plotDeltaR1->setColor(Qt::black);  // contra
        _plotDeltaR1->setLineThickness(2);
    }
    if ( _checkBoxSagittal->isChecked() )
    {
        _plotDeltaR1->addCurve(0,_columnNamesShortTE.at(1));
        _plotDeltaR1->setData(xTime, _deltaR1_sinus);
        _plotDeltaR1->setColor(Qt::red);  // sinus
    }
    _plotDeltaR1->conclude(0,true);
    _plotDeltaR1->plotDataAndFit(true);
}

void MainWindow::addKTransPlot()
{
    _plotKTrans->init();
    _plotKTrans->setLabelXAxis("time");
    _plotKTrans->setLabelYAxis("unitless");

    dVector xTime, yLesionUnitless, yContraUnitLess;
    int nTime = _tableShortTE.size();
    int lowCutoff=1;
    FUNC_INFO << "_deltaR1_sinus" << _deltaR1_sinus;
    for (int jt=0; jt<nTime; jt++)
    {
        if ( jt > lowCutoff )
        {
            xTime.append(integrate(_deltaR1_sinus,jt) / _deltaR1_sinus[jt]);
            yLesionUnitless.append((1.-protocolPars.Hct) * _deltaR1_lesion[jt] / _deltaR1_sinus[jt]);
            yContraUnitLess.append((1.-protocolPars.Hct) * _deltaR1_contra[jt] / _deltaR1_sinus[jt]);
            FUNC_INFO << "integrateR1_sinus[" << jt << "] =" << integrate(_deltaR1_sinus,jt);
        }
    }
    FUNC_INFO << "xTime" << xTime;
    FUNC_INFO << "yLesionUnitless" << yLesionUnitless;
    FUNC_INFO << "yContraUnitLess" << yContraUnitLess;

    if ( _checkBoxLesion->isChecked() )
    {
        _plotKTrans->addCurve(0,_columnNamesShortTE.at(1));
        _plotKTrans->setData(xTime, yLesionUnitless);
        _plotKTrans->setColor(Qt::blue);  // lesion
        _plotKTrans->setLineThickness(0);
        _plotKTrans->setPointSize(5);
        // fit
        SimplePolynomial poly;
        poly.define(2,xTime);
        poly.fitWLS(yLesionUnitless,true);
        dVector fit = poly.getFitAll();
        qInfo() << "lesion fit" << poly.getBeta(0) << poly.getBeta(1);
        _plotKTrans->addCurve(0,"fit");
        _plotKTrans->setData(xTime, fit);
        _plotKTrans->setColor(Qt::blue);  // lesion
        _plotKTrans->setLineThickness(2);
        _kTransOffsetLesion->setText(QString("lesion offset: %1").arg(poly.getBeta(0)));
        _kTransSlopeLesion->setText(QString("lesion Ki: %1").arg(poly.getBeta(1)));
    }

    if ( _checkBoxContra->isChecked() )
    {
        _plotKTrans->addCurve(0,_columnNamesShortTE.at(2));
        _plotKTrans->setData(xTime, yContraUnitLess);
        _plotKTrans->setColor(Qt::black);  // contra
        _plotKTrans->setLineThickness(0);
        _plotKTrans->setPointSize(5);
        // fit
        SimplePolynomial poly;
        poly.define(2,xTime);
        poly.fitWLS(yContraUnitLess,true);
        dVector fit = poly.getFitAll();
        qInfo() << "contra fit" << poly.getBeta(0) << poly.getBeta(1);
        _plotKTrans->addCurve(0,"fit");
        _plotKTrans->setData(xTime, fit);
        _plotKTrans->setColor(Qt::black);  // lesion
        _plotKTrans->setLineThickness(2);
        _kTransSlopeContra->setText(QString("contra Ki*1000: %1").arg(poly.getBeta(1)*1000.));
    }

    _plotKTrans->conclude(0,true);
    _plotKTrans->plotDataAndFit(true);
}

void MainWindow::normalizeToFirstPoint(dVector &vector)
{
    double S0  = vector.at(0);
    for (int jt=0; jt<vector.size(); jt++)
        vector[jt] = (vector[jt])/S0;
}

dVector MainWindow::computeDeltaR1(int iColumn)
{ // columns: x(0), lesion(1), contra(2), sag(3)
    int nTime = _tableShortTE.size();
    dVector xTime, yCorrected;
    double exponent = protocolPars.TE1 / (protocolPars.TE2 - protocolPars.TE1);
    for (int jt=0; jt<nTime; jt++)
    {
        xTime.append(_tableShortTE[jt][0]);
        double y = _tableShortTE[jt][iColumn] * qPow(_tableShortTE[jt][iColumn]/_tableLongTE[jt][iColumn],exponent);
        yCorrected.append(y);
    }
    normalizeToFirstPoint(yCorrected);
    // Convert from normalized signal to delta_R1
    dVector DR1; DR1.resize(yCorrected.size());
    FUNC_INFO << "correction" << _temporaryR1 << protocolPars.TRGado;
    FUNC_INFO << "correction" << ( 1. + qExp(_temporaryR1 * protocolPars.TRGado) ) / _temporaryR1;
    for (int jt=0; jt<nTime; jt++)
        DR1[jt] = (yCorrected[jt]-1.) * ( 1. + qExp(_temporaryR1 * protocolPars.TRGado) ) / _temporaryR1;
    return DR1;
}

dVector MainWindow::fitKi(dVector dR1Tissue, dVector dR1Sinus)
{
    // DR1_tiss = Ki/(1-Hct) * int_R1 + vd/(1-Hct) * R1
    dVector integraldR1Sinus;
    dVector weights;
    int lowCutoff=1;
    for (int jt=0; jt<dR1Tissue.size(); jt++)
    {
        integraldR1Sinus.append(integrate(dR1Tissue,jt));
        if ( jt > lowCutoff )
//            weights.append(SQR(dR1Tissue.at(jt)));
            weights.append(1.);
        else
            weights.append(0.);
    }
    FUNC_INFO << "integraldR1Sinus" << integraldR1Sinus;
    GeneralGLM fitter;
    fitter.init(dR1Tissue.size(),0);           // ntime, ncoeff
//    fitter.setOLS();                           // set weights to 1
    fitter.setWeights(weights);
    fitter.addBasisFunction(dR1Sinus);         // basis function "0" --> offset
    fitter.addBasisFunction(integraldR1Sinus); // basis function "1" --> ki
    fitter.fitWLS(dR1Tissue,true);
    dVector fit = fitter.getFitAll();
    double off = fitter.getBeta(0) * (1.-protocolPars.Hct);
    double ki  = fitter.getBeta(1) * (1.-protocolPars.Hct);
    qInfo() << "*** ki, off ***" << ki << off;
    return fit;
}

double MainWindow::integrate(dVector vector, int iTime)
{
    double dt = protocolPars.timeStepGd;
    double sum=0.;
    for (int jt=0; jt<=iTime; jt++)
        sum += vector.at(jt) * dt;
    return sum;
}

void MainWindow::fitVariableTR(int iColumn)
{
    // Signal model assuming 90 degree flip: S(TR) = S0 * (1 - exp(-TR*R1))
    double S0Guess = 1.5 * _tableVTR[iColumn].last();  // a bit bigger than the long TR signal
    double T1Guess = 2500.;
    double R1Guess = 1./T1Guess;

}
