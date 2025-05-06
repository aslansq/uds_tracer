#ifndef CANSTDFORM_H
#define CANSTDFORM_H

#include <QWidget>
#include "config.h"

namespace Ui {
class CanStdForm;
}

class CanStdForm : public QWidget
{
	Q_OBJECT

public:
	explicit CanStdForm(QWidget *parent = nullptr);
	~CanStdForm();
	const ConfigStd &getConfig(void);

public slots:
	void onConfigLoaded(const ConfigStd &configRef);

signals:
	void cfgChanged(const ConfigStd &configRef);

private slots:
	void on_devLineEdit_editingFinished();

	void on_devPushButton_clicked();

	void on_baudComboBox_currentIndexChanged(int index);

private:
	class Baud {
	public:
		Baud(QString s, uint64_t b) :
			name(s),
			baudrate(b)
		{}
		QString name;
		uint64_t baudrate;
	};

	Ui::CanStdForm *ui;
	ConfigStd config;
	QVector<Baud> availableBaudrates;
	int getIndexOfAvailableBaud(uint64_t baudrate);
};

#endif // CANSTDFORM_H
