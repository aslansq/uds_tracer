#ifndef CANREPLAYFORM_H
#define CANREPLAYFORM_H

#include <QWidget>
#include "config.h"

namespace Ui {
class CanReplayForm;
}

class CanReplayForm : public QWidget
{
	Q_OBJECT

public:
	explicit CanReplayForm(QWidget *parent = nullptr);
	~CanReplayForm();
	const ConfigReplay &getConfig(void);
public slots:
	void onConfigLoaded(const ConfigReplay &configReplayRef);

signals:
	void cfgChanged(const ConfigReplay &configRef);

private slots:
	void on_filePathLineEdit_editingFinished();

	void on_filePathPushButton_clicked();

private:
	Ui::CanReplayForm *ui;
	ConfigReplay config;
};

#endif // CANREPLAYFORM_H
