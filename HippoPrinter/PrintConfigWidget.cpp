#include "PrintConfigWidget.h"

#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QDebug>
PrintConfigWidget::PrintConfigWidget(QWidget * parent)
	:QWidget(parent)
{

	InitWidgets();
	InitLayout();
	InitConnections();
	setStyleSheet(
		"QWidget{font-size:12pt;}"
		"QGroupBox{font-size:16pt; background-color:papayawhip; padding:10px;margin:20px;}"
		"QSpinBox{min-height:30px;}"
		"QComboBox{min-height:30px;}"
		"QDoubleSpinBox{min-height:30px;}"
	);
}

PrintConfigWidget::~PrintConfigWidget()
{
}


/*
	��ʼ���ؼ�
*/
void PrintConfigWidget::InitWidgets() {
	//��ʼ������
	print_config_main_layout_ = new QGridLayout();
	//print_config_main_layout_->setSpacing(30);
	general_config_layout_ = new QGridLayout();
	general_config_layout_->setSpacing(10);
	infill_config_layout_ = new QGridLayout();
	infill_config_layout_->setSpacing(20);
	support_config_layout_ = new QGridLayout();
	support_config_layout_->setSpacing(20);
	speed_config_layout_ = new QGridLayout();
	speed_config_layout_->setSpacing(20);

	//*������������
	general_groupbox_ = new QGroupBox(QString::fromLocal8Bit("��������"));

	general_groupbox_->setStyleSheet(
		"QGroupBox{font:16px Times}"
	);

	//���ò��
	layer_height_label_ = new QLabel(QString::fromLocal8Bit("���:"));
	layer_height_spinbox_ = new QDoubleSpinBox();
	layer_height_spinbox_->setRange(0.1, 0.3);
	layer_height_spinbox_->setSingleStep(0.05);
	layer_height_spinbox_->setSuffix(QString::fromLocal8Bit("  mm"));
	layer_height_spinbox_->setWrapping(false);
	layer_height_spinbox_->setValue(0.2);		//Ĭ��ֵ

	//���õ�һ����
	first_layer_height_label_ = new QLabel(QString::fromLocal8Bit("��һ����:"));
	first_layer_height_spinbox_ = new QDoubleSpinBox();
	first_layer_height_spinbox_->setRange(0.1, 0.5);
	first_layer_height_spinbox_->setSingleStep(0.05);
	first_layer_height_spinbox_->setSuffix(QString::fromLocal8Bit("  mm"));
	first_layer_height_spinbox_->setWrapping(false);
	first_layer_height_spinbox_->setValue(0.2);
	
	//������ǲ���
	perimeters_label_ = new QLabel(QString::fromLocal8Bit("��ǲ���:"));
	perimeter_spinbox_ = new QSpinBox();
	perimeter_spinbox_->setRange(0, 50);
	perimeter_spinbox_->setSuffix(QString::fromLocal8Bit("  (��Сֵ)"));
	perimeter_spinbox_->setWrapping(false);
	perimeter_spinbox_->setSingleStep(1);
	perimeter_spinbox_->setValue(2);		//Ĭ��ֵ
	

	//����ʵ�Ĳ����
	solid_layer_label_ = new QLabel(QString::fromLocal8Bit("ʵ�Ĳ�����"));
	//topSolidLabel = new QLabel(QString::fromLocal8Bit("���㣺"));
	top_solid_spinbox_ = new QSpinBox();
	top_solid_spinbox_->setMinimum(0);
	top_solid_spinbox_->setPrefix(QString::fromLocal8Bit("���㣺 "));
	top_solid_spinbox_->setSuffix(QString::fromLocal8Bit("  ��"));
	top_solid_spinbox_->setWrapping(false);
	top_solid_spinbox_->setValue(2);		//Ĭ��ֵ
	//bottoSolidLabel = new QLabel(QString::fromLocal8Bit("�ײ㣺"));
	bottom_solid_spinbox_ = new QSpinBox();
	bottom_solid_spinbox_->setMinimum(0);
	bottom_solid_spinbox_->setPrefix(QString::fromLocal8Bit("�ײ㣺  "));
	bottom_solid_spinbox_->setSuffix(QString::fromLocal8Bit("  ��"));
	bottom_solid_spinbox_->setWrapping(false);
	bottom_solid_spinbox_->setValue(2);	//Ĭ��ֵ
	


	//*����������
	infill_groupbox_ = new QGroupBox(QString::fromLocal8Bit("��������"));

	//��������ܶ�
	fill_desnity_label_ = new QLabel(QString::fromLocal8Bit("����ܶ�"));
	fill_desnity_spinbox_ = new QSpinBox();
	fill_desnity_spinbox_->setRange(0, 100);
	fill_desnity_spinbox_->setSuffix(QString::fromLocal8Bit("  %"));
	fill_desnity_spinbox_->setWrapping(false);
	fill_desnity_spinbox_->setValue(60);	//Ĭ��ֵ
	

	//���ģʽ����
	fill_pattern_label_ = new QLabel(QString::fromLocal8Bit("���ģʽ��"));
	fill_pattern_combobox_ = new QComboBox();
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Rectilinear"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Grid"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Line"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Concentric"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Honeycomb"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("3D Honeycomb"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	fill_pattern_combobox_->setCurrentIndex(0);	//Ĭ��ֵ

	//�������ģʽ
	top_fill_pattern_label_ = new QLabel(QString::fromLocal8Bit("�������ģʽ��"));
	top_fill_pattern_combobox_ = new QComboBox();
	top_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Rectilinear"));
	top_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Concentric"));
	top_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	top_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	top_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	top_fill_pattern_combobox_->setCurrentIndex(0);	//Ĭ��ֵ


	//�ײ����ģʽ
	bottom_fill_pattern_label_ = new QLabel(QString::fromLocal8Bit("�ײ����ģʽ��"));
	bottom_fill_pattern_combobox_ = new QComboBox();
	bottom_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Rectilinear"));
	bottom_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Concentric"));
	bottom_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	bottom_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	bottom_fill_pattern_combobox_->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	bottom_fill_pattern_combobox_->setCurrentIndex(0);	//Ĭ��ֵ


	//֧�Ų�������
	support_groupbox_ = new QGroupBox(QString::fromLocal8Bit("֧�Ų�������"));

	//����֧�Žṹ
	gen_support_label_ = new QLabel(QString::fromLocal8Bit("֧�Žṹ����"));
	gen_support_combobox_ = new QComboBox();
	gen_support_combobox_->addItem(QString::fromLocal8Bit("��"));
	gen_support_combobox_->addItem(QString::fromLocal8Bit("��"));
	gen_support_combobox_->setCurrentIndex(0);	//Ĭ��ֵ
	//TODO: ��Ӽ�������

	//֧�Ų���֮��ļ��
	pattern_spacing_label_ = new QLabel(QString::fromLocal8Bit("֧�ż����"));
	pattern_spacing_spinbox_ = new QDoubleSpinBox();
	pattern_spacing_spinbox_->setRange(0, 5);
	pattern_spacing_spinbox_->setSingleStep(0.1);
	pattern_spacing_spinbox_->setWrapping(false);
	pattern_spacing_spinbox_->setSuffix(QString::fromLocal8Bit("  mm"));
	pattern_spacing_spinbox_->setValue(2.5);	//Ĭ��ֵ

	//֧�Ų��Ϻ�ģ��֮��ļ��
	contact_Zdistance_label_ = new QLabel(QString::fromLocal8Bit("֧����ģ�ͼ����"));
	contact_Zdistance_combobox_ = new QComboBox();
	contact_Zdistance_combobox_->addItem(QString::fromLocal8Bit("0mm(soluble)"));
	contact_Zdistance_combobox_->addItem(QString::fromLocal8Bit("0.2mm(detachable)"));
	contact_Zdistance_combobox_->setCurrentIndex(1);	//Ĭ��ֵ

	//֧�Žṹ����ģʽ
	support_pattern_label_ = new QLabel(QString::fromLocal8Bit("֧�Žṹ����ģʽ:"));
	support_pattern_combobox_ = new QComboBox();
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("pillars"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("reclinnear"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("reclinear grid"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("honeycomb"));
	support_pattern_combobox_->setCurrentIndex(1);

	//�Ƿ�֧��Bridge�ṹ
	support_bridge_label_ = new QLabel(QString::fromLocal8Bit("֧��Bridge�ṹ:"));
	support_bridge_combobox_ = new QComboBox();
	support_bridge_combobox_->addItem(QString::fromLocal8Bit("��"));
	support_bridge_combobox_->addItem(QString::fromLocal8Bit("��"));
	support_bridge_combobox_->setCurrentIndex(1);

	//raft����
	raft_layers_label_ = new QLabel(QString::fromLocal8Bit("Raft������"));
	raft_layers_spinbox_ = new QSpinBox();
	raft_layers_spinbox_->setMinimum(0);
	raft_layers_spinbox_->setSuffix(QString::fromLocal8Bit("  ��"));
	raft_layers_spinbox_->setWrapping(false);

	//�ٶȲ�������
	speed_groupbox_ = new QGroupBox(QString::fromLocal8Bit("�ٶȲ�������"));

	//��Ե����Ĵ�ӡ�ٶ�
	peri_speed_label_ = new QLabel(QString::fromLocal8Bit("��Ǵ�ӡ�ٶ�:"));
	peri_speed_spinbox_ = new QSpinBox();
	peri_speed_spinbox_->setMinimum(0);
	peri_speed_spinbox_->setSuffix(QString::fromLocal8Bit("  mm/s"));
	peri_speed_spinbox_->setSingleStep(1);
	peri_speed_spinbox_->setMinimum(1);
	peri_speed_spinbox_->setWrapping(false);
	peri_speed_spinbox_->setValue(50);	//Ĭ��ֵ

	//�������Ĵ�ӡ�ٶ�
	infill_speed_label_ = new QLabel(QString::fromLocal8Bit("��������ӡ�ٶ�:"));
	infill_speed_spinbox_ = new QSpinBox();
	infill_speed_spinbox_->setSuffix(QString::fromLocal8Bit("  mm/s"));
	infill_speed_spinbox_->setSingleStep(1);
	infill_speed_spinbox_->setWrapping(false);
	infill_speed_spinbox_->setMinimum(1);
	infill_speed_spinbox_->setValue(60);	//Ĭ��ֵ

	//���������ӡ�ٶ�
	bridge_speed_label_ = new QLabel(QString::fromLocal8Bit("���������ӡ�ٶ�:"));
	bridge_speed_spinbox_ = new QSpinBox();
	bridge_speed_spinbox_->setSuffix(QString::fromLocal8Bit("  mm/s"));
	bridge_speed_spinbox_->setSingleStep(1);
	bridge_speed_spinbox_->setWrapping(false);
	bridge_speed_spinbox_->setMinimum(1);
	bridge_speed_spinbox_->setValue(60);

	//֧�Žṹ��ӡ�ٶ�
	support_speed_label_ = new QLabel(QString::fromLocal8Bit("֧�Žṹ��ӡ�ٶ�:"));
	support_speed_spinbox_ = new QSpinBox();
	support_speed_spinbox_->setSuffix(QString::fromLocal8Bit("  mm/s"));
	support_speed_spinbox_->setSingleStep(1);
	support_speed_spinbox_->setMinimum(1);
	support_speed_spinbox_->setValue(60);

	//��ӡ������ƶ��ٶ�
	travel_speed_label_ = new QLabel(QString::fromLocal8Bit("����ƶ��ٶȣ�"));
	travel_speed_spinbox_ = new QSpinBox();
	travel_speed_spinbox_->setSuffix(QString::fromLocal8Bit("  mm/s"));
	travel_speed_spinbox_->setSingleStep(1);
	travel_speed_spinbox_->setWrapping(false);
	travel_speed_spinbox_->setValue(60);	//Ĭ��ֵ
}

//************************************************************************
// ���ڣ�2016/12/26 
// ����: void
// ����: ���ÿؼ�����
//************************************************************************

void PrintConfigWidget::InitLayout() {

	//�����������ò��ֳ�ʼ��
	general_config_layout_->addWidget(layer_height_label_, 0, 0);
	general_config_layout_->addWidget(layer_height_spinbox_, 0, 1);
	general_config_layout_->addWidget(first_layer_height_label_, 1, 0);
	general_config_layout_->addWidget(first_layer_height_spinbox_, 1, 1);
	general_config_layout_->addWidget(perimeters_label_, 2, 0);
	general_config_layout_->addWidget(perimeter_spinbox_, 2, 1);
	general_config_layout_->addWidget(solid_layer_label_, 3, 0);
	general_config_layout_->addWidget(top_solid_spinbox_, 3, 1);
	general_config_layout_->addWidget(bottom_solid_spinbox_, 4, 1);
	general_groupbox_->setLayout(general_config_layout_);

	//���������ò��ֳ�ʼ��
	infill_config_layout_->addWidget(fill_desnity_label_, 0, 0);
	infill_config_layout_->addWidget(fill_desnity_spinbox_, 0, 1);
	infill_config_layout_->addWidget(fill_pattern_label_, 1, 0);
	infill_config_layout_->addWidget(fill_pattern_combobox_, 1, 1);
	infill_config_layout_->addWidget(top_fill_pattern_label_, 2, 0);
	infill_config_layout_->addWidget(top_fill_pattern_combobox_, 2, 1);
	infill_config_layout_->addWidget(bottom_fill_pattern_label_, 3, 0);
	infill_config_layout_->addWidget(bottom_fill_pattern_combobox_, 3, 1);
	infill_groupbox_->setLayout(infill_config_layout_);

	//֧�Ų������ò��ֳ�ʼ��
	support_config_layout_->addWidget(gen_support_label_,0,0);
	support_config_layout_->addWidget(gen_support_combobox_, 0, 1);
	support_config_layout_->addWidget(pattern_spacing_label_, 1, 0);
	support_config_layout_->addWidget(pattern_spacing_spinbox_, 1, 1);
	support_config_layout_->addWidget(contact_Zdistance_label_, 2, 0);
	support_config_layout_->addWidget(contact_Zdistance_combobox_, 2, 1);
	support_config_layout_->addWidget(support_pattern_label_, 3, 0);
	support_config_layout_->addWidget(support_pattern_combobox_, 3, 1);
	support_config_layout_->addWidget(support_bridge_label_, 4, 0);
	support_config_layout_->addWidget(support_bridge_combobox_, 4, 1);
	support_config_layout_->addWidget(raft_layers_label_, 5, 0);
	support_config_layout_->addWidget(raft_layers_spinbox_, 5, 1);
	support_groupbox_->setLayout(support_config_layout_);

	//�ٶȲ������ò��ֳ�ʼ��
	speed_config_layout_->addWidget(peri_speed_label_, 0, 0);
	speed_config_layout_->addWidget(peri_speed_spinbox_, 0, 1);
	speed_config_layout_->addWidget(infill_speed_label_,1,0);
	speed_config_layout_->addWidget(infill_speed_spinbox_, 1, 1);
	speed_config_layout_->addWidget(bridge_speed_label_, 2, 0);
	speed_config_layout_->addWidget(bridge_speed_spinbox_, 2, 1);
	speed_config_layout_->addWidget(support_speed_label_, 3, 0);
	speed_config_layout_->addWidget(support_speed_spinbox_, 3, 1);
	speed_config_layout_->addWidget(travel_speed_label_, 4, 0);
	speed_config_layout_->addWidget(travel_speed_spinbox_, 4, 1);
	speed_groupbox_->setLayout(speed_config_layout_);

	//ȫ�ֲ��ֳ�ʼ��
	print_config_main_layout_->addWidget(general_groupbox_,0,0);
	print_config_main_layout_->addWidget(infill_groupbox_,0,1);
	print_config_main_layout_->addWidget(support_groupbox_,1,0);
	print_config_main_layout_->addWidget(speed_groupbox_,1,1);
	print_config_main_layout_->setRowStretch(0, 4);
	print_config_main_layout_->setRowStretch(1, 4);
	print_config_main_layout_->setRowStretch(2, 1);
	//print_config_main_layout_->setColumnStretch(2, 1);
	setLayout(print_config_main_layout_);
}


void PrintConfigWidget::ValidateSupport(int index) {
	bool valid = (index == 0);
	pattern_spacing_spinbox_->setEnabled(valid);
	contact_Zdistance_combobox_->setEnabled(valid);
	support_pattern_combobox_->setEnabled(valid);
	support_bridge_combobox_->setEnabled(valid);
	raft_layers_spinbox_->setEnabled(valid);
}


void PrintConfigWidget::InitConnections() {
	connect(gen_support_combobox_, SIGNAL(currentIndexChanged(int)), this, SLOT(ValidateSupport(int)));
}