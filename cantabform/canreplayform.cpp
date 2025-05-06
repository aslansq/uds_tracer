#include <QFileDialog>
#include <QDir>
#include "canreplayform.h"
#include "ui_canreplayform.h"

CanReplayForm::CanReplayForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::CanReplayForm)
{
	ui->setupUi(this);
	ui->filePathLineEdit->setPlaceholderText(this->config.getDev());
}

CanReplayForm::~CanReplayForm()
{
	delete ui;
}

const ConfigReplay &CanReplayForm::getConfig(void)
{
	return this->config;
}

void CanReplayForm::onConfigLoaded(const ConfigReplay &configReplayRef)
{
	this->config.setMap(configReplayRef.getMap());

	ui->filePathLineEdit->setText(this->config.getDev());
}

void CanReplayForm::on_filePathLineEdit_editingFinished()
{
	this->config.setDev(ui->filePathLineEdit->text());
	emit cfgChanged(this->config);
}


void CanReplayForm::on_filePathPushButton_clicked()
{
	QString replayFilePath = QFileDialog::getOpenFileName(this, "Select CAN Log to Play", this->config.getDev(), "All files (*)");
	if (!replayFilePath.isEmpty())
	{
		this->config.setDev(replayFilePath);
		ui->filePathLineEdit->setText(replayFilePath);
		emit cfgChanged(this->config);
	}
}

