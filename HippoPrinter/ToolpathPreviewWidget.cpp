#include "ToolpathPreviewWidget.h"

#include <QDebug>
#include <QAction>
#include <QMenu>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QOpenGLFunctions>
#include <QGLFunctions>

#include <unordered_map>

#include <GL\glut.h>


#include <src/slic3r/GUI/3DScene.hpp>

const float COLORS[4][4] = { { 1,0.95,0.2,1 },{ 1,0.45,0.45,1 },{ 0.5,1,0.5,1 },{ 0.5,0.5,1,1 } };
const float DEFAULT_COLOR[4] = { 1,1,0,1 };
const float SELECTED_COLOR[4] = { 0, 1, 0, 1 };
const float HOVER_COLOR[4] = { 0.4,0.9,0,1 };


/*
 *	构造函数
 *  参数为Print指针
 */
ToolpathPreviewWidget::ToolpathPreviewWidget(Print* print, 
	std::map<int, double>* layer_values, QWidget* parent)
	:QGLWidget(parent),
	loaded_(false),
	color_toolpaths_by_(ctRole),
	min_z_(-1), max_z_(-1),scale_(1)
{
	print_ = print;
	layer_values_ = layer_values;

	LoadBedShape();
	ResetVolumes();
	
	setMouseTracking(true);


	InitActions();
}


/*
 *	析构函数
 */
ToolpathPreviewWidget::~ToolpathPreviewWidget(){

}


/*
 *	初始化控件
 */
void ToolpathPreviewWidget::initializeGL() {
	glClearColor(1, 1, 1, 1);
	glColor3f(1, 0, 0);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0);
	//glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glDepthRange(0.0f, 1.0f);
	

	GLfloat material_ambient[] = { 0.3,0.3,0.3,1 };
	GLfloat material_specular[] = { 1,1,1,1 };
	GLfloat material_shiness = 50;
	GLfloat material_emission[] = { 0.1,0.1,0.1,0.9 };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, material_ambient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material_specular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material_shiness);
	glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, material_emission);

	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_MULTISAMPLE);

	//glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_NORMALIZE);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glColor4f(1, 1, 1, 1);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	//glEnable(GL_LIGHT1);


	GLfloat light_model_ambient1[] = { 0.3,0.3,0.3,1 };
	GLfloat light0_diffuse1[] = { 0.2,0.2,0.2,1 };
	GLfloat light_specular1[] = { 0.3,0.3,0.3,1 };
	GLfloat light_position1[] = { 1,0,1,0 };
	glLightfv(GL_LIGHT1, GL_POSITION, light_position1);
	glLightfv(GL_LIGHT1, GL_AMBIENT, light_model_ambient1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, light0_diffuse1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular1);

	//trackball_.center = vcg::Point3f(0, 0, 0);
	//trackball_.radius = 50;
	

	glLoadIdentity();
	//gluLookAt(1, 1, 1, 0, 0, 0, 0, 0, 1);

	ZoomToBed();
}


/*
 *	渲染控件
 */
void ToolpathPreviewWidget::paintGL() {
	glClearColor(1, 1, 1, 1);
	glClearDepth(1);
	glDepthFunc(GL_LESS);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

	glPushMatrix();

	//gluLookAt(0.06, 0, 0.1, 0, 1, 0, 0, 0, 1);
	trackball_.GetView();
	trackball_.Apply();

	glScaled(scale_, scale_, scale_);
	glTranslatef(-bed_shape_.center().x, -bed_shape_.center().y, 0);
	gluLookAt(trackball_.center.X(), trackball_.center.Y() - 1, trackball_.center.Z() + 1,
		trackball_.center.X(), trackball_.center.Y(), trackball_.center.Z(),
		0, 0, 1);

	//绘制背景

	//绘制坐标系


	//绘制底板5
	glEnable(GL_LIGHTING);
	DrawBedShape();
	glDisable(GL_LIGHTING);
	DrawAxes();


	glEnable(GL_LIGHTING);
	//绘制打印路径
	DrawVolumes();
	

	glPopMatrix();

}


/*
 *	当控件大小发生改变时的操作
 */
void ToolpathPreviewWidget::resizeGL(int width, int height) {
	if (height == 0) {
		height = 1;
	}
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();


	Pointf3 bbox_size = max_bbox_.size();
	double depth = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z) * 10;
	glOrtho(-width/(2*scale_), width /(2*scale_), 
		-height/(2*scale_), height /(2*scale_), 
		-depth, depth * 2);
	
	LoadMaxBBox();
	ZoomToBBox(max_bbox_);
}


/*
 *	重新载入打印路径
 */
void ToolpathPreviewWidget::ReloadVolumes() {
	ResetVolumes();		//清空当前的打印路径
	max_z_ = -1;
	loaded_ = false;
	LoadPrint();		//载入Print对象的打印路径
	//update();
	LoadBedShape();
	LoadMaxBBox();
	ZoomToVolumes();
}



/*
 *	清空所有的打印路径
 */
void ToolpathPreviewWidget::ResetVolumes() {
	volumes_.clear();
	color_volumes_.clear();
	layer_values_->clear();
}


/*
 *	载入Print对象的打印路径
 */
void ToolpathPreviewWidget::LoadPrint() {
	//如果打印对象还没有完成Slice操作，则退出
	if (!print_->step_done(posSlice)) {
		volumes_.clear();
		update();
		return;
	}

	//保存print_z,并进行排序
	std::set<double> values;
	for (PrintObject* object : print_->objects) {
		for (Layer* layer : object->layers) {
			values.insert(layer->print_z);
		}
	}

	//存储<layer_id, print_z>，方便后期通过layer_id获取print_z
	auto iterator = values.begin();
	for (int i = 0; i < values.size(); i++) {
		(*layer_values_)[i] = *iterator;
		++iterator;
	}


	//分别导入每一个打印对象的打印路径
	for (PrintObject* object : print_->objects) {
		LoadPrintObjectToolpaths(object);
	}

	//将载入状态设为1
	loaded_ = true;

	//设置显示的上界为整个模型的Bouding box
	min_z_ = 0;
	max_z_ = (*layer_values_)[values.size() - 1];
}




/*
 *	导入打印对象的toolpaths，目前不包括support materials
 */
void ToolpathPreviewWidget::LoadPrintObjectToolpaths(const PrintObject* object) {
	
	//按照print_z对Layers进行排序,可能并不需要
	std::vector<Layer*> layers(object->layers.begin(), object->layers.end());
	//layers.insert(layers.end(), object->support_layers.begin(), object->support_layers.end());

	std::sort(layers.begin(), layers.end(), [=](Layer* x, Layer* y) {
		return x->print_z < y->print_z; 
	});

	//计算打印对象的Bounding box,包括平移后的复制品
	BoundingBoxf3 bbox;
	BoundingBox obj_bbox = object->bounding_box();
	for (auto& copy : object->_shifted_copies) {
		BoundingBox copy_bbox = obj_bbox;
		copy_bbox.translate(copy.x, copy.y);
		bbox.merge(Pointf3::new_unscale(copy_bbox.min.x, copy_bbox.min.y, 0));
		bbox.merge(Pointf3::new_unscale(copy_bbox.max.x, copy_bbox.max.y, 0));
	}
	

	long alloc_size_max = 32 * 1048576 / 4;

	//是否按照extruder对打印路径进行染色
	bool color_toolpaths_by_extruder = (color_toolpaths_by_ == ctExtruder);

	//分别将每一层上的每一个reigion内的打印路径添加进来
	for (Layer* layer : layers) {
		coordf_t top_z = layer->print_z;
		for (auto& copy : object->_shifted_copies) {
			for (LayerRegion* region : layer->regions) {
				//添加perimters路径
				if (object->state.is_done(posPerimeters)) {
					 int color_index = color_toolpaths_by_extruder ?
						 (region->region()->config.perimeter_extruder - 1) % 4 : 0;
					//AddScenevolume(region->perimeters, top_z, copy, color, bbox);
					AddScenevolume(region->perimeters, top_z, copy, color_index, bbox);
				}

				//添加Infill路径
				if (object->state.is_done(posInfill)) {
					int color_index = color_toolpaths_by_extruder ?
						(region->region()->config.infill_extruder - 1) % 4 : 1;
					if (color_toolpaths_by_extruder&&
						region->region()->config.infill_extruder != region->region()->config.solid_infill_extruder) {
					
						ExtrusionEntityCollection solid_entities;
						ExtrusionEntityCollection non_solid_entities;

						//对solid和non-solid infill使用不同的颜色显示
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

// 			if (typeid(layer) == typeid(SupportLayer) && object->state.is_done(posSupportMaterial)) {
// 				int color_index = color_toolpaths_by_extruder ?
// 					(layer->object()->config.support_material_extruder - 1) % 4 : 2;
// 				SupportLayer* support_layer = dynamic_cast<SupportLayer*>(layer);
// 				AddScenevolume(support_layer->support_fills, top_z, copy, color_index, bbox);
// 
// 				color_index = color_toolpaths_by_extruder ?
// 					(layer->object()->config.support_material_interface_extruder - 1) % 4 : 3;
// 				AddScenevolume(support_layer->support_interface_fills, top_z, copy, color_index,bbox);
// 			}
		}
	}
	std::vector<SupportLayer*> support_layers(object->support_layers.begin(), object->support_layers.end());
	std::sort(support_layers.begin(), support_layers.end(), [](Layer* x, Layer* y) {
		return x->print_z < y->print_z;
	});

	for (SupportLayer* support_layer : support_layers) {
		coordf_t top_z = support_layer->print_z;
		for (auto& copy : object->_shifted_copies) {
			if (object->state.is_done(posSupportMaterial)) {
				int color_index = color_toolpaths_by_extruder ?
					(support_layer->object()->config.support_material_extruder - 1) % 4 : 2;
				AddScenevolume(support_layer->support_fills, top_z, copy, color_index, bbox);

				color_index = color_toolpaths_by_extruder ?
					(support_layer->object()->config.support_material_interface_extruder - 1) % 4 : 3;
				AddScenevolume(support_layer->support_interface_fills, top_z, copy, color_index, bbox);
			}
			
		}
	}
}


/*
 *	将打印路径添加到打印对象上
 *  如果已经存在该color_index对应的SceneVolume对象，就直接在其qverts上添加内容
 *  如果当前不存在该color_index对应的SceneVolume对象，则添加新的SceneVolume对象
 */
void ToolpathPreviewWidget::AddScenevolume(ExtrusionEntityCollection& entities, double top_z,
	const Point& copy,int color_index,BoundingBoxf3& bbox) {

	//当前已经存在该color_index对应的对象，则直接添加其qverts和tverts的内容
	if (color_volumes_.find(color_index) != color_volumes_.end()) {
		int volume_index = color_volumeidx[color_index];
		SceneVolume& volume = volumes_[volume_index];

		

		//将ExtrustionEntity转换为qverts和tverts，添加到当前volume
		ExtrusionEntityToVerts(entities, top_z, copy, volume);

		//<top_z, [qverts_offset,tverts_offset]>
		//即print_z对应的qverts和tverts的偏移，用于绘制SceneVolume的一部分
		volume.offsets[top_z] = { volume.qverts_.verts.size(),volume.tverts_.verts.size() };
	}
	else {		//如果还没有存在color_index对应的对象，就添加新的SceneVolume对象
		
		//添加新的SceneVolume对象，并设置其颜色参数
		color_volumes_[color_index]++;
		SceneVolume volume;
		volume.color[0] = COLORS[color_index][0]; volume.color[1] = COLORS[color_index][1];
		volume.color[2] = COLORS[color_index][2]; volume.color[3] = COLORS[color_index][3];
		volume.bbox_ = bbox;
		volume.offsets[0.0] = { 0,0 };

		

		//将ExtrusionEntity转换为qverts和tverts，并添加到新建的Volume中国
		ExtrusionEntityToVerts(entities, top_z, copy, volume);

		//<print_z,[qverts_offset, tverts_offset]>
		//print_z对应的qverts和tverts的偏移，用于绘制SceneVolume的一部分
		volume.offsets[top_z] = { volume.qverts_.verts.size(),volume.tverts_.verts.size() };

		volumes_.push_back(volume);
		
		//保存<color_id, volume_index>，方便判断是否已存在SceneVolume,并对其进行索引
		color_volumeidx[color_index] = volumes_.size() - 1;
	}
}


/*
 *	将ExtrusionEntity转换为verts，并添加到volume中
 *	top_z：extrusion的top height
 *  copy：当前PrintObject的offset,需要对打印路径的Polyline进行偏移
 *	volume：当前的SceneVolume，ExtrusionEntity转换后的结果存储在该对象中
 */
void ToolpathPreviewWidget::ExtrusionEntityToVerts(ExtrusionEntity& entity, coordf_t top_z,
	const Point& copy, SceneVolume& volume) {
	std::vector<double> widths;
	std::vector<double> heights;
	bool closed = false;
	Lines lines;

	//如果对象的动态类型为ExtrusionEntityCollection，则对其中每一个entity分别进行操作
	if (typeid(entity) == typeid(ExtrusionEntityCollection)) {
		ExtrusionEntityCollection& entity_collection = dynamic_cast<ExtrusionEntityCollection&>(entity);
		for (ExtrusionEntity* temp_entity : entity_collection.entities) {
			ExtrusionEntityToVerts(*temp_entity, top_z, copy, volume);
		}
		return;
	}
	//如果当前对象的动态类型为ExtrusionPath，则使用其Polyline生成verts
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
	//如果当前打印对象的动态类型为ExtrusionLoop，则用其polyline集合生成verts
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
	//将extrusion entity转换为qverts和tverts，并添加volume中
	_3DScene::_extrusionentity_to_verts_do(lines, widths, heights, closed,
		top_z, copy, &volume.qverts_, &volume.tverts_);

}




///将Qt鼠标键盘事件转换为VCG库内的鼠标键盘事件
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
 *	鼠标移动事件
 */
void ToolpathPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
	trackball_.MouseMove(event->x(), height() - event->y());
	update();
}


/*
 *	鼠标滚轮事件
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
 *	鼠标按键事件
 */
void ToolpathPreviewWidget::mousePressEvent(QMouseEvent* event) {
	trackball_.MouseDown(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
	update();
}


/*
 *	鼠标松开按键事件
 */
void ToolpathPreviewWidget::mouseReleaseEvent(QMouseEvent *event) {
	trackball_.MouseUp(event->x(), height() - event->y(), QT2VCG(event->button(), event->modifiers()));
	update();
}

/*
 *	键盘按键事件
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
 *	鼠标右键事件
 */
void ToolpathPreviewWidget::contextMenuEvent(QContextMenuEvent* event) {
	right_click_menu_->clear();

	right_click_menu_->addAction(reset_trackball_action_);
	right_click_menu_->exec(QCursor::pos());
}


/*
 *	初始化右键菜单及其上面的菜单项
 */
void ToolpathPreviewWidget::InitActions() {
	right_click_menu_ = new QMenu();

	reset_trackball_action_ = new QAction(QString::fromLocal8Bit("重置"), this);

	connect(reset_trackball_action_, &QAction::triggered, this, &ToolpathPreviewWidget::ResetTrackball);
}


/*
 *	重置当前控件的trackball
 */
void ToolpathPreviewWidget::ResetTrackball() {
	trackball_.SetIdentity();
	update();
}


/*
 *	绘制打印路径对象SceneVolumes
 */
void ToolpathPreviewWidget::DrawVolumes()  {

	glDepthFunc(GL_LESS);
	glEnable(GL_DEPTH_TEST);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	
	glEnable(GL_CULL_FACE);
	//glDrawBuffer(GL_FRONT);
	for(SceneVolume& volume: volumes_){
	//for(int i = volumes_.size() - 1; i >= 0; i--){
		//SceneVolume& volume = volumes_[i];
		glPushMatrix();

		//平移操作
		glTranslatef(volume.Origin().x, volume.Origin().y, volume.Origin().z);

		glColor4fv(volume.color);


		int min_offset = 0;
		int max_offset = volume.qverts_.verts.size();

		int min_qverts_offset = 0;
		int max_qverts_offset = volume.qverts_.verts.size();

		int min_tverts_offset = 0;
		int max_tverts_offset = volume.tverts_.verts.size();

		//计算显示模型的print_z上界和下界
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

		//绘制三维模型
		//绘制Quads
		
		glCullFace(GL_BACK);
		if (!volume.qverts_.verts.empty()) {
			glVertexPointer(3, GL_FLOAT, 0, volume.qverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.qverts_.norms.data());
			glDrawArrays(GL_QUADS, min_qverts_offset/3, (max_qverts_offset- min_qverts_offset)/ 3);
		}

		//绘制Triangles
		if (!volume.tverts_.verts.empty()) {
			
			glVertexPointer(3, GL_FLOAT, 0, volume.tverts_.verts.data());
			glNormalPointer(GL_FLOAT, 0, volume.tverts_.norms.data());
			glDrawArrays(GL_TRIANGLES, min_tverts_offset / 3, (max_tverts_offset - min_tverts_offset) / 3);
		}

		glPopMatrix();
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glDisable(GL_BLEND);
}



/*
 *	绘制三维坐标轴
 */
void ToolpathPreviewWidget::DrawAxes() const{
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
 *	绘制底板（热床）
 */
void ToolpathPreviewWidget::DrawBedShape() const{
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

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

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

	//glFlush();
	glColor4fv(fCursor);
	glEnable(GL_LIGHTING);

	glPopMatrix();
}



/*
 *	设置当前模型的显示上下界
 */
void ToolpathPreviewWidget::SetLayerZ(int layer) {
	if (volumes_.empty()) return;

	max_z_ = (*layer_values_)[layer-1];
	update();
}


void ToolpathPreviewWidget::LoadBedShape() {
	bed_shape_ = BoundingBoxf(print_->config.bed_shape.values);
}

void ToolpathPreviewWidget::ZoomToBed() {
	BoundingBoxf3 bbox;
	bbox.merge(Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0));
	bbox.merge(Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0));

	ZoomToBBox(bbox);
}

void ToolpathPreviewWidget::ZoomToBBox(BoundingBoxf3& bbox) {
// 	Pointf3 bbox_size = bbox.size();
// 	double max_size = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z)*1.5;
// 	int min_viewport_size = std::min(width(), height());
// 
// 	if (max_size != 0) {
// 		trackball_.radius = std::min(bbox_size.x, bbox_size.y);
// 		glScaled(max_size / min_viewport_size, max_size / min_viewport_size, max_size / min_viewport_size);
// 		glTranslatef(-bbox.center().x, -bbox.center().y, 0);
// 	}
	Pointf3 bbox_size = bbox.size();
	double max_size = std::max(std::max(bbox_size.x, bbox_size.y), bbox_size.z);
	int min_viewport_size = std::min(width(), height());

	

	if (max_size != 0) {
		trackball_.radius = std::min(bbox_size.x, bbox_size.y);
		scale_ = min_viewport_size / max_size*1.05;
	}
}

void ToolpathPreviewWidget::ZoomToVolumes() {
	BoundingBoxf3 bbox;

	for (SceneVolume& volume : volumes_) {
		bbox.merge(volume.TransformedBBox());
	}

	ZoomToBBox(bbox);
}

void ToolpathPreviewWidget::LoadMaxBBox() {
	max_bbox_.min = Pointf3(bed_shape_.min.x, bed_shape_.min.y, 0);
	max_bbox_.max = Pointf3(bed_shape_.max.x, bed_shape_.max.y, 0);

	for (SceneVolume& volume : volumes_) {
		max_bbox_.merge(volume.TransformedBBox());
	}
}