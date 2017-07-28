#include "ToolpathPlaneWidget.h"

#include <algorithm>

#include <QWheelEvent>
#include <QMouseEvent>

#include <GL\glut.h>

#include <src/libslic3r/ExPolygon.hpp>
#include <src/libslic3r/ExPolygonCollection.hpp>
ToolpathPlaneWidget::ToolpathPlaneWidget(Slic3r::Print* print,
	std::map<int,double>* layer_values,QGLWidget* parent)
	:QGLWidget(parent),scale_(1),offset_(0,0),old_pos_(0,0),left_pressed_(false)
	{
	layer_values_ = layer_values;
	print_ = print;

	setMouseTracking(true);
	LoadBedShape();
	
	view[0] = 0,view[1] = 0,view[2] = 1;
	view[3] = 0, view[4] = 0, view[5] = 0;
	view[6] = 0, view[7] = 0, view[8] = 0;

}

ToolpathPlaneWidget::~ToolpathPlaneWidget() {

}

void ToolpathPlaneWidget::initializeGL() {


	glLoadIdentity();
	
	//ZoomToBed();
}

void ToolpathPlaneWidget::paintGL() {
	glClearColor(1, 1, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	

	GLfloat light_model_ambient0[] = { 0.3,0.3,0.3,1 };
	GLfloat light0_diffuse0[] = { 0.5,0.5,0.5,1 };
	GLfloat light_specular0[] = { 0.2,0.2,0.2,1 };
	GLfloat light_position0[] = { -0.5,-0.5,1,0 };
	glLightfv(GL_LIGHT0, GL_POSITION, light_position0);
	glLightfv(GL_LIGHT0, GL_AMBIENT, light_model_ambient0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, light0_diffuse0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glTranslatef(offset_.x, offset_.y, 0);
	glScaled(scale_, scale_, scale_);

	glEnable(GL_LIGHTING);
	DrawBedShape();

	if (layers_.empty())	return;

	//glEnable(GL_LIGHTING);
	BoundingBox bb;
	// draw slice contour
	for (Layer* layer : layers_) {
		if(abs(layer->print_z - offset_z_ ) < EPSILON)
			continue;

		PrintObject* print_object = layer->object();

	
		for (Point& copy : print_object->_shifted_copies) {
			glPushMatrix();
			glTranslated(unscale(copy.x),unscale(copy.y), 0);

			for (ExPolygon& slice : layer->slices.expolygons) {
				glColor3f(0.9, 0.9, 0.9);

				glBegin(GL_LINES);
				Lines lines = slice.lines();

				//bb.merge(slice.bounding_box());
				for (Line& line : lines) {
					glVertex2d(unscale(line.a.x),unscale(line.a.y));
					glVertex2d(unscale(line.b.x),unscale(line.b.y));
				}
				glEnd();
			}
			glPopMatrix();
		}
	}


	for (Layer* layer : layers_) {
		PrintObject* print_object = layer->object();
		double print_z = layer->print_z;

		
		for (LayerRegion* region : layer->regions) {
			//draw perimeters
			if (print_object->state.is_done(posPerimeters)) {
				glColor3f(0.7, 0, 0);
				DrawEntity(region->perimeters, print_z, print_object);
			}

			//draw infills
			if (print_object->state.is_done(posInfill)) {
				glColor3f(0, 0, 0.7);
				DrawEntity(region->fills, print_z, print_object);
			}
		}
	}

	
}




void ToolpathPlaneWidget::resizeGL(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	double x = w / scale_;
	double y = h / scale_;
	glOrtho(0, x, 0, y, -2, 2);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	ZoomToBed();
}

void ToolpathPlaneWidget::ReloadVolumes() {

}


void ToolpathPlaneWidget::SetLayerZ(int layer_z) {
	double offset_z = (*layer_values_)[layer_z];

	layers_.clear();

	double max_layer_height = print_->max_allowed_layer_height();

	bool has_support_material = false;
	for (auto object : print_->objects) {
		if (object->config.support_material) {
			has_support_material = true;
			break;
		}
	}

	bool infill_every_layer = false;
	for (auto region : print_->regions) {
		if (region->config.infill_every_layers > 0) {
			infill_every_layer = true;
			break;
		}
	}

	bool interlaced = has_support_material || infill_every_layer;

	//要添加support material
	for (PrintObject* object : print_->objects) {
		for (Layer* layer : object->layers) {
			if (interlaced) {
				if (offset_z > (layer->print_z - max_layer_height - EPSILON) &&
					(offset_z <= (layer->print_z + EPSILON)) ){
					layers_.push_back(layer);
				}
			}
			else {
				if (abs(layer->print_z - offset_z) < EPSILON) {
					layers_.push_back(layer);
				}
			}
		}
	}

	//reverse layers so that we draw the lowermost on top
	std::reverse(layers_.begin(),layers_.end());
	offset_z_ = offset_z;
	update();

}


/*
 *	绘制打印路径，参数可能为ExtrusionEntityCollection, ExtrusionPath, ExtrusionLoop
 */
void ToolpathPlaneWidget::DrawEntity(ExtrusionEntity& entity, double print_z,
	PrintObject* print_object) {
	if (typeid(entity) == typeid(ExtrusionEntityCollection)) {
		ExtrusionEntityCollection& entity_collection = 
			dynamic_cast<ExtrusionEntityCollection&>(entity);
		for (ExtrusionEntity* temp_entity : entity_collection.entities) {
			DrawEntity(*temp_entity, print_z, print_object);
		}
		return;
	}
	else if (typeid(entity) == typeid(ExtrusionPath)) {
		ExtrusionPath& temp_path = dynamic_cast<ExtrusionPath&>(entity);
		DrawPath(temp_path, print_z, print_object);
	}
	else {
		ExtrusionLoop& loop = dynamic_cast<ExtrusionLoop&>(entity);
		for (ExtrusionPath& path : loop.paths) {
			DrawPath(path, print_z, print_object);
		}
	}
	glFlush();
}


/*
 *	绘制Polyline,即路径
 */
void ToolpathPlaneWidget::DrawPath(ExtrusionPath& path, double print_z, 
	PrintObject* print_object) {

	if (abs(print_z - offset_z_) >= EPSILON) {
		glColor3f(0.8, 0.8, 0.8);
	}

	glLineWidth(1);

	if (print_object != nullptr) {
		for (auto& copy : print_object->_shifted_copies) {
			glPushMatrix();
			glTranslated(unscale(copy.x),unscale(copy.y), 0);

			Lines lines = path.polyline.lines();
			glBegin(GL_LINES);
			for (Line& line : lines) {
				glVertex2d(unscale(line.a.x),unscale(line.a.y));
				glVertex2d(unscale(line.b.x),unscale(line.b.y));
			}
			glEnd();
			glPopMatrix();
		}
	}
	else {
		Lines lines = path.polyline.lines();
		glBegin(GL_LINES);
		for (Line& line : lines) {
			glVertex2d(unscale(line.a.x),unscale(line.a.y));
			glVertex2d(unscale(line.b.x),unscale(line.b.y));
		}
		glEnd();

	}

}


void ToolpathPlaneWidget::LoadBedShape() {
	bed_shape_ = BoundingBoxf(print_->config.bed_shape.values);
}


void ToolpathPlaneWidget::DrawBedShape() {
	float fCursor[4];
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//获取当前颜色
	glPushMatrix();

	glColor4f(0.8, 0.6, 0.5, 0.4);
	glNormal3d(1, 1, 1);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();

	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glColor4f(0.8, 0.6, 0.5, 0.4);
	glNormal3d(0, 0, 1);
	glBegin(GL_TRIANGLES);

	glVertex3f(bed_shape_.min.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.max.y, 0);
	glVertex3f(bed_shape_.min.x, bed_shape_.max.y, 0);

	glVertex3f(bed_shape_.min.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.max.y, 0);

	glEnd();

	glLineWidth(0.5);
	glColor4f(0.2, 0.2, 0.2, 0.4);
	glBegin(GL_LINES);
	for (int i = 0; i <= 180; i += 10) {
		glVertex3f(0, i, 0);
		glVertex3f(280, i, 0);
	}
	for (int i = 0; i < 280; i += 10) {
		glVertex3f(i, 0, 0);
		glVertex3f(i, 180, 0);
	}
	glEnd();
	
	glColor4fv(fCursor);
	

	glPopMatrix();
}


void ToolpathPlaneWidget::mouseMoveEvent(QMouseEvent *event) {
	if (left_pressed_) {
		//double cur_x = (event->x()) / static_cast<double>(width());
		//double cur_y = (height() - event->y()) / static_cast<double>(height());
		int cur_x = event->x();
		int cur_y = height() - event->y();

		offset_.x += cur_x - old_pos_.x;
		offset_.y += cur_y - old_pos_.y;

		old_pos_.x = cur_x;
		old_pos_.y = cur_y;

	}
	update();
	//update();
}

void ToolpathPlaneWidget::ZoomToBed() {
	scale_ = std::min(width() / bed_shape_.size().x, height() / bed_shape_.size().y) * 0.8;
	offset_.x =  (width() - scale_* bed_shape_.size().x) / 2;
	offset_.y =  (height() -scale_ * bed_shape_.size().y) / 2;
}


void ToolpathPlaneWidget::wheelEvent(QWheelEvent *event) {
	const int wheel_step = 120;
	double change_rate = 0.1;
	double change = (event->delta() < 0) ? (1 + change_rate) : (1 - change_rate);
	
	double scale1 = scale_;
	scale_ *= change;

	Pointf old_vec = Pointf(event->x() - offset_.x, height()-event->y()-offset_.y);
	Pointf new_vec = old_vec;
	new_vec.scale(change);
	offset_.x -= (new_vec.x - old_vec.x);
	offset_.y -= (new_vec.y - old_vec.y);

	update();
}


void ToolpathPlaneWidget::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		left_pressed_ = true;
		old_pos_.x = event->x();
		old_pos_.y = height() - event->y();
		//old_pos_.x = (event->x()) / static_cast<double>(width());
		//old_pos_.y = (height() - event->x()) / static_cast<double>(height());
	}
	update();
}

void ToolpathPlaneWidget::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		left_pressed_ = false;
		
	}
	update();
}


