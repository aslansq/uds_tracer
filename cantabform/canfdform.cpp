#include <QFileDialog>
#include "canfdform.h"
#include "ui_canfdform.h"

CanFdForm::CanFdForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::CanFdForm)
{
	ui->setupUi(this);

	this->validatorPtr = new QIntValidator(this);

	ui->devLineEdit->setPlaceholderText(this->config.getDev());
	ui->clkFreqLineEdit->setPlaceholderText(this->config.getClkFreq());
	ui->arbitBaudLineEdit->setPlaceholderText(this->config.getArbitBaud());
	ui->dataBaudLineEdit->setPlaceholderText(this->config.getDataBaud());
	ui->arbitPreLineEdit->setPlaceholderText(this->config.getArbitPre());
	ui->dataPreLineEdit->setPlaceholderText(this->config.getDataPre());
	ui->arbitTseg1LineEdit->setPlaceholderText(this->config.getArbitTseg1());
	ui->dataTseg1LineEdit->setPlaceholderText(this->config.getDataTseg1());
	ui->arbitTseg2LineEdit->setPlaceholderText(this->config.getArbitTseg2());
	ui->dataTseg2LineEdit->setPlaceholderText(this->config.getDataTseg2());
	ui->arbitSjwLineEdit->setPlaceholderText(this->config.getArbitSjw());
	ui->dataSjwLineEdit->setPlaceholderText(this->config.getDataSjw());

#ifdef Q_OS_WIN32
	ui->devLineEdit->setValidator(this->validatorPtr);
	ui->devPushButton->setVisible(false);
#endif
	ui->clkFreqLineEdit->setValidator(this->validatorPtr);
	ui->arbitBaudLineEdit->setValidator(this->validatorPtr);
	ui->dataBaudLineEdit->setValidator(this->validatorPtr);
	ui->arbitPreLineEdit->setValidator(this->validatorPtr);
	ui->dataPreLineEdit->setValidator(this->validatorPtr);
	ui->arbitTseg1LineEdit->setValidator(this->validatorPtr);
	ui->dataTseg1LineEdit->setValidator(this->validatorPtr);
	ui->arbitTseg2LineEdit->setValidator(this->validatorPtr);
	ui->dataTseg2LineEdit->setValidator(this->validatorPtr);
	ui->arbitSjwLineEdit->setValidator(this->validatorPtr);
	ui->dataSjwLineEdit->setValidator(this->validatorPtr);
}

CanFdForm::~CanFdForm()
{
	delete ui;
	delete this->validatorPtr;
}

const ConfigFd &CanFdForm::getConfig(void)
{
	return this->config;
}

void CanFdForm::onConfigLoaded(const ConfigFd &configFdRef)
{
	this->config.setMap(configFdRef.getMap());

	ui->devLineEdit->setText(this->config.getDev());
	ui->clkFreqLineEdit->setText(this->config.getClkFreq());
	ui->arbitBaudLineEdit->setText(this->config.getArbitBaud());
	ui->dataBaudLineEdit->setText(this->config.getDataBaud());
	ui->arbitPreLineEdit->setText(this->config.getArbitPre());
	ui->dataPreLineEdit->setText(this->config.getDataPre());
	ui->arbitTseg1LineEdit->setText(this->config.getArbitTseg1());
	ui->dataTseg1LineEdit->setText(this->config.getDataTseg1());
	ui->arbitTseg2LineEdit->setText(this->config.getArbitTseg2());
	ui->dataTseg2LineEdit->setText(this->config.getDataTseg2());
	ui->arbitSjwLineEdit->setText(this->config.getArbitSjw());
	ui->dataSjwLineEdit->setText(this->config.getDataSjw());
}

void CanFdForm::on_devLineEdit_editingFinished()
{
	this->config.setDev(ui->devLineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_arbitBaudLineEdit_editingFinished()
{
	this->config.setArbitBaud(ui->arbitBaudLineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_dataBaudLineEdit_editingFinished()
{
	this->config.setDataBaud(ui->dataBaudLineEdit->text());
	emit cfgChanged(this->config);
}

void CanFdForm::on_arbitTseg1LineEdit_editingFinished()
{
	this->config.setArbitTseg1(ui->arbitTseg1LineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_dataTseg1LineEdit_editingFinished()
{
	this->config.setDataTseg1(ui->dataTseg1LineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_arbitTseg2LineEdit_editingFinished()
{
	this->config.setArbitTseg2(ui->arbitTseg2LineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_dataTseg2LineEdit_editingFinished()
{
	this->config.setDataTseg2(ui->dataTseg2LineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_arbitSjwLineEdit_editingFinished()
{
	this->config.setArbitSjw(ui->arbitSjwLineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_dataSjwLineEdit_editingFinished()
{
	this->config.setDataSjw(ui->dataSjwLineEdit->text());
	emit cfgChanged(this->config);
}


void CanFdForm::on_devPushButton_clicked()
{
	QString devFilePath = QFileDialog::getOpenFileName(this, "Select CAN device", "/dev", "All files (*)");
	if (!devFilePath.isEmpty())
	{
		this->config.setDev(devFilePath);
		ui->devLineEdit->setText(devFilePath);
		emit cfgChanged(this->config);
	}
}


void CanFdForm::on_clkFreqLineEdit_editingFinished()
{
	this->config.setClkFreq(ui->clkFreqLineEdit->text());
	emit cfgChanged(this->config);
}

void CanFdForm::on_arbitPreLineEdit_editingFinished()
{
	this->config.setArbitPre(ui->arbitPreLineEdit->text());
	emit cfgChanged(this->config);
}

void CanFdForm::on_dataPreLineEdit_editingFinished()
{
	this->config.setDataPre(ui->dataPreLineEdit->text());
	emit cfgChanged(this->config);
}

