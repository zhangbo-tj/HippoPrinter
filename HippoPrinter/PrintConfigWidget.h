#pragma once
#include <QWidget>
#include <src/libslic3r/Print.hpp>

class QLabel;
class QGroupBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QVBoxLayout;


/*
 *	设置参数的自定义控件，包括基本参数、耗材参数、固件参数等
 */
class PrintConfigWidget :public QWidget
{
	Q_OBJECT

public:
	PrintConfigWidget(DynamicPrintConfig* config, QWidget* parent = 0);
	~PrintConfigWidget();
	
	void InitMainLayout();
	void InitGeneralSettting();
	void InitInfillSetting();
	void InitSupportSetting();
	void InitSpeedSetting();
	void InitFilaSetting();
	void InitTempSettting();
	void InitSizeSetting();
	void InitFirmwareSetting();

	

private slots:
	void ValidateSupport(int valid);
	void ChangeBedShape();


	//窗口控件
private:
	DynamicPrintConfig* config_;

	int scale0_;
	int scale1_;
	int hspacing_;
	int vertical_spacing_;
	int horizon_spacing_;
	
	///基本参数
	QGroupBox* general_groupbox_;

	///填充相关参数
	QGroupBox* infill_groupbox_;	//*填充参数设置

	///支撑结构相关参数
	QGroupBox* support_groupbox_;
	QComboBox* gen_support_combobox_;	//是否生成支撑
	QDoubleSpinBox* pattern_spacing_spinbox_;	//支撑材料之间的间隔
	QComboBox* contact_Zdistance_combobox_;	//支撑材料和模型之间的间隔
	QComboBox* support_pattern_combobox_;	//支撑结构生成模式
	QComboBox* support_bridge_combobox_;	//是否支撑Bridge结构
	QSpinBox* raft_layers_spinbox_;		//raft层数

	///打印速度相关参数
	QGroupBox* speed_groupbox_;


	///打印耗材相关参数
	QGroupBox* fila_config_groupbox_;	

	///打印温度相关参数
	QGroupBox* temp_config_groupbox_;		//温度设置

	///打印机尺寸相关参数
	QGroupBox* size_config_groupbox_;
	QSpinBox* size_x_spinbox_;	//热床尺寸-x
	QSpinBox* size_y_spinbox_;	//热床尺寸-y
	QSpinBox* origin_x_spinbox_;	//热床原点位置-x
	QSpinBox* origin_y_spinbox_;	//热床原点位置-y

	///固件相关参数
	QGroupBox* firmware_config_groupbox_;
};

