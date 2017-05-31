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
	//�˵���
	QMenu* file_menu_;	//�ļ��˵�
	QMenu* setting_menu_;	//���ò˵�
	QMenu* help_menu_;	//�����˵�
	QToolBar* action_toolbar_;

private:
	/*�ļ��˵��µĲ���*/
	QAction* load_model_action_;	//��ȡģ���ļ�
	QAction* save_model_action_;	//����ģ���ļ�
	QAction* save_GCode_action_;	//����GCode�ļ�
	QAction* print_action_;	//��ӡģ��
	QAction* clear_platform_action_;	//ɾ������ģ��
	QAction* quit_action_;	//�˳�����

							/*���ò˵��µĲ���*/
	QAction* global_setting_action_;	//��ӡ������

										/*�����˵��µĲ���*/
	QAction* about_hippo_action_;	//��ȡHippo����
	QAction* about_manual_action_;	//��ȡ���ʹ���ֲ�
	QAction* about_qt_action_;	//��ȡ�������


	QAction* gen_toolpath_action_;
};



#endif	//!HIPPOPRINTER_H__
