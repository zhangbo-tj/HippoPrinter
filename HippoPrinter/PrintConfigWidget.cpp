#include "PrintConfigWidget.h"


#include <iostream>

#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDebug>
PrintConfigWidget::PrintConfigWidget(DynamicPrintConfig* config, QWidget * parent)
	:QWidget(parent),config_(config)
{
	scale0_ = 1;
	scale1_ = 1;
	horizon_spacing_ = 0;

	InitMainLayout();

	setStyleSheet(
		"QGroupBox{font:14pt \"微软雅黑\"; background:rgba(197,197,197,75%); padding:5px;margin:0px;min-width:10px;}"
		"QWidget{font:11pt \"微软雅黑\";}"
		"QSpinBox{min-height:20px;}"
		"QComboBox{min-height:20px;}"
		"QDoubleSpinBox{min-height:20px;}"
	);
}

PrintConfigWidget::~PrintConfigWidget(){

}


/*
 *	初始化页面布局和子控件
 */
void PrintConfigWidget::InitMainLayout() {
	//设置垂直布局
	QVBoxLayout* print_config_main_layout = new QVBoxLayout;
	print_config_main_layout->setSpacing(20);
	//添加控件

	//初始化并添加基本参数相关控件
	InitGeneralSettting();
	print_config_main_layout->addWidget(general_groupbox_);
	
	//初始化并添加填充参数相关控件
	InitInfillSetting();
	print_config_main_layout->addWidget(infill_groupbox_);
	
	//初始化并添加支撑参数相关控件
	InitSupportSetting();
	print_config_main_layout->addWidget(support_groupbox_);

	//初始化并添加速度参数相关控件
	InitSpeedSetting();
	print_config_main_layout->addWidget(speed_groupbox_);

	//初始化并添加耗材参数相关控件
	InitFilaSetting();
	print_config_main_layout->addWidget(fila_config_groupbox_);

	//初始化并添加温度参数相关控件
	InitTempSettting();
	print_config_main_layout->addWidget(temp_config_groupbox_);

	//初始化并添加尺寸参数相关控件
	InitSizeSetting();
	print_config_main_layout->addWidget(size_config_groupbox_);

	//初始化并添加固件参数相关控件
	InitFirmwareSetting();
	print_config_main_layout->addWidget(firmware_config_groupbox_);

	//设置布局
	setLayout(print_config_main_layout);
}


/*
 *	初始化基本参数相关控件，包括控件属性及其信号槽
 */
void PrintConfigWidget::InitGeneralSettting() {
	//*基本参数设置
	general_groupbox_ = new QGroupBox(QString::fromLocal8Bit("基本参数"));

	///设置层高的相关控件
	QLabel* layer_height_label = new QLabel(QString::fromLocal8Bit("层高:"));
	QDoubleSpinBox* layer_height_spinbox = new QDoubleSpinBox();
	layer_height_spinbox->setRange(0.1, 0.3);
	layer_height_spinbox->setSingleStep(0.05);
	layer_height_spinbox->setSuffix(QString::fromLocal8Bit("  mm"));
	layer_height_spinbox->setWrapping(false);

	config_->optptr("layer_height", true)->set(*(config_->def->get("layer_height")->default_value));
	layer_height_spinbox->setValue(config_->option("layer_height")->getFloat());
	connect(layer_height_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
		[=](double d) {
		config_->option("layer_height")->set(ConfigOptionFloatOrPercent(d, false));
	});


	///设置第一层层高的相关控件
	QLabel* first_layer_height_label = new QLabel(QString::fromLocal8Bit("第一层层高:"));
	QDoubleSpinBox* first_layer_height_spinbox = new QDoubleSpinBox();
	first_layer_height_spinbox->setRange(0.1, 0.5);
	first_layer_height_spinbox->setSingleStep(0.05);
	first_layer_height_spinbox->setSuffix(QString::fromLocal8Bit("  mm"));
	first_layer_height_spinbox->setWrapping(false);
	//first_layer_height_spinbox_->setValue(0.2);
	config_->optptr("first_layer_height", true)->set(*(config_->def->get("first_layer_height")->default_value));
	first_layer_height_spinbox->setValue(config_->option("first_layer_height")->getFloat());
	connect(first_layer_height_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
		[=](double d) {
		config_->option("first_layer_height")->set(ConfigOptionFloatOrPercent(d, false));
		//qDebug() << "first_layer_height:" << config_->option("first_layer_height")->getFloat();
	});


	///设置外壳层数的相关控件
	QLabel* perimeters_label = new QLabel(QString::fromLocal8Bit("外壳层数:"));
	QSpinBox* perimeter_spinbox = new QSpinBox();
	perimeter_spinbox->setRange(0, 50);
	perimeter_spinbox->setSuffix(QString::fromLocal8Bit("  (最小值)"));
	perimeter_spinbox->setWrapping(false);
	perimeter_spinbox->setSingleStep(1);
	//perimeter_spinbox_->setValue(2);		//默认值
	config_->optptr("perimeters", true)->set(*(config_->def->get("perimeters")->default_value));
	perimeter_spinbox->setValue(config_->option("perimeters")->getInt());
	connect(perimeter_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("perimeters")->set(ConfigOptionInt(i));
		//qDebug() << "perimeters: " << config_->option("perimeters")->getInt();
	});


	///设置实心层层数的相关控件
	QLabel* solid_layer_label = new QLabel(QString::fromLocal8Bit("实心层数："));
	//顶层实心层数量
	QSpinBox* top_solid_spinbox = new QSpinBox();
	top_solid_spinbox->setMinimum(0);
	top_solid_spinbox->setPrefix(QString::fromLocal8Bit("顶层: "));
	top_solid_spinbox->setSuffix(QString::fromLocal8Bit(" 层"));
	top_solid_spinbox->setWrapping(false);
	//top_solid_spinbox_->setValue(2);		//默认值
	config_->optptr("top_solid_layers", true)->set(*(config_->def->get("top_solid_layers")->default_value));
	top_solid_spinbox->setValue(config_->option("top_solid_layers")->getInt());
	connect(top_solid_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("top_solid_layers")->set(ConfigOptionInt(i));
		//qDebug() << "top_solid_layers: " << config_->option("top_solid_layers")->getInt();
	});


	///设置底层实心层数的相关控件
	QSpinBox* bottom_solid_spinbox = new QSpinBox();
	bottom_solid_spinbox->setMinimum(0);
	bottom_solid_spinbox->setPrefix(QString::fromLocal8Bit("底层: "));
	bottom_solid_spinbox->setSuffix(QString::fromLocal8Bit(" 层"));
	bottom_solid_spinbox->setWrapping(false);
	//bottom_solid_spinbox_->setValue(2);	//默认值
	config_->optptr("bottom_solid_layers", true)->set(*(config_->def->get("bottom_solid_layers")->default_value));
	bottom_solid_spinbox->setValue(config_->option("bottom_solid_layers")->getInt());
	connect(bottom_solid_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("bottom_solid_layers")->set(ConfigOptionInt(i));
		//qDebug() << "bottom_solid_layers: " << config_->option("bottom_solid_layers")->getInt();
	});


	//初始化网格布局
	QGridLayout* general_config_layout = new QGridLayout;
	general_config_layout->setHorizontalSpacing(horizon_spacing_);
	general_config_layout->setColumnStretch(0, scale0_);
	general_config_layout->setColumnStretch(1, scale1_);

	//将相关控件添加到网格布局中
	general_config_layout->addWidget(layer_height_label, 0, 0);
	general_config_layout->addWidget(layer_height_spinbox, 0, 1);
	general_config_layout->addWidget(first_layer_height_label, 1, 0);
	general_config_layout->addWidget(first_layer_height_spinbox, 1, 1);
	general_config_layout->addWidget(perimeters_label, 2, 0);
	general_config_layout->addWidget(perimeter_spinbox, 2, 1);
	general_config_layout->addWidget(solid_layer_label, 3, 0);
	general_config_layout->addWidget(top_solid_spinbox, 3, 1);
	general_config_layout->addWidget(bottom_solid_spinbox, 4, 1);
	
	//设置布局
	general_groupbox_->setLayout(general_config_layout);
}


/*
 *	初始化设置填充参数相关控件，包括控件属性和信号量
 */
void PrintConfigWidget::InitInfillSetting() {
	//*填充参数设置
	infill_groupbox_ = new QGroupBox(QString::fromLocal8Bit("填充参数："));

	///设置填充密度的相关控件
	QLabel* fill_desnity_label = new QLabel(QString::fromLocal8Bit("填充密度"));
	QSpinBox* fill_desnity_spinbox = new QSpinBox();
	fill_desnity_spinbox->setRange(0, 100);
	fill_desnity_spinbox->setSuffix(QString::fromLocal8Bit("  %"));
	fill_desnity_spinbox->setWrapping(false);
	fill_desnity_spinbox->setSingleStep(10);
	//fill_desnity_spinbox_->setValue(60);	//默认值
	config_->optptr("fill_density", true)->set(*(config_->def->get("fill_density")->default_value));
	fill_desnity_spinbox->setValue(config_->option("fill_density")->getFloat());
	connect(fill_desnity_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		ConfigOption& option = ConfigOptionPercent(i);
		config_->option("fill_density")->set(option);
		//qDebug() << "fill_density:" << config_->option("fill_density")->getFloat();
	});


	///设置填充模式的相关控件
	QLabel* fill_pattern_label = new QLabel(QString::fromLocal8Bit("填充模式："));
	QComboBox* fill_pattern_combobox = new QComboBox();
	fill_pattern_combobox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Rectilinear"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Aligned Rectilinear"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Grid"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Triangles"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Stars"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Cubic"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Concentric"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Honeycomb"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("3D Honeycomb"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	fill_pattern_combobox->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	fill_pattern_combobox->setCurrentIndex(4);	//默认值
	config_->option("fill_pattern", true)->set(*(config_->def->get("fill_pattern")->default_value));
	connect(fill_pattern_combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index) {
		case 0:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipRectilinear)); 
			break;
		case 1:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipAlignedRectilinear));
			break;
		case 2:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipGrid));
			break;
		case 3:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipTriangles));
			break;
		case 4:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipStars));
			break;
		case 5:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipCubic));
			break;
		case 6:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipConcentric));
			break;
		case 7:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipHoneycomb));
			break;
		case 8:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ip3DHoneycomb));
			break;
		case 9:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipHilbertCurve));
			break;
		case 10:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipArchimedeanChords));
			break;
		default:
			config_->option("fill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipOctagramSpiral));
			break;
		}
		//std::string res =  "fill_pattern:" + config_->option("fill_pattern")->serialize();
		//qDebug() << res.c_str();
	});


	///设置顶层填充模式的相关控件
	QLabel* top_fill_pattern_label = new QLabel(QString::fromLocal8Bit("顶层填充模式："));
	QComboBox* top_fill_pattern_combobox = new QComboBox();
	top_fill_pattern_combobox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	top_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Rectilinear"));
	top_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Concentric"));
	top_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	top_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	top_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	top_fill_pattern_combobox->setCurrentIndex(0);	//默认值
	ConfigOption* top_fill_pattern_config = config_->optptr("top_infill_pattern", true);
	top_fill_pattern_config->set(*(config_->def->get("top_infill_pattern")->default_value));
	connect(top_fill_pattern_combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
		[=](int index) {
		switch (index) {
		case 0:
			config_->option("top_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipRectilinear));
			break;
		case 1:
			config_->option("top_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipConcentric));
			break;
		case 2:
			config_->option("top_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipHilbertCurve));
			break;
		case 3:
			config_->option("top_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipArchimedeanChords));
			break;
		default:
			config_->option("top_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipOctagramSpiral));
			break;
		}
		//std::string res = "top_infill_pattern:" +  config_->option("top_infill_pattern")->serialize();
		//qDebug() << res.c_str();
	});


	///设置底层填充模式的相关控件
	QLabel* bottom_fill_pattern_label = new QLabel(QString::fromLocal8Bit("底层填充模式："));
	QComboBox* bottom_fill_pattern_combobox = new QComboBox();
	bottom_fill_pattern_combobox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	bottom_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Rectilinear"));
	bottom_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Concentric"));
	bottom_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Hilbert Curve"));
	bottom_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Archimedean Chords"));
	bottom_fill_pattern_combobox->addItem(QString::fromLocal8Bit("Octagram Spiral"));
	bottom_fill_pattern_combobox->setCurrentIndex(0);	//默认值
	ConfigOption* bottom_fill_pattern_config = config_->optptr("bottom_infill_pattern", true);
	bottom_fill_pattern_config->set(*(config_->def->get("bottom_infill_pattern")->default_value));
	connect(bottom_fill_pattern_combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
		[=](int index) {
		switch (index) {
		case 0:
			config_->option("bottom_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipRectilinear));
			break;
		case 1:
			config_->option("bottom_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipConcentric));
			break;
		case 2:
			config_->option("bottom_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipHilbertCurve));
			break;
		case 3:
			config_->option("bottom_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipArchimedeanChords));
			break;
		default:
			config_->option("bottom_infill_pattern")->set(ConfigOptionEnum<InfillPattern>(ipOctagramSpiral));
			break;
		}
		//std::string res = "bottom_infill_pattern:" + config_->option("bottom_infill_pattern")->serialize();
		//qDebug() << res.c_str();
	});


	//初始化网格布局
	QGridLayout* infill_config_layout = new QGridLayout;
	infill_config_layout->setHorizontalSpacing(horizon_spacing_);
	infill_config_layout->setColumnStretch(0, scale0_);
	infill_config_layout->setColumnStretch(1, scale1_);

	//将控件添加到网格布局中
	infill_config_layout->addWidget(fill_desnity_label, 0, 0);
	infill_config_layout->addWidget(fill_desnity_spinbox, 0, 1);
	infill_config_layout->addWidget(fill_pattern_label, 1, 0);
	infill_config_layout->addWidget(fill_pattern_combobox, 1, 1);
	infill_config_layout->addWidget(top_fill_pattern_label, 2, 0);
	infill_config_layout->addWidget(top_fill_pattern_combobox, 2, 1);
	infill_config_layout->addWidget(bottom_fill_pattern_label, 3, 0);
	infill_config_layout->addWidget(bottom_fill_pattern_combobox, 3, 1);
	
	//设置布局
	infill_groupbox_->setLayout(infill_config_layout);
}

/*
 *	初始化支撑结构参数相关控件，包括控件属性及其信号量
 */
void PrintConfigWidget::InitSupportSetting() {
	//支撑参数设置
	support_groupbox_ = new QGroupBox(QString::fromLocal8Bit("支撑参数设置"));

	///设置生成支撑结构相关控件
	QLabel* gen_support_label = new QLabel(QString::fromLocal8Bit("生成支撑:"));
	gen_support_combobox_ = new QComboBox();
	gen_support_combobox_->addItem(QString::fromLocal8Bit("是"));
	gen_support_combobox_->addItem(QString::fromLocal8Bit("否"));
	gen_support_combobox_->setCurrentIndex(0);	//默认值
	ConfigOption* gen_support_config = config_->optptr("support_material", true);
	gen_support_config->set(ConfigOptionBool(true));
	connect(gen_support_combobox_, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index){
		case 0:
			config_->option("support_material")->set(ConfigOptionBool(true));
			break;
		default:
			config_->option("support_material")->set(ConfigOptionBool(false));
			break;
		}
		////qDebug() << "gen_support_material:" << (index == 0 ? "false" : "true");
	});
	connect(gen_support_combobox_, SIGNAL(currentIndexChanged(int)), this, SLOT(ValidateSupport(int)));

	///设置支撑材料之间的间隔的相关控件
	QLabel* pattern_spacing_label = new QLabel(QString::fromLocal8Bit("间隔："));
	pattern_spacing_spinbox_ = new QDoubleSpinBox();
	pattern_spacing_spinbox_->setRange(0, 5);
	pattern_spacing_spinbox_->setSingleStep(0.1);
	pattern_spacing_spinbox_->setWrapping(false);
	pattern_spacing_spinbox_->setSuffix(QString::fromLocal8Bit("  mm"));
	//pattern_spacing_spinbox_->setValue(2.5);	//默认值
	ConfigOption* pattern_spacing_config = config_->optptr("support_material_spacing", true);
	pattern_spacing_config->set(*(config_->def->get("support_material_spacing")->default_value));
	pattern_spacing_spinbox_->setValue(config_->option("support_material_spacing")->getFloat());
	connect(pattern_spacing_spinbox_, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), 
		[=](double d) {
		config_->option("support_material_spacing")->set(ConfigOptionFloat(d));
		//qDebug() << "support_material_spacing" << config_->option("support_material_spacing")->getFloat();
	});



	///设置支撑材料和模型之间的间隔的相关控件
	QLabel* contact_Zdistance_label = new QLabel(QString::fromLocal8Bit("与模型间隔："));
	contact_Zdistance_combobox_ = new QComboBox();
	contact_Zdistance_combobox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	contact_Zdistance_combobox_->addItem(QString::fromLocal8Bit("0mm(soluble)"));
	contact_Zdistance_combobox_->addItem(QString::fromLocal8Bit("0.2mm(detachable)"));
	contact_Zdistance_combobox_->setCurrentIndex(1);	//默认值
	config_->optptr("support_material_contact_distance", true)->set(*(config_->def->get("support_material_contact_distance")->default_value));
	connect(contact_Zdistance_combobox_, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index)
		{
		case 0:
			config_->option("support_material_contact_distance")->set(ConfigOptionFloat(0));
			break;
		default:
			config_->option("support_material_contact_distance")->set(ConfigOptionFloat(0.2));
			break;
		}
		//qDebug() << "support_material_contact_distance: " << config_->option("support_material_contact_distance")->getFloat();
	});


	///设置支撑结构生成模式的相关控件
	QLabel* support_pattern_label = new QLabel(QString::fromLocal8Bit("生成模式:"));
	support_pattern_combobox_ = new QComboBox();
	support_pattern_combobox_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("reclinnear"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("reclinear grid"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("honeycomb"));
	support_pattern_combobox_->addItem(QString::fromLocal8Bit("pillars"));
	support_pattern_combobox_->setCurrentIndex(3);
	config_->optptr("support_material_pattern", true)->set(*(config_->def->get("support_material_pattern")->default_value));
	connect(support_pattern_combobox_, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index){
		case  0:
			config_->option("support_material_pattern")->set(ConfigOptionEnum<SupportMaterialPattern>(smpRectilinear));
			break;
		case 1:
			config_->option("support_material_pattern")->set(ConfigOptionEnum<SupportMaterialPattern>(smpRectilinearGrid));
			break;
		case 2:
			config_->option("support_material_pattern")->set(ConfigOptionEnum<SupportMaterialPattern>(smpHoneycomb));
			break;
		default:
			config_->option("support_material_pattern")->set(ConfigOptionEnum<SupportMaterialPattern>(smpPillars));
			break;
		}
		//std::string res = "support_material_pattern" + config_->option("support_material_pattern")->serialize();
		//qDebug() << res.c_str();
	});


	///设置是否支撑Bridge结构的相关控件
	QLabel* support_bridge_label = new QLabel(QString::fromLocal8Bit("支撑Bridge结构:"));
	support_bridge_combobox_ = new QComboBox();
	support_bridge_combobox_->addItem(QString::fromLocal8Bit("是"));
	support_bridge_combobox_->addItem(QString::fromLocal8Bit("否"));
	support_bridge_combobox_->setCurrentIndex(1);
	config_->optptr("dont_support_bridges", true)->set(*(config_->def->get("dont_support_bridges")->default_value));
	connect(support_bridge_combobox_, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index){
		case 0:
			config_->option("dont_support_bridges")->set(ConfigOptionBool(false));
			break;
		default:
			config_->option("dont_support_bridges")->set(ConfigOptionBool(true));
			break;
		}
		//std::string res ="dont_support_bridges" +  config_->option("dont_support_bridges")->serialize();
		//qDebug() << res.c_str();
	});


	///设置raft层数的相关控件
	QLabel* raft_layers_label = new QLabel(QString::fromLocal8Bit("Raft层数："));
	raft_layers_spinbox_ = new QSpinBox();
	raft_layers_spinbox_->setMinimum(0);
	raft_layers_spinbox_->setSuffix(QString::fromLocal8Bit("  层"));
	raft_layers_spinbox_->setWrapping(false);
	config_->optptr("raft_layers", true)->set(*(config_->def->get("raft_layers")->default_value));
	raft_layers_spinbox_->setValue(config_->option("raft_layers")->getInt());
	connect(raft_layers_spinbox_, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
		[=](int i) {
		config_->option("raft_layers")->set(ConfigOptionInt(i));
		//qDebug() << "raft_layers:" << config_->option("raft_layers")->getInt();
	});

	//初始化网格布局
	QGridLayout* support_config_layout = new QGridLayout;
	support_config_layout->setHorizontalSpacing(horizon_spacing_);
	support_config_layout->setColumnStretch(0, scale0_);
	support_config_layout->setColumnStretch(1, scale1_);

	//将控件添加到布局中
	support_config_layout->addWidget(gen_support_label, 0, 0);
	support_config_layout->addWidget(gen_support_combobox_, 0, 1);
	support_config_layout->addWidget(pattern_spacing_label, 1, 0);
	support_config_layout->addWidget(pattern_spacing_spinbox_, 1, 1);
	support_config_layout->addWidget(contact_Zdistance_label, 2, 0);
	support_config_layout->addWidget(contact_Zdistance_combobox_, 2, 1);
	support_config_layout->addWidget(support_pattern_label, 3, 0);
	support_config_layout->addWidget(support_pattern_combobox_, 3, 1);
	support_config_layout->addWidget(support_bridge_label, 4, 0);
	support_config_layout->addWidget(support_bridge_combobox_, 4, 1);
	support_config_layout->addWidget(raft_layers_label, 5, 0);
	support_config_layout->addWidget(raft_layers_spinbox_, 5, 1);
	
	//设置GroupBox的布局
	support_groupbox_->setLayout(support_config_layout);
}


/*
 *	设置速度参数相关控件，包括属性及其信号量
 */
void PrintConfigWidget::InitSpeedSetting() {
	//速度参数设置
	speed_groupbox_ = new QGroupBox(QString::fromLocal8Bit("速度参数设置"));

	///设置边缘区域的打印速度的相关控件
	QLabel* peri_speed_label = new QLabel(QString::fromLocal8Bit("外壳打印速度:"));
	QSpinBox* peri_speed_spinbox = new QSpinBox();
	peri_speed_spinbox->setMinimum(0);
	peri_speed_spinbox->setSuffix(QString::fromLocal8Bit("  mm/s"));
	peri_speed_spinbox->setSingleStep(1);
	peri_speed_spinbox->setMinimum(1);
	peri_speed_spinbox->setWrapping(false);
	//peri_speed_spinbox_->setValue(50);	//默认值
	config_->optptr("perimeter_speed",true)->set(*(config_->def->get("perimeter_speed")->default_value));
	peri_speed_spinbox->setValue(config_->option("perimeter_speed")->getFloat());
	connect(peri_speed_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
		[=](int i) {
		config_->option("perimeter_speed")->set(ConfigOptionFloat(i));
		//qDebug() << config_->option("perimeter_speed")->getFloat();
	});


	///设置填充区间的打印速度的相关控件
	QLabel* infill_speed_label = new QLabel(QString::fromLocal8Bit("Infill打印速度:"));
	QSpinBox* infill_speed_spinbox = new QSpinBox();
	infill_speed_spinbox->setSuffix(QString::fromLocal8Bit("  mm/s"));
	infill_speed_spinbox->setSingleStep(1);
	infill_speed_spinbox->setWrapping(false);
	infill_speed_spinbox->setMinimum(1);
	//infill_speed_spinbox_->setValue(60);	//默认值
	config_->optptr("infill_speed", true)->set(*(config_->def->get("infill_speed")->default_value));
	infill_speed_spinbox->setValue(config_->option("infill_speed")->getFloat());
	connect(infill_speed_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
		[=](int i) {
		config_->option("infill_speed")->set(ConfigOptionFloat(i));
		//qDebug() << "infill_speed: " << config_->option("infill_speed")->getFloat();
	});


	///设置悬空区域打印速度的相关控件
	QLabel* bridge_speed_label = new QLabel(QString::fromLocal8Bit("Bridge打印速度:"));
	QSpinBox* bridge_speed_spinbox = new QSpinBox();
	bridge_speed_spinbox->setSuffix(QString::fromLocal8Bit("  mm/s"));
	bridge_speed_spinbox->setSingleStep(1);
	bridge_speed_spinbox->setWrapping(false);
	bridge_speed_spinbox->setMinimum(1);
	//bridge_speed_spinbox_->setValue(60);
	config_->optptr("bridge_speed", true)->set(*(config_->def->get("bridge_speed")->default_value));
	bridge_speed_spinbox->setValue(config_->option("bridge_speed")->getFloat());
	connect(bridge_speed_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), 
		[=](int i) {
		config_->option("bridge_speed")->set(ConfigOptionFloat(i));
		//qDebug() << "bridge_speed: " << config_->option("bridge_speed")->getFloat();
	});


	///设置支撑结构打印速度的相关控件
	QLabel* support_speed_label = new QLabel(QString::fromLocal8Bit("Support打印速度:"));
	QSpinBox* support_speed_spinbox = new QSpinBox();
	support_speed_spinbox->setSuffix(QString::fromLocal8Bit("  mm/s"));
	support_speed_spinbox->setSingleStep(1);
	support_speed_spinbox->setMinimum(1);
	//support_speed_spinbox_->setValue(60);
	config_->optptr("support_material_speed", true)->set(*(config_->def->get("support_material_speed")->default_value));
	support_speed_spinbox->setValue(config_->option("support_material_speed")->getFloat());
	connect(support_speed_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("support_material_speed")->set(ConfigOptionFloat(i));
		//qDebug() << "support_material_speed: " << config_->option("support_material_speed")->getFloat();
	});


	///设置打印间隔的移动速度的相关控件
	QLabel* travel_speed_label = new QLabel(QString::fromLocal8Bit("间隔移动速度："));
	QSpinBox* travel_speed_spinbox = new QSpinBox();
	travel_speed_spinbox->setSuffix(QString::fromLocal8Bit("  mm/s"));
	travel_speed_spinbox->setSingleStep(1);
	travel_speed_spinbox->setWrapping(false);
	//travel_speed_spinbox_->setValue(60);	//默认值
	config_->optptr("travel_speed", true)->set(*(config_->def->get("travel_speed")->default_value));
	travel_speed_spinbox->setValue(config_->option("travel_speed")->getFloat());
	connect(travel_speed_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("travel_speed")->set(ConfigOptionFloat(i));
		//qDebug() << "travel_speed: " << config_->option("travel_speed")->getFloat();
	});

	//初始化网格布局
	QGridLayout* speed_config_layout = new QGridLayout;
	speed_config_layout->setHorizontalSpacing(horizon_spacing_);
	speed_config_layout->setColumnStretch(0, scale0_);
	speed_config_layout->setColumnStretch(1, scale1_);

	///将控件添加到网格布局中
	speed_config_layout->addWidget(peri_speed_label, 0, 0);
	speed_config_layout->addWidget(peri_speed_spinbox, 0, 1);
	speed_config_layout->addWidget(infill_speed_label, 1, 0);
	speed_config_layout->addWidget(infill_speed_spinbox, 1, 1);
	speed_config_layout->addWidget(bridge_speed_label, 2, 0);
	speed_config_layout->addWidget(bridge_speed_spinbox, 2, 1);
	speed_config_layout->addWidget(support_speed_label, 3, 0);
	speed_config_layout->addWidget(support_speed_spinbox, 3, 1);
	speed_config_layout->addWidget(travel_speed_label, 4, 0);
	speed_config_layout->addWidget(travel_speed_spinbox, 4, 1);
	
	///设置为网格布局
	speed_groupbox_->setLayout(speed_config_layout);
}



/*
 *	初始化耗材参数相关的控件，包括属性及其信号量
 */
void PrintConfigWidget::InitFilaSetting() {
	//初始化分组
	fila_config_groupbox_ = new QGroupBox(QString::fromLocal8Bit("耗材设置"));

	///设置耗材半径的的相关控件
	QLabel* diameter_label = new QLabel(QString::fromLocal8Bit("耗材半径："));
	QDoubleSpinBox* diameter_spinbox = new QDoubleSpinBox();
	diameter_spinbox->setRange(0, 3);
	diameter_spinbox->setWrapping(false);
	diameter_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	diameter_spinbox->setSingleStep(0.01);
	//diameter_spinbox_->setValue(1.75);	//默认值
	config_->optptr("filament_diameter", true)->set(*(config_->def->get("filament_diameter")->default_value));
	diameter_spinbox->setValue(dynamic_cast<ConfigOptionFloats*>(config_->option("filament_diameter"))->values[0]);
	connect(diameter_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), 
		[=](double d) {
		config_->option("filament_diameter")->set(ConfigOptionFloats(std::vector<double>(1,d)));
		//std::string res =  "filament_diameter: " + config_->option("filament_diameter")->serialize();
		//qDebug() << res.c_str();
	});



	///设置喷头挤出系数的相关控件
	QLabel* extru_multi_label = new QLabel(QString::fromLocal8Bit("喷头挤出系数："));
	QDoubleSpinBox* extru_multi_spinbox = new QDoubleSpinBox();
	extru_multi_spinbox->setRange(0.9, 1.1);
	extru_multi_spinbox->setWrapping(false);
	extru_multi_spinbox->setSingleStep(0.01);
	//extru_multi_spinbox_->setValue(1);	//默认值
	config_->optptr("extrusion_multiplier", true)->set(*(config_->def->get("extrusion_multiplier")->default_value));
	extru_multi_spinbox->setValue(dynamic_cast<ConfigOptionFloats*>(config_->option("extrusion_multiplier"))->values[0]);
	connect(extru_multi_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
		[=](double d) {
		config_->option("extrusion_multiplier")->set(ConfigOptionFloats(std::vector<double>(1,d)));
		//std::string res = "extrusion_multiplier: " + config_->option("extrusion_multiplier")->serialize();
		//qDebug() << res.c_str();
	});


	//初始化网格布局
	QGridLayout* fila_config_layout = new QGridLayout;
	fila_config_layout->setHorizontalSpacing(horizon_spacing_);
	fila_config_layout->setColumnStretch(0, scale0_);
	fila_config_layout->setColumnStretch(1, scale1_);

	//添加控件到网格布局中
	fila_config_layout->addWidget(diameter_label, 0, 0);
	fila_config_layout->addWidget(diameter_spinbox, 0, 1);
	fila_config_layout->addWidget(extru_multi_label, 1, 0);
	fila_config_layout->addWidget(extru_multi_spinbox, 1, 1);
	
	//设置为网格布局
	fila_config_groupbox_->setLayout(fila_config_layout);
}


/*
 *	初始化设置温度相关参数的控件，包括属性及其信号量
 */
void PrintConfigWidget::InitTempSettting() {
	//初始化分组
	temp_config_groupbox_ = new QGroupBox(QString::fromLocal8Bit("温度设置"));

	///设置喷头温度的相关控件
	QLabel* extruder_temp_label = new QLabel(QString::fromLocal8Bit("喷头:"));
	
	//设置打印第一层时喷头温度的相关控件
	QSpinBox* extruder_first_temp_spinbox = new QSpinBox();
	extruder_first_temp_spinbox->setRange(0, 300);
	extruder_first_temp_spinbox->setWrapping(false);
	extruder_first_temp_spinbox->setSingleStep(1);
	extruder_first_temp_spinbox->setPrefix(QString::fromLocal8Bit("第一层："));
	extruder_first_temp_spinbox->setSuffix(QString::fromLocal8Bit("℃"));
	//extruder_first_temp_spinbox_->setValue(205);	//默认值
	config_->optptr("first_layer_temperature", true)->set(*(config_->def->get("first_layer_temperature")->default_value));
	extruder_first_temp_spinbox->setValue(dynamic_cast<ConfigOptionInts*>(config_->option("first_layer_temperature"))->values[0]);
	connect(extruder_first_temp_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("first_layer_temperature")->set(ConfigOptionInts(std::vector<int>(1,i)));
		//std::string res = "first_layer_temperature: " + config_->option("first_layer_temperature")->serialize();
		//qDebug() << res.c_str();
	});

	//设置打印其它层时喷头温度的相关控件
	QSpinBox* extruder_other_temp_spinbox = new QSpinBox();
	extruder_other_temp_spinbox->setRange(0, 300);
	extruder_other_temp_spinbox->setWrapping(false);
	extruder_other_temp_spinbox->setSingleStep(1);
	extruder_other_temp_spinbox->setPrefix(QString::fromLocal8Bit("其他层："));
	extruder_other_temp_spinbox->setSuffix(QString::fromLocal8Bit("℃"));
	//extruder_other_temp_spinbox_->setValue(205);	//默认值
	config_->optptr("temperature", true)->set(*(config_->def->get("temperature")->default_value));
	extruder_other_temp_spinbox->setValue(dynamic_cast<ConfigOptionInts*>(config_->option("temperature"))->get_at(0));
	connect(extruder_other_temp_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("temperature")->set(ConfigOptionInts(std::vector<int>(1,i)));
		//std::string res = "temperature: " + config_->option("temperature")->serialize();
		//qDebug() << res.c_str();
	});

	///设置热床温度的相关控件
	QLabel* bed_temp_label = new QLabel(QString::fromLocal8Bit("热床:"));
	//设置打印第一层时热床温度的相关控件
	QSpinBox* bed_first_temp_spinbox = new QSpinBox();
	bed_first_temp_spinbox->setRange(0, 100);
	bed_first_temp_spinbox->setWrapping(false);
	bed_first_temp_spinbox->setSingleStep(1);
	bed_first_temp_spinbox->setPrefix(QString::fromLocal8Bit("第一层: "));
	bed_first_temp_spinbox->setSuffix(QString::fromLocal8Bit("℃"));
	//bed_first_temp_spinbox_->setValue(55);	//默认值
	config_->optptr("first_layer_bed_temperature", true)->set(*(config_->def->get("first_layer_bed_temperature")->default_value));
	bed_first_temp_spinbox->setValue(config_->option("first_layer_bed_temperature")->getInt());
	connect(bed_first_temp_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("first_layer_bed_temperature")->set(ConfigOptionInt(i));
		//std::string res = "first_layer_bed_temperature: " + config_->option("first_layer_bed_temperature")->serialize();
		//qDebug() << res.c_str();
	});

	//设置打印其它层时热床温度的相关控件
	QSpinBox* bed_other_temp_spinbox = new QSpinBox();
	bed_other_temp_spinbox->setRange(0, 100);
	bed_other_temp_spinbox->setWrapping(false);
	bed_other_temp_spinbox->setSingleStep(1);
	bed_other_temp_spinbox->setPrefix(QString::fromLocal8Bit("其他层: "));
	bed_other_temp_spinbox->setSuffix(QString::fromLocal8Bit("℃"));
	//bed_other_temp_spinbox_->setValue(55);	//默认值
	config_->optptr("bed_temperature", true)->set(*(config_->def->get("bed_temperature")->default_value));
	bed_other_temp_spinbox->setValue(config_->option("bed_temperature")->getInt());
	connect(bed_other_temp_spinbox, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged),
		[=](int i) {
		config_->option("bed_temperature")->set(ConfigOptionInt(i));
		//std::string res = "bed_temperature: " + config_->option("bed_temperature")->serialize();
		//qDebug() << res.c_str();
	});

	//初始化网格布局
	QGridLayout* temp_config_layout = new QGridLayout;
	temp_config_layout->setHorizontalSpacing(horizon_spacing_);
	temp_config_layout->setColumnStretch(0, 1);
	temp_config_layout->setColumnStretch(1, 2);

	//添加控件到网格布局
	temp_config_layout->addWidget(extruder_temp_label, 0, 0);
	temp_config_layout->addWidget(extruder_first_temp_spinbox, 0, 1);
	temp_config_layout->addWidget(extruder_other_temp_spinbox, 1, 1);
	temp_config_layout->addWidget(bed_temp_label, 3, 0);
	temp_config_layout->addWidget(bed_first_temp_spinbox, 3, 1);
	temp_config_layout->addWidget(bed_other_temp_spinbox, 4, 1);
	
	//设置为网格布局
	temp_config_groupbox_->setLayout(temp_config_layout);
}


/*
 *	初始化打印机尺寸相关控件，包括属性及其信号槽
 */
void PrintConfigWidget::InitSizeSetting() {
	//设置热床参数
	size_config_groupbox_ = new QGroupBox(QString::fromLocal8Bit("热床参数:"));

	///设置热床尺寸的相关控件
	QLabel* bed_size_label = new QLabel(QString::fromLocal8Bit("尺寸:"));
	//设置打印机热床长度的相关控件
	size_x_spinbox_ = new QSpinBox();
	size_x_spinbox_->setMinimum(0);
	size_x_spinbox_->setMaximum(1000);
	size_x_spinbox_->setSingleStep(1);
	size_x_spinbox_->setWrapping(false);
	size_x_spinbox_->setPrefix(QString::fromLocal8Bit("x: "));
	size_x_spinbox_->setSuffix(QString::fromLocal8Bit("mm"));
	size_x_spinbox_->setValue(280);		//热床长度
	connect(size_x_spinbox_, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrintConfigWidget::ChangeBedShape);
	
	//设置打印机热床宽度的相关控件
	size_y_spinbox_ = new QSpinBox();
	size_y_spinbox_->setMinimum(0);
	size_y_spinbox_->setSingleStep(1);
	size_y_spinbox_->setMaximum(1000);
	size_y_spinbox_->setWrapping(false);
	size_y_spinbox_->setPrefix(QString::fromLocal8Bit("y: "));
	size_y_spinbox_->setSuffix(QString::fromLocal8Bit("mm"));
	size_y_spinbox_->setValue(180);		//热床宽度
	connect(size_y_spinbox_, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrintConfigWidget::ChangeBedShape);

	///设置热床原点位置的相关控件
	QLabel* origin_label = new QLabel(QString::fromLocal8Bit("原点："));
	//设置打印机原点的相关控件
	origin_x_spinbox_ = new QSpinBox();
	origin_x_spinbox_->setWrapping(false);
	origin_x_spinbox_->setPrefix(QString::fromLocal8Bit("x: "));
	origin_x_spinbox_->setSingleStep(1);
	origin_x_spinbox_->setValue(0);
	connect(origin_x_spinbox_, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrintConfigWidget::ChangeBedShape);

	origin_y_spinbox_ = new QSpinBox();
	origin_y_spinbox_->setWrapping(false);
	origin_y_spinbox_->setPrefix(QString::fromLocal8Bit("y: "));
	origin_y_spinbox_->setSingleStep(1);
	origin_y_spinbox_->setValue(0);
	connect(origin_y_spinbox_, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &PrintConfigWidget::ChangeBedShape);

	config_->optptr("bed_shape", true);
	ChangeBedShape();


	///打印机高度
	QLabel* printer_height_label_ = new QLabel(QString::fromLocal8Bit("打印机高度:"));
	QSpinBox* printer_height_spinbox = new QSpinBox();
	printer_height_spinbox->setMinimum(0);
	printer_height_spinbox->setSingleStep(1);
	printer_height_spinbox->setWrapping(false);
	printer_height_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	//printer_height_spinbox_->setValue(180);		//打印机高度


	///设置竖直偏移量的相关控件
	QLabel* z_offset_label_ = new QLabel(QString::fromLocal8Bit("竖直偏移量:"));
	QDoubleSpinBox* z_offset_spinbox = new QDoubleSpinBox();
	z_offset_spinbox->setWrapping(false);
	z_offset_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	z_offset_spinbox->setSingleStep(0.01);
	//z_offset_spinbox_->setValue(0);
	config_->optptr("z_offset", true)->set(*(config_->def->get("z_offset")->default_value));
	z_offset_spinbox->setValue(config_->option("z_offset")->getFloat());
	connect(z_offset_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), 
		[=](double d) {
		config_->option("z_offset")->set(ConfigOptionFloat(d));
		//qDebug() << "z_offset: " << config_->option("z_offset")->getFloat();
	});

	//初始化网格布局
	QGridLayout* bed_size_config_layout = new QGridLayout();
	bed_size_config_layout->setHorizontalSpacing(horizon_spacing_);
	bed_size_config_layout->setColumnStretch(0, scale0_);
	bed_size_config_layout->setColumnStretch(1, scale1_);

	//添加控件到网格布局
	bed_size_config_layout->addWidget(bed_size_label, 0, 0);
	bed_size_config_layout->addWidget(size_x_spinbox_, 0, 1);
	bed_size_config_layout->addWidget(size_y_spinbox_, 1, 1);
	bed_size_config_layout->addWidget(origin_label, 2, 0);
	bed_size_config_layout->addWidget(origin_x_spinbox_, 2, 1);
	bed_size_config_layout->addWidget(origin_y_spinbox_, 3, 1);
	bed_size_config_layout->addWidget(printer_height_label_, 4, 0);
	bed_size_config_layout->addWidget(printer_height_spinbox, 4, 1);
	bed_size_config_layout->addWidget(z_offset_label_, 5, 0);
	bed_size_config_layout->addWidget(z_offset_spinbox, 5, 1);
	
	//设置为布局
	size_config_groupbox_->setLayout(bed_size_config_layout);
}


/*
 *	设置打印机固件的参数的相关控件，包括属性及其信号槽
 */
void PrintConfigWidget::InitFirmwareSetting() {
	//设置固件参数
	firmware_config_groupbox_ = new QGroupBox(QString::fromLocal8Bit("固件参数:"));

	///设置GCode类型的相关控件
	QLabel* gcode_flavor_label = new QLabel(QString::fromLocal8Bit("GCode类型： "));
	QComboBox* gcode_flavor_combobox = new QComboBox();
	gcode_flavor_combobox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
	
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("RepRap (Marlin/Sprinter)"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("Repetier"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("Teacup"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("MakerWare (MakerBot)"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("Sailfish (MakerBot)"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("Mach3/LinuxCNC"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("3D Machinekit"));
	gcode_flavor_combobox->addItem(QString::fromLocal8Bit("No extrusion"));
	gcode_flavor_combobox->setCurrentIndex(0);	//默认值
	config_->optptr("gcode_flavor", true)->set(*(config_->def->get("gcode_flavor")->default_value));
	connect(gcode_flavor_combobox, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), 
		[=](int index) {
		switch (index) {
		case 0:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfRepRap));
			break;
		case 1:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfRepetier));
			break;
		case 2:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfTeacup));
			break;
		case 3:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfMakerWare));
			break;
		case 4:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfSailfish));
			break;
		case 5:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfMach3));
			break;
		case 6:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfMachinekit));
			break;
		default:
			config_->option("gcode_flavor")->set(ConfigOptionEnum<GCodeFlavor>(gcfNoExtrusion));
			break;
		}
		//std::string res = "gcode_flavor: " + config_->option("gcode_flavor")->serialize();
		//qDebug() << res.c_str();
	});

	///设置喷嘴半径的相关控件
	QLabel* nozzle_diameter_label = new QLabel(QString::fromLocal8Bit("喷嘴大小:"));
	QDoubleSpinBox* nozzle_diameter_spinbox = new QDoubleSpinBox();
	nozzle_diameter_spinbox->setWrapping(false);
	nozzle_diameter_spinbox->setSingleStep(0.1);
	nozzle_diameter_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	nozzle_diameter_spinbox->setMinimum(0.0);
	//nozzle_diameter_spinbox_->setValue(0.5);
	config_->optptr("nozzle_diameter", true)->set(*(config_->def->get("nozzle_diameter")->default_value));
	nozzle_diameter_spinbox->setValue(dynamic_cast<ConfigOptionFloats*>(config_->option("nozzle_diameter"))->values[0]);
	connect(nozzle_diameter_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), 
		[=](double d) {
		config_->option("nozzle_diameter")->set(ConfigOptionFloats(std::vector<double>(1,d)));

		//qDebug() << "nozzle_diameter: " << dynamic_cast<ConfigOptionFloats*>(config_->option("nozzle_diameter"))->values[0];
	});



	///设置回抽长度的相关控件
	QLabel* retract_length_label = new QLabel(QString::fromLocal8Bit("回抽长度:"));
	QDoubleSpinBox* retract_length_spinbox = new QDoubleSpinBox();
	retract_length_spinbox->setWrapping(false);
	retract_length_spinbox->setSingleStep(0.1);
	retract_length_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	retract_length_spinbox->setMinimum(0.0);
	//retract_length_spinbox_->setValue(0);
	config_->optptr("retract_length", true)->set(*(config_->def->get("retract_length")->default_value));
	retract_length_spinbox->setValue(dynamic_cast<ConfigOptionFloats*>(config_->option("retract_length"))->values[0]);
	connect(retract_length_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), 
		[=](double d) {
		config_->option("retract_length")->set(ConfigOptionFloats(std::vector<double>(1,d)));

		//qDebug() << "retract_length: " << dynamic_cast<ConfigOptionFloats*>(config_->option("retract_length"))->values[0];
	});


	///设置回抽高度的相关控件
	QLabel* lift_z_label = new QLabel(QString::fromLocal8Bit("回抽高度:"));
	QDoubleSpinBox* lift_z_spinbox = new QDoubleSpinBox();
	lift_z_spinbox->setWrapping(false);
	lift_z_spinbox->setSingleStep(0.01);
	lift_z_spinbox->setSuffix(QString::fromLocal8Bit("mm"));
	//lift_z_spinbox_->setValue(0.0);
	config_->optptr("retract_lift", true)->set(*(config_->def->get("retract_lift")->default_value));
	lift_z_spinbox->setValue(dynamic_cast<ConfigOptionFloats*>(config_->option("retract_lift"))->values[0]);
	connect(lift_z_spinbox, static_cast<void(QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
		[=](double d) {
		config_->option("retract_lift")->set(ConfigOptionFloats(std::vector<double>(1,d)));

		//qDebug() << "retract_lift: " << dynamic_cast<ConfigOptionFloats*>(config_->option("retract_lift"))->values[0];
	});

	////是否开启回抽，即在回抽时移动喷头
	//QLabel* wipe_retracting_label = new QLabel(QString::fromLocal8Bit("开启回抽:"));
	//wipe_retracting_combobox_ = new QComboBox();
	//wipe_retracting_combobox_->addItem(QString::fromLocal8Bit("是"));
	//wipe_retracting_combobox_->addItem(QString::fromLocal8Bit("否"));
	//wipe_retracting_combobox_->setCurrentIndex(0);


	//初始化网格布局
	QGridLayout* firmware_config_layout = new QGridLayout;
	firmware_config_layout->setHorizontalSpacing(horizon_spacing_);
	firmware_config_layout->setColumnStretch(0, scale0_);
	firmware_config_layout->setColumnStretch(1, scale1_);

	//添加控件到网格布局
	firmware_config_layout->addWidget(gcode_flavor_label, 0, 0);
	firmware_config_layout->addWidget(gcode_flavor_combobox, 0, 1);
	firmware_config_layout->addWidget(nozzle_diameter_label, 1, 0);
	firmware_config_layout->addWidget(nozzle_diameter_spinbox, 1, 1);
	firmware_config_layout->addWidget(retract_length_label, 2, 0);
	firmware_config_layout->addWidget(retract_length_spinbox, 2, 1);
	firmware_config_layout->addWidget(lift_z_label, 3, 0);
	firmware_config_layout->addWidget(lift_z_spinbox, 3, 1);
	//firmware_config_layout->addWidget(wipe_retracting_label, 4, 0);
	//firmware_config_layout->addWidget(wipe_retracting_combobox_, 4, 1);
	
	//设置为网格布局
	firmware_config_groupbox_->setLayout(firmware_config_layout);
}


/*
 *	当修改热床尺寸或者热床原点时，修改参数
 */
void PrintConfigWidget::ChangeBedShape() {
	int width = size_x_spinbox_->value();
	int height = size_y_spinbox_->value();
	int origin_x = origin_x_spinbox_->value();
	int origin_y = origin_y_spinbox_->value();

	ConfigOptionPoints size;
	size.values.push_back(Pointf(origin_x, origin_y));
	size.values.push_back(Pointf(width - origin_x, origin_y));
	size.values.push_back(Pointf(width - origin_x, height - origin_y));
	size.values.push_back(Pointf(origin_x, height - origin_y));
	config_->option("bed_shape")->set(size);
}



/*
 *	当不需要生成支撑结构时，将相关参数设定为不可以用的
 */
void PrintConfigWidget::ValidateSupport(int index) {
	bool valid = (index == 0);
	pattern_spacing_spinbox_->setEnabled(valid);
	contact_Zdistance_combobox_->setEnabled(valid);
	support_pattern_combobox_->setEnabled(valid);
	support_bridge_combobox_->setEnabled(valid);
	raft_layers_spinbox_->setEnabled(valid);
}