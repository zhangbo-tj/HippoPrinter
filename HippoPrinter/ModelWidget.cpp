#include "ModelWidget.h"

#include <algorithm>

#include <QLabel>
#include <QIcon>
#include <GL\glut.h>

#include <QMouseEvent>
#include <QOpenGLFunctions>
#include <QDebug>
#include <QContextMenuEvent>
#include <QAction>
#include <QMenu>
#include <QInputDialog>

const float COLORS[4][4] = { { 1,0.95,0.2,1 },{ 1,0.45,0.45,1 },{ 0.5,1,0.5,1 },{ 0.5,0.5,1,1} };
const float DEFAULT_COLOR[4] = { 1,1,0,1};
const float SELECTED_COLOR[4] = { 0, 1, 0, 1 };
const float HOVER_COLOR[4] = { 0.4,0.9,0,1 };

ModelWidget::ModelWidget(Print* print,Model* model,QWidget * parent)
	:QGLWidget(parent),
	enable_picking_(true),
	color_by_(color_by_volume),select_by_(select_by_object),drag_by_(drag_by_instance),

	hovered_volume_index_(-1),
	selected_volume_index_(-1),
	left_pressed_(false),
	right_pressed_(true),
	cur_mouse_x_(0), cur_mouse_y_(0),
	pre_mouse_x_(0), pre_mouse_y_(0),
	need_arrange_(false),scale_(1)
{
	print_ = print;
	model_ = model;
	
	LoadBedShape();

	ResetVolumes();

	setMouseTracking(true);

	max_bbox_.defined = true;
	
	InitActions();
}



ModelWidget::~ModelWidget(){
	
}

void ModelWidget::initializeGL() {
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
	GLfloat light_specular1[] = {0.3,0.3,0.3,1};
	GLfloat light_position1[] = { 1,0,1,0 };
	glLightfv(GL_LIGHT1, GL_POSITION, light_position1);
	glLightfv(GL_LIGHT1, GL_AMBIENT, light_model_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light0_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);
	
	
	glLoadIdentity();
	//gluLookAt(1, 1, 1, 0, 0, 0, 0, 0, 1);

	ZoomToBed();
}


void ModelWidget::paintGL() {
	glClearColor(1, 1, 1, 1);
	glClearDepth(1);
	glDepthFunc(GL_LESS);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();

	glPushMatrix();

	
	trackball_.GetView();
	trackball_.Apply();

	glScaled(scale_, scale_, scale_);
	glTranslatef(-bed_shape_.center().x, -bed_shape_.center().y, 0);
	gluLookAt(trackball_.center.X(), trackball_.center.Y() - 1, trackball_.center.Z() + 1,
		trackball_.center.X(), trackball_.center.Y(), trackball_.center.Z(),
		0, 0, 1);
	

	if (enable_picking_) {
		glDisable(GL_LIGHTING);
		DrawVolumes(true);
		glFlush();
		glFinish();

		GLbyte color[4];
		glReadPixels(cur_mouse_x_, height() - cur_mouse_y_, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
		int volume_index = color[0] + color[1] * 256 + color[2] * 256 * 256 - 1;
		hovered_volume_index_ = -1;
		for (SceneVolume& volume : volumes_) {
			volume.SetHover(false);
		}
		if (volume_index >= 0 && volume_index <= volumes_.size()) {
			hovered_volume_index_ = volume_index;

			volumes_[volume_index].SetHover(true);

			int group_id = volumes_[volume_index].select_group_id_;
			if (group_id != -1) {
				for (SceneVolume& volume : volumes_) {
					if (volume.select_group_id_ == group_id) {
						volume.SetHover(true);
					}
				}
			}

		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glFlush();
		glFinish();
		glEnable(GL_LIGHTING);
	}

	//绘制背景

	//绘制坐标系
	
	DrawBedShape();
	DrawVolumes();
	
	glDisable(GL_LIGHTING);
	DrawAxes();
	glEnable(GL_LIGHTING);

	glPopMatrix();
}


void ModelWidget::resizeGL(int width, int height) {
	if (height == 0) {
		height = 1;
	}
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	Pointf3 bbox_size = max_bbox_.size();
	double max_size = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z)*10;


	glOrtho(-width / (2 * scale_), width / (2 * scale_), -height / (2 * scale_),
		height / (2 * scale_), -max_size, max_size*2);


	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


// 	if (volumes_.empty()) {
// 		ZoomToBed();
// 	}
// 	else {
// 		ZoomToVolumes();
// 	}
	LoadMaxBBox();
	ZoomToBBox(max_bbox_);
}


void ModelWidget::InitModel() {
	char* model_name = "3Dowllovely_face.stl";
	//LoadModel(model_name);
}


void ModelWidget::DrawAxes()
{
	glDisable(GL_DEPTH_TEST);

	Pointf3 bbox_size = max_bbox_.size();
	float axis_len = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z) * 0.8;

	glLineWidth(2);
	float fCursor[4];
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//获取当前颜色

	glBegin(GL_LINES);
	glColor3f(1.0, 0.0, 0.0); //X
	glVertex3f(0, 0, 0);
	glVertex3f(axis_len, 0, 0);

	glColor3f(0.0, 1.0, 0.0);//Y
	glVertex3f(0, 0, 0);
	glVertex3f(0, axis_len, 0);

	glEnable(GL_DEPTH_TEST);
	glColor3f(0.0, 0.0, 1.0);//Z
	glVertex3f(0, 0, 0);
	glVertex3f(0, 0, axis_len);
	glEnd();

	glColor4fv(fCursor);
	glLineWidth(1.0);
}



/*
 *	绘制模型
 */
void ModelWidget::DrawVolumes(bool fakecolor)const {

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	for (int volume_index = 0; volume_index < volumes_.size(); volume_index++) {
		SceneVolume volume = volumes_[volume_index];
		
		
		glPushMatrix();
		glTranslatef(volume.Origin().x, volume.Origin().y, volume.Origin().z);

		if (fakecolor) {
			int r = ((volume_index+1) & 0x000000FF) >> 0;
			int g = ((volume_index + 1) & 0x0000FF00) >> 8;
			int b = ((volume_index + 1) & 0x00FF0000) >> 16;
			glColor4f(r / 255.0, g / 255.0, b / 255.0, 1);
		}
		else if (volume.Selected()) {
			glColor4fv(SELECTED_COLOR);
		}
		else if (volume.Hover()) {
			glColor4fv(HOVER_COLOR);
		}
		else {
			glColor4fv(DEFAULT_COLOR);
		}

		
		//绘制三维模型
		if (!volume.qverts_.verts.empty()) {
			int min_offset = 0;
			int max_offset = volume.qverts_.verts.size();
			glCullFace(GL_BACK);
			glVertexPointer(3, GL_FLOAT, 0, volume.qverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.qverts_.norms.data());
			glDrawArrays(GL_QUADS, 0, max_offset / 3);
		}
		if (!volume.tverts_.verts.empty()) {
			int min_offset = 0;
			int max_offset = volume.tverts_.verts.size();
			glCullFace(GL_BACK);
			glVertexPointer(3, GL_FLOAT, 0, volume.tverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.tverts_.norms.data());
			glDrawArrays(GL_TRIANGLES, 0, max_offset / 3);
		}
		
		glPopMatrix();
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
}

void ModelWidget::DrawBedShape()const {
	float fCursor[4];
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//获取当前颜色
	glPushMatrix();
	
	glColor4f(0.8, 0.6, 0.5,0.4);
	glNormal3d(1, 1, 1);
	glDisable(GL_LIGHTING);
	glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();

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
	for(int i =  0; i < 280;i+=10){
		glVertex3f(i, 0, 0);
		glVertex3f(i, 180, 0);
	}
	glEnd();

	//glFlush();
	glColor4fv(fCursor);
	glEnable(GL_LIGHTING);
	
	glPopMatrix();
}


///将Qt鼠标键盘事件转换为VCG库内的鼠标键盘事件
vcg::Trackball::Button ModelWidget::QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers)
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

void ModelWidget::mouseMoveEvent(QMouseEvent *event) {
	pre_mouse_x_ = cur_mouse_x_;
	pre_mouse_y_ = cur_mouse_y_;

	cur_mouse_x_ = event->x();
	cur_mouse_y_ = event->y();

	if (hovered_volume_index_ == -1) {
		if (selected_volume_index_ >= 0 && selected_volume_index_ < volumes_.size()) {
			volumes_[selected_volume_index_].SetSelected(false);
		}
		selected_volume_index_ = -1;
		trackball_.MouseMove(event->x(), height()- event->y());
		update();
		return;
	}
	if (left_pressed_ && selected_volume_index_ >= 0 && selected_volume_index_ < volumes_.size()) {
		Pointf3 curPos;
		UnProject(cur_mouse_x_, height() - cur_mouse_y_, curPos);
		Pointf3 prePos;
		UnProject(pre_mouse_x_, height() - pre_mouse_y_, prePos);

		Pointf3 trans_vector(curPos.x - prePos.x, curPos.y - prePos.y, curPos.z - prePos.z);
		

		int group_id = volumes_[selected_volume_index_].drag_group_id_;
		if (group_id != -1) {
			for (SceneVolume& volume : volumes_) {
				volume.Origin().translate(trans_vector.x, trans_vector.y, 0);
			}
		}
		else {
			volumes_[selected_volume_index_].Origin().translate(trans_vector.x, trans_vector.y, 0);
		}
	}
	update();
}

void ModelWidget::wheelEvent(QWheelEvent* event) {
	if (event->delta() > 0) {
		trackball_.MouseWheel(1);
	}
	else {
		trackball_.MouseWheel(-1);
	}
	update();
}

void ModelWidget::mousePressEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		left_pressed_ = true;
		if (enable_picking_) {
			selected_volume_index_ = hovered_volume_index_;
			for (SceneVolume& volume : volumes_) {
				volume.SetSelected(false);
			}
			if (selected_volume_index_ != -1) {
				volumes_[selected_volume_index_].SetSelected(true);
				int group_id = volumes_[selected_volume_index_].select_group_id_;
				if (group_id != -1) {
					for (SceneVolume& volume : volumes_) {
						if (volume.select_group_id_ == group_id) {
							volume.SetSelected(true);
						}
					}
				}

			}

		}
	}
	if (selected_volume_index_ == -1) {
		trackball_.MouseDown(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
	}
	update();
}

void ModelWidget::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		left_pressed_ = false;
	}
	else if (event->button() == Qt::RightButton) {
		right_pressed_ = false;
	}
	trackball_.MouseUp(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
}


void ModelWidget::keyReleaseEvent(QKeyEvent *event) {
	if (event->key() == Qt::Key_Control) {
		trackball_.ButtonUp(QT2VCG(Qt::NoButton, Qt::ControlModifier));
	}
	if (event->key() == Qt::Key_Shift) {
		trackball_.ButtonUp(QT2VCG(Qt::NoButton, Qt::ShiftModifier));
	}
	if (event->key() == Qt::Key_Alt) {
		trackball_.ButtonUp(QT2VCG(Qt::NoButton, Qt::AltModifier));
	}
}

void ModelWidget::contextMenuEvent(QContextMenuEvent *event) {
	right_button_menu_->clear();
	right_button_menu_->addAction(reset_trackball_action_);
	right_button_menu_->addAction(reload_volumes_action_);

	if (selected_volume_index_ != -1) {
		right_button_menu_->addSeparator();
		right_button_menu_->addAction(delete_volume_action_);

		rotate_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("旋转"));
		rotate_volume_menu_->addAction(rotate_volume_x_action_);
		rotate_volume_menu_->addAction(rotate_volume_y_action_);
		rotate_volume_menu_->addAction(rotate_volume_z_action_);

		mirror_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("镜像"));
		mirror_volume_menu_->addAction(mirror_volume_x_action_);
		mirror_volume_menu_->addAction(mirror_volume_y_action_);
		mirror_volume_menu_->addAction(mirror_volume_z_action_);

		scale_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("缩放"));
		scale_volume_menu_->addAction(scale_volume_u_action_);
		scale_volume_menu_->addAction(scale_volume_x_action_);
		scale_volume_menu_->addAction(scale_volume_y_action_);
		scale_volume_menu_->addAction(scale_volume_z_action_);
	}
	

	right_button_menu_->exec(QCursor::pos());
}



void ModelWidget::ReloadMaxBBox() {
	max_bbox_.min = Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0);
	max_bbox_.max = Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0);

	for (SceneVolume volume : volumes_) {
		max_bbox_.merge(volume.BBox());
	}
}


/*
 *	设置热床（底板）形状
 */
void ModelWidget::SetBedShape(const BoundingBoxf& bed) {
	bed_shape_ = bed;
	ReloadMaxBBox();
}


/*
 *	设置默认的热床（底板）形状
 */
void ModelWidget::SetDefaultBedShape() {
	bed_shape_ = BoundingBoxf(Pointf(0,0),Pointf(280,180));
	ReloadMaxBBox();
}

/*
 *	获取热床（底板）的中心
 */
Pointf ModelWidget::GetBedCenter() {
	return bed_shape_.center();
}

void ModelWidget::InitActions() {
	right_button_menu_ = new QMenu();

	reset_trackball_action_ = new QAction(QString::fromLocal8Bit("重置"), this);
	delete_volume_action_ = new QAction(QString::fromLocal8Bit("删除"), this);
	reload_volumes_action_ = new QAction(QString::fromLocal8Bit("重载模型"), this);

	rotate_volume_x_action_ = new QAction(QString::fromLocal8Bit("绕X轴"), this);
	rotate_volume_y_action_ = new QAction(QString::fromLocal8Bit("绕Y轴"), this);
	rotate_volume_z_action_ = new QAction(QString::fromLocal8Bit("绕Z轴"), this);
	
	mirror_volume_x_action_ = new QAction(QString::fromLocal8Bit("X方向"));
	mirror_volume_y_action_ = new QAction(QString::fromLocal8Bit("Y方向"));
	mirror_volume_z_action_ = new QAction(QString::fromLocal8Bit("Z方向"));

	scale_volume_u_action_ = new QAction(QString::fromLocal8Bit("等比缩放"),this);
	scale_volume_x_action_ = new QAction(QString::fromLocal8Bit("沿X轴"), this);
	scale_volume_y_action_ = new QAction(QString::fromLocal8Bit("沿Y轴"), this);
	scale_volume_z_action_ = new QAction(QString::fromLocal8Bit("沿Z轴"), this);

	connect(reset_trackball_action_, &QAction::triggered, this, &ModelWidget::ResetTrackball);
	connect(delete_volume_action_, &QAction::triggered, this, &ModelWidget::DeleteVolume);
	connect(reload_volumes_action_ , &QAction::triggered, this, &ModelWidget::ReloadAllVolumes);

	connect(rotate_volume_x_action_, &QAction::triggered, this, &ModelWidget::RotateVolumeX);
	connect(rotate_volume_y_action_, &QAction::triggered, this, &ModelWidget::RotateVolumeY);
	connect(rotate_volume_z_action_, &QAction::triggered, this, &ModelWidget::RotateVolumeZ);

	connect(mirror_volume_x_action_, &QAction::triggered, this, &ModelWidget::MirrorVolumeX);
	connect(mirror_volume_y_action_, &QAction::triggered, this, &ModelWidget::MirrorVolumeY);
	connect(mirror_volume_z_action_, &QAction::triggered, this, &ModelWidget::MirrorVolumeZ);

	connect(scale_volume_u_action_, &QAction::triggered, this, &ModelWidget::ScaleVolumeUniformly);
	connect(scale_volume_x_action_, &QAction::triggered, this, &ModelWidget::ScaleVolumeX);
	connect(scale_volume_y_action_, &QAction::triggered, this, &ModelWidget::ScaleVolumeY);
	connect(scale_volume_z_action_, &QAction::triggered, this, &ModelWidget::ScaleVolumeZ);
}

void ModelWidget::ResetTrackball() {
	trackball_.SetIdentity();
	update();
}

void ModelWidget::UnProject(int mouse_x, int mouse_y, Pointf3& world) {
	glPushMatrix();

	GLdouble model_view[16];
	GLdouble projection[16];
	GLint view_port[4];

	glGetDoublev(GL_MODELVIEW_MATRIX, model_view);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetIntegerv(GL_VIEWPORT, view_port);

	float win_z;
	glReadPixels(mouse_x, mouse_y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &win_z);

	gluUnProject((GLdouble)mouse_x, (GLdouble)mouse_y, win_z,
		model_view, projection, view_port,
		&world.x, &world.y, &world.z);
	glPopMatrix();
}


void ModelWidget::DeleteVolume() {
	if (selected_volume_index_ == -1) {
		return;
	}

	int object_index = volume_to_object_[selected_volume_index_].first.first;

	model_->delete_object(object_index);
	print_->delete_object(object_index);
	ReloadVolumes();

	//model_.delete_object(selected_volume_index_);
	selected_volume_index_ = -1;
	//LoadVolumes();
	update();
}


/*
 *	对选中的打印对象绕X轴进行旋转
 */
void ModelWidget::RotateVolumeX() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("绕X轴旋转"),
		QString::fromLocal8Bit("旋转的角度："),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || selected_volume_index_ == -1) {
			return;
		}

		int object_index = volume_to_object_[selected_volume_index_].first.first;
		int instance_index = volume_to_object_[selected_volume_index_].second;

		ModelObject* object = model_->objects[object_index];
		ModelInstance* instance = object->instances[instance_index];

		object->transform_by_instance(*instance, 1);

		object->rotate(input_num, Axis::X);
		object->center_around_origin();

		object->update_bounding_box();
		print_->add_model_object(object, object_index);
		ReloadVolumes();
		//model_.objects[selected_volume_index_]->rotate(input_num, Axis::Y);
		//LoadVolumes();
		update();
	}
	return;
}


/*
 *	对选中的打印对象绕Y轴进行旋转
 */
void ModelWidget::RotateVolumeY() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("绕Y轴旋转"),
		QString::fromLocal8Bit("旋转的角度："),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00||selected_volume_index_ == -1) {
			return;
		}

		int object_index = volume_to_object_[selected_volume_index_].first.first;
		int instance_index = volume_to_object_[selected_volume_index_].second;

		ModelObject* object = model_->objects[object_index];
		ModelInstance* instance = object->instances[instance_index];

		object->transform_by_instance(*instance, 1);

		object->rotate(input_num, Axis::Y);
		object->center_around_origin();

		object->update_bounding_box();
		print_->add_model_object(object, object_index);
		ReloadVolumes();
		//model_.objects[selected_volume_index_]->rotate(input_num, Axis::Y);
		//LoadVolumes();
		update();
	}
	return;
}


/*
 *	对选中的打印对象绕Z轴进行旋转
 */
void ModelWidget::RotateVolumeZ() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("绕Z轴旋转"),
		QString::fromLocal8Bit("旋转的角度："),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 ||selected_volume_index_ == -1) {
			return;
		}

		int object_index = volume_to_object_[selected_volume_index_].first.first;
		
		ModelObject* object = model_->objects[object_index];
		for (ModelInstance* instance : object->instances) {
			double rotation = instance->rotation;
			instance->SetRotation(rotation + input_num);
		}

		object->update_bounding_box();
		print_->add_model_object(object, object_index);

		ReloadVolumes();
		//model_.objects[selected_volume_index_]->rotate(input_num, Axis::Z);
		//LoadVolumes();
		update();
	}
	return;
}


/*
 *	对打印对象做镜像操作
 */
void ModelWidget::MirrorVolumeX() {
	MirrorVolume(Axis::X);
}

void ModelWidget::MirrorVolumeY() {
	MirrorVolume(Axis::Y);
}

void ModelWidget::MirrorVolumeZ() {
	MirrorVolume(Axis::Z);
}


/*
 *	对选中的打印对象进行镜像操作
 */
void ModelWidget::MirrorVolume(Axis axis) {
	if (selected_volume_index_ == -1) return;

	int selected_obj_idx = volume_to_object_[selected_volume_index_].first.first;
	int selected_instance_idx = volume_to_object_[selected_volume_index_].second;

	ModelObject* object = model_->objects[selected_obj_idx];
	ModelInstance* instance = object->instances[selected_instance_idx];

	object->transform_by_instance(*instance, 1);

	model_->objects[selected_obj_idx]->mirror(axis);
	object->update_bounding_box();
	object->center_around_origin();

	print_->add_model_object(object, selected_obj_idx);

	//model_->center_instances_around_point(bed_shape_.center());
	//model_->objects[selected_volume_index_]->mirror(Axis::X);
	ReloadVolumes();
	update();
}

/*
 *	对打印对象等比缩放
 */
void ModelWidget::ScaleVolumeUniformly() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("等比缩放"),
		QString::fromLocal8Bit("比例（0-100）："),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00||selected_volume_index_ == -1) {
			return;
		}
		int selected_object_index = volume_to_object_[selected_volume_index_].first.first;
		ModelObject* object = model_->objects[selected_object_index];
		for (ModelInstance* instance : object->instances) {
			instance->SetScalingFactor(input_num / 100.0);
		}

		object->update_bounding_box();
		print_->add_model_object(object, selected_object_index);
		ReloadVolumes();
		update();
	}
	return;
}


/*
 *	对打印对象沿X轴缩放
 */
void ModelWidget::ScaleVolumeX() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("X轴方向缩放"),
		QString::fromLocal8Bit("比例（0-100）："),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00||selected_volume_index_ == -1) {
			return;
		}
		

		int selected_object_idx = volume_to_object_[selected_volume_index_].first.first;
		int selected_instance_idx = volume_to_object_[selected_volume_index_].second;

		ModelObject* object = model_->objects[selected_object_idx];
		ModelInstance* instance = object->instances[selected_instance_idx];

		object->transform_by_instance(*instance, 1);
		object->scale(Pointf3(input_num / 100.0, 1, 1));

		object->update_bounding_box();
		print_->add_model_object(object, selected_object_idx);
		ReloadVolumes();
		update();

	}
	return;
}


/*
 *	对打印对象沿Y轴方向缩放
 */
void ModelWidget::ScaleVolumeY() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("Y轴方向缩放"),
		QString::fromLocal8Bit("比例（0-100）："),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00 || selected_volume_index_ == -1) {
			return;
		}


		int selected_object_idx = volume_to_object_[selected_volume_index_].first.first;
		int selected_instance_idx = volume_to_object_[selected_volume_index_].second;

		ModelObject* object = model_->objects[selected_object_idx];
		ModelInstance* instance = object->instances[selected_instance_idx];

		object->transform_by_instance(*instance, 1);
		object->scale(Pointf3(1, input_num / 100.0, 1));

		object->update_bounding_box();
		print_->add_model_object(object, selected_object_idx);
		ReloadVolumes();
		update();
	}
	return;
}

void ModelWidget::ScaleVolumeZ() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("Z轴方向缩放"),
		QString::fromLocal8Bit("比例（0-100）："),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00 || selected_volume_index_ == -1) {
			return;
		}


		int selected_object_idx = volume_to_object_[selected_volume_index_].first.first;
		int selected_instance_idx = volume_to_object_[selected_volume_index_].second;

		ModelObject* object = model_->objects[selected_object_idx];
		ModelInstance* instance = object->instances[selected_instance_idx];

		object->transform_by_instance(*instance, 1);
		object->scale(Pointf3(1, 1, input_num / 100.0));

		object->update_bounding_box();
		print_->add_model_object(object, selected_object_idx);
		ReloadVolumes();
		update();
	}
	return;
}

void ModelWidget::ReloadAllVolumes() {
	//LoadVolumes();
	update();
}

void ModelWidget::AlignObjectToGround(ModelObject* new_object) {
	BoundingBoxf3 bb;
	for (const ModelVolume* v : new_object->volumes) {
		bb.merge(v->mesh.bounding_box());
	}
	new_object->translate(0, 0, -bb.min.z);
	new_object->origin_translation.translate(0, 0, -bb.min.z);
}

void ModelWidget::ReloadVolumes() {
	ResetVolumes();
	
	LoadBedShape();

	for (int obj_idx = 0; obj_idx < model_->objects.size(); obj_idx++) {
		LoadModelObject(obj_idx);
	}

	LoadMaxBBox();
	//ZoomToVolumes();
	ZoomToBBox(max_bbox_);
}


/*
 *	载入底板形状
 */
void ModelWidget::LoadBedShape() {
	bed_shape_ = BoundingBoxf(print_->config.bed_shape.values);
}

/*
 *	清空所有的volumes
 */
void ModelWidget::ResetVolumes() {
	volumes_.clear();
}




/*
 *	导入ModelObject
 */
void ModelWidget::LoadModelObject(int object_idx) {
	ModelObject* object = model_->objects[object_idx];

	std::vector<int> volumes_idx;
	for (int volume_idx = 0; volume_idx < object->volumes.size(); volume_idx++) {
		ModelVolume* volume = object->volumes[volume_idx];
		for (int instance_idx = 0; instance_idx < object->instances.size(); instance_idx++) {
			ModelInstance* instance = object->instances[instance_idx];

			TriangleMesh mesh = volume->mesh;
			instance->transform_mesh(&mesh);


			int color_idx;
			if (color_by_ == color_by_volume) {
				color_idx = volume_idx;
			}
			else {
				color_idx = object_idx;
			}

			const float *color = COLORS[color_idx % 4];

			SceneVolume volume;
			volume.bbox_ = mesh.bounding_box();
			volume.color[0] = color[0];	volume.color[1] = color[1];
			volume.color[2] = color[2];	volume.color[3] = color[3];

			//设置select by group id
			if (select_by_ == select_by_object) {
				volume.select_group_id_ = object_idx * 1000000;
			}
			else if (select_by_ == select_by_volume) {
				volume.select_group_id_ = object_idx * 1000000 + volume_idx * 1000;
			}
			else {
				volume.select_group_id_ = object_idx * 1000000 + volume_idx * 1000 + instance_idx;
			}

			//设置drag by group id
			if (drag_by_ == drag_by_object) {
				volume.drag_group_id_ = object_idx * 1000;
			}
			else {
				volume.drag_group_id_ = object_idx * 1000 + instance_idx;
			}

			volume.LoadMesh(mesh);

			volumes_.push_back(volume);
			
			int scenevolume_index = volumes_.size();
			volumes_idx.push_back(scenevolume_index);
			volume_to_object_[scenevolume_index] = { {object_idx,volume_idx},instance_idx };
			
		}
	}
	object_to_volumes_[object_idx] = volumes_idx;
}



/*
 *	对BoundingBox进行缩放
 */
void ModelWidget::ZoomToBBox(BoundingBoxf3& bbox) {
	Pointf3 bbox_size = bbox.size();
	double max_size = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z)*1.1;
	int min_viewport_size = std::min(width(), height());

	if (max_size != 0) {
		trackball_.radius = std::min(std::min(bbox_size.x,bbox_size.y),bbox_size.z);
		scale_ = min_viewport_size / max_size;
		//glTranslatef(-bbox.center().x, -bbox.center().y, 0);
	}
}


void ModelWidget::ZoomToVolumes() {
	BoundingBoxf3 bbox;

	for (SceneVolume& volume : volumes_) {
		bbox.merge(volume.TransformedBBox());
	}

	ZoomToBBox(bbox);
}

void ModelWidget::ZoomToBed() {
	BoundingBoxf3 bbox;
	
	bbox.merge(Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0));
	bbox.merge(Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0));
	trackball_.radius = std::min(bbox.size().x, bbox.size().y);

	ZoomToBBox(bbox);
}


/*
 *	计算最大Bouding Box(包括Bed Shape和Scene Volumes)
 */
void ModelWidget::LoadMaxBBox() {
	max_bbox_.min = Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0);
	max_bbox_.max = Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0);

	for (SceneVolume& volume : volumes_) {
		max_bbox_.merge(volume.TransformedBBox());
	}
}