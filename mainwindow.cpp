#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include "cmddef.h"

MainWindow::MainWindow(QWidget *parent, Cli &cliRef)
	: QMainWindow(parent)
	, cliRef(cliRef)
	, ui(new Ui::MainWindow)
	, settings("MyCompany", "UdsTracer")
	, prjFilePath("")
{
	ui->setupUi(this);
	this->canTabFormPtr = new CanTabForm(this);
	this->tracerTabFormPtr = new TracerTabForm(this);

	ui->tabWidget->addTab(this->canTabFormPtr, "CAN");
	ui->tabWidget->addTab(this->tracerTabFormPtr, "Tracer");

	connect(&this->cliRef, &Cli::commandReceived, this->tracerTabFormPtr, &TracerTabForm::onCommandReceived);
	connect(this->canTabFormPtr, &CanTabForm::cfgChanged, &this->cliRef, &Cli::commandMap);
	connect(this->tracerTabFormPtr, &TracerTabForm::cfgChanged, &this->cliRef, &Cli::commandMap);
	connect(this->tracerTabFormPtr, &TracerTabForm::req, &this->cliRef, &Cli::commandMapWThrow);

	connect(&this->cliRef, &Cli::configAllLoaded, this->tracerTabFormPtr, &TracerTabForm::onConfigAllLoaded);
	connect(&this->cliRef, &Cli::configAllLoaded, this->canTabFormPtr, &CanTabForm::onConfigAllLoaded);
	connect(&this->cliRef, &Cli::canConnectionEvented, this->tracerTabFormPtr, &TracerTabForm::onCanConnectionEvented);
	connect(&this->cliRef, &Cli::canConnectionEvented, this, &MainWindow::onCanConnectionEvented);
	connect(&this->cliRef, &Cli::udsPacketReceived, this->tracerTabFormPtr, &TracerTabForm::onUdsPacketReceived);

	ui->tabWidget->setCurrentIndex(tracerTabIdx);

	connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpenAction);
	connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSaveAction);
	connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveAsAction);
	connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAboutAction);

	QString lastProjectPath = settings.value("lastProjectPath", "").toString();
	if ((!lastProjectPath.isEmpty()) && QFile::exists(lastProjectPath)) {
		this->prjFilePath = QFileInfo(lastProjectPath);
		this->cliRef.commandMapWThrow({{CmdDef::loadConfig.name, this->prjFilePath.absoluteFilePath()}});
	}
}

MainWindow::~MainWindow()
{
	delete ui;
	delete this->canTabFormPtr;
	delete this->tracerTabFormPtr;
}

void MainWindow::onCanConnectionEvented(bool isConnected)
{
	this->canTabFormPtr->setEnabled(!isConnected);
}

void MainWindow::onOpenAction()
{
	QString fileName = QFileDialog::getOpenFileName(this, "Open Project", QDir::homePath(), "XML Files (*.xml)");

	try {
		QFileInfo fileInfo(fileName);
		this->cliRef.commandMapWThrow({{CmdDef::loadConfig.name, fileInfo.absoluteFilePath()}});
		this->setPrjFilePath(fileName);
	} catch (const std::exception &e) {
		QMessageBox::critical(this, "Error", e.what());
	}
}

void MainWindow::onSaveAction()
{
	if(this->prjFilePath.filePath() == "") {
		onSaveAsAction();
	} else {
		this->cliRef.commandMapWThrow({{CmdDef::storeConfig.name, this->prjFilePath.absoluteFilePath()}});
	}
}

void MainWindow::onSaveAsAction()
{
	QString fileName = QFileDialog::getSaveFileName(this, "Save Project", QDir::homePath(), "XML Files (*.xml)");
	if(!fileName.endsWith(".xml", Qt::CaseInsensitive)) {
		fileName += ".xml";
	}
	if(!fileName.isEmpty()) {
		this->prjFilePath = QFileInfo(fileName);
		this->cliRef.commandMap({{CmdDef::storeConfig.name, this->prjFilePath.absoluteFilePath()}});
		this->setPrjFilePath(fileName);
	}
}

void MainWindow::onAboutAction()
{
	QString s = 
		"CAN UDS Tracer\n"
		"Cli Version: " + this->cliRef.version + "\n"
		"Gui Version: " + QCoreApplication::applicationVersion() + "\n"
		"Qt Version: " + QT_VERSION_STR + "\n";
	QMessageBox::about(this, "About", s);
}

void MainWindow::setPrjFilePath(const QString &filePathRef)
{
	this->prjFilePath = QFileInfo(filePathRef);
	this->settings.setValue("lastProjectPath", filePathRef);

}

