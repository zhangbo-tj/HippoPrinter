#ifndef TOOLPATHPREVIEWWIDGET_H__
#define TOOLPATHPREVIEWWIDGET_H__

#pragma once

#include <vector>
#include <set>
#include <unordered_map>

#include <QGLWidget>

#include <wrap/gui/trackball.h>

#include <src/libslic3r/Print.hpp>

#include "scenevolume.h"

class QAction;
class QMenu;
class QSlider;
enum ColorToolpathsBy {
	ctRole,
	ctExtruder
};


/*
 *	预览三维打印路径的控件，与
 */
class ToolpathPreviewWidget:public QGLWidget
{
	Q_OBJECT
public:
	ToolpathPreviewWidget(Print* print,std::map<int,double>* layer_values, QWidget* parent = 0);
	~ToolpathPreviewWidget();

	void initializeGL();
	void paintGL();
	void resizeGL(int width, int height);
	

	//重置模型
private slots:
	void ResetTrackball();

	//通过Slider控制显示模型的范围
public slots:
	void SetLayerZ(int max_z);


	//鼠标键盘事件
public:
	void mouseMoveEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent *event);	
	void keyReleaseEvent(QKeyEvent* event);
	void contextMenuEvent(QContextMenuEvent* event);

	//将QT事件转换为VCG事件
	vcg::Trackball::Button QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers);


	//导入单个打印对象的打印路径
	void LoadPrintObjectToolpaths(const PrintObject* object);
	
	//重新载入打印路径
	void ReloadVolumes();

	//载入打印路径
	void LoadPrint();

	//重置所有的显示对象
	void ResetVolumes();

	void LoadBedShape();

	void ZoomToBed();

	void ZoomToVolumes();

	void ZoomToBBox(BoundingBoxf3& bbox);

	void LoadMaxBBox();


private:
	//绘制打印对象，即打印对象的打印路径
	void DrawVolumes() ;

	//绘制三维坐标轴
	void DrawAxes() const;
	
	//控件中底板的设置和显示
	void DrawBedShape() const;
	void SetBedshape(const BoundingBoxf& bed);
	void SetDefaultBedshape();

	//重新计算Bounding box
	//void ReloadBBox();


	//初始化Actions，并关联操作
	void InitActions();


// 	// 添加ExtrusionEntity为新的volume对象
	void AddScenevolume(ExtrusionEntityCollection& entities, coordf_t top_z,
		const Point& copy, int color_index, BoundingBoxf3& bbox);

	//将extrusion entiti 转换为verts
	void ExtrusionEntityToVerts(ExtrusionEntity& entity, coordf_t top_z,
		const Point& copy, SceneVolume& volume);


private:
	//控制三维控件的缩放、旋转等操作
	vcg::Trackball trackball_;

	//控件的显示对象
	std::vector<SceneVolume> volumes_;

	//<color_index,0|1>如果已经存在该color_index,则为1；否则为9
	std::unordered_map<int, int> color_volumes_;

	//<color__index, volume_index>
	std::unordered_map<int, int> color_volumeidx;


	//打印对象
	Print* print_;
	
	//控件的Bounding Box
	BoundingBoxf3 max_bbox_;

	//底板形状
	BoundingBoxf bed_shape_;

	//是否已经载入模型
	bool loaded_;

	//鼠标右键菜单和菜单项
	QMenu* right_click_menu_;
	QAction* reset_trackball_action_;

	//通过role|extruder对打印路径进行染色
	ColorToolpathsBy color_toolpaths_by_;

	//绘制模型时设置的上下界，如果没有设置则为-1，-1
	double min_z_;
	double max_z_;

	double scale_;

public:
	//<layer_id, print_z> 获取每一层的print_z
	std::map<int, double>* layer_values_;
};



#endif	//TOOLPATHPREVIEWWIDGET_H__

