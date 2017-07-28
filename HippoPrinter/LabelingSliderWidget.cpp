#include "LabelingSliderWidget.h"


#include <QPalette>

LabelingSliderWidget::LabelingSliderWidget(QWidget* parent):QSlider(parent){
	value_label_ = new QLabel(this);
	value_label_->setFixedSize(QSize(30, 20));

	QPalette palette;
	palette.setColor(QPalette::Background, Qt::white);
	value_label_->setPalette(palette);

	value_label_->setAlignment(Qt::AlignCenter);

	value_label_->setVisible(true);
	value_label_->setText(QString::number(this->value()));
	value_label_->move(-3,-value_label_->height()/2);
	value_label_->setStyleSheet(
	"QLabel{font:10pt,Arial}");
	this->setStyleSheet(
		"QSlider{margin: 6px}");
}


LabelingSliderWidget::~LabelingSliderWidget(){

}


void LabelingSliderWidget::mousePressEvent(QMouseEvent* event) {
	if (!value_label_->isVisible()) {
		value_label_->setVisible(true);
		value_label_->setText(QString::number(this->value()));
	}
	QSlider::mousePressEvent(event);
}

void LabelingSliderWidget::mouseReleaseEvent(QMouseEvent* event) {
// 	if (value_label_->isVisible()) {
// 		value_label_->setVisible(false);
// 	}
	QSlider::mouseReleaseEvent(event);
}

void LabelingSliderWidget::mouseMoveEvent(QMouseEvent* event) {
	value_label_->setText(QString::number(this->value()));
	if (this->maximum() - this->minimum() == 0)
		return;
	int y_pos = (this->height() - value_label_->height())*(this->maximum() - this->value()) / (this->maximum() - this->minimum());
	value_label_->move(-3, -value_label_->height() / 2 + y_pos);
	QSlider::mouseMoveEvent(event);
}


void LabelingSliderWidget::valueChanged(int value) {
	QSlider::valueChanged(value);
	value_label_->setText(QString::number(this->value()));
}