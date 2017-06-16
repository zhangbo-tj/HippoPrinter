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
class QListWidget;
class QStackedWidget;

#include <src/libslic3r/Print.hpp>
#include <src/libslic3r/BoundingBox.hpp>

#include "ModelWidget.h"
#include "FilamentConfigWidget.h"
#include "PrintConfigWidget.h"
#include "PrinterConfigWidget.h"

#include "ToolpathPreviewWidget.h"
#include "ToolpathPlaneWidget.h"


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


private:

	QTabWidget* central_tabwidget_;

	//��ʾ��άģ��
	ModelWidget* model_widget_;

	//��ʾ��ά��ӡ·��
	QWidget* toolpath_3d_widget_;
	ToolpathPreviewWidget* toolpath_preview_widget_;
	QSlider* toolpath_3d_slider_;
	QHBoxLayout* toolpath_3d_layout_;

	//��ʾ��ά��ӡ·��
	QWidget* toolpath_2d_widget_;
	ToolpathPlaneWidget* toolpath_plane_widget_;
	QSlider* toolpath_2d_slider_;
	QHBoxLayout* toolpath_2d_layout_;


	//���úĲĲ�������ӡ��������ӡ������
	QWidget* setting_main_widget_;
	QHBoxLayout* setting_hlayout_;
	QListWidget* setting_listwidget_;
	QStackedWidget* setting_stackedwidget_;
	FilamentConfigWidget* fila_config_widget_;
	PrintConfigWidget* print_config_widget_;
	PrinterConfigWidget* printer_config_widget_;


private slots:
	void StartProcess();

private:
	void LoadFile(char* file_name);
	void LoadModelObjects();

	void SetDefaultBedShape();
	void LoadBedShape();

	void ArrangeObjects();

	//��ӡ�������ı�
	void OnModelChanged();

	//ˢ�¿ؼ�����
	void RefreshWidgets();


	//���ɴ�ӡ·������
	void OnProcessCompleted();


private:
	Model* model_;		//Model����
	Print* print_;		//Print����

	BoundingBoxf bed_shape_;

	bool need_arrange_;		//�Ƿ���Ҫ�Դ�ӡ�������²��֣�Ĭ��Ϊfalse
	bool force_autocenter_;		//����ӡ�������ڵװ�����

	bool processed_;		//�Ƿ��Ѿ����ɴ�ӡ·��

	//<layer_id, print_z>ÿһ���Ӧ��print_z
	std::map<int, double> layer_values_;		

};



#endif	//!HIPPOPRINTER_H__
