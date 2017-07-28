#ifndef HIPPOPRINTER_H__
#define HIPPOPRINTER_H__


#include <QtWidgets/QMainWindow>
#include <QStatusBar>
#include <QProgressBar>
//#include "ui_mainwindow.h"


#include <src/libslic3r/Print.hpp>
#include <src/libslic3r/BoundingBox.hpp>

#include "ModelWidget.h"
#include "ToolpathPreviewWidget.h"
#include "ToolpathPlaneWidget.h"
#include "PrintConfigWidget.h"
#include "LabelingSliderWidget.h"


class QTabWidget;
class QMenu;
class QAction;
class QWidget;
class QLabel;
class QWidget;
class QHBoxLayout;
class QToolBar;
class QSlider;
class QListWidget;
class QStackedWidget;
class QScrollArea;

class HippoPrinter : public QMainWindow
{
	Q_OBJECT

public:
	HippoPrinter(QWidget *parent = Q_NULLPTR);


	//Ui::MainWindowClass ui;

private:
	void InitActions();
	void InitMenus();
	void InitToolBars();
	void InitWidgets();
	void InitLayout();
	void InitConnections();
	void SetupWindowStyle();

private slots:
	void OpenFile();
	void SwitchTab();

private:
	//菜单项
	QMenu* file_menu_;	//文件菜单
	QMenu* setting_menu_;	//设置菜单
	QMenu* help_menu_;	//帮助菜单
	QToolBar* action_toolbar_;	//工具栏
	QStatusBar* statusbar_;		//状态栏

private:
	/*文件菜单下的操作*/
	QAction* load_model_action_;	//读取模型文件
	QAction* save_model_action_;	//保存模型文件
	QAction* save_GCode_action_;	//保存GCode文件
	QAction* print_action_;	//打印模型
	QAction* clear_platform_action_;	//删除所有模型
	QAction* quit_action_;	//退出操作

							/*设置菜单下的操作*/
	QAction* global_setting_action_;	//打印机设置

										/*帮助菜单下的操作*/
	QAction* about_hippo_action_;	//获取Hippo资料
	QAction* about_manual_action_;	//获取软件使用手册
	QAction* about_qt_action_;	//获取相关资料

	QAction* gen_toolpath_action_;		//生成打印路径

	QProgressBar* process_progressbar_;

private:

	QHBoxLayout* central_layout_;
	QWidget* central_widget_;

	//中心TabWidget,用于显示所有的内容
	QTabWidget* central_tabwidget_;

	//显示三维模型
	ModelWidget* model_widget_;

	//显示三维打印路径
	QWidget* toolpath_3d_widget_;
	ToolpathPreviewWidget* toolpath_preview_widget_;
	//QSlider* toolpath_3d_slider_;
	LabelingSliderWidget* toolpath_3d_slider_;
	QHBoxLayout* toolpath_3d_layout_;

	//显示二维打印路径
	QWidget* toolpath_2d_widget_;
	ToolpathPlaneWidget* toolpath_plane_widget_;
	LabelingSliderWidget* toolpath_2d_slider_;
	//QSlider* toolpath_2d_slider_;
	QHBoxLayout* toolpath_2d_layout_;

	//设置耗材参数、打印参数、打印机参数
	PrintConfigWidget* print_config_widget_;
	QScrollArea* config_scrollarea_;

		
private slots:
	void StartProcess();
	void ExportGCode();

private:
	void LoadFile(char* file_name);

	void SetDefaultBedShape();
	void LoadBedShape();

	void ArrangeObjects();

	//打印对象发生改变
	void OnModelChanged();

	//刷新控件内容
	void RefreshWidgets();


	//生成打印路径过程
	void OnProcessCompleted();

	void PrintProcess();


private:
	Model* model_;		//Model对象
	Print* print_;		//Print对象
	DynamicPrintConfig dynamic_config_;

	BoundingBoxf bed_shape_;

	bool need_arrange_;		//是否需要对打印对象重新布局，默认为false
	bool force_autocenter_;		//将打印对象置于底板中心

	bool processed_;		//是否已经生成打印路径

	//<layer_id, print_z>每一层对应的print_z
	std::map<int, double> layer_values_;		

};



#endif	//!HIPPOPRINTER_H__
