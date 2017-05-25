#include "PreviewWidget.h"

#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>

#include <unordered_map>

#include <src/slic3r/GUI/3DScene.hpp>

const float COLORS[4][4] = { { 1,0.95,0.2,1 },{ 1,0.45,0.45,1 },{ 0.5,1,0.5,1 },{ 0.5,0.5,1,1 } };

PreviewWidget::PreviewWidget(QWidget* parent)
	:QGLWidget(parent),
	enable_picking_(true),loaded_(false),
	color_toolpaths_by_(ctRole)
{
	setMouseTracking(true);

	
}


PreviewWidget::~PreviewWidget(){

}

void PreviewWidget::initializeGL() {
	glClearColor(1, 1, 1, 1);
	glColor3f(1, 0, 0);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0);
	glEnable(GL_COLOR_MATERIAL);
	glEnable(GL_MULTISAMPLE);

	glDisable(GL_BLEND);
	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glColor4f(1, 1, 1, 1);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	//glEnable(GL_LIGHT1);
	GLfloat light_model_ambient0[] = { 0.3,0.3,0.3,1 };
	GLfloat light0_diffuse0[] = { 0.5,0.5,0.5,1 };
	GLfloat light_specular0[] = { 0.2,0.2,0.2,1 };
	GLfloat light_position0[] = { -0.5,-0.5,1,0 };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_model_ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0);

	GLfloat light_model_ambient1[] = { 0.3,0.3,0.3,1 };
	GLfloat light0_diffuse1[] = { 0.2,0.2,0.2,1 };
	GLfloat light_specular1[] = { 0.3,0.3,0.3,1 };
	GLfloat light_position1[] = { 1,0,1,0 };
	glLightfv(GL_LIGHT1, GL_POSITION, light_position1);
	glLightfv(GL_LIGHT1, GL_AMBIENT, light_model_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light0_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);

	trackball_.center = vcg::Point3f(0, 0, 0);
	trackball_.radius = 50;

	glLoadIdentity();
}


void PreviewWidget::paintGL() {
	
}


void PreviewWidget::resizeGL(int width, int height) {

}

struct{
	bool operator()(Layer* x, Layer* y) {
		return x->print_z < y->print_z;
	}
}CompareLayer;


/*
 *	导入打印对象的toolpaths，目前不包括support materials
 */
void PreviewWidget::LoadPrintObjectToolpaths(const PrintObject* object) {
	//按照print_z对Layers进行排序

	std::vector<Layer*> layers(object->layers.begin(), object->layers.end());
	std::sort(layers.begin(), layers.end(), [](Layer* x, Layer* y) {
		return x->print_z < y->print_z; 
	});

	BoundingBoxf3 bbox;

	BoundingBox obj_bbox = object->bounding_box();
	for (auto& copy : object->_shifted_copies) {
		BoundingBox copy_bbox = obj_bbox;
		copy_bbox.translate(copy.x, copy.y);
		bbox.merge(Pointf3::new_unscale(copy_bbox.min.x, copy_bbox.min.y, 0));
		bbox.merge(Pointf3::new_unscale(copy_bbox.max.x, copy_bbox.max.y, 0));
	}

	//<color_index, volume>
	std::unordered_map<int, std::vector<SceneVolume>> color_volumes;


	long alloc_size_max = 32 * 1048576 / 4;

	bool color_toolpaths_by_extruder = color_toolpaths_by_ == ctExtruder;

	for (Layer* layer : layers) {
		coordf_t top_z = layer->print_z;
		for (auto& copy : object->_shifted_copies) {
			for (LayerRegion* region : layer->regions) {
				if (object->state.is_done(posPerimeters)) {
					 const float* color = color_toolpaths_by_extruder?
						COLORS[(region->region()->config.perimeter_extruder - 1) % 4] :
						COLORS[0];
					AddScenevolume(region->perimeters, top_z, copy, color, bbox);
				}

				if (object->state.is_done(posInfill)) {
					const float* color = color_toolpaths_by_extruder ?
						COLORS[(region->region()->config.infill_extruder - 1) % 4] :
						COLORS[1];
					if (color_toolpaths_by_extruder&&
						region->region()->config.infill_extruder != region->region()->config.solid_infill_extruder) {
					
						ExtrusionEntityCollection solid_entities;
						ExtrusionEntityCollection non_solid_entities;

						for (ExtrusionEntity* entity : region->fills.entities) {
							if (entity->is_loop()) {
								ExtrusionLoop* loop = dynamic_cast<ExtrusionLoop*>(entity);
								if (loop->is_solid_infill()) {
									solid_entities.append(*entity);
								}
								else {
									non_solid_entities.append(*entity);
								}
							}
							else {
								ExtrusionPath* path = dynamic_cast<ExtrusionPath*>(entity);
								if (path->is_solid_infill()) {
									solid_entities.append(*entity);
								}
								else {
									non_solid_entities.append(*entity);
								}
							}
						}

						AddScenevolume(non_solid_entities, top_z, copy, color, bbox);
						color = COLORS[(region->region()->config.solid_infill_extruder - 1) % 4];
						AddScenevolume(solid_entities, top_z, copy, color, bbox);
					}
					else {
						AddScenevolume(region->fills, top_z, copy, color, bbox);
					}
				}
			}
		}
	}
}

void PreviewWidget::AddScenevolume(ExtrusionEntityCollection& entities, coordf_t top_z,
	const Point& copy,const float* color,BoundingBoxf3& bbox) {
	SceneVolume scenevolume;
	scenevolume.bbox_ = bbox;
	scenevolume.color[0] = color[0]; scenevolume.color[1] = color[1];
	scenevolume.color[2] = color[2]; scenevolume.color[3] = color[3];

	ExtrusionentitiesToVerts(entities, top_z, copy, scenevolume);
	//scenevolume.offsets[top_z] = { scenevolume.qverts_.verts.size(),scenevolume.tverts_.verts.size()};
	scenevolume.top_z = top_z;

	volumes_.push_back(scenevolume);


}


void PreviewWidget::ExtrusionentitiesToVerts(ExtrusionEntityCollection& entities, coordf_t top_z,
	const Point& copy, SceneVolume& scenevolume) {
	for (ExtrusionEntity* entity : entities.entities) {
		if (entity->is_loop()) {
			ExtrusionLoopToVerts(dynamic_cast<ExtrusionLoop&>(*entity), top_z, copy, scenevolume);
		}
		else {
			ExtrusionPathToVerts(dynamic_cast<ExtrusionPath&>(*entity), top_z, copy, scenevolume);
		}
	}
}


void PreviewWidget::ExtrusionPathToVerts(ExtrusionPath& entity, coordf_t top_z,
	const Point& copy, SceneVolume& volume) {
	Slic3r::Polyline polyline = entity.polyline;
	polyline.remove_duplicate_points();
	polyline.translate(copy);
	Slic3r::Lines lines = polyline.lines();

	std::vector<double> widths(lines.size(), entity.width);
	std::vector<double> heights(lines.size(), entity.height);

	bool closed = 0;

	_3DScene::_extrusionentity_to_verts_do(lines, widths, heights, closed,
		top_z, copy, &volume.qverts_, &volume.tverts_);
}


void PreviewWidget::ExtrusionLoopToVerts(ExtrusionLoop& loop, coordf_t top_z,
	const Point& copy, SceneVolume& volume) {

	std::vector<double> widths;
	std::vector<double> heights;
	std::vector<Line> lines;

	for (ExtrusionPath& path : loop.paths) {
		Slic3r::Polyline polyline = path.polyline;
		polyline.remove_duplicate_points();
		polyline.translate(copy);
		Slic3r::Lines path_lines = polyline.lines();
		
		lines.insert(lines.end(), path_lines.begin(), path_lines.end());
		widths.insert(widths.end(), path_lines.size(), path.width);
		heights.insert(heights.end(), path_lines.size(), path.height);
	}

	bool closed = true;

	_3DScene::_extrusionentity_to_verts_do(lines, widths, heights, closed,
		top_z, copy, &volume.qverts_, &volume.tverts_);
}


void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {

}

void PreviewWidget::wheelEvent(QMouseEvent* event) {


}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {

}

void PreviewWidget::keyReleaseEvent(QMouseEvent* event) {

}

void PreviewWidget::contextMenuEvent(QContextMenuEvent* event) {

}

