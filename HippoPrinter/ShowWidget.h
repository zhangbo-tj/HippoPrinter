#ifndef SHOWWIDGET_H__
#define SHOWWIDGET_H__

#pragma once

#include <iostream>
#include <vector>

#include <QGLWidget>

#include <src/libslic3r/Model.hpp>
#include <src/libslic3r/TriangleMesh.hpp>
#include <src/libslic3r/Print.hpp>


#include <wrap/gui/trackball.h>
#include <vcg/space/deprecated_point2.h>

#include "scenevolume.h"


class QAction;
class QMenu;

class ShowWidget :
	public QGLWidget
{
	Q_OBJECT


public:
	ShowWidget(QWidget* parent = 0);
	~ShowWidget();
	
	void initializeGL();	//
	void paintGL();
	void resizeGL(int width, int height);
	void InitModel();

public:
	void mouseMoveEvent(QMouseEvent *event);		//����ƶ��¼�
	void wheelEvent(QWheelEvent* event);		//�������¼�
	void mousePressEvent(QMouseEvent* event);		//������¼�
	void mouseReleaseEvent(QMouseEvent *event);		//��갴���ͷ��¼�
	void keyReleaseEvent(QKeyEvent *event);		//���̰����ͷ��¼�
	void contextMenuEvent(QContextMenuEvent *event);		//����Ҽ��¼�

public:
	void LoadModel(char* file_name);		//����ģ��

private:
	void DrawXYZ();		//��������ϵ
	void DrawVolumes(bool fakecolor = false) const;		//����volumes
	void DrawBedShape()const;			//����xoyƽ��

	void AlignObjectToGround(ModelObject* new_object);
	void LoadVolumes();		//����volumes
	void ReloadMaxBBox();		//���¼���Bouding Box

	void SetBedShape(const BoundingBoxf& bed);		//���õװ���״�ʹ�С
	void SetDefaultBedShape();		//����Ĭ�ϵװ���״�ʹ�С
	Pointf GetBedCenter();

	void InitActions();		//��ʼ������Ҽ�Actions
	void UnProject(int mouse_x, int mouse_y, Pointf3& world);		//�����λ��ӳ�䵽��ά��

private slots:
	void ResetTrackball();		//����Trackball
	void DeleteVolume();		//ɾ��ѡ�е�Volume
	void ReloadAllVolumes();	//�������еĵ�Volume

	void RotateVolumeX();		//��X����תVolume
	void RotateVolumeY();		//��Y����תVolume
	void RotateVolumeZ();		//��Z����תVolume
	
	void MirrorVolumeX();		//��X�᷽��������
	void MirrorVolumeY();		//��Y�᷽��������
	void MirrorVolumeZ();		//��Z�᷽��������

	void ScaleVolumeUniformly();	//�ȱ�����
	void ScaleVolumeX();		//��X�᷽������
	void ScaleVolumeY();		//��Y�᷽������
	void ScaleVolumeZ();		//��Z�᷽������

public slots:
	void BackgroundProcess();

	void ArrangeObjects();


private:
	vcg::Trackball trackball_;		//���Ƶ�ǰ�������ź���ת������trackball

	TriangleMesh trimesh_;	

	Model model_;
	Print print_;
	std::vector<SceneVolume> volumes_;
	BoundingBoxf bed_shape_;


	Pointf3 origin_;
	BoundingBoxf3 max_bbox_;
	

	int cur_mouse_x_;
	int cur_mouse_y_;
	int pre_mouse_x_;
	int pre_mouse_y_;

	bool enable_picking_;
	int hovered_volume_index_;
	int selected_volume_index_;

	bool left_pressed_;
	bool right_pressed_;

	bool need_arrange_;		//��Ҫ�Ե�ǰ��ģ�ͽ������²���



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
#endif //SHOWWIDGET_H__ 

