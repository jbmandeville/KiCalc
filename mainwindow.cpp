#include <QtWidgets>
#include "mainwindow.h"

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
    auto *radioLayout = new QHBoxLayout();
    radioLayout->addWidget(_checkBoxLesion);
    radioLayout->addWidget(_checkBoxContra);
    radioLayout->addWidget(_checkBoxSagittal);

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

    _kTransSlope  = new QLabel("slope (Ki): ?");
    _kTransOffset = new QLabel("offset    : ?");
    auto *kTransLayout = new QVBoxLayout();
    kTransLayout->addWidget(_kTransSlope);
    kTransLayout->addWidget(_kTransOffset);

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
    if ( numberArguments < 3 )
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

    // positional arguments: [variable TR table] [gado table]
    if ( parser.positionalArguments().size() < 2 ) return CommandLineError;
    _inputOptions.variableTRFileName = parser.positionalArguments().at(0);
    _inputOptions.gadoTableFileName  = parser.positionalArguments().at(1);

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
