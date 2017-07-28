#pragma once


#include <QLabel>
#include <QSlider>
#include <QMouseEvent>



class LabelingSliderWidget:public QSlider
{
public:
	LabelingSliderWidget(QWidget* parent = 0);
	~LabelingSliderWidget();

protected:
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	void valueChanged(int value);

private:
	QLabel* value_label_;
};

