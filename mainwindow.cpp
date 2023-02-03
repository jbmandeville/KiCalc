#include <QtWidgets>
#include "mainwindow.h"
#include "generalglm.h"

void MainWindow::updateGraphs()
{
    FUNC_ENTER;

    for (int jLevel=1; jLevel<8; jLevel++)
        _fitR1_lesion = fitVariableTR(1,jLevel);
    for (int jLevel=1; jLevel<8; jLevel++)
        _fitR1_contra = fitVariableTR(2, jLevel);
    for (int jLevel=1; jLevel<8; jLevel++)
        _fitR1_sinus = fitVariableTR(3, jLevel);
    FUNC_INFO << "R1 values" << _R1Lesion << _R1Contra << _R1Sag;
    // columns name: x(0), lesion(1), contra(2), sag(3)
    _deltaR1_lesion = computeDeltaR1(1);
    _deltaR1_contra = computeDeltaR1(2);
    _deltaR1_sinus  = computeDeltaR1(3);

    // fit the lesion and contra side using GLM
    double offsetLesion, offsetContra;
    _fitLesion = fitKi(_deltaR1_lesion,_deltaR1_sinus,_kiLesion,offsetLesion);
    _fitContra = fitKi(_deltaR1_contra,_deltaR1_sinus,_kiContra,offsetContra);

    updateVariableTRPlot();
    updateGdPlot();
    updateDeltaR1Plot();
    updateKTransPlot();
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
    for (int jt=0; jt<nTime; jt++)
        DR1[jt] = (yCorrected[jt]-1.) * ( 1. + qExp(_temporaryR1 * protocolPars.TRGado) ) / _temporaryR1;
    return DR1;
}

void MainWindow::normalizeToFirstPoint(dVector &vector)
{
    double S0  = vector.at(0);
    for (int jt=0; jt<vector.size(); jt++)
        vector[jt] = (vector[jt])/S0;
}

dVector MainWindow::fitKi(dVector dR1Tissue, dVector dR1Sinus, double &Ki, double &offset)
{
    // DR1_tiss = Ki/(1-Hct) * int_R1 + vd/(1-Hct) * R1

    // dVector weights;
    // int lowCutoff=3;
    dVector integraldR1Sinus;
    for (int jt=0; jt<dR1Tissue.size(); jt++)
    {
        integraldR1Sinus.append(integrate(dR1Sinus,jt));
        /*
        if ( jt > lowCutoff )
            weights.append(1.);
        else
            weights.append(0.);
        */
    }
    GeneralGLM fitter;
    // fitter.setWeights(weights);
    fitter.init(dR1Tissue.size(),0);           // ntime, ncoeff
    fitter.setOLS();                           // set weights to 1
    fitter.addBasisFunction(dR1Sinus);         // basis function "0" --> offset
    fitter.addBasisFunction(integraldR1Sinus); // basis function "1" --> ki
    fitter.fitWLS(dR1Tissue,true);
    dVector fit = fitter.getFitAll();
    offset = fitter.getBeta(0) * (1.-protocolPars.Hct);
    Ki     = fitter.getBeta(1) * (1.-protocolPars.Hct);
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

dVector MainWindow::fitVariableTR(int iColumn, int iLevel)
{
    FUNC_ENTER << iColumn << iLevel;
    int nTime = _tableVTR.size();
    if ( iLevel == 1 )
    {
        _S0Iterate = 1.5 * _tableVTR[nTime-1][iColumn];  // a bit bigger than the long TR signal
        _R1Iterate = 0.35;
    }
    FUNC_INFO << " " << _S0Iterate << _R1Iterate;
    double S0Width = _S0Iterate / qPow(2,iLevel);  // 2^7>100 should be enough
    double R1Width = _R1Iterate / qPow(2,iLevel);
    FUNC_INFO << "widths" << S0Width << R1Width;

    double stepDivider = 5.;
    double S0Step  = S0Width / stepDivider;
    double S0Start = _S0Iterate-S0Width;
    double S0Stop  = _S0Iterate+S0Width;
    double R1Step  = R1Width / stepDivider;
    double R1Start = _R1Iterate-R1Width;
    double R1Stop  = _R1Iterate+R1Width;

    dVector fit;        fit.resize(_tableVTR.size());
    dVector fitBest;    fitBest=fit;
    dVector TR, signal;
    for (int jt=0; jt<nTime; jt++)
    {
        TR.append(_tableVTR[jt][0]/1000.);
        signal.append(_tableVTR[jt][iColumn]);
    }
    FUNC_INFO << "signal" << signal;
    FUNC_INFO << "TR" << TR;

    _S0Iterate = S0Start;
    _R1Iterate = R1Start;
    dVector bestPars = {_S0Iterate, _R1Iterate};
    double bestSOS = 1.e20;
    FUNC_INFO << "_S0Iterate S0Start S0Stop S0Step" << _S0Iterate << S0Start << S0Stop << S0Step;
    while (_S0Iterate <= S0Stop)
    {
        while (_R1Iterate <= R1Stop)
        {
            dVector pars = {_S0Iterate, _R1Iterate};
            double sos=0.;
            FUNC_INFO << "try pars" << pars;
            for ( int jt=0; jt<nTime; jt++ )
            {
                fit[jt] = _S0Iterate * ( 1. - qExp(-_R1Iterate * TR[jt]) );
                sos += SQR(signal[jt]-fit[jt]);
            }
            FUNC_INFO << "sos" << sos << "fit" << fit;
            if ( sos < bestSOS )
            {
                bestSOS = sos;
                bestPars = pars;
                fitBest  = fit;
            }
            _R1Iterate += R1Step;
        } // R1
        _R1Iterate = R1Start;
        _S0Iterate += S0Step;
        FUNC_INFO << "change _S0Iterate to" << _S0Iterate << "stop =" << S0Stop;
    } // S0

    _S0Iterate = bestPars.at(0);
    _R1Iterate = bestPars.at(1);
    FUNC_INFO << "iColumn" << iColumn << "bestPars" << bestPars;

    if ( iColumn == 1 )
        _R1Lesion = _R1Iterate;
    else if ( iColumn == 2 )
        _R1Contra = _R1Iterate;
    else
        _R1Sag = _R1Iterate;
    FUNC_EXIT << fitBest;
    return fitBest;
}

MainWindow::MainWindow()
{
    _centralWidget = new QWidget(this);
    this->setCentralWidget( _centralWidget );

    auto *mainLayout = new QVBoxLayout( _centralWidget );
    mainLayout->addLayout(createTopLayout());
    mainLayout->addWidget(createTopPlotDuo());
    mainLayout->addLayout(createMiddleLayout());
    mainLayout->addWidget(createBottomPlotDuo());
    mainLayout->addLayout(createBottomLayout());
    mainLayout->setStretch(0,1);
    mainLayout->setStretch(1,20);
    mainLayout->setStretch(2,1);
    mainLayout->setStretch(3,20);
    mainLayout->setStretch(4,1);
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
    _checkBoxLesion   = new QCheckBox("lesion");
    _checkBoxContra   = new QCheckBox("contralateral");
    _checkBoxSagittal = new QCheckBox("sag sinus");
    _checkBoxLesion->setChecked(true);
    _checkBoxContra->setChecked(true);
    _checkBoxSagittal->setChecked(true);
    connect(_checkBoxLesion,   SIGNAL(toggled(bool)), this, SLOT(updateGraphs()));
    connect(_checkBoxContra,   SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_checkBoxSagittal, SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));

    _radioRaw  = new QRadioButton("raw");
    _radioNorm = new QRadioButton("normalized");
    _radioNorm->setChecked(true);
    connect(_radioRaw,         SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));
    connect(_radioNorm,        SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));

    QFrame* separator = new QFrame();
    separator->setFrameShape(QFrame::VLine);
    separator->setLineWidth(3);
    separator->setFixedHeight(20);
    separator->setFrameShadow(QFrame::Raised);

    auto *regionLayout = new QHBoxLayout();
    regionLayout->addWidget(_checkBoxLesion);
    regionLayout->addWidget(_checkBoxContra);
    regionLayout->addWidget(_checkBoxSagittal);
    regionLayout->addWidget(separator);
    regionLayout->addWidget(_radioRaw);
    regionLayout->addWidget(_radioNorm);

    auto *regionBox = new QGroupBox("Choose curves & normalization");
    regionBox->setLayout(regionLayout);
    regionBox->setStyleSheet("background-color:lightYellow;");

    auto *topLayout = new QHBoxLayout();
    topLayout->addWidget(regionBox);

    return topLayout;
}

QHBoxLayout *MainWindow::createMiddleLayout()
{
    FUNC_ENTER;
    _T1LesionLabel = new QLabel("T1 lesion: ?");
    _T1ContraLabel = new QLabel("T1 contra: ?");
    _T1SagLabel    = new QLabel("T1 sinus : ?");
    auto *T1Layout = new QHBoxLayout();
    T1Layout->addWidget(_T1LesionLabel);
    T1Layout->addWidget(_T1ContraLabel);
    T1Layout->addWidget(_T1SagLabel);

//    auto *T1Box = new QGroupBox("T1 values from fit:");
    auto *T1Box = new QGroupBox();
    T1Box->setLayout(T1Layout);
    T1Box->setStyleSheet("background-color:lightYellow;");

    _includeCorrected = new QCheckBox("corrected S(t)");
    connect(_includeCorrected, SIGNAL(clicked(bool)), this, SLOT(updateGraphs()));

    auto *corrLayout = new QHBoxLayout();
    corrLayout->addWidget(_includeCorrected);
    auto *corrBox = new QGroupBox();
    corrBox->setLayout(corrLayout);
    corrBox->setStyleSheet("background-color:lightYellow;");

    auto *middleLayout = new QHBoxLayout();
    middleLayout->addWidget(T1Box);
    middleLayout->addWidget(corrBox);
    return middleLayout;
}

QHBoxLayout *MainWindow::createBottomLayout()
{
    FUNC_ENTER;
    _kTransLesionLabel  = new QLabel("lesion    : ?");
    _kTransContraLabel  = new QLabel("contra    : ?");
    auto *kTransLayout1 = new QHBoxLayout();
    kTransLayout1->addWidget(_kTransLesionLabel);
    kTransLayout1->addWidget(_kTransContraLabel);

    auto *kTransBox1 = new QGroupBox("Ki values (1/hr):");
    kTransBox1->setLayout(kTransLayout1);
    kTransBox1->setStyleSheet("background-color:lightCyan;");

    _kTransSlopeLesionLabel  = new QLabel("lesion KTrans    : ?");
    _kTransSlopeContraLabel  = new QLabel("contra KTrans    : ?");
    _kTransOffsetLesionLabel = new QLabel("lesion offset: ?");
    auto *kTransLayout2 = new QHBoxLayout();
    kTransLayout2->addWidget(_kTransSlopeLesionLabel);
    kTransLayout2->addWidget(_kTransSlopeContraLabel);
//    kTransLayout2->addWidget(_kTransOffsetLesionLabel);

    auto *kTransBox2 = new QGroupBox("values from linear fit:");
    kTransBox2->setLayout(kTransLayout2);
    kTransBox2->setStyleSheet("background-color:lightYellow;");

    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(kTransBox1);
    bottomLayout->addWidget(spacer);
    bottomLayout->addWidget(kTransBox2);

    return bottomLayout;
}

QWidget *MainWindow::createTopPlotDuo()
{
    _plotVariableTr = new plotData(0);
    _plotEchoes     = new plotData(1);

    auto *plotLayout = new QHBoxLayout();
    plotLayout->addWidget(_plotVariableTr->getPlotSurface());
    plotLayout->addWidget(_plotEchoes->getPlotSurface());

    auto *plotWidget = new QWidget();
    plotWidget->setLayout(plotLayout);
    QSize size; size.setWidth(900);  size.setHeight(300);
    plotWidget->setMinimumSize(size);

    return plotWidget;
}

QWidget *MainWindow::createBottomPlotDuo()
{
    _plotDeltaR1    = new plotData(2);
    _plotKTrans     = new plotData(3);

    auto *plotLayout = new QHBoxLayout();
    plotLayout->addWidget(_plotDeltaR1->getPlotSurface());
    plotLayout->addWidget(_plotKTrans->getPlotSurface());

    auto *plotWidget = new QWidget();
    plotWidget->setLayout(plotLayout);
    QSize size; size.setWidth(400);  size.setHeight(300);
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

void MainWindow::updateVariableTRPlot()
{
    dVector xFit;
    for (int jt=0; jt<_tableVTR.size(); jt++)
        xFit.append(_tableVTR[jt][0]);

    QString number;  number.setNum(1./_R1Lesion,'g',2);
    _T1LesionLabel->setText(QString("T1 lesion: %1").arg(number));
    number.setNum(1./_R1Contra,'g',2);
    _T1ContraLabel->setText(QString("T1 contra: %1").arg(number));
    number.setNum(1./_R1Sag,'g',2);
    _T1SagLabel->setText(QString("T1 sinus: %1").arg(number));

    // Variable-TR plot
    _plotVariableTr->init();
    _plotVariableTr->setLabelXAxis("TR (sec)");
    _plotVariableTr->setLabelYAxis("Signal");
    // columns name: x(0), lesion(1), contra(2), sag(3)
    if ( _checkBoxLesion->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 1);
        _plotVariableTr->setPointSize(5);
        _plotVariableTr->addCurve(0,"fit");
        _plotVariableTr->setData(xFit,_fitR1_lesion);
        _plotVariableTr->setColor(Qt::blue);  // lesion
    }
    if ( _checkBoxContra->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 2);
        _plotVariableTr->setPointSize(5);
        _plotVariableTr->addCurve(0,"fit");
        _plotVariableTr->setData(xFit,_fitR1_contra);
        _plotVariableTr->setColor(Qt::black);  // contra
    }
    if ( _checkBoxSagittal->isChecked() )
    {
        addCurveToPlot(_plotVariableTr, _columnNamesVTR, _tableVTR, 3);
        _plotVariableTr->setPointSize(5);
        _plotVariableTr->addCurve(0,"fit");
        _plotVariableTr->setData(xFit,_fitR1_sinus);
        _plotVariableTr->setColor(Qt::red);  // lesion
    }
    _plotVariableTr->conclude(0,true);
    _plotVariableTr->plotDataAndFit(true);
}

void MainWindow::updateGdPlot()
{
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

void MainWindow::updateDeltaR1Plot()
{
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
        QString number;
        number.setNum(_kiLesion*60.,'g',2);
        _kTransLesionLabel->setText(QString("lesion: %1").arg(number));
        _plotDeltaR1->addCurve(0,"fit");
        _plotDeltaR1->setData(xTime, _fitLesion);
        _plotDeltaR1->setColor(Qt::blue);  // lesion
        _plotDeltaR1->setLineThickness(2);
    }
    if ( _checkBoxContra->isChecked() )
    {
        _plotDeltaR1->addCurve(0,_columnNamesShortTE.at(1));
        _plotDeltaR1->setData(xTime, _deltaR1_contra);
        _plotDeltaR1->setColor(Qt::black);  // contra
        QString number;
        number.setNum(_kiContra*60.,'g',2);
        _kTransContraLabel->setText(QString("contra: %1").arg(number));
        _plotDeltaR1->addCurve(0,"fit");
        _plotDeltaR1->setData(xTime, _fitContra);
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

void MainWindow::updateKTransPlot()
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
        _kTransOffsetLesionLabel->setText(QString("lesion offset: %1").arg(poly.getBeta(0)));
        QString number;
        number.setNum(poly.getBeta(1)*60.,'g',2);
        _kTransSlopeLesionLabel->setText(QString("lesion: %1").arg(number));
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
        QString number;
        number.setNum(poly.getBeta(1)*60.,'g',2);
        _kTransSlopeContraLabel->setText(QString("contra: %1").arg(number));
    }

    _plotKTrans->conclude(0,true);
    _plotKTrans->plotDataAndFit(true);
}

