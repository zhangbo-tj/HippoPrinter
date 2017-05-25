#ifndef PREVIEWWIDGET_H__
#define PREVIEWWIDGET_H__

#pragma once

#include <vector>

#include <QGLWidget>

#include <wrap/gui/trackball.h>

#include <src/libslic3r/Print.hpp>

#include "scenevolume.h"

class QAction;
class QMenu;
enum ColorToolpathsBy {
	ctRole,
	ctExtruder
};

class PreviewWidget:public QGLWidget
{
	Q_OBJECT
public:
	PreviewWidget(QWidget* parent = 0);
	~PreviewWidget();

	void initializeGL();
	void paintGL();
	void resizeGL(int width, int height);
	

public:
	void mouseMoveEvent(QMouseEvent* event);
	void wheelEvent(QMouseEvent* event);
	void mousePressEvent(QMouseEvent* event);
	void keyReleaseEvent(QMouseEvent* event);
	void contextMenuEvent(QContextMenuEvent* event);


	void LoadPrintObjectToolpaths(const PrintObject* object);
	void ReloadPrint();
	void LoadPrint();

private:
	void DrawVolumes();
	void InitActions();
	void ExtrusionentitiesToVerts(ExtrusionEntityCollection& entities,coordf_t top_z,
			const Point& copy,SceneVolume& scenevolume);
	void ExtrusionPathToVerts(ExtrusionPath& entity, coordf_t top_z,
		const Point& copy, SceneVolume& volume);
	void ExtrusionLoopToVerts(ExtrusionLoop& entity, coordf_t top_z,
		const Point& copy, SceneVolume& volume);
	void AddScenevolume(ExtrusionEntityCollection& entities,coordf_t top_z,
		const Point& copy,const float *color,BoundingBoxf3& bbox);

private:
	vcg::Trackball trackball_;
	std::vector<SceneVolume> volumes_;
	BoundingBoxf3 bed_shape_;
	Print* print_;

	bool enable_picking_;
	bool loaded_;

	QMenu* right_click_menu_;
	QAction* reset_trackball_action_;

	ColorToolpathsBy color_toolpaths_by_;
};



#endif	//PREVIEWWIDGET_H__

