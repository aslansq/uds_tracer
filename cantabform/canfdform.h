#ifndef CANFDFORM_H
#define CANFDFORM_H

#include <QWidget>
#include <QIntValidator>

#include "config.h"

namespace Ui {
class CanFdForm;
}

class CanFdForm : public QWidget
{
	Q_OBJECT

public:
	explicit CanFdForm(QWidget *parent = nullptr);
	~CanFdForm();

	const ConfigFd &getConfig(void);
public slots:
	void onConfigLoaded(const ConfigFd &configFdRef);

signals:
	void cfgChanged(const ConfigFd &configFdRef);

private slots:
	void on_devPushButton_clicked();

	void on_arbitBaudLineEdit_editingFinished();

	void on_devLineEdit_editingFinished();

	void on_dataBaudLineEdit_editingFinished();

	void on_arbitTseg1LineEdit_editingFinished();

	void on_dataTseg1LineEdit_editingFinished();

	void on_arbitTseg2LineEdit_editingFinished();

	void on_dataTseg2LineEdit_editingFinished();

	void on_arbitSjwLineEdit_editingFinished();

	void on_dataSjwLineEdit_editingFinished();

	void on_clkFreqLineEdit_editingFinished();

	void on_arbitPreLineEdit_editingFinished();

	void on_dataPreLineEdit_editingFinished();

private:
	Ui::CanFdForm *ui;
	QIntValidator *validatorPtr;
	ConfigFd config;
};

#endif // CANFDFORM_H
