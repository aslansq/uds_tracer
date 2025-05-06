#ifndef CANTABFORM_H
#define CANTABFORM_H

#include <QButtonGroup>
#include <QWidget>
#include "canfdform.h"
#include "canstdform.h"
#include "canreplayform.h"

namespace Ui {
class CanTabForm;
}

class CanTabForm : public QWidget
{
	Q_OBJECT

public:
	explicit CanTabForm(QWidget *parent = nullptr);
	~CanTabForm();

	const QMap<QString, QString> &getCurrentConfig(void);
public slots:
	void onConfigAllLoaded(const ConfigAll &cfgAllRef);

signals:
	void cfgChanged(const QMap<QString, QString> &configRef);
	void configFdLoaded(const ConfigFd &configFdRef);
	void configStdLoaded(const ConfigStd &configStdRef);
	void configReplayLoaded(const ConfigReplay &configReplayRef);

private slots:
	void onStdCfgChanged(const ConfigStd &configRef);
	void onFdCfgChanged(const ConfigFd &configRef);
	void onReplayCfgChanged(const ConfigReplay &configRef);

	void on_stdCheckBox_stateChanged(int arg1);

	void on_fdCheckBox_stateChanged(int arg1);

	void on_replayCheckBox_stateChanged(int arg1);

private:
	Ui::CanTabForm *ui;
	QButtonGroup *checkButtonGroupPtr;
	QMap<QString, QString> currentConfig;

	CanStdForm *canStdFormPtr;
	CanFdForm *canFdFormPtr;
	CanReplayForm *canReplayFormPtr;

	void hideForms(void);
};

#endif // CANTABFORM_H
