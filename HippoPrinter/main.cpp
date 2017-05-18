#include "HippoPrinter.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	HippoPrinter w;
	w.show();
	return a.exec();
}
