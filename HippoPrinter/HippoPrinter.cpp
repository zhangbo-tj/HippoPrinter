#include "HippoPrinter.h"
#include <iostream>

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

HippoPrinter::HippoPrinter(QWidget *parent)
	: QMainWindow(parent)
{
	print_ = new Print();
	//ui.setupUi(this);
	setWindowTitle(QString::fromLocal8Bit("Hippo"));
	setWindowIcon(QIcon("./Resources/hippo.svg"));
	//setMinimumSize(QSize(1000, 800));

	//��ʼ���˵��Ͳ˵���
	InitActions();
	InitMenus();
	InitToolBars();
	InitWidgets();
	InitLayout();
	InitConnections();

	setContentsMargins(5, 5, 10, 5);
	statusBar()->showMessage(tr("Ready"));

	if (!isMaximized()) {
		showMaximized();
	}
}


void HippoPrinter::InitActions() {

	/*�ļ��˵��µĲ���*/
	//��ģ��
	load_model_action_ = new QAction(QString::fromLocal8Bit("��ģ��"));
	load_model_action_->setShortcut(tr("Ctrl+O"));
	load_model_action_->setStatusTip(QString::fromLocal8Bit("��ģ���ļ�"));

	//����ģ��
	save_model_action_ = new QAction(QString::fromLocal8Bit("����ģ��"));
	save_model_action_->setShortcut(tr("Ctrl+S"));
	save_model_action_->setStatusTip(QString::fromLocal8Bit("����ģ���ļ�"));

	//����GCode�ļ�
	save_GCode_action_ = new QAction(QString::fromLocal8Bit("����G����"));
	save_GCode_action_->setShortcut(tr("Ctrl+G"));
	save_GCode_action_->setStatusTip(QString::fromLocal8Bit("����GCode�ļ�"));

	//��ӡģ��
	print_action_ = new QAction(QString::fromLocal8Bit("��ӡģ��"));
	print_action_->setShortcut(tr("Ctrl+P"));
	print_action_->setStatusTip(QString::fromLocal8Bit("��ӡ��άģ��"));

	//���ƽ̨
	clear_platform_action_ = new QAction(QString::fromLocal8Bit("���ƽ̨"));
	clear_platform_action_->setStatusTip(QString::fromLocal8Bit("���������άģ��"));

	//�˳�����
	quit_action_ = new QAction(QString::fromLocal8Bit("�˳�"));
	quit_action_->setShortcut(tr("Ctrl+Q"));
	quit_action_->setStatusTip(QString::fromLocal8Bit("�˳�����"));


	/*���ò˵��µĲ���*/
	//���ô�ӡ����
	global_setting_action_ = new QAction(QString::fromLocal8Bit("��ӡ������"));
	global_setting_action_->setStatusTip(QString::fromLocal8Bit("���ô�ӡ����ز���"));


	/*�����˵��µĲ���*/
	//��ȡHippo����
	about_hippo_action_ = new QAction(QString::fromLocal8Bit("����Hippo"));
	about_hippo_action_->setStatusTip(QString::fromLocal8Bit("����Hippo������"));

	//��ȡ����ֲ�
	about_manual_action_ = new QAction(QString::fromLocal8Bit("ʹ���ֲ�"));
	about_manual_action_->setStatusTip(QString::fromLocal8Bit("���ʹ���ֲ�"));

	//��ȡQt����
	about_qt_action_ = new QAction(QString::fromLocal8Bit("����Qt"));
	about_qt_action_->setStatusTip(QString::fromLocal8Bit("��ȡ����Qt������"));

	//��ʼ���ɴ�ӡ·��
	gen_toolpath_action_ = new QAction(QString::fromLocal8Bit("����·��"));
	gen_toolpath_action_->setStatusTip(QString::fromLocal8Bit("���ɴ�ӡ·��"));
}

void HippoPrinter::InitToolBars() {
	action_toolbar_ = addToolBar(QString::fromLocal8Bit("actions"));

	action_toolbar_->addAction(gen_toolpath_action_);
}

void HippoPrinter::SetupWindowStyle() {
	setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint | Qt::Tool);
	setAttribute(Qt::WA_TranslucentBackground);
	setWindowState(Qt::WindowNoState | Qt::WindowFullScreen);
	setFocusPolicy(Qt::NoFocus);
	setWindowOpacity(1.0);

	setWindowIcon(QIcon("icon.png"));
}

//************************************
// ����: big-hippo
// ���ڣ�2016/12/19 
// ����: void
// ����: ��ʼ���˵���
//************************************
void HippoPrinter::InitMenus() {
	//�ļ��˵�
	file_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("�ļ�"));
	file_menu_->addAction(load_model_action_);
	file_menu_->addAction(save_model_action_);
	file_menu_->addAction(save_GCode_action_);
	file_menu_->addSeparator();
	file_menu_->addAction(print_action_);
	file_menu_->addAction(clear_platform_action_);
	file_menu_->addSeparator();
	file_menu_->addAction(quit_action_);

	//���ò˵�
	setting_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("����"));
	setting_menu_->addAction(global_setting_action_);


	//�����˵�
	help_menu_ = menuBar()->addMenu(QString::fromLocal8Bit("����"));
	help_menu_->addAction(about_hippo_action_);
	help_menu_->addAction(about_hippo_action_);
	help_menu_->addAction(about_hippo_action_);
}



//************************************
// ����: big-hippo
// ���ڣ�2016/12/19 
// ����: void
// ����:
//************************************
void HippoPrinter::InitWidgets() {
// 	print_config_widget_ = new PrintConfigWidget();
// 	fila_config_layout_ = new FilamentConfigWidget();
// 	printer_config_layout_ = new PrinterConfigWidget();
// 
// 	left_tabWidget_ = new QTabWidget();
// 	left_tabWidget_->addTab(print_config_widget_, QString::fromLocal8Bit("��ӡ����"));
// 	left_tabWidget_->addTab(fila_config_layout_, QString::fromLocal8Bit("�Ĳ�����"));
// 	left_tabWidget_->addTab(printer_config_layout_, QString::fromLocal8Bit("��ӡ������"));
// 
// 	show_widget_ = new ShowWidget();
// 	central_widget_ = new QWidget();
// 	central_widget_layout_ = new QHBoxLayout();
// 	central_widget_layout_->addWidget(left_tabWidget_, 1);
// 	central_widget_layout_->addWidget(show_widget_, 6);
	central_tabwidget_ = new QTabWidget();
	model_widget_ = new ModelWidget(print_);
	
	toolpath_3d_widget_ = new QWidget();
	toolpath_preview_widget_ = new ToolpathPreviewWidget(print_);
	toolpath_slider_ = new QSlider(Qt::Vertical);
	toolpath_slider_->setRange(0, 1);
	toolpath_slider_->setSingleStep(1);
		

	toolpath_layout_ = new QHBoxLayout(toolpath_3d_widget_);
	toolpath_layout_->addWidget(toolpath_preview_widget_);
	toolpath_layout_->addWidget(toolpath_slider_);


	central_tabwidget_->addTab(model_widget_, QString::fromLocal8Bit("3D Model"));
	central_tabwidget_->addTab(toolpath_3d_widget_, QString::fromLocal8Bit("3D Toolpath Preview"));

	central_tabwidget_->setTabPosition(QTabWidget::West);
}

//************************************************************************
// ���ڣ�2016/12/26 
// ����: void
// ����: ���ô��ڲ���
//************************************************************************

void HippoPrinter::InitLayout() {
	//central_widget_->setLayout(central_widget_layout_);
	setCentralWidget(central_tabwidget_);
}

void HippoPrinter::InitConnections() {
	connect(load_model_action_, SIGNAL(triggered()), this, SLOT(OpenFile()));
	connect(gen_toolpath_action_, &QAction::triggered, model_widget_, &ModelWidget::BackgroundProcess);
	connect(central_tabwidget_, &QTabWidget::currentChanged, this, &HippoPrinter::ChangeTab);

	connect(toolpath_slider_, &QSlider::valueChanged, toolpath_preview_widget_, &ToolpathPreviewWidget::SetLayerZ);
}


void HippoPrinter::OpenFile() {
	QString file = QFileDialog::getOpenFileName(this, QString::fromLocal8Bit("���ļ�"), "", "*.stl");
	model_widget_->LoadModel(file.toLatin1().data());
}

void HippoPrinter::ChangeTab() {
	
	qDebug() << "cur tab: " << central_tabwidget_->currentIndex();

	int current_tab_index = central_tabwidget_->currentIndex();

	if (current_tab_index == 1) {
		model_widget_->BackgroundProcess();
		toolpath_preview_widget_->ReloadPrint();
		toolpath_slider_->setMaximum(toolpath_preview_widget_->layer_values_.size());
		toolpath_slider_->setValue(toolpath_slider_->maximum());
	}
}


