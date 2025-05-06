#include "cantabform.h"
#include "ui_cantabform.h"
#include <QDebug>

CanTabForm::CanTabForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::CanTabForm)
{
	ui->setupUi(this);

	this->checkButtonGroupPtr = new QButtonGroup(this);
	this->checkButtonGroupPtr->addButton(ui->stdCheckBox);
	this->checkButtonGroupPtr->addButton(ui->fdCheckBox);
	this->checkButtonGroupPtr->addButton(ui->replayCheckBox);
	this->checkButtonGroupPtr->setExclusive(true);

	this->canStdFormPtr = new CanStdForm(this);
	this->canFdFormPtr = new CanFdForm(this);
	this->canReplayFormPtr = new CanReplayForm(this);

	ui->mainVLayout->addWidget(this->canStdFormPtr);
	ui->mainVLayout->addWidget(this->canFdFormPtr);
	ui->mainVLayout->addWidget(this->canReplayFormPtr);

	connect(this->canFdFormPtr, &CanFdForm::cfgChanged, this, &CanTabForm::onFdCfgChanged);
	connect(this->canStdFormPtr, &CanStdForm::cfgChanged, this, &CanTabForm::onStdCfgChanged);
	connect(this->canReplayFormPtr, &CanReplayForm::cfgChanged, this, &CanTabForm::onReplayCfgChanged);

	connect(this, &CanTabForm::configFdLoaded, this->canFdFormPtr, &CanFdForm::onConfigLoaded);
	connect(this, &CanTabForm::configStdLoaded, this->canStdFormPtr, &CanStdForm::onConfigLoaded);
	connect(this, &CanTabForm::configReplayLoaded, this->canReplayFormPtr, &CanReplayForm::onConfigLoaded);

	hideForms();
	ui->stdCheckBox->setCheckState(Qt::Checked);
}

CanTabForm::~CanTabForm()
{
	delete ui;
	delete this->checkButtonGroupPtr;
	delete this->canStdFormPtr;
	delete this->canFdFormPtr;
	delete this->canReplayFormPtr;
}

void CanTabForm::onConfigAllLoaded(const ConfigAll &cfgAllRef)
{
	emit configFdLoaded(cfgAllRef.fd);
	emit configStdLoaded(cfgAllRef.std);
	emit configReplayLoaded(cfgAllRef.replay);

	QString canTypeStr = cfgAllRef.generic.getCanType();

	if(CanType::Std == canTypeStr) {
		ui->stdCheckBox->setCheckState(Qt::Checked);
	} else if(CanType::Fd == canTypeStr) {
		ui->fdCheckBox->setCheckState(Qt::Checked);
	} else if(CanType::Replay == canTypeStr) {
		ui->replayCheckBox->setCheckState(Qt::Checked);
	}
}

const QMap<QString, QString> &CanTabForm::getCurrentConfig(void)
{
	return this->currentConfig;
}

void CanTabForm::onStdCfgChanged(const ConfigStd &config)
{
	this->currentConfig = config.getMap();
	emit cfgChanged(this->currentConfig);
}

void CanTabForm::onFdCfgChanged(const ConfigFd &config)
{
	this->currentConfig = config.getMap();
	emit cfgChanged(this->currentConfig);
}

void CanTabForm::onReplayCfgChanged(const ConfigReplay &config)
{
	this->currentConfig = config.getMap();
	emit cfgChanged(this->currentConfig);
}

void CanTabForm::hideForms(void)
{
	this->canStdFormPtr->setVisible(false);
	this->canFdFormPtr->setVisible(false);
	this->canReplayFormPtr->setVisible(false);
}

void CanTabForm::on_stdCheckBox_stateChanged(int arg1)
{
	if(arg1 == Qt::Checked) {
		this->currentConfig = this->canStdFormPtr->getConfig().getMap();
		hideForms();
		this->canStdFormPtr->setVisible(true);
		emit cfgChanged(this->currentConfig);
		emit cfgChanged({{"canType", "Std"}});
	}
}


void CanTabForm::on_fdCheckBox_stateChanged(int arg1)
{
	if(arg1 == Qt::Checked) {
		this->currentConfig = this->canFdFormPtr->getConfig().getMap();
		hideForms();
		this->canFdFormPtr->setVisible(true);
		emit cfgChanged(this->currentConfig);
		emit cfgChanged({{"canType", "Fd"}});
	}
}


void CanTabForm::on_replayCheckBox_stateChanged(int arg1)
{
	if(arg1 == Qt::Checked) {
		this->currentConfig = this->canReplayFormPtr->getConfig().getMap();
		hideForms();
		this->canReplayFormPtr->setVisible(true);
		emit cfgChanged(this->currentConfig);
		emit cfgChanged({{"canType", "Replay"}});
	}
}

