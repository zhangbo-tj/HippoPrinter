#ifndef TOOLPATHPLANEWIDGET_H__
#define TOOLPATHPLANEWIDGET_H__

#pragma once

#include <vector>

#include <QGLWidget>


#include <src/libslic3r/Print.hpp>

class ToolpathPlaneWidget:public QGLWidget
{
	Q_OBJECT
public:
	ToolpathPlaneWidget(Slic3r::Print* print,std::map<int,double>* layer_values, QGLWidget* parent = 0);
	~ToolpathPlaneWidget();

private:
	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);
	void DrawEntity(ExtrusionEntity& entity,double print_z,PrintObject* print_object);
	void DrawPath(ExtrusionPath& path,double print_z, PrintObject* print_object);


	void wheelEvent(QWheelEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent *event);
	

	void LoadBedShape();
	void DrawBedShape();

	void ZoomToBed();
public:
	void ReloadVolumes();



public slots:
	void SetLayerZ(int max_z);

private:
	Print* print_;		//打印对象
	double scale_;		//控件缩放操作
	Pointf offset_;		//控件平移操作
	Pointf old_pos_;
	bool left_pressed_;

	BoundingBoxf bed_shape_;	//Bounding Box
	
	LayerPtrs layers_;		//控件显示内容
	double offset_z_;		//Z偏移值
	std::map<int, double>* layer_values_;

	int view[9];
};



#endif

