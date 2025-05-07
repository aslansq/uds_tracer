#include <QIcon>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include "util.h"
#include "tracertabform.h"
#include "ui_tracertabform.h"

TraceLevelProp::TraceLevelProp() :
	backgroundColor({QColor("#2eafcf")}),
	alignment(Qt::AlignCenter),
	backgroundColorIndex(0)
{}

TraceLevelProp::TraceLevelProp(QVector<QColor> backgroundColor, int alignment) :
	backgroundColor(backgroundColor),
	alignment(alignment),
	backgroundColorIndex(0)
{
}

QColor TraceLevelProp::getBackgroundColor(void)
{
	nextBackgroundColorIndex();
	return backgroundColor[backgroundColorIndex];
}

int TraceLevelProp::getAlignment(void)
{
	return alignment;
}

void TraceLevelProp::nextBackgroundColorIndex(void)
{
	backgroundColorIndex++;
	if(backgroundColorIndex >= backgroundColor.size()) {
		backgroundColorIndex = 0;
	}
}

TracerTabForm::TracerTabForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::TracerTabForm),
	reqSucceeded(false),
	totalNumOfItem(0)
{
	ui->setupUi(this);
	this->hexValidatorPtr = new HexValidator(this);

	this->numOfItemToShow = ui->numOfItemsSpinBox->value();

	this->configTracer.setReqIdHex("7df");
	this->configTracer.setRespIdHex("7e0");
	this->configTracer.setLogDirPath(QDir::homePath());

	ui->logPathLineEdit->setPlaceholderText(this->configTracer.getLogDirPath());
	ui->reqIdLineEdit->setPlaceholderText(this->configTracer.getReqIdHex());
	ui->respIdLineEdit->setPlaceholderText(this->configTracer.getRespIdHex());

	ui->reqIdLineEdit->setValidator(this->hexValidatorPtr);
	ui->respIdLineEdit->setValidator(this->hexValidatorPtr);

	ui->connectPushButton->setIcon(QIcon::fromTheme("network-connect"));
	ui->clearPushButton->setIcon(QIcon::fromTheme("edit-clear"));

	traceLevelProp.clear();
	traceLevelProp.insert(TraceLevelEnum::Info, TraceLevelProp({QColor("#afcf2e"), QColor("#d2f44a")}, Qt::AlignCenter));
	traceLevelProp.insert(TraceLevelEnum::Req, TraceLevelProp({QColor("#fbffd5")}, Qt::AlignLeft));
	traceLevelProp.insert(TraceLevelEnum::Resp, TraceLevelProp({QColor("#cbfbff")}, Qt::AlignRight));
}

TracerTabForm::~TracerTabForm()
{
	delete ui;
	delete this->hexValidatorPtr;
}

void TracerTabForm::addTrace(TraceLevelEnum level, QString s)
{
	TraceUiData traceUiData = {
		.level = level,
		.s = s
	};
	this->traceUiDataVect.append(traceUiData);

	if(ui->tracerListWidget->count() > (int)this->numOfItemToShow) {
		ui->tracerListWidget->takeItem(0);
	}

	auto item = new QListWidgetItem(s);
	item->setTextAlignment(this->traceLevelProp[level].getAlignment());
	item->setBackground(this->traceLevelProp[level].getBackgroundColor());
	ui->tracerListWidget->addItem(item);
	ui->tracerListWidget->scrollToBottom();
	this->totalNumOfItem++;
	ui->totalItemCountLabel->setText(
		QString("%1").arg(this->totalNumOfItem)
	);
}

void TracerTabForm::onUdsPacketReceived(bool isReq, const QVector<UdsInfo> &packetInfoRef)
{
	if(isReq) {
		QString s = "";
		for(const UdsInfo &info : packetInfoRef) {
			for(uint32_t i = 0; i < (info.hexIdx * 3); ++i) {
				s += " ";
			}
			s += info.getHexStr(8) + "\n";
			if(info.name != "") {
				s += info.name + "\n";
			}
		}
		s = s.trimmed();
		addTrace(TraceLevelEnum::Req, s);
	} else {
		uint32_t packetHexStrLen = 0;
		QString s = "";
		if(packetInfoRef.length() != 0) {
			s = packetInfoRef[0].getHexStr(8).trimmed();
		}
		packetHexStrLen = s.length();

		for(int i = 1; i < packetInfoRef.length(); ++i) {
			QString hexStr = packetInfoRef[i].getHexStr(8).trimmed();
			QString paddedHexStr = hexStr;
			int paddingLen = packetHexStrLen - (packetInfoRef[i].hexIdx * 3) - hexStr.length();
			s += "\n";
			for(int j = 0; j < paddingLen; j++) {
				paddedHexStr += " ";
			}
			if (paddedHexStr.endsWith(" ")) {
				paddedHexStr.chop(1);
				paddedHexStr += ".";
			}
			s += paddedHexStr + "\n";
			s += packetInfoRef[i].name;
		}

		addTrace(TraceLevelEnum::Resp, s);
	}
}

void TracerTabForm::onCommandReceived(const QString &cmdStrRef)
{
	addTrace(TraceLevelEnum::Info, cmdStrRef);
}

void TracerTabForm::onConfigAllLoaded(const ConfigAll &cfgAllRef)
{
	this->configTracer.setMap(cfgAllRef.tracer.getMap());

	ui->reqIdLineEdit->setText(this->configTracer.getReqIdHex());
	ui->respIdLineEdit->setText(this->configTracer.getRespIdHex());
	ui->logPathLineEdit->setText(this->configTracer.getLogDirPath());
}

void TracerTabForm::onCanConnectionEvented(bool isConnected)
{
	bool isInputEn = !isConnected;
	ui->reqIdLineEdit->setEnabled(isInputEn);
	ui->respIdLineEdit->setEnabled(isInputEn);
	ui->logPathPushButton->setEnabled(isInputEn);
	ui->logPathLineEdit->setEnabled(isInputEn);
	ui->connectPushButton->setChecked(isConnected);
}

void TracerTabForm::on_reqIdLineEdit_editingFinished()
{
	this->configTracer.setReqIdHex(ui->reqIdLineEdit->text());
	emit cfgChanged(this->configTracer.getMap());
}


void TracerTabForm::on_respIdLineEdit_editingFinished()
{
	this->configTracer.setRespIdHex(ui->respIdLineEdit->text());
	emit cfgChanged(this->configTracer.getMap());
}


void TracerTabForm::on_logPathLineEdit_editingFinished()
{
	this->configTracer.setLogDirPath(ui->logPathLineEdit->text());
	emit cfgChanged(this->configTracer.getMap());
}


void TracerTabForm::on_clearPushButton_clicked(bool checked)
{
	(void)checked;
	ui->tracerListWidget->clear();
	this->totalNumOfItem = 0;
	this->traceUiDataVect.clear();
	ui->totalItemCountLabel->setText(
		QString("%1").arg(this->totalNumOfItem)
	);
}


void TracerTabForm::on_connectPushButton_clicked(bool checked)
{
	if(checked) {
		on_clearPushButton_clicked(true);
	}

	QString st = checked ? "on" : "off";
	try {
		emit req({{CmdDef::connect.name, st}});
		emit req({{CmdDef::storeConfig.name, ui->logPathLineEdit->text() + "/" + Util::getFileName() + ".xml"}});
	} catch (const std::exception &e) {
		QString unescapedStr = QString(e.what()).replace("\\n", "\n").replace("\\\"", "\"");
		QMessageBox::critical(this, "Error", unescapedStr);
		ui->connectPushButton->setChecked(false);
	}
}

void TracerTabForm::on_logPathPushButton_clicked(bool checked)
{
	(void)checked;
	QString dirPath = QFileDialog::getExistingDirectory(this, "Select log directory", this->configTracer.getLogDirPath());
	if(!dirPath.isEmpty()) {
		this->configTracer.setLogDirPath(dirPath);
		ui->logPathLineEdit->setText(this->configTracer.getLogDirPath());
	}
}


void TracerTabForm::on_numOfItemsSpinBox_valueChanged(int arg1)
{
    this->numOfItemToShow = arg1;
}

