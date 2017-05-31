#pragma once

#include <QMouseEvent>
#include <wrap/gui/trackball.h>
///将Qt鼠标键盘事件转换为VCG库内的鼠标键盘事件
vcg::Trackball::Button QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers)
{
	int vcgbt = vcg::Trackball::BUTTON_NONE;
	if (qtbt & Qt::LeftButton) vcgbt |= vcg::Trackball::BUTTON_LEFT;
	if (qtbt & Qt::RightButton) vcgbt |= vcg::Trackball::BUTTON_RIGHT;
	if (qtbt & Qt::MidButton) vcgbt |= vcg::Trackball::BUTTON_MIDDLE;
	if (modifiers & Qt::ShiftModifier)	vcgbt |= vcg::Trackball::KEY_SHIFT;
	if (modifiers & Qt::ControlModifier) vcgbt |= vcg::Trackball::KEY_CTRL;
	if (modifiers & Qt::AltModifier) vcgbt |= vcg::Trackball::KEY_ALT;
	return vcg::Trackball::Button(vcgbt);
}