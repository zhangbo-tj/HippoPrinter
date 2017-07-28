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
	//�˵���
	QMenu* file_menu_;	//�ļ��˵�
	QMenu* setting_menu_;	//���ò˵�
	QMenu* help_menu_;	//�����˵�
	QToolBar* action_toolbar_;	//������
	QStatusBar* statusbar_;		//״̬��

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

	QAction* gen_toolpath_action_;		//���ɴ�ӡ·��

	QProgressBar* process_progressbar_;

private:

	QHBoxLayout* central_layout_;
	QWidget* central_widget_;

	//����TabWidget,������ʾ���е�����
	QTabWidget* central_tabwidget_;

	//��ʾ��άģ��
	ModelWidget* model_widget_;

	//��ʾ��ά��ӡ·��
	QWidget* toolpath_3d_widget_;
	ToolpathPreviewWidget* toolpath_preview_widget_;
	//QSlider* toolpath_3d_slider_;
	LabelingSliderWidget* toolpath_3d_slider_;
	QHBoxLayout* toolpath_3d_layout_;

	//��ʾ��ά��ӡ·��
	QWidget* toolpath_2d_widget_;
	ToolpathPlaneWidget* toolpath_plane_widget_;
	LabelingSliderWidget* toolpath_2d_slider_;
	//QSlider* toolpath_2d_slider_;
	QHBoxLayout* toolpath_2d_layout_;

	//���úĲĲ�������ӡ��������ӡ������
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

	//��ӡ�������ı�
	void OnModelChanged();

	//ˢ�¿ؼ�����
	void RefreshWidgets();


	//���ɴ�ӡ·������
	void OnProcessCompleted();

	void PrintProcess();


private:
	Model* model_;		//Model����
	Print* print_;		//Print����
	DynamicPrintConfig dynamic_config_;

	BoundingBoxf bed_shape_;

	bool need_arrange_;		//�Ƿ���Ҫ�Դ�ӡ�������²��֣�Ĭ��Ϊfalse
	bool force_autocenter_;		//����ӡ�������ڵװ�����

	bool processed_;		//�Ƿ��Ѿ����ɴ�ӡ·��

	//<layer_id, print_z>ÿһ���Ӧ��print_z
	std::map<int, double> layer_values_;		

};



#endif	//!HIPPOPRINTER_H__
