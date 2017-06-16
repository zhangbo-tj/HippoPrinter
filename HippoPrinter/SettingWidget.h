#pragma once


#include <QWidget>

class SettingWidget:public QWidget{
	Q_OBJECT

public:
	SettingWidget(QWidget* parent = 0);
	~SettingWidget();

	void InitWidgets();
	void InitLayout();

private:
	

};

