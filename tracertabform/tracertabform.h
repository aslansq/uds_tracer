#ifndef TRACERTABFORM_H
#define TRACERTABFORM_H
/// @file tracertabform.h
/// @defgroup tracertabform_h
/// @{
#include <QWidget>
#include <QColor>
#include "hexvalidator.h"
#include "config.h"
#include "can.h"
#include "uds.h"

namespace Ui {
class TracerTabForm;
}

/// @brief Which trace data to show.
enum class TraceLevelEnum {
	Info,
	Req,
	Resp
};

class TraceLevelProp {
public:
	TraceLevelProp();
	TraceLevelProp(QVector<QColor> backgroundColor, int alignment);
	QColor getBackgroundColor(void);
	int getAlignment(void);
private:
	QVector<QColor> backgroundColor;
	int alignment;
	int backgroundColorIndex;
	void nextBackgroundColorIndex(void);
};

class TraceUiData {
public:
	TraceLevelEnum level;
	QString s;
};

class TracerTabForm : public QWidget
{
	Q_OBJECT

public:
	explicit TracerTabForm(QWidget *parent = nullptr);
	~TracerTabForm();

signals:
	/// @brief Emit signal to cli at configuration changed.
	void cfgChanged(QMap<QString, QString> map);
	/// @brief Make request to cli. Cli could throw an exception.
	void req(QMap<QString, QString> map);

public slots:
	void onCommandReceived(const QString &cmdStrRef);
	void onConfigAllLoaded(const ConfigAll &cfgAllRef);
	void onCanConnectionEvented(bool isConnected);
	void onCanMsgReceived(const CanMsg &canMsgRef);
	void onUdsPacketReceived(bool isReq, const QVector<UdsInfo> &packetInfoRef);

private slots:
	void on_reqIdLineEdit_editingFinished();

	void on_respIdLineEdit_editingFinished();

	void on_logPathLineEdit_editingFinished();

	void on_clearPushButton_clicked(bool checked);

	void on_connectPushButton_clicked(bool checked);

	void on_logPathPushButton_clicked(bool checked);

	void on_numOfItemsSpinBox_valueChanged(int arg1);

private:
	Ui::TracerTabForm *ui;
	HexValidator *hexValidatorPtr;
	ConfigTracer configTracer;
	bool reqSucceeded;
	QMap<TraceLevelEnum, TraceLevelProp> traceLevelProp;
	uint64_t numOfItemToShow;
	uint64_t totalNumOfItem;
	QVector<TraceUiData> traceUiDataVect;

	void addTrace(TraceLevelEnum level, QString s);

};
/// @}
#endif // TRACERTABFORM_H
