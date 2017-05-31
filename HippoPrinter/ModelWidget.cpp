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


const float DEFAULT_COLOR[4] = { 1,1,0,1};
const float SELECTED_COLOR[4] = { 0, 1, 0, 1 };
const float HOVER_COLOR[4] = { 0.4,0.9,0,1 };

ModelWidget::ModelWidget(Print* print,QWidget * parent)
	:QGLWidget(parent),
	enable_picking_(true),
	hovered_volume_index_(-1),
	selected_volume_index_(-1),
	left_pressed_(false),
	right_pressed_(true),
	cur_mouse_x_(0), cur_mouse_y_(0),
	pre_mouse_x_(0), pre_mouse_y_(0),
	need_arrange_(false)
{
	print_ = print;

	setMouseTracking(true);
	InitModel();

	max_bbox_.defined = true;
	SetDefaultBedShape();
	
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
	
	trackball_.center = vcg::Point3f(0, 0, 0);
	trackball_.radius = 50;
	
	glLoadIdentity();
	//gluLookAt(1, 1, 1, 0, 0, 0, 0, 0, 1);
}


void ModelWidget::paintGL() {
	glClearColor(1, 1, 1, 1);
	glClearDepth(1);
	glDepthFunc(GL_LESS);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glPushMatrix();

	gluLookAt(0.06, 0, 0.1, 0, 1, 0, 0, 0, 1);
	trackball_.GetView();
	trackball_.Apply();

	glTranslatef(-max_bbox_.center().x, -max_bbox_.center().y, -max_bbox_.center().z);

	if (enable_picking_) {
		glDisable(GL_LIGHTING);
		DrawVolumes(true);
		glFlush();
		glFinish();

		GLbyte color[4];
		glReadPixels(cur_mouse_x_, height() - cur_mouse_y_, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &color);
		int volume_index = color[0] + color[1] * 256 + color[2] * 256 * 256 - 1;
		if (hovered_volume_index_ >= 0 && hovered_volume_index_ < volumes_.size()) {
			volumes_[hovered_volume_index_].SetHover(false);
		}
		if (volume_index >= 0 && volume_index < volumes_.size()) {
			hovered_volume_index_ = volume_index;
			volumes_[volume_index].SetHover(true);
		}
		else {
			hovered_volume_index_ = -1;
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glFlush();
		glFinish();
		glEnable(GL_LIGHTING);
	}

	//���Ʊ���

	//��������ϵ
	glDisable(GL_LIGHTING);
	DrawXYZ();
	glEnable(GL_LIGHTING);
	DrawBedShape();
	DrawVolumes();

	
	glPopMatrix();
	
}


void ModelWidget::resizeGL(int width, int height) {
	if (height == 0) {
		height = 1;
	}
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	Pointf3 max_bbox_size = max_bbox_.size();
	float max_size = std::max(std::max(max_bbox_size.x, max_bbox_size.y), max_bbox_size.z) * 2;
	int min_viewport_size = std::min(width, height);

	float zoom = min_viewport_size / max_size;

	float x = width / zoom;
	float y = height / zoom;
	float depth = std::max(std::max(max_bbox_size.x, max_bbox_size.y), max_bbox_size.z) * 2;
	//glOrtho(-x / 2, x / 2, -y / 2, y / 2, -depth, depth * 2);
	//glOrtho(-width / 2, width / 2, -height / 2, height / 2, -50, 50);
	glOrtho(-280, 280, -280, 280, -280, 280);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	//gluLookAt(1, 1, 1, 0, 0, 0, 0, 0, 1);
}


void ModelWidget::InitModel() {
	char* model_name = "3Dowllovely_face.stl";
	LoadModel(model_name);
}


void ModelWidget::DrawXYZ()
{
	glDisable(GL_DEPTH_TEST);

	Pointf3 bbox_size = max_bbox_.size();
	float axis_len = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z) * 0.3;

	glLineWidth(2);
	float fCursor[4];
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//��ȡ��ǰ��ɫ

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

		
		//������άģ��
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
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//��ȡ��ǰ��ɫ
	glPushMatrix();
	
	glColor4f(0.8, 0.6, 0.5,0.4);
	glNormal3d(1, 1, 1);
	glDisable(GL_LIGHTING);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glBegin(GL_QUADS);
	glColor3f(0, 0, 0);
	glVertex2f(-1.0, -1.0);
	glVertex2f(1, -1.0);
	glColor3f(10 / 255.0, 98 / 255.0, 144 / 255.0);
	glVertex2f(1, 1);
	glVertex2f(-1.0, 1);
	glEnd();

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	//glPopMatrix();

	glBegin(GL_TRIANGLES);

	glVertex3f(bed_shape_.min.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.max.y, 0);
	glVertex3f(bed_shape_.min.x, bed_shape_.max.y, 0);

	glVertex3f(bed_shape_.min.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.min.y, 0);
	glVertex3f(bed_shape_.max.x, bed_shape_.max.y, 0);

	glEnd();

	glLineWidth(3);
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


///��Qt�������¼�ת��ΪVCG���ڵ��������¼�
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
		volumes_[selected_volume_index_].Origin().translate(trans_vector.x, trans_vector.y,0);
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
	}
	else if (event->button() == Qt::RightButton) {
		right_pressed_ = true;
	}
	if (selected_volume_index_ >= 0 && selected_volume_index_ < volumes_.size()) {
		volumes_[selected_volume_index_].SetSelected(false);
		selected_volume_index_ = -1;
	}
	if (hovered_volume_index_ >= 0 && hovered_volume_index_ < volumes_.size()) {
		selected_volume_index_ = hovered_volume_index_;
		volumes_[selected_volume_index_].SetSelected(true);
	}
	trackball_.MouseDown(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
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

		rotate_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("��ת"));
		rotate_volume_menu_->addAction(rotate_volume_x_action_);
		rotate_volume_menu_->addAction(rotate_volume_y_action_);
		rotate_volume_menu_->addAction(rotate_volume_z_action_);

		mirror_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("����"));
		mirror_volume_menu_->addAction(mirror_volume_x_action_);
		mirror_volume_menu_->addAction(mirror_volume_y_action_);
		mirror_volume_menu_->addAction(mirror_volume_z_action_);

		scale_volume_menu_ = right_button_menu_->addMenu(QString::fromLocal8Bit("����"));
		scale_volume_menu_->addAction(scale_volume_u_action_);
		scale_volume_menu_->addAction(scale_volume_x_action_);
		scale_volume_menu_->addAction(scale_volume_y_action_);
		scale_volume_menu_->addAction(scale_volume_z_action_);
	}
	

	right_button_menu_->exec(QCursor::pos());
}


void ModelWidget::LoadModel(char* file_name) {
	Pointf bed_size = bed_shape_.size();
	Pointf bed_center = GetBedCenter();
	Model temp_model = Model::read_from_file(file_name);
	if (temp_model.objects.empty())
		return;
	for (ModelObject* object : temp_model.objects) {
		ModelObject* added_object = model_.add_object(*object);
		added_object->repair();

		if (object->instances.size() == 0) {
			need_arrange_ = true;
			added_object->center_around_origin();
			added_object->add_instance();
			//added_object->instances[0]->SetOffset(bed_center);
		}
		else {
			added_object->align_to_ground();
		}

// 		{
// 			Pointf3 size = added_object->bounding_box().size();
// 			double ratio = std::max(size.x, size.y) / unscale(std::max(bed_size.x, bed_size.y));
// 
// 			if (ratio > 5) {
// 				for (ModelInstance* instance : added_object->instances) {
// 					instance->SetScalingFactor(1 / ratio);
// 				}
// 			}
// 		}
		print_->auto_assign_extruders(added_object);
		print_->add_model_object(added_object);
	}
	LoadVolumes();
	if (need_arrange_)
		ArrangeObjects();
}

void ModelWidget::LoadVolumes() {
	volumes_.clear();
	for (ModelObject* object : model_.objects) {
		for (ModelVolume* model_volume : object->volumes) {
			SceneVolume scene_volume;
			scene_volume.SetBBox(model_volume->mesh.bounding_box());
			scene_volume.LoadMesh(model_volume->mesh);
			volumes_.push_back(scene_volume);
		}
	}
}

void ModelWidget::ReloadMaxBBox() {
	max_bbox_.min = Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0);
	max_bbox_.max = Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0);

	for (SceneVolume volume : volumes_) {
		max_bbox_.merge(volume.BBox());
	}
}


/*
 *	�����ȴ����װ壩��״
 */
void ModelWidget::SetBedShape(const BoundingBoxf& bed) {
	bed_shape_ = bed;
	ReloadMaxBBox();
}


/*
 *	����Ĭ�ϵ��ȴ����װ壩��״
 */
void ModelWidget::SetDefaultBedShape() {
	bed_shape_ = BoundingBoxf(Pointf(0,0),Pointf(280,180));
	ReloadMaxBBox();
}

/*
 *	��ȡ�ȴ����װ壩������
 */
Pointf ModelWidget::GetBedCenter() {
	return bed_shape_.center();
}

void ModelWidget::InitActions() {
	right_button_menu_ = new QMenu();

	reset_trackball_action_ = new QAction(QString::fromLocal8Bit("����"), this);
	delete_volume_action_ = new QAction(QString::fromLocal8Bit("ɾ��"), this);
	reload_volumes_action_ = new QAction(QString::fromLocal8Bit("����ģ��"), this);

	rotate_volume_x_action_ = new QAction(QString::fromLocal8Bit("��X��"), this);
	rotate_volume_y_action_ = new QAction(QString::fromLocal8Bit("��Y��"), this);
	rotate_volume_z_action_ = new QAction(QString::fromLocal8Bit("��Z��"), this);
	
	mirror_volume_x_action_ = new QAction(QString::fromLocal8Bit("X����"));
	mirror_volume_y_action_ = new QAction(QString::fromLocal8Bit("Y����"));
	mirror_volume_z_action_ = new QAction(QString::fromLocal8Bit("Z����"));

	scale_volume_u_action_ = new QAction(QString::fromLocal8Bit("�ȱ�����"),this);
	scale_volume_x_action_ = new QAction(QString::fromLocal8Bit("��X��"), this);
	scale_volume_y_action_ = new QAction(QString::fromLocal8Bit("��Y��"), this);
	scale_volume_z_action_ = new QAction(QString::fromLocal8Bit("��Z��"), this);

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
	model_.delete_object(selected_volume_index_);
	selected_volume_index_ = -1;
	LoadVolumes();
	update();
}

void ModelWidget::RotateVolumeX() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("��X����ת"),
		QString::fromLocal8Bit("��ת�ĽǶȣ�"),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00) {
			return;
		}
		model_.objects[selected_volume_index_]->rotate(input_num, Axis::X);
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::RotateVolumeY() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("��Y����ת"),
		QString::fromLocal8Bit("��ת�ĽǶȣ�"),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00) {
			return;
		}
		model_.objects[selected_volume_index_]->rotate(input_num, Axis::Y);
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::RotateVolumeZ() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("��Z����ת"),
		QString::fromLocal8Bit("��ת�ĽǶȣ�"),
		0,
		-180, 180,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00) {
			return;
		}
		model_.objects[selected_volume_index_]->rotate(input_num, Axis::Z);
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::MirrorVolumeX() {
	model_.objects[selected_volume_index_]->mirror(Axis::X);
	LoadVolumes();
	update();
}

void ModelWidget::MirrorVolumeY() {
	model_.objects[selected_volume_index_]->mirror(Axis::Y);
	LoadVolumes();
	update();
}

void ModelWidget::MirrorVolumeZ() {
	model_.objects[selected_volume_index_]->mirror(Axis::Z);
	model_.objects[selected_volume_index_]->update_bounding_box();
	//����print�ڵ�model object
	print_->add_model_object(model_.objects[selected_volume_index_], selected_volume_index_);
	LoadVolumes();
	update();
}

void ModelWidget::ScaleVolumeUniformly() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("�ȱ�����"),
		QString::fromLocal8Bit("������0-100����"),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00) {
			return;
		}
		model_.objects[selected_volume_index_]->scale(Pointf3(input_num/100.0, input_num/100.0, input_num/100.0));
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::ScaleVolumeX() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("X�᷽������"),
		QString::fromLocal8Bit("������0-100����"),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00) {
			return;
		}
		model_.objects[selected_volume_index_]->scale(Pointf3(input_num / 100.0, 1, 1));
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::ScaleVolumeY() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("Y�᷽������"),
		QString::fromLocal8Bit("������0-100����"),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00) {
			return;
		}
		model_.objects[selected_volume_index_]->scale(Pointf3(1, input_num/100.0, 1));
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::ScaleVolumeZ() {
	bool ok;
	double input_num = QInputDialog::getDouble(this,
		QString::fromLocal8Bit("Z�᷽������"),
		QString::fromLocal8Bit("������0-100����"),
		100,
		0, 1000,
		2,
		&ok);
	if (ok) {
		if (input_num == 0.00 || input_num == 100.00) {
			return;
		}
		model_.objects[selected_volume_index_]->scale(Pointf3(1, 1, input_num / 100.0));
		LoadVolumes();
		update();
	}
	return;
}

void ModelWidget::ReloadAllVolumes() {
	LoadVolumes();
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



/*
	��ʼ����ģ��
*/
void ModelWidget::BackgroundProcess() {
	print_->Process();
}



/*
 *	�Դ�ӡ����������²���
 */
void ModelWidget::ArrangeObjects() {
	model_.arrange_objects(print_->config.min_object_distance(), &bed_shape_);
	LoadVolumes();
}