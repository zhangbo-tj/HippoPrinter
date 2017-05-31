#include "ToolpathPreviewWidget.h"

#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QOpenGLFunctions>

#include <unordered_map>

#include <GL\glut.h>

#include <src/slic3r/GUI/3DScene.hpp>

const float COLORS[4][4] = { { 1,0.95,0.2,1 },{ 1,0.45,0.45,1 },{ 0.5,1,0.5,1 },{ 0.5,0.5,1,1 } };
const float DEFAULT_COLOR[4] = { 1,1,0,1 };
const float SELECTED_COLOR[4] = { 0, 1, 0, 1 };
const float HOVER_COLOR[4] = { 0.4,0.9,0,1 };


/*
 *	���캯��
 *  ����ΪPrintָ��
 */
ToolpathPreviewWidget::ToolpathPreviewWidget(Print* print,QWidget* parent)
	:QGLWidget(parent),
	loaded_(false),
	color_toolpaths_by_(ctRole),
	min_z_(-1), max_z_(-1)
{
	print_ = print;
	setMouseTracking(true);

	bbox_.defined = true;
	SetDefaultBedshape();

	InitActions();
}


/*
 *	��������
 */
ToolpathPreviewWidget::~ToolpathPreviewWidget(){

}


/*
 *	��ʼ���ؼ�
 */
void ToolpathPreviewWidget::initializeGL() {
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


/*
 *	��Ⱦ�ؼ�
 */
void ToolpathPreviewWidget::paintGL() {
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

	glTranslatef(-bbox_.center().x, -bbox_.center().y, -bbox_.center().z);


	//���Ʊ���

	//��������ϵ
	glDisable(GL_LIGHTING);
	DrawAxes();
	glEnable(GL_LIGHTING);

	//���Ƶװ�
	DrawBedShape();

	//���ƴ�ӡ·��
	DrawVolumes();


	glPopMatrix();
}


/*
 *	���ؼ���С�����ı�ʱ�Ĳ���
 */
void ToolpathPreviewWidget::resizeGL(int width, int height) {
	if (height == 0) {
		height = 1;
	}
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	Pointf3 max_bbox_size = bbox_.size();
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


/*
 *	���������ӡ·��
 */
void ToolpathPreviewWidget::ReloadPrint() {
	ResetVolumes();		//��յ�ǰ�Ĵ�ӡ·��
	max_z_ = -1;
	loaded_ = false;
	LoadPrint();		//����Print����Ĵ�ӡ·��
	update();
}


/*
 *	������еĴ�ӡ·��
 */
void ToolpathPreviewWidget::ResetVolumes() {
	volumes_.clear();
}


/*
 *	����Print����Ĵ�ӡ·��
 */
void ToolpathPreviewWidget::LoadPrint() {
	//�����ӡ����û�����Slice���������˳�
	if (!print_->step_done(posSlice)) {
		volumes_.clear();
		update();
		return;
	}

	//����print_z,����������
	std::set<double> values;
	for (PrintObject* object : print_->objects) {
		for (Layer* layer : object->layers) {
			values.insert(layer->print_z);
		}
	}

	//�洢<layer_id, print_z>���������ͨ��layer_id��ȡprint_z
	auto iterator = values.begin();
	for (int i = 0; i < values.size(); i++) {
		layer_values_[i] = *iterator;
		++iterator;
	}


	//�ֱ���ÿһ����ӡ����Ĵ�ӡ·��
	for (PrintObject* object : print_->objects) {
		LoadPrintObjectToolpaths(object);
	}

	//������״̬��Ϊ1
	loaded_ = true;

	//������ʾ���Ͻ�Ϊ����ģ�͵�Bouding box
	min_z_ = 0;
	max_z_ = layer_values_[values.size() - 1];
}




/*
 *	�����ӡ�����toolpaths��Ŀǰ������support materials
 */
void ToolpathPreviewWidget::LoadPrintObjectToolpaths(const PrintObject* object) {
	
	//����print_z��Layers��������,���ܲ�����Ҫ
	std::vector<Layer*> layers(object->layers.begin(), object->layers.end());
	std::sort(layers.begin(), layers.end(), [](Layer* x, Layer* y) {
		return x->print_z < y->print_z; 
	});

	//�����ӡ�����Bounding box,����ƽ�ƺ�ĸ���Ʒ
	BoundingBoxf3 bbox;
	BoundingBox obj_bbox = object->bounding_box();
	for (auto& copy : object->_shifted_copies) {
		BoundingBox copy_bbox = obj_bbox;
		copy_bbox.translate(copy.x, copy.y);
		bbox.merge(Pointf3::new_unscale(copy_bbox.min.x, copy_bbox.min.y, 0));
		bbox.merge(Pointf3::new_unscale(copy_bbox.max.x, copy_bbox.max.y, 0));
	}
	

	long alloc_size_max = 32 * 1048576 / 4;

	//�Ƿ���extruder�Դ�ӡ·������Ⱦɫ
	bool color_toolpaths_by_extruder = color_toolpaths_by_ == ctExtruder;

	//�ֱ�ÿһ���ϵ�ÿһ��reigion�ڵĴ�ӡ·����ӽ���
	for (Layer* layer : layers) {
		coordf_t top_z = layer->print_z;
		for (auto& copy : object->_shifted_copies) {
			for (LayerRegion* region : layer->regions) {
				//���perimters·��
				if (object->state.is_done(posPerimeters)) {
					 int color_index = color_toolpaths_by_extruder ?
						 (region->region()->config.perimeter_extruder - 1) % 4 : 0;
					//AddScenevolume(region->perimeters, top_z, copy, color, bbox);
					AddScenevolume(region->perimeters, top_z, copy, color_index, bbox);
				}

				//���Infill·��
				if (object->state.is_done(posInfill)) {
					int color_index = color_toolpaths_by_extruder ?
						(region->region()->config.infill_extruder - 1) % 4 : 1;
					if (color_toolpaths_by_extruder&&
						region->region()->config.infill_extruder != region->region()->config.solid_infill_extruder) {
					
						ExtrusionEntityCollection solid_entities;
						ExtrusionEntityCollection non_solid_entities;

						//��solid��non-solid infillʹ�ò�ͬ����ɫ��ʾ
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

						AddScenevolume(non_solid_entities, top_z, copy, color_index, bbox);
						color_index = (region->region()->config.solid_infill_extruder - 1) % 4;
						AddScenevolume(solid_entities, top_z, copy, color_index, bbox);
					}
					else {
						AddScenevolume(region->fills, top_z, copy, color_index, bbox);
					}
				}
			}
		}
	}
}


/*
 *	����ӡ·����ӵ���ӡ������
 *  ����Ѿ����ڸ�color_index��Ӧ��SceneVolume���󣬾�ֱ������qverts���������
 *  �����ǰ�����ڸ�color_index��Ӧ��SceneVolume����������µ�SceneVolume����
 */
void ToolpathPreviewWidget::AddScenevolume(ExtrusionEntityCollection& entities, coordf_t top_z,
	const Point& copy,int color_index,BoundingBoxf3& bbox) {

	//��ǰ�Ѿ����ڸ�color_index��Ӧ�Ķ�����ֱ�������qverts��tverts������
	if (color_volumes.find(color_index) != color_volumes.end()) {
		int volume_index = color_volumeidx[color_index];
		SceneVolume& volume = volumes_[volume_index];

		

		//��ExtrustionEntityת��Ϊqverts��tverts����ӵ���ǰvolume
		ExtrusionEntityToVerts(entities, top_z, copy, volume);

		//<top_z, [qverts_offset,tverts_offset]>
		//��print_z��Ӧ��qverts��tverts��ƫ�ƣ����ڻ���SceneVolume��һ����
		volume.offsets[top_z] = { volume.qverts_.verts.size(),volume.tverts_.verts.size() };
	}
	else {		//�����û�д���color_index��Ӧ�Ķ��󣬾�����µ�SceneVolume����
		
		//����µ�SceneVolume���󣬲���������ɫ����
		color_volumes[color_index]++;
		SceneVolume volume;
		volume.color[0] = COLORS[color_index][0]; volume.color[1] = COLORS[color_index][1];
		volume.color[2] = COLORS[color_index][2]; volume.color[3] = COLORS[color_index][3];

		volume.offsets[0] = { 0,0 };

		

		//��ExtrusionEntityת��Ϊqverts��tverts������ӵ��½���Volume�й�
		ExtrusionEntityToVerts(entities, top_z, copy, volume);

		//<print_z,[qverts_offset, tverts_offset]>
		//print_z��Ӧ��qverts��tverts��ƫ�ƣ����ڻ���SceneVolume��һ����
		volume.offsets[top_z] = { volume.qverts_.verts.size(),volume.tverts_.verts.size() };

		volumes_.push_back(volume);
		
		//����<color_id, volume_index>�������ж��Ƿ��Ѵ���SceneVolume,�������������
		color_volumeidx[color_index] = volumes_.size() - 1;
	}
}


// void ToolpathPreviewWidget::ExtrusionentitiesToVerts(ExtrusionEntityCollection& entities, coordf_t top_z,
// 	const Point& copy, SceneVolume& scenevolume) {
// 	for (ExtrusionEntity* entity : entities.entities) {
// 		if (entity->is_loop()) {
// 			ExtrusionLoopToVerts(dynamic_cast<ExtrusionLoop&>(*entity), top_z, copy, scenevolume);
// 		}
// 		else {
// 			//ExtrusionPathToVerts(dynamic_cast<ExtrusionPath&>(*entity), top_z, copy, scenevolume);
// 		}
// 	}
// }


/*
 *	��ExtrusionEntityת��Ϊverts������ӵ�volume��
 *	top_z��extrusion��top height
 *  copy����ǰPrintObject��offset,��Ҫ�Դ�ӡ·����Polyline����ƫ��
 *	volume����ǰ��SceneVolume��ExtrusionEntityת����Ľ���洢�ڸö�����
 */
void ToolpathPreviewWidget::ExtrusionEntityToVerts(ExtrusionEntity& entity, coordf_t top_z,
	const Point& copy, SceneVolume& volume) {
	std::vector<double> widths;
	std::vector<double> heights;
	bool closed = false;
	Lines lines;

	//�������Ķ�̬����ΪExtrusionEntityCollection���������ÿһ��entity�ֱ���в���
	if (typeid(entity) == typeid(ExtrusionEntityCollection)) {
		ExtrusionEntityCollection& entity_collection = dynamic_cast<ExtrusionEntityCollection&>(entity);
		for (ExtrusionEntity* temp_entity : entity_collection.entities) {
			ExtrusionEntityToVerts(*temp_entity, top_z, copy, volume);
		}
		return;
	}
	//�����ǰ����Ķ�̬����ΪExtrusionPath����ʹ����Polyline����verts
	else if (typeid(entity) == typeid(ExtrusionPath)) {
		ExtrusionPath& path = dynamic_cast<ExtrusionPath&>(entity);
		Slic3r::Polyline polyline = path.polyline;
		polyline.remove_duplicate_points();
		polyline.translate(copy);
		lines = polyline.lines();
		widths.clear();		widths.insert(widths.end(), lines.size(), path.width);
		heights.clear();	heights.insert(heights.end(), lines.size(), path.height);
		closed = 0;
	}
	//�����ǰ��ӡ����Ķ�̬����ΪExtrusionLoop��������polyline��������verts
	else {
		widths.clear(); heights.clear(); lines.clear(); closed = true;
		ExtrusionLoop loop = dynamic_cast<ExtrusionLoop&>(entity);

		for (ExtrusionPath& path : loop.paths) {
			Slic3r::Polyline polyline = path.polyline;
			polyline.remove_duplicate_points();
			polyline.translate(copy);
			Lines path_lines = polyline.lines();

			lines.insert(lines.end(), path_lines.begin(), path_lines.end());
			widths.insert(widths.end(), path_lines.size(), path.width);
			heights.insert(heights.end(), path_lines.size(), path.height);
		}
	}
	//��extrusion entityת��Ϊqverts��tverts�������volume��
	_3DScene::_extrusionentity_to_verts_do(lines, widths, heights, closed,
		top_z, copy, &volume.qverts_, &volume.tverts_);

}




// void ToolpathPreviewWidget::ExtrusionLoopToVerts(ExtrusionLoop& loop, coordf_t top_z,
// 	const Point& copy, SceneVolume& volume) {
// 
// 	std::vector<double> widths;
// 	std::vector<double> heights;
// 	std::vector<Line> lines;
// 
// 	for (ExtrusionPath& path : loop.paths) {
// 		Slic3r::Polyline polyline = path.polyline;
// 		polyline.remove_duplicate_points();
// 		polyline.translate(copy);
// 		Slic3r::Lines path_lines = polyline.lines();
// 		
// 		lines.insert(lines.end(), path_lines.begin(), path_lines.end());
// 		widths.insert(widths.end(), path_lines.size(), path.width);
// 		heights.insert(heights.end(), path_lines.size(), path.height);
// 	}
// 
// 	bool closed = true;
// 
// 	_3DScene::_extrusionentity_to_verts_do(lines, widths, heights, closed,
// 		top_z, copy, &volume.qverts_, &volume.tverts_);
// }


///��Qt�������¼�ת��ΪVCG���ڵ��������¼�
vcg::Trackball::Button ToolpathPreviewWidget::QT2VCG(Qt::MouseButton qtbt, Qt::KeyboardModifiers modifiers)
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


/*
 *	����ƶ��¼�
 */
void ToolpathPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
	trackball_.MouseMove(event->x(), height() - event->y());
	update();
}


/*
 *	�������¼�
 */
void ToolpathPreviewWidget::wheelEvent(QWheelEvent* event) {
	if (event->delta() > 0) {
		trackball_.MouseWheel(1);
	}
	else {
		trackball_.MouseWheel(-1);
	}
	update();

}


/*
 *	��갴���¼�
 */
void ToolpathPreviewWidget::mousePressEvent(QMouseEvent* event) {
	trackball_.MouseDown(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
	update();
}


/*
 *	����ɿ������¼�
 */
void ToolpathPreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
	trackball_.MouseUp(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
	update();
}

/*
 *	���̰����¼�
 */
void ToolpathPreviewWidget::keyReleaseEvent(QKeyEvent* event) {
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

/*
 *	����Ҽ��¼�
 */
void ToolpathPreviewWidget::contextMenuEvent(QContextMenuEvent* event) {
	right_click_menu_->clear();

	right_click_menu_->addAction(reset_trackball_action_);
}


/*
 *	��ʼ���Ҽ��˵���������Ĳ˵���
 */
void ToolpathPreviewWidget::InitActions() {
	right_click_menu_ = new QMenu();

	reset_trackball_action_ = new QAction(QString::fromLocal8Bit("����"), this);

	connect(reset_trackball_action_, &QAction::triggered, this, &ToolpathPreviewWidget::ResetTrackball);
}


/*
 *	���õ�ǰ�ؼ���trackball
 */
void ToolpathPreviewWidget::ResetTrackball() {
	trackball_.SetIdentity();
	update();
}


/*
 *	���ƴ�ӡ·������SceneVolumes
 */
void ToolpathPreviewWidget::DrawVolumes()  {
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	for (SceneVolume& volume : volumes_){
		glPushMatrix();

		//ƽ�Ʋ���
		glTranslatef(volume.Origin().x, volume.Origin().y, volume.Origin().z);

		glColor4fv(volume.color);


		int min_offset = 0;
		int max_offset = volume.qverts_.verts.size();

		int min_qverts_offset = 0;
		int max_qverts_offset = volume.qverts_.verts.size();

		int min_tverts_offset = 0;
		int max_tverts_offset = volume.tverts_.verts.size();

		//������ʾģ�͵�print_z�Ͻ���½�
		if (min_z_ >= 0 && max_z_ >= 0 && max_z_ >= min_z_ && !volume.offsets.empty()) {
			
			auto lower_it= volume.offsets.lower_bound(min_z_);
			auto upper_it = volume.offsets.upper_bound(max_z_);
			--upper_it;

			if (upper_it->first >= lower_it->first) {
				min_qverts_offset = (*lower_it).second.first;
				min_tverts_offset = (*lower_it).second.second;

				max_qverts_offset = (*upper_it).second.first;
				max_tverts_offset = (*upper_it).second.second;
			}

		}

		//������άģ��
		//����Quads
		if (!volume.qverts_.verts.empty()) {
			glCullFace(GL_BACK);
			glVertexPointer(3, GL_FLOAT, 0, volume.qverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.qverts_.norms.data());
			glDrawArrays(GL_QUADS, min_qverts_offset/3, (max_qverts_offset- min_qverts_offset)/ 3);
		}

		//����Triangles
		if (!volume.tverts_.verts.empty()) {
			glCullFace(GL_BACK);
			glVertexPointer(3, GL_FLOAT, 0, volume.tverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.tverts_.norms.data());
			glDrawArrays(GL_TRIANGLES, min_tverts_offset / 3, (max_tverts_offset - min_tverts_offset) / 3);
		}

		glPopMatrix();
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
}



/*
 *	������ά������
 */
void ToolpathPreviewWidget::DrawAxes() const{
	glDisable(GL_DEPTH_TEST);

	//������ĳ���
	Pointf3 bbox_size = bbox_.size();
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


/*
 *	���Ƶװ壨�ȴ���
 */
void ToolpathPreviewWidget::DrawBedShape() const{
	float fCursor[4];
	glGetFloatv(GL_CURRENT_COLOR, fCursor);	//��ȡ��ǰ��ɫ
	glPushMatrix();

	glColor4f(0.8, 0.6, 0.5, 0.4);
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
	for (int i = 0; i < 280; i += 10) {
		glVertex3f(i, 0, 0);
		glVertex3f(i, 180, 0);
	}
	glEnd();

	//glFlush();
	glColor4fv(fCursor);
	glEnable(GL_LIGHTING);

	glPopMatrix();
}


/*
 *	���õװ���״
 */
void ToolpathPreviewWidget::SetBedshape(const BoundingBoxf& bed) {
	bed_shape_ = bed;
	ReloadBBox();
}


/*
 *	����Ĭ�ϵĵװ���״��280��180��
 */
void ToolpathPreviewWidget::SetDefaultBedshape() {
	bed_shape_ = BoundingBoxf(Pointf(0, 0), Pointf(280, 180));
	ReloadBBox();
}

/*
 *	���¼���ؼ���BoundingBox
 */
void ToolpathPreviewWidget::ReloadBBox() {
	bbox_.min = Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0);
	bbox_.max = Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0);

	for (SceneVolume volume : volumes_) {
		bbox_.merge(volume.BBox());
	}
}


/*
 *	���õ�ǰģ�͵���ʾ���½�
 */
void ToolpathPreviewWidget::SetLayerZ(int layer) {
	if (volumes_.empty()) return;
	max_z_ = layer_values_[layer-1];
	update();
}