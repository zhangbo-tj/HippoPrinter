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
 *	���ò������Զ���ؼ������������������ĲĲ������̼�������
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


	//���ڿؼ�
private:
	DynamicPrintConfig* config_;

	int scale0_;
	int scale1_;
	int hspacing_;
	int vertical_spacing_;
	int horizon_spacing_;
	
	///��������
	QGroupBox* general_groupbox_;

	///�����ز���
	QGroupBox* infill_groupbox_;	//*����������

	///֧�Žṹ��ز���
	QGroupBox* support_groupbox_;
	QComboBox* gen_support_combobox_;	//�Ƿ�����֧��
	QDoubleSpinBox* pattern_spacing_spinbox_;	//֧�Ų���֮��ļ��
	QComboBox* contact_Zdistance_combobox_;	//֧�Ų��Ϻ�ģ��֮��ļ��
	QComboBox* support_pattern_combobox_;	//֧�Žṹ����ģʽ
	QComboBox* support_bridge_combobox_;	//�Ƿ�֧��Bridge�ṹ
	QSpinBox* raft_layers_spinbox_;		//raft����

	///��ӡ�ٶ���ز���
	QGroupBox* speed_groupbox_;


	///��ӡ�Ĳ���ز���
	QGroupBox* fila_config_groupbox_;	

	///��ӡ�¶���ز���
	QGroupBox* temp_config_groupbox_;		//�¶�����

	///��ӡ���ߴ���ز���
	QGroupBox* size_config_groupbox_;
	QSpinBox* size_x_spinbox_;	//�ȴ��ߴ�-x
	QSpinBox* size_y_spinbox_;	//�ȴ��ߴ�-y
	QSpinBox* origin_x_spinbox_;	//�ȴ�ԭ��λ��-x
	QSpinBox* origin_y_spinbox_;	//�ȴ�ԭ��λ��-y

	///�̼���ز���
	QGroupBox* firmware_config_groupbox_;
};

