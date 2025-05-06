#include "mainwindow.h"

#include <QApplication>
#include <QMap>
#include <QString>
#include <QDebug>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "cli.h"


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setApplicationName("CAN UDS Tracer");
	a.setApplicationVersion("0.0.1");

	bool libMode = true;
	for (int i = 1; i < argc; ++i) {
		if (QString(argv[i]) == "--cli") {
			libMode = false;
			break;
		}
	}

	Cli cli(nullptr);
	cli.init(argc, argv);
	MainWindow w(nullptr, cli);

	if(libMode) {
		w.show();
	}
	return a.exec();
	return 0;
}
