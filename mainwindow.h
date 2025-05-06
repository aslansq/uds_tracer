#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QIntValidator>
#include <QSettings>
#include <QFileInfo>

#include "cantabform.h"
#include "tracertabform.h"
#include "cli.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent, Cli &cliRef);
	~MainWindow();
private:
	const int canTabIdx = 0;
	const int tracerTabIdx = 1;

	Cli &cliRef;
	CanTabForm *canTabFormPtr;
	TracerTabForm *tracerTabFormPtr;
	Ui::MainWindow *ui;

	QSettings settings;
	QFileInfo prjFilePath;

	void onOpenAction();
	void onSaveAction();
	void onSaveAsAction();
	void onAboutAction();

	void setPrjFilePath(const QString &filePathRef);

private slots:
	void onCanConnectionEvented(bool isConnected);
};
#endif // MAINWINDOW_H
