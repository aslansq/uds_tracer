#include <QFileDialog>
#include "canstdform.h"
#include "ui_canstdform.h"
#include "util.h"

CanStdForm::CanStdForm(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::CanStdForm),
	availableBaudrates({
		{"1 MBit/s"  , 1000000},
		{"800 kBit/s", 800000 },
		{"500 kBit/s", 500000 },
		{"250 kBit/s", 250000 },
		{"125 kBit/s", 125000 },
		{"100 kBit/s", 100000 },
		{"95 kBit/s" , 95000  },
		{"83 kBit/s" , 83000  },
		{"50 kBit/s" , 50000  },
		{"47 kBit/s" , 47000  },
		{"33 kBit/s" , 33000  },
		{"20 kBit/s" , 20000  },
		{"10 kBit/s" , 10000  },
		{"5 kBit/s"  , 5000   }
	})
{
	ui->setupUi(this);
	ui->devLineEdit->setPlaceholderText(this->config.getDev());
	for(const Baud &baudRef : this->availableBaudrates) {
		ui->baudComboBox->addItem(baudRef.name);
	}
	ui->baudComboBox->setCurrentIndex(getIndexOfAvailableBaud(this->config.getBaud().toInt()));
}

CanStdForm::~CanStdForm()
{
	delete ui;
}

const ConfigStd &CanStdForm::getConfig(void)
{
	return this->config;
}

int CanStdForm::getIndexOfAvailableBaud(uint64_t baudrate)
{
	int idx = -1;
	for(int i = 0; i < this->availableBaudrates.length(); ++i) {
		if(this->availableBaudrates[i].baudrate == baudrate) {
			idx = i;
			break;
		}
	}
	return idx;
}

void CanStdForm::onConfigLoaded(const ConfigStd &configRef)
{
	int idx;
	this->config.setMap(configRef.getMap());
	idx = getIndexOfAvailableBaud(configRef.getBaud().toInt());

	if(idx == -1) {
		Util::log(LogType::GenericThrow, LogSt::Nok, "Unknown baud: " + this->config.getBaud());
	} else {
		ui->baudComboBox->setCurrentIndex(idx);
	}

	ui->devLineEdit->setText(this->config.getDev());
}

void CanStdForm::on_devLineEdit_editingFinished(void)
{
	this->config.setDev(ui->devLineEdit->text());
	emit cfgChanged(this->config);
}

void CanStdForm::on_devPushButton_clicked()
{
	QString devPath = QFileDialog::getOpenFileName(this, "Select CAN Log to Play", "/dev", "All files (*)");
	if (!devPath.isEmpty())
	{
		this->config.setDev(devPath);
		ui->devLineEdit->setText(devPath);
		emit cfgChanged(this->config);
	}
}

void CanStdForm::on_baudComboBox_currentIndexChanged(int index)
{
	QString s = QString("%1").arg(this->availableBaudrates[index].baudrate);
	this->config.setBaud(s);
	emit cfgChanged(this->config);
}

