#ifndef HIPPOPRINTER_H__
#define HIPPOPRINTER_H__


#include <QtWidgets/QMainWindow>
//#include "ui_mainwindow.h"

class QTabWidget;

class QMenu;
class QAction;
class QWidget;
class QLabel;
class QWidget;
//class QVBoxLayout;
class QHBoxLayout;
class QToolBar;
class QSlider;

#include <src/libslic3r/Print.hpp>

#include "ModelWidget.h"
// #include "FilamentConfigWidget.h"
// #include "PrintConfigWidget.h"
// #include "PrinterConfigWidget.h"

#include "ToolpathPreviewWidget.h"


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
	void ChangeTab();

private:
	Print* print_;

private:
// 	QWidget* central_widget_;
// 	QHBoxLayout* central_widget_layout_;
// 	QTabWidget* left_tabWidget_;
// 
// 	PrintConfigWidget* print_config_widget_;
// 	FilamentConfigWidget* fila_config_layout_;
// 	PrinterConfigWidget* printer_config_layout_;


	QTabWidget* central_tabwidget_;

	ModelWidget* model_widget_;


	QWidget* toolpath_3d_widget_;
	ToolpathPreviewWidget* toolpath_preview_widget_;
	QSlider* toolpath_slider_;
	QHBoxLayout* toolpath_layout_;



private:
	//菜单项
	QMenu* file_menu_;	//文件菜单
	QMenu* setting_menu_;	//设置菜单
	QMenu* help_menu_;	//帮助菜单
	QToolBar* action_toolbar_;

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


	QAction* gen_toolpath_action_;
};



#endif	//!HIPPOPRINTER_H__
