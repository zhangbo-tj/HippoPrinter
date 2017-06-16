#pragma once
#include "qwidget.h"

class QLabel;
class QGroupBox;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QGridLayout;
class QVBoxLayout;


class PrintConfigWidget :public QWidget
{
	Q_OBJECT

public:
	PrintConfigWidget(QWidget* parent = 0);
	~PrintConfigWidget();
	void InitWidgets();	//��ʼ���ؼ�
	void InitLayout();	//��ʼ������
	void InitConnections();

private slots:
	void ValidateSupport(int valid);

	//���ڿؼ�
private:
	
	QGridLayout* print_config_main_layout_;	//ȫ�ֲ���
	QGridLayout* general_config_layout_;	//�����������ò���
	QGridLayout* infill_config_layout_;		//���������ò���
	QGridLayout* support_config_layout_;	//֧�Ų������ò���
	QGridLayout* speed_config_layout_;		//�ٶȲ������ò���

	//*������������
	QGroupBox* general_groupbox_;

	
	QLabel* layer_height_label_;	//���
	QDoubleSpinBox* layer_height_spinbox_;

	QLabel* first_layer_height_label_;	//��һ����
	QDoubleSpinBox* first_layer_height_spinbox_;

	
	QLabel* perimeters_label_;		//��ǲ���
	QSpinBox* perimeter_spinbox_;

	
	QLabel* solid_layer_label_;		// ʵ�Ĳ���Ŀ
	QSpinBox* top_solid_spinbox_;	//����ʵ�Ĳ���Ŀ
	QSpinBox* bottom_solid_spinbox_;	//�ײ�ʵ������Ŀ


	
	QGroupBox* infill_groupbox_;	//*����������

	
	QLabel* fill_desnity_label_;		//����ܶ�
	QSpinBox* fill_desnity_spinbox_;

	
	QLabel* fill_pattern_label_;	//���ģʽ
	QComboBox* fill_pattern_combobox_;

	
	QLabel* top_fill_pattern_label_;	//�������ģʽ
	QComboBox* top_fill_pattern_combobox_;

	QLabel* bottom_fill_pattern_label_;		//�Ͳ����ģʽ
	QComboBox* bottom_fill_pattern_combobox_;

	//֧�Ų�������
	QGroupBox* support_groupbox_;

	
	QLabel* gen_support_label_;		//�Ƿ�����֧��
	QComboBox* gen_support_combobox_;

	
	QLabel* pattern_spacing_label_;		//֧�Ų���֮��ļ��
	QDoubleSpinBox* pattern_spacing_spinbox_;

	
	QLabel* contact_Zdistance_label_;		//֧�Ų��Ϻ�ģ��֮��ļ��
	QComboBox* contact_Zdistance_combobox_;

	QLabel* support_pattern_label_;		//֧�Žṹ����ģʽ
	QComboBox* support_pattern_combobox_;

	
	QLabel* support_bridge_label_;		//֧��Bridge�ṹ
	QComboBox* support_bridge_combobox_;

	
	QLabel* raft_layers_label_;		//ģ�͵װ�߶�
	QSpinBox* raft_layers_spinbox_;

	//�ٶȲ�������
	QGroupBox* speed_groupbox_;

	
	QLabel* peri_speed_label_;		//��Ե�����ƶ��ٶ�
	QSpinBox* peri_speed_spinbox_;

	
	QLabel* infill_speed_label_;	//��������ƶ��ٶ�	
	QSpinBox* infill_speed_spinbox_;

	QLabel* bridge_speed_label_;	//���������ӡ�ٶ�
	QSpinBox* bridge_speed_spinbox_;

	QLabel* support_speed_label_;	//֧�Žṹ��ӡ�ٶ�
	QSpinBox* support_speed_spinbox_;

	QLabel* travel_speed_label_;	//��ӡ������ƶ��ٶ�
	QSpinBox* travel_speed_spinbox_;
};

