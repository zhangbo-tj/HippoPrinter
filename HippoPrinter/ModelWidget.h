#ifndef MODELWIDGET_H__
#define MODELWIDGET_H__

#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>

#include <QGLWidget>

#include <src/libslic3r/Model.hpp>
#include <src/libslic3r/TriangleMesh.hpp>
#include <src/libslic3r/Print.hpp>


#include <wrap/gui/trackball.h>
#include <vcg/space/deprecated_point2.h>

#include "scenevolume.h"


class QAction;
class QMenu;

enum Color_By{color_by_volume,color_by_object};
enum Select_By{select_by_object, select_by_volume, select_by_instance};
enum Drag_By {drag_by_object, drag_by_instance};

class ModelWidget :
	public QGLWidget
{
	Q_OBJECT


public:
	ModelWidget(Print* print,Model* model, QWidget* parent = 0);
	~ModelWidget();
	
	void initializeGL();	//
	void paintGL();
	void resizeGL(int width, int height);
	void InitModel();

public:
	void mouseMoveEvent(QMouseEvent *event);		//鼠标移动事件
	void wheelEvent(QWheelEvent* event);		//鼠标滚轮事件
	void mousePressEvent(QMouseEvent* event);		//鼠标点击事件
	void mouseReleaseEvent(QMouseEvent *event);		//鼠标按键释放事件
	void keyReleaseEvent(QKeyEvent *event);		//键盘按键释放事件
	void contextMenuEvent(QContextMenuEvent *event);		//鼠标右键事件
	vcg::Trackball::Button QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers);

public:
	//void LoadModel(char* file_name);		//导入模型

private:
	void DrawAxes();		//绘制坐标系
	void DrawVolumes(bool fakecolor = false) const;		//绘制volumes
	void DrawBedShape()const;			//绘制xoy平面

	void AlignObjectToGround(ModelObject* new_object);
	//void LoadVolumes();		//载入volumes
	void ReloadMaxBBox();		//重新计算Bouding Box

	void SetBedShape(const BoundingBoxf& bed);		//设置底板形状和大小
	void SetDefaultBedShape();		//设置默认底板形状和大小
	Pointf GetBedCenter();

	void InitActions();		//初始化鼠标右键Actions
	void UnProject(int mouse_x, int mouse_y, Pointf3& world);		//将鼠标位置映射到三维点

private slots:
	void ResetTrackball();		//重置Trackball
	void DeleteVolume();		//删除选中的Volume
	void ReloadAllVolumes();	//重载所有的的Volume

	void RotateVolumeX();		//绕X轴旋转Volume
	void RotateVolumeY();		//绕Y轴旋转Volume
	void RotateVolumeZ();		//绕Z轴旋转Volume
	
	void MirrorVolumeX();		//沿X轴方向做镜像
	void MirrorVolumeY();		//沿Y轴方向做镜像
	void MirrorVolumeZ();		//沿Z轴方向做镜像

	void ScaleVolumeUniformly();	//等比缩放
	void ScaleVolumeX();		//在X轴方向缩放
	void ScaleVolumeY();		//在Y轴方向缩放
	void ScaleVolumeZ();		//在Z轴方向缩放


public:
	void ReloadVolumes();
	void ZoomToVolumes();

private:
	void ResetVolumes();

	void LoadModelObject(int object_idx);

	void LoadBedShape();

	void ZoomToBBox(BoundingBoxf3& bbox);

	void ZoomToBed();

	void LoadMaxBBox();

	void MirrorVolume(Axis axis);


private:
	vcg::Trackball trackball_;		//控制当前场景缩放和旋转操作的trackball

	Model* model_;		//打印模型
	Print* print_;		//打印对象
	BoundingBoxf bed_shape_;		//底板形状
	BoundingBoxf3 max_bbox_;
	std::vector<SceneVolume> volumes_;	//渲染对象

	Color_By color_by_;
	Select_By select_by_;
	Drag_By drag_by_;

	//<volume_idx, {obj_idx, volume_idx, instance_idx}>
	std::unordered_map<int, std::pair<std::pair<int, int>, int>> volume_to_object_;

	// <obj_idx, {volume_idx1, volume_idx2, ....}>
	std::unordered_map<int, std::vector<int>> object_to_volumes_;

	bool enable_picking_;
	int hovered_volume_index_;		//hovered volume index
	int selected_volume_index_;		//selected volume index



	TriangleMesh trimesh_;	

	
	
	


	Pointf3 origin_;
	double scale_;
	

	int cur_mouse_x_;
	int cur_mouse_y_;
	int pre_mouse_x_;
	int pre_mouse_y_;

	

	bool left_pressed_;
	bool right_pressed_;

	bool need_arrange_;		//需要对当前的模型进行重新布局



	QMenu* right_button_menu_;
	QMenu* mirror_volume_menu_;
	QMenu* scale_volume_menu_;
	QMenu* rotate_volume_menu_;
	
	QAction* reset_trackball_action_;
	QAction* reload_volumes_action_;

	QAction* delete_volume_action_;
	QAction* rotate_volume_x_action_;
	QAction* rotate_volume_y_action_;
	QAction* rotate_volume_z_action_;
	
	QAction* mirror_volume_x_action_;
	QAction* mirror_volume_y_action_;
	QAction* mirror_volume_z_action_;

	QAction* scale_volume_u_action_;
	QAction* scale_volume_x_action_;
	QAction* scale_volume_y_action_;
	QAction* scale_volume_z_action_;
};
#endif //MODELWIDGET_H__ 

