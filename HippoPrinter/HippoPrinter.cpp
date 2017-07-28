#include "HippoPrinter.h"
#include <iostream>

#include <thread>
#include <future>
#include <functional>

#include<QSize>
#include <QString>
#include <QDebug>

#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QListWidget>
#include <QStackedWidget>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QIcon>
#include <QToolBar>
#include <QSlider>
#include <QScrollArea>
#include <QIcon>
#include <QFileDialog>
#include <QMessageBox>

HippoPrinter::HippoPrinter(QWidget *parent)
	: QMainWindow(parent),
	need_arrange_(false),force_autocenter_(true),
	processed_(false)
{
	print_ = new Print();
	model_ = new Model();

	SetDefaultBedShape();
	LoadBedShape();
	

	setWindowTitle(QString::fromLocal8Bit("Hippo"));
	setWindowIcon(QIcon("./Resources/hippo.svg"));
	//setMinimumSize(QSize(1000, 800));

	//初始化菜单和菜单项
	InitActions();
	InitMenus();
	InitToolBars();
	InitWidgets();
	InitLayout();
	InitConnections();

	setContentsMargins(5, 5, 10, 5);
	statusbar_ = statusBar();
	statusbar_->showMessage("Ready");
	process_progressbar_ = new QProgressBar(this);
	process_progressbar_->setRange(0, 100);
	process_progressbar_->setValue(0);
	statusbar_->addPermanentWidget(process_progressbar_);
	print_->SetStatusBar(statusbar_,process_progressbar_);

	if (!isMaximized()) {
		showMaximized();
	}

	LoadFile("3Dowllovely_face.stl");
}


void HippoPrinter::InitActions() {

	/*文件菜单下的操作*/
	//打开模型
	load_model_action_ = new QAction(QString::fromLocal8Bit("打开模型"));
	load_model_action_->setShortcut(tr("Ctrl+O"));
	load_model_action_->setStatusTip(QString::fromLocal8Bit("打开模型文件"));

	//保存模型
	save_model_action_ = new QAction(QString::fromLocal8Bit("保存模型"));
	save_model_action_->setShortcut(tr("Ctrl+S"));
	save_model_action_->setStatusTip(QString::fromLocal8Bit("保存模型文件"));

	//保存GCode文件
	save_GCode_action_ = new QAction(QString::fromLocal8Bit("保存GCode"));
	save_GCode_action_->setShortcut(tr("Ctrl+G"));
	save_GCode_action_->setStatusTip(QString::fromLocal8Bit("保存GCode文件"));

	//打印模型
	print_action_ = new QAction(QString::fromLocal8Bit("打印模型"));
	print_action_->setShortcut(tr("Ctrl+P"));
	print_action_->setStatusTip(QString::fromLocal8Bit("打印三维模型"));

	//清除平台
	clear_platform_action_ = new QAction(QString::fromLocal8Bit("清除平台"));
	clear_platform_action_->setStatusTip(QString::fromLocal8Bit("清除所有三维模型"));

	//退出程序
	quit_action_ = new QAction(QString::fromLocal8Bit("退出"));
	quit_action_->setShortcut(tr("Ctrl+Q"));
	quit_action_->setStatusTip(QString::fromLocal8Bit("退出程序"));


	/*设置菜单下的操作*/
	//设置打印参数
	global_setting_action_ = new QAction(QString::fromLocal8Bit("打印机设置"));
	global_setting_action_->setStatusTip(QString::fromLocal8Bit("设置打印机相关参数"));


	/*帮助菜单下的操作*/
	//获取Hippo资料
	about_hippo_action_ = new QAction(QString::fromLocal8Bit("关于Hippo"));
	about_hippo_action_->setStatusTip(QString::fromLocal8Bit("关于Hippo的资料"));

	//获取软件手册
	about_manual_action_ = new QAction(QString::fromLocal8Bit("使用手册"));
	about_manual_action_->setStatusTip(QString::fromLocal8Bit("软件使用手册"));

	//获取Qt资料
	about_qt_action_ = new QAction(QString::fromLocal8Bit("关于Qt"));
	about_qt_action_->setStatusTip(QString::fromLocal8Bit("获取关于Qt的资料"));

	//开始生成打印路径
	gen_toolpath_action_ = new QAction(QString::fromLocal8Bit("生成路径"));
	gen_toolpath_action_->setStatusTip(QString::fromLocal8Bit("生成打印路径"));

}

void HippoPrinter::InitToolBars() {
	action_toolbar_ = addToolBar(QString::fromLocal8Bit("actions"));

	QIcon gen_toolpath_icon;
	gen_toolpath_icon.addFile(QString::fromLocal8Bit("Resources/Icons/gen_toolpath.png"), 
		QSize(), QIcon::Normal, QIcon::Off);
	gen_toolpath_action_->setIcon(gen_toolpath_icon);
	gen_toolpath_action_->setIconText(QString::fromLocal8Bit("生成路径"));
	action_toolbar_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	action_toolbar_->addAction(gen_toolpath_action_);

	QIcon save_gcode_icon;
	save_gcode_icon.addFile(QString::fromLocal8Bit("Resources/Icons/save_gcode.png"),
		QSize(), QIcon::Normal, QIcon::Off);
	save_GCode_action_->setIcon(save_gcode_icon);
	action_toolbar_->addAction(save_GCode_action_);
	
}

void HippoPrinter::SetupWindowStyle() {
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint | Qt::Tool);
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowState(Qt::WindowNoState | Qt::WindowFullScreen);
	setFocusPolicy(Qt::NoFocus);
	setWindowOpacity(1.0);

	setWindowIcon(QIcon("icon.png"));
}


void HippoPrinter::InitMenus() {
	//文件菜单
	file_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("文件"));
	file_menu_->addAction(load_model_action_);
	file_menu_->addAction(save_model_action_);
	file_menu_->addAction(save_GCode_action_);
	file_menu_->addSeparator();
	file_menu_->addAction(print_action_);
	file_menu_->addAction(clear_platform_action_);
	file_menu_->addSeparator();
	file_menu_->addAction(quit_action_);

	//设置菜单
	setting_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("设置"));
	setting_menu_->addAction(global_setting_action_);


	//帮助菜单
	help_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("帮助"));
	help_menu_->addAction(about_hippo_action_);
	help_menu_->addAction(about_hippo_action_);
	help_menu_->addAction(about_hippo_action_);
}




/*
 *	初始化控件
 */
void HippoPrinter::InitWidgets() {
	//显示三维模型的控件
	model_widget_ = new ModelWidget(print_,model_);

	//显示三维打印路径Tab页
	toolpath_3d_widget_ = new QWidget();
	toolpath_preview_widget_ = new ToolpathPreviewWidget(print_,&layer_values_);
	//toolpath_3d_slider_ = new QSlider(Qt::Vertical);
	toolpath_3d_slider_ = new LabelingSliderWidget();
	toolpath_3d_slider_->setOrientation(Qt::Vertical);
	toolpath_3d_slider_->setRange(0, 0);
	toolpath_3d_slider_->setSingleStep(1);
	
	
	//分别添加自定义的三维路径显示控件及其右侧Slider
	toolpath_3d_layout_ = new QHBoxLayout(toolpath_3d_widget_);
	toolpath_3d_layout_->addWidget(toolpath_preview_widget_);
	toolpath_3d_layout_->addWidget(toolpath_3d_slider_);
	toolpath_3d_slider_->setEnabled(false);

	//显示二维打印路径Tab页
	toolpath_2d_widget_ = new QWidget();
	toolpath_plane_widget_ = new ToolpathPlaneWidget(print_,&layer_values_);
	//toolpath_2d_slider_ = new QSlider(Qt::Vertical);
	toolpath_2d_slider_ = new LabelingSliderWidget();
	toolpath_2d_slider_->setOrientation(Qt::Vertical);
	toolpath_2d_slider_->setRange(0, 0);
	toolpath_2d_slider_->setSingleStep(1);
	toolpath_2d_slider_->setEnabled(false);
	//分别添加显示二维路径的控件以及右侧的QSlider
	toolpath_2d_layout_ = new QHBoxLayout(toolpath_2d_widget_);
	toolpath_2d_layout_->addWidget(toolpath_plane_widget_);
	toolpath_2d_layout_->addWidget(toolpath_2d_slider_);


	print_config_widget_ = new PrintConfigWidget(&dynamic_config_);
	config_scrollarea_ = new QScrollArea;
	config_scrollarea_->setWidget(print_config_widget_);
	config_scrollarea_->setWidgetResizable(true);
	config_scrollarea_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

	//中心显示控件，并分别添加三维模型Tab页、三维打印路径Tab页、二维打印路径Tab页、设置参数Tab页
	central_tabwidget_ = new QTabWidget();
	central_tabwidget_->addTab(model_widget_, QString::fromLocal8Bit("3D Model"));
	central_tabwidget_->addTab(toolpath_3d_widget_, QString::fromLocal8Bit("3D Preview"));
	central_tabwidget_->addTab(toolpath_2d_widget_, QString::fromLocal8Bit("2D Preview"));
	//central_tabwidget_->addTab(setting_main_widget_, QString::fromLocal8Bit("Setting"));


	central_tabwidget_->setTabPosition(QTabWidget::North);
	central_tabwidget_->setStyleSheet(
		"QTabWidget::Pane{border:2px solid gray; border-radius:5px;}"
		"QTabWidget::tab-bar{alignment:left; left:5px;}"
		"QTabBar::tab{border:2px solid gray; border-bottom-color:#C2C7CB;font:16px Times ;}"
		"QTabBar::tab{border-top-left-radius:4px; border-top-right-radius:4px; min-width:100px;min-height:30px;}"

		"QTabBar::tab:hover{background:lightgray;}"
		"QTabBar::tab:selected{background:skyblue;color:white;border-color:skyblue}"
		"QTabBar::tab:!selected{margin-top:2px}"
	);


	central_widget_ = new QWidget(this);
}


/*
 *	设置页面布局，可能还需要添加控制打印参数的控件
 */
void HippoPrinter::InitLayout() {
	central_layout_ = new QHBoxLayout();
	central_layout_->addWidget(central_tabwidget_,8);
	central_layout_->addWidget(config_scrollarea_,2);
	central_widget_->setLayout(central_layout_);

	setCentralWidget(central_widget_);
}


/*
 *	为控件添加信号槽
 */
void HippoPrinter::InitConnections() {
	//打开模型文件Action
	connect(load_model_action_, SIGNAL(triggered()), this, SLOT(OpenFile()));

	//生成打印路径Action
	connect(gen_toolpath_action_, &QAction::triggered, this, &HippoPrinter::StartProcess);

	connect(save_GCode_action_, &QAction::triggered, this, &HippoPrinter::ExportGCode);

	//切换Tab页
	connect(central_tabwidget_, &QTabWidget::currentChanged, this, &HippoPrinter::SwitchTab);

	//3D Slider
	connect(toolpath_3d_slider_, &QSlider::valueChanged, toolpath_preview_widget_, &ToolpathPreviewWidget::SetLayerZ);

	//2D Slider
	connect(toolpath_2d_slider_, &QSlider::valueChanged, toolpath_plane_widget_, &ToolpathPlaneWidget::SetLayerZ);

	//connect(setting_listwidget_, &QListWidget::currentRowChanged, setting_stackedwidget_, &QStackedWidget::setCurrentIndex);
}


void HippoPrinter::OpenFile() {
	QString file = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("打开文件"), "", "*.stl");
	//model_widget_->LoadModel(file.toLatin1().data());
}



/*
 *	当前Tab控件发生改变
 */
void HippoPrinter::SwitchTab() {
	
	//qDebug() << "cur tab: " << central_tabwidget_->currentIndex();

	int current_tab_index = central_tabwidget_->currentIndex();

	if (current_tab_index == 1) {
		if (!processed_) {
			StartProcess();
		}
		else {
			toolpath_preview_widget_->show();
			//toolpath_preview_widget_->ReloadVolumes();
		}
		return;
	}
	if (current_tab_index == 2) {
		toolpath_plane_widget_->show();
	}
	if (current_tab_index == 2) {
		if (!processed_) {
//			StartProcess();
		}
		else {
			toolpath_plane_widget_->show();
		}
	}

}



void HippoPrinter::LoadFile(char* file_name) {
	Pointf bed_center = bed_shape_.center();
	Pointf bed_size = bed_shape_.size();

	Model tmp_model = Model::read_from_file(file_name);
	if (tmp_model.objects.empty())
		return;

	for (ModelObject* object : tmp_model.objects) {
		ModelObject* added_object = model_->add_object(*object);
		added_object->repair();

		if (object->instances.size() == 0) {
			need_arrange_ = true;
			//添加默认的instance，并将center object around origin
			added_object->center_around_origin();
			added_object->add_instance();
			added_object->instances[0]->SetOffset(bed_center);
		}
		else {
			added_object->align_to_ground();
		}

		{
			Pointf3 size = added_object->bounding_box().size();
			double ratio = std::max(size.x, size.y) / std::max(bed_size.x, bed_size.y);

			if (ratio > 5) {
				for (ModelInstance* instance : added_object->instances) {
					instance->SetScalingFactor(1 / ratio);
				}
				//scaled_down = true;
			}
		}

		//将ModelObject添加到Print上
		print_->auto_assign_extruders(added_object);
		print_->add_model_object(added_object);
	}


	if (need_arrange_) {
		ArrangeObjects();
	}

	OnModelChanged();

	model_widget_->ZoomToVolumes();
}


/*
 *	设置默认的底板形状
 */
void HippoPrinter::SetDefaultBedShape() {
	print_->config.bed_shape.values.clear();
	print_->config.bed_shape.values.push_back(Pointf(0, 0));
	print_->config.bed_shape.values.push_back(Pointf(280, 0));
	print_->config.bed_shape.values.push_back(Pointf(280, 180));
	print_->config.bed_shape.values.push_back(Pointf(180, 0));
}


/*
 *	载入底板形状
 */
void HippoPrinter::LoadBedShape() {
	bed_shape_ = BoundingBoxf(print_->config.bed_shape.values);
}



/*
 *	对打印对象进行重新布局
 */
void HippoPrinter::ArrangeObjects() 
{
	bool success = model_->arrange_objects(print_->config.min_object_distance(), &bed_shape_);
}

void HippoPrinter::PrintProcess() {
	//print_->Process(sta);
}

/*
*	开始进行处理
*/
void HippoPrinter::StartProcess() {
	print_->apply_config(dynamic_config_);
	if (model_->objects.empty()) {
		OnProcessCompleted();
		return;
	}
	print_->Process();

	OnProcessCompleted();
	qDebug() << "finished";
}



/*
 *	当打印模型出现变化时要采取的操作
 */
void HippoPrinter::OnModelChanged() {
	if (force_autocenter_) {
		model_->center_instances_around_point(bed_shape_.center());
	}

	RefreshWidgets();

	print_->reload_model_instances();

	processed_ = false;
}


/*
 *	更新控件显示内容
 */
void HippoPrinter::RefreshWidgets() {
	model_widget_->ReloadVolumes();
	toolpath_preview_widget_->ReloadVolumes();
	toolpath_3d_slider_->setEnabled(true);
	toolpath_3d_slider_->setMaximum(layer_values_.size());
	toolpath_3d_slider_->setMaximum(layer_values_.size());
	//toolpath_3d_slider_->setMaximum(toolpath_preview_widget_->layer_values_.size());
	toolpath_3d_slider_->setValue(toolpath_3d_slider_->maximum());

	toolpath_2d_slider_->setEnabled(true);
	toolpath_2d_slider_->setMaximum(layer_values_.size());
	toolpath_2d_slider_->setValue(toolpath_2d_slider_->maximum());
	if (central_tabwidget_->currentIndex() == 2) {
		toolpath_plane_widget_->update();
	}
	//toolpath_2d_widget_->update();

}



/*
 *	打印路径生成完成
 */
void HippoPrinter::OnProcessCompleted() {
	processed_ = true;
	toolpath_preview_widget_->ReloadVolumes();
	toolpath_3d_slider_->setMaximum(layer_values_.size());
	//toolpath_3d_slider_->setMaximum(toolpath_preview_widget_->layer_values_.size());
	toolpath_3d_slider_->setValue(toolpath_3d_slider_->maximum());

	//toolpath_plane_widget_->ReloadVolumes();
	toolpath_2d_slider_->setMaximum(layer_values_.size());
	toolpath_2d_slider_->setValue(toolpath_2d_slider_->maximum());
	toolpath_plane_widget_->update();
}


void HippoPrinter::ExportGCode() {
	QString file_name = QFileDialog::getSaveFileName(this,
		QString::fromLocal8Bit("保存GCode文件"),
		"",
		"*.gcode");
	if (!file_name.isNull()) {
		print_->apply_config(dynamic_config_);
		print_->ExportGCode(file_name.toLatin1().data());
		QMessageBox::information(this, 
			QString::fromLocal8Bit("提示"),
			QString::fromLocal8Bit("GCode文件保存成功"));
	}
}