/**
 * @defgroup tracertabform_h
 * @{
 * @file tracertabform.h
 * @brief This interfac file for Tracer tab in main window. It contains:
 * - List widget to show trace data.
 * - Request ID
 * - Response ID
 * - Log path
 * - Clear button
 * - Connect button
 * - Show spin box to choose number of items to show.
 */
#ifndef TRACERTABFORM_H
#define TRACERTABFORM_H
#include <QWidget>
#include <QColor>
#include "hexvalidator.h"
#include "config.h"
#include "can.h"
#include "uds.h"

namespace Ui {
class TracerTabForm;
}

/// @brief This is used for coloring of trace and alignment of text in list widget.
enum class TraceLevelEnum {
	Info,
	Req, //!< left aligned
	Resp //!< right aligned
};

/// @brief Trace Level properties. It contains background color and alignment of text.
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

/// @brief Every trace data is stored in this type in a vector.
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
	/// @brief gui generates commands and sends to cli. Cli will process the command and emit signal to this class
	/// and we show every command in the list widget.
	void onCommandReceived(const QString &cmdStrRef);
	/// @brief On config loaded, set the values in UI.
	void onConfigAllLoaded(const ConfigAll &cfgAllRef);
	/// @brief Used just for logging in trace list widget.
	void onCanConnectionEvented(bool isConnected);
	/// @brief Used to show UDS packet in trace list widget.
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
#endif // TRACERTABFORM_H
/// @}