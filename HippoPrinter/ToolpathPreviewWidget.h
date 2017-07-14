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
 *	Ԥ����ά��ӡ·���Ŀؼ�����
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
	

	//����ģ��
private slots:
	void ResetTrackball();

	//ͨ��Slider������ʾģ�͵ķ�Χ
public slots:
	void SetLayerZ(int max_z);


	//�������¼�
public:
	void mouseMoveEvent(QMouseEvent* event);
	void wheelEvent(QWheelEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void mouseReleaseEvent(QMouseEvent *event);	
	void keyReleaseEvent(QKeyEvent* event);
	void contextMenuEvent(QContextMenuEvent* event);

	//��QT�¼�ת��ΪVCG�¼�
	vcg::Trackball::Button QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers);


	//���뵥����ӡ����Ĵ�ӡ·��
	void LoadPrintObjectToolpaths(const PrintObject* object);
	
	//���������ӡ·��
	void ReloadVolumes();

	//�����ӡ·��
	void LoadPrint();

	//�������е���ʾ����
	void ResetVolumes();

	void LoadBedShape();

	void ZoomToBed();

	void ZoomToVolumes();

	void ZoomToBBox(BoundingBoxf3& bbox);

	void LoadMaxBBox();


private:
	//���ƴ�ӡ���󣬼���ӡ����Ĵ�ӡ·��
	void DrawVolumes() ;

	//������ά������
	void DrawAxes() const;
	
	//�ؼ��еװ�����ú���ʾ
	void DrawBedShape() const;
	void SetBedshape(const BoundingBoxf& bed);
	void SetDefaultBedshape();

	//���¼���Bounding box
	//void ReloadBBox();


	//��ʼ��Actions������������
	void InitActions();


// 	// ���ExtrusionEntityΪ�µ�volume����
	void AddScenevolume(ExtrusionEntityCollection& entities, coordf_t top_z,
		const Point& copy, int color_index, BoundingBoxf3& bbox);

	//��extrusion entiti ת��Ϊverts
	void ExtrusionEntityToVerts(ExtrusionEntity& entity, coordf_t top_z,
		const Point& copy, SceneVolume& volume);


private:
	//������ά�ؼ������š���ת�Ȳ���
	vcg::Trackball trackball_;

	//�ؼ�����ʾ����
	std::vector<SceneVolume> volumes_;

	//<color_index,0|1>����Ѿ����ڸ�color_index,��Ϊ1������Ϊ9
	std::unordered_map<int, int> color_volumes_;

	//<color__index, volume_index>
	std::unordered_map<int, int> color_volumeidx;


	//��ӡ����
	Print* print_;
	
	//�ؼ���Bounding Box
	BoundingBoxf3 max_bbox_;

	//�װ���״
	BoundingBoxf bed_shape_;

	//�Ƿ��Ѿ�����ģ��
	bool loaded_;

	//����Ҽ��˵��Ͳ˵���
	QMenu* right_click_menu_;
	QAction* reset_trackball_action_;

	//ͨ��role|extruder�Դ�ӡ·������Ⱦɫ
	ColorToolpathsBy color_toolpaths_by_;

	//����ģ��ʱ���õ����½磬���û��������Ϊ-1��-1
	double min_z_;
	double max_z_;

	double scale_;

public:
	//<layer_id, print_z> ��ȡÿһ���print_z
	std::map<int, double>* layer_values_;
};



#endif	//TOOLPATHPREVIEWWIDGET_H__

