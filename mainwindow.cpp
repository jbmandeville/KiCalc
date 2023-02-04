#include <QtWidgets>
#include "mainwindow.h"
#include "generalglm.h"

void MainWindow::calculateKi()
{
    FUNC_ENTER;

    double R1Lesion, R1Contra, R1Sinus;
    for (int jLevel=1; jLevel<8; jLevel++)
        R1Lesion = fitVariableTR(_lesionVTR,jLevel);
    for (int jLevel=1; jLevel<8; jLevel++)
        R1Contra = fitVariableTR(_contraVTR, jLevel);
    for (int jLevel=1; jLevel<8; jLevel++)
        R1Sinus = fitVariableTR(_sinusVTR, jLevel);

    FUNC_INFO << "R1 values" << R1Lesion << R1Contra << R1Sinus;
    // columns name: x(0), lesion(1), contra(2), sag(3)
    _lesionDR1.y = computeDeltaR1(_lesionGdShortTE, _lesionGdLongTE, R1Lesion);
    _contraDR1.y = computeDeltaR1(_contraGdShortTE, _contraGdLongTE, R1Contra);
    _sinusDR1.y  = computeDeltaR1(_sinusGdShortTE,  _sinusGdLongTE,  R1Sinus);

    // fit the lesion and contra side using GLM
    double kiLesion, kiContra;
    double offsetLesion, offsetContra;
    _lesionDR1.fit = fitKi(_lesionDR1,_sinusDR1,kiLesion,offsetLesion);
    _contraDR1.fit = fitKi(_contraDR1,_sinusDR1,kiContra,offsetContra);

    qInfo() << "Ki     values (1/min)" << kiLesion*60. << kiContra*60.;
    qInfo() << "offset values" << offsetLesion << offsetContra;
    if ( _inputOptions.batchMode )
        exit(0);
    else
    {
        // update R1 values in the GUI
        QString number;  number.setNum(1./R1Lesion,'g',2);
        _T1LesionLabel->setText(QString("T1 lesion: %1").arg(number));
        number.setNum(1./R1Contra,'g',2);
        _T1ContraLabel->setText(QString("T1 contra: %1").arg(number));
        number.setNum(1./R1Sinus,'g',2);
        _T1SagLabel->setText(QString("T1 sinus: %1").arg(number));

        // update Ki values in the GUI
        number.setNum(kiLesion*60.,'g',2);
        _kTransLesionLabel->setText(QString("lesion: %1").arg(number));
        number.setNum(kiContra*60.,'g',2);
        _kTransContraLabel->setText(QString("contra: %1").arg(number));

        updateGraphs();
        show();
    }
}

void MainWindow::updateGraphs()
{
    updateVariableTRPlot();
    updateGdPlot();
    updateDeltaR1Plot();
    updateKTransPlot();
}

dVector MainWindow::computeDeltaR1(GraphVector shortTE, GraphVector longTE, double R1)
{ // columns: x(0), lesion(1), contra(2), sag(3)
    int nTime = shortTE.y.size();
    dVector yCorrected;
    double exponent = protocolPars.TE1 / (protocolPars.TE2 - protocolPars.TE1);
    for (int jt=0; jt<nTime; jt++)
        yCorrected.append(shortTE.y[jt] * qPow(shortTE.y[jt]/longTE.y[jt],exponent));
    normalizeToFirstPoint(yCorrected);
    // Convert from normalized signal to delta_R1
    dVector DR1; DR1.resize(yCorrected.size());
    for (int jt=0; jt<nTime; jt++)
        DR1[jt] = (yCorrected[jt]-1.) * ( 1. + qExp(R1 * protocolPars.TRGado) ) / R1;
    return DR1;
}

void MainWindow::normalizeToFirstPoint(dVector &data)
{  // normalize input vector to 1st point
    double S0  = data.at(0);
    for (int jt=0; jt<data.size(); jt++)
        data[jt] = (data[jt])/S0;
}
void MainWindow::normalizeToFirstPoint(dVector data, dVector &fit)
{ // normalize fit vector to 1st point of data vector
    double S0  = data.at(0);
    for (int jt=0; jt<data.size(); jt++)
        fit[jt]  = (fit[jt])/S0;
}

dVector MainWindow::fitKi(GraphVector tissueVector, GraphVector sinusVector, double &Ki, double &offset)
{
    // DR1_tiss = Ki/(1-Hct) * int_R1 + vd/(1-Hct) * R1

    // dVector weights;
    // int lowCutoff=3;
    dVector integraldR1Sinus;
    for (int jt=0; jt<tissueVector.y.size(); jt++)
    {
        integraldR1Sinus.append(integrate(sinusVector.y,jt));
        /*
        if ( jt > lowCutoff )
            weights.append(1.);
        else
            weights.append(0.);
        */
    }
    GeneralGLM fitter;
    // fitter.setWeights(weights);
    fitter.init(tissueVector.y.size(),0);      // ntime, ncoeff
    fitter.setOLS();                           // set weights to 1
    fitter.addBasisFunction(sinusVector.y);    // basis function "0" --> offset
    fitter.addBasisFunction(integraldR1Sinus); // basis function "1" --> ki
    fitter.fitWLS(tissueVector.y,true);
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

double MainWindow::fitVariableTR(GraphVector &dataVector, int iLevel)
{
    FUNC_ENTER << iLevel;
    int nTime = dataVector.y.size();
    if ( iLevel == 1 )
    {
        _S0Iterate = 1.5 * dataVector.y[nTime-1];  // a bit bigger than the long TR signal
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

    dVector fit;        fit.resize(nTime);
    dVector fitBest;    fitBest=fit;

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
                double TR = dataVector.x[jt]/1000.;
//                FUNC_INFO << "TR[" << jt << "] =" << TR << "value" << _S0Iterate * ( 1. - qExp(-_R1Iterate * TR) );
                fit[jt] = _S0Iterate * ( 1. - qExp(-_R1Iterate * TR) );
                sos += SQR(dataVector.y[jt]-fit[jt]);
            }
            FUNC_INFO << "sos" << sos << "fit" << fit;
            if ( sos < bestSOS )
            {
                bestSOS = sos;
                bestPars = pars;
                dataVector.fit = fit;
            }
            _R1Iterate += R1Step;
        } // R1
        _R1Iterate = R1Start;
        _S0Iterate += S0Step;
        FUNC_INFO << "change _S0Iterate to" << _S0Iterate << "stop =" << S0Stop;
    } // S0

    _S0Iterate = bestPars.at(0);
    _R1Iterate = bestPars.at(1);
    FUNC_INFO << "bestPars" << bestPars;
    return _R1Iterate;
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
    reformatData();
    calculateKi();
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

void MainWindow::reformatData()
{
    // VTR table
    reformatDataVector(_tableVTR, _columnNamesVTR, 1, _lesionVTR);
    reformatDataVector(_tableVTR, _columnNamesVTR, 2, _contraVTR);
    reformatDataVector(_tableVTR, _columnNamesVTR, 3, _sinusVTR);
    // short-TE table
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 1, _lesionGdShortTE);
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 2, _contraGdShortTE);
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 3, _sinusGdShortTE);
    // long-TE table
    reformatDataVector(_tableLongTE, _columnNamesLongTE, 1, _lesionGdLongTE);
    reformatDataVector(_tableLongTE, _columnNamesLongTE, 2, _contraGdLongTE);
    reformatDataVector(_tableLongTE, _columnNamesLongTE, 3, _sinusGdLongTE);
    // DR1 vectors: copy short-TE data
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 1, _lesionDR1);
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 2, _contraDR1);
    reformatDataVector(_tableShortTE, _columnNamesShortTE, 3, _sinusDR1);
}

void MainWindow::reformatDataVector(dMatrix table, QStringList columnNames, int iColumn, GraphVector &vector)
{
    vector.name = columnNames.at(iColumn);
    vector.x    = table[0];
    vector.y    = table[iColumn];
    vector.fit.fill(0.,vector.y.size());
}

void MainWindow::updateVariableTRPlot()
{
    // Variable-TR plot
    _plotVariableTr->init();
    _plotVariableTr->setLabelXAxis("TR (sec)");
    _plotVariableTr->setLabelYAxis("Signal");
    // columns name: x(0), lesion(1), contra(2), sag(3)
    if ( _checkBoxLesion->isChecked() )
        addCurveToPlot(_plotVariableTr, _lesionVTR, 1, true, true);
    if ( _checkBoxContra->isChecked() )
        addCurveToPlot(_plotVariableTr, _contraVTR, 2, true, true);
    if ( _checkBoxSagittal->isChecked() )
        addCurveToPlot(_plotVariableTr, _sinusVTR, 3, true, true);
    _plotVariableTr->conclude(0,true);
    _plotVariableTr->plotDataAndFit(true);
}

void MainWindow::updateGdPlot()
{
    _plotEchoes->init();
    _plotEchoes->setLabelXAxis("time");
    _plotEchoes->setLabelYAxis("Signal (Gd)");
    if ( _checkBoxLesion->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _lesionGdShortTE, 1, false, true);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _lesionGdLongTE, 1, false, true);
        if ( _includeCorrected->isChecked() )
            addCorrectedCurveToPlot(_lesionGdShortTE, _lesionGdLongTE, 1);
    }
    if ( _checkBoxContra->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _contraGdShortTE, 2, false, true);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _contraGdLongTE, 2, false, true);
        if ( _includeCorrected->isChecked() )
            addCorrectedCurveToPlot(_contraGdShortTE, _contraGdLongTE, 2);
    }
    if ( _checkBoxSagittal->isChecked() )
    {
        addCurveToPlot(_plotEchoes, _sinusGdShortTE, 3, false, true);
        _plotEchoes->setLineThickness(2); // thick lines for short TE
        addCurveToPlot(_plotEchoes, _sinusGdLongTE, 3, false, true);
        if ( _includeCorrected->isChecked() )
            addCorrectedCurveToPlot(_sinusGdShortTE, _sinusGdLongTE, 3);
    }
    _plotEchoes->conclude(0,true);
    _plotEchoes->plotDataAndFit(true);
}

void MainWindow::addCurveToPlot(plotData *plot, GraphVector vector, int color, bool addFit, bool normalizeOK)
{
    // data
    plot->addCurve(0,vector.name);
    dVector yCurve = vector.y;
    if ( _radioNorm->isChecked() && normalizeOK )
        normalizeToFirstPoint(yCurve);
    plot->setData(vector.x, yCurve);
    if ( addFit )
    {
        plot->setLineThickness(0);
        plot->setPointSize(6);
    }
    else
        _plotVariableTr->setLineThickness(1);
    if ( color == 1 )
        plot->setColor(Qt::blue);  // lesion
    else if ( color == 2 )
        plot->setColor(Qt::black); // contra
    else
        plot->setColor(Qt::red);   // sag sinus

    // fit
    if ( addFit )
    {
        plot->addCurve(0,vector.name+"_fit");
        dVector yFit   = vector.fit;
        if ( _radioNorm->isChecked()  && normalizeOK )
            normalizeToFirstPoint(vector.y, yFit);
        plot->setData(vector.x, yFit);
        if ( color == 1 )
            plot->setColor(Qt::blue);  // lesion
        else if ( color == 2 )
            plot->setColor(Qt::black); // contra
        else
            plot->setColor(Qt::red);   // sag sinus
        plot->setLineThickness(2);
    }
}

void MainWindow::addCorrectedCurveToPlot(GraphVector shortTE, GraphVector longTE, int color)
{
    _plotEchoes->addCurve(0,shortTE.name);

    int nTime = shortTE.y.size();
    dVector yCorrected;
    double exponent = protocolPars.TE1 / (protocolPars.TE2 - protocolPars.TE1);
    for (int jt=0; jt<nTime; jt++)
        yCorrected.append(shortTE.y[jt] * qPow(shortTE.y[jt]/longTE.y[jt],exponent));
    if ( _radioNorm->isChecked() ) normalizeToFirstPoint(yCorrected);

    _plotEchoes->setData(shortTE.x, yCorrected);
    if ( color == 1 )
        _plotEchoes->setColor(Qt::blue);  // lesion
    else if ( color == 2 )
        _plotEchoes->setColor(Qt::black); // contra
    else
        _plotEchoes->setColor(Qt::red);   // sag sinus
    _plotEchoes->setPointSize(3);
}

void MainWindow::updateDeltaR1Plot()
{
    _plotDeltaR1->init();
    _plotDeltaR1->setLabelXAxis("time");
    _plotDeltaR1->setLabelYAxis("Delta_R1 (Gd)");
    if ( _checkBoxLesion->isChecked() )
        addCurveToPlot(_plotDeltaR1, _lesionDR1, 1, true, false);
    if ( _checkBoxContra->isChecked() )
        addCurveToPlot(_plotDeltaR1, _contraDR1, 2, true, false);
    if ( _checkBoxSagittal->isChecked() )
        addCurveToPlot(_plotDeltaR1, _sinusDR1, 3, false, false);
    _plotDeltaR1->conclude(0,true);
    _plotDeltaR1->plotDataAndFit(true);
}

void MainWindow::updateKTransPlot()
{
    _plotKTrans->init();
    _plotKTrans->setLabelXAxis("time");
    _plotKTrans->setLabelYAxis("unitless");

    dVector xTime, yLesionUnitless, yContraUnitLess;
    int nTime = _lesionDR1.y.size();
    int lowCutoff=1;
    FUNC_INFO << "_sinusDR1.y" << _sinusDR1.y;
    for (int jt=0; jt<nTime; jt++)
    {
        if ( jt > lowCutoff )
        {
            xTime.append(integrate(_sinusDR1.y,jt) / _sinusDR1.y[jt]);
            yLesionUnitless.append((1.-protocolPars.Hct) * _lesionDR1.y[jt] / _sinusDR1.y[jt]);
            yContraUnitLess.append((1.-protocolPars.Hct) * _contraDR1.y[jt] / _sinusDR1.y[jt]);
            FUNC_INFO << "integrateR1_sinus[" << jt << "] =" << integrate(_sinusDR1.y,jt);
        }
    }
    FUNC_INFO << "xTime" << xTime;
    FUNC_INFO << "yLesionUnitless" << yLesionUnitless;
    FUNC_INFO << "yContraUnitLess" << yContraUnitLess;

    if ( _checkBoxLesion->isChecked() )
    {
        _plotKTrans->addCurve(0,_lesionDR1.name);
        _plotKTrans->setData(xTime, yLesionUnitless);
        _plotKTrans->setColor(Qt::blue);  // lesion
        _plotKTrans->setLineThickness(0);
        _plotKTrans->setPointSize(6);
        // fit
        SimplePolynomial poly;
        poly.define(2,xTime);
        poly.fitWLS(yLesionUnitless,true);
        dVector fit = poly.getFitAll();
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
        _plotKTrans->addCurve(0,_contraDR1.name);
        _plotKTrans->setData(xTime, yContraUnitLess);
        _plotKTrans->setColor(Qt::black);  // contra
        _plotKTrans->setLineThickness(0);
        _plotKTrans->setPointSize(6);
        // fit
        SimplePolynomial poly;
        poly.define(2,xTime);
        poly.fitWLS(yContraUnitLess,true);
        dVector fit = poly.getFitAll();
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
