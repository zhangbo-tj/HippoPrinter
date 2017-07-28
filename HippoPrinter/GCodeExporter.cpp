#include "GCodeExporter.h"
#include <ctime>
#include <algorithm>
#include <memory>

#include <QDebug>

#include <src/libslic3r/Geometry.hpp>


/*
 *	构造函数
 */
GCodeExporter::GCodeExporter(Print* print){
	print_ = print;
	objects_ = print_->objects;
	placeholder_parser_ = print_->placeholder_parser;
	printconfig_ = print_->config;
	brim_done_ = false;
	last_obj_copy_ = Point(0, 0);

	//计算layer的总数
	int layer_count = 0;
	if (printconfig_.complete_objects) {
		for (auto object : objects_) {
			layer_count += static_cast<int>(object->total_layer_count()) * object->copies().size();
		}
	}
	else {
		for (auto object : objects_) {
			layer_count += static_cast<int>(object->total_layer_count());
		}
	}
	
	//gcodegen_ = GCode();
	gcodegen_.placeholder_parser = &placeholder_parser_;
	gcodegen_.layer_count = layer_count;
	gcodegen_.enable_cooling_markers = true;
	gcodegen_.apply_print_config(printconfig_);
	
	std::set<size_t> print_extruders = print_->extruders();
	std::vector<unsigned int> extruders(print_extruders.begin(),print_extruders.end());
	gcodegen_.set_extruders(extruders);


	cooling_buffer_ = new CoolingBuffer(gcodegen_);
	
	//先不考虑spiral_vase, vibration_limit, arc_fitting等参数

}


GCodeExporter::~GCodeExporter(){

}


/*
 *	导出GCode文件
 */
void GCodeExporter::Export(char* file_path) {
	std::ofstream fout(file_path,std::ios::out);
	
	//判断文件是否打开
	if (!fout.is_open()) {
		qDebug() << "cannot open file";
		return;
	}
	
	//写入日期
	std::time_t t = std::time(nullptr);
	char time_str[32];
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&t));
	fout << "; generated on " << time_str<<"\n\n";
	//qDebug() << "; generated on " << time_str;


	//写入GCode注释
	//fout << gcodegen_.notes();

	//写入参数信息
	ExportParameter(fout);
	
	//更新placeholders
	placeholder_parser_.update_timestamp();

	//GCode在每次change_layer时都会自动设置该值
	//但是需要为skirt和brim手动设置
	gcodegen_.first_layer = true;

	//停止风扇
	if (printconfig_.cooling && printconfig_.disable_fan_first_layers) {
		fout << gcodegen_.writer.set_fan(0, 1);
		//qDebug() << gcodegen_.writer.set_fan(0, 1).c_str();
	}
		
	//TODO: 此处考虑start_gcode
	//设置热床温度
	if (printconfig_.has_heatbed && (printconfig_.first_layer_bed_temperature != 0)) {
		fout << gcodegen_.writer.set_bed_temperature(printconfig_.first_layer_bed_temperature, 1) << "\n";
		//qDebug() << gcodegen_.writer.set_bed_temperature(printconfig_.first_layer_bed_temperature, 1).c_str();
	}

	// 在start_gcode的前后分别设置Extruder温度
	PrintFirstLayerTemp(fout, false);
	fout << gcodegen_.placeholder_parser->process(printconfig_.start_gcode) << "\n";
	//qDebug() << gcodegen_.placeholder_parser->process(printconfig_.start_gcode).c_str();
	for (auto start_gcode : printconfig_.start_filament_gcode.values) {
		fout << gcodegen_.placeholder_parser->process(start_gcode) << "\n";
		//qDebug() << gcodegen_.placeholder_parser->process(start_gcode).c_str();
	}
	PrintFirstLayerTemp(fout, true);

	//设置通用参数（打印打印机类型，并travel-to-z）
	fout << gcodegen_.preamble();
	//qDebug() << gcodegen_.preamble().c_str();

	//初始化motion planner
	InitMotionPlanner();

	//初始化gcodegen_.ooze_prevention(计算需要删除的点)
	CalWipingPoints();

	//设置初始extruder
	fout << gcodegen_.set_extruder(*(print_->extruders().begin()));
	//qDebug() << gcodegen_.set_extruder(*(print_->extruders().begin())).c_str();


	//判断是否需要对整个Object进行处理，原程序中可以只打印一部分模型
	if (printconfig_.complete_objects) {

		//对print object按照索引进行排序，避免打印过程中在移动到新的print object时出现碰撞
		std::vector<int> obj_indexes;
		for (int i = 0; i < print_->objects.size(); i++) {
			obj_indexes.push_back(i);
		}
		std::sort(obj_indexes.begin(), obj_indexes.end(), 
			[&](int a, int b) {
			return objects_[a]->size.z < objects_[b]->size.z;
		});

		//已经打印完成的Object数量
		int finished_objects = 0;
		for (auto obj_index : obj_indexes) {
			auto object = objects_[obj_index];

			//对printobject的每一个copy单独进行处理
			for (auto copy : object->_shifted_copies) {
				//当Z再次移动到layer 0时，移动到copy的原点位置
				if (finished_objects > 0) {
					gcodegen_.set_origin(Pointf(unscale(copy.x), unscale(copy.y)));
					gcodegen_.enable_cooling_markers = false;
					gcodegen_.avoid_crossing_perimeters.use_external_mp_once = true;
					fout << gcodegen_.retract();

					fout << gcodegen_.travel_to(Point(0, 0), 
						erNone,
						"move to origin position for next object");

// 					qDebug() << gcodegen_.retract().c_str();
// 
// 					qDebug() << gcodegen_.travel_to(Point(0, 0),
// 						erNone,
// 						"move to origin position for next object").c_str();

					gcodegen_.enable_cooling_markers = true;

					//当移动到第一个object上的点时，disable motion planner
					gcodegen_.avoid_crossing_perimeters.disable_once = true;
				}

				std::vector<Layer*> layers;
				layers.insert(layers.end(), object->layers.begin(), object->layers.end());
				layers.insert(layers.end(), object->support_layers.begin(), object->support_layers.end());

				std::sort(layers.begin(), layers.end(), 
					[](Layer* layer1, Layer* layer2) {
					return layer1->print_z < layer2->print_z;
				});

				//TODO: 正则表达式 190|140
				for (Layer* layer : layers) {
					if (layer->id() == 0 && finished_objects > 0) {
						if (printconfig_.first_layer_bed_temperature != 0 && printconfig_.has_heatbed) {
							fout << gcodegen_.writer.set_bed_temperature(printconfig_.first_layer_bed_temperature);
							//qDebug() << gcodegen_.writer.set_bed_temperature(printconfig_.first_layer_bed_temperature).c_str();
						}
						PrintFirstLayerTemp(fout, false);
						fout << gcodegen_.placeholder_parser->process(printconfig_.between_objects_gcode);
						//qDebug() << gcodegen_.placeholder_parser->process(printconfig_.between_objects_gcode).c_str();
						fout << "\n";
					}
					ProcessLayer(layer, Points({copy}), fout);
				}
				//TODO: flush filters
				finished_objects++;
				second_layer_done_ = false;
			}
		}
	}
	else {
		//使用nearest neighbor search对objects进行排序
		Points chained_points;
		for (auto object : objects_) {
			chained_points.push_back(object->_shifted_copies[0]);
		}

		std::vector<Points::size_type> obj_indexes;
		Geometry::chained_path(chained_points, obj_indexes);

		//对object layers按照print_z进行排序
		//<print_z, <obj_idx, [layer1, layer2, layer3...]> >
		std::map<double, std::map<int, std::vector<Layer*>>> object_layers;

		for (int obj_index = 0; obj_index < print_->objects.size(); ++obj_index) {
			auto object = objects_[obj_index];
			for (auto layer : object->layers) {
				object_layers[layer->print_z][obj_index].push_back(layer);
			}
			for (auto support_layer : object->support_layers) {
				object_layers[support_layer->print_z][obj_index].push_back(support_layer);
			}
		}

		for (auto& obj_layer : object_layers) {
			std::map<int, std::vector<Layer*>>& per_obj_layer = obj_layer.second;
			for (int obj_index : obj_indexes) {
				if (per_obj_layer.find(obj_index) != per_obj_layer.end()) {
					for (auto* layer : per_obj_layer[obj_index]) {
						ProcessLayer(layer, layer->object()->_shifted_copies,fout);
					}
				}
			}
		}
		//TODO: flush_filters
		//FlushFilters();
	}

	//写入end commandss
	ExportEndCommands(fout);
	
	//关闭文件
	fout.close();
}


/*
 *	初始化Motion Planner
 *	（avoid corssing perimeters的external motion planner）
 */
void GCodeExporter::InitMotionPlanner() {
	if (printconfig_.avoid_crossing_perimeters) {
		auto distance_from_objects = scale_(1);

		//为每个object计算convex hull, 并重复到每个copy上
		Polygons island_polygons;
		for (auto object : objects_) {
			Polygons polygons;

			//删除只有thin walls的objects,原因是无法对empty polygon进行偏移
			for (auto layer : object->layers) {
				for (auto expolygon : layer->slices.expolygons) {
					polygons.push_back(expolygon.contour);
				}
			}
			if (polygons.empty())
				continue;

			//对每个convel hull copy of object进行平移，并将其添加到islands上
			for (auto copy : object->_shifted_copies) {
				Polygons copy_islands_polygons = polygons;
				for (auto& p : copy_islands_polygons) {
					p.translate(copy);
				}
				island_polygons.insert(island_polygons.end(),
					copy_islands_polygons.begin(), copy_islands_polygons.end());
			}

		}
		gcodegen_.avoid_crossing_perimeters.init_external_mp(union_ex(island_polygons));
	}
}

/*
 *	计算需要删除的点（如果需要的话）
 */
void GCodeExporter::CalWipingPoints() {
	if (printconfig_.ooze_prevention && print_->extruders().size() > 1) {
		Points skirt_points;
		for (auto entity : print_->skirt.entities) {
			auto loop = dynamic_cast<ExtrusionLoop*>(entity);
			for (auto point : loop->as_polyline().points) {
				skirt_points.push_back(point);
			}
		}

		if (!skirt_points.empty()) {

			//对每个extruder的skirt进行offset, 然后得到其convex hull
			//对该convex hull进行offset之后，距离为10取点
			//将得到的点集作为ooze_prevention的standby_points
			//将在gcodegen->set_extruder()时设置extruder的位置和顺序，防止交叉

			Polygon outer_skirt = Geometry::convex_hull(skirt_points);

			Polygons skirts_polygons;
			std::set<size_t> extruders = print_->extruders();
			for (auto extruder_id : extruders) {
				auto extruder_offset = printconfig_.extruder_offset.get_at(extruder_id);
				skirts_polygons.push_back(outer_skirt);
				skirts_polygons.back().translate(Point(-scale_(extruder_offset.x), -scale_(extruder_offset.y)));
			}
			Polygon convex_hull = Geometry::convex_hull(skirts_polygons);

			gcodegen_.ooze_prevention.enable = true;
			auto offseted_polygons = offset(convex_hull, scale_(3));
			Points temp_points;
			for (auto polygon : offseted_polygons) {
				Points spaced_points = polygon.equally_spaced_points(scale_(10));
				temp_points.insert(temp_points.end(), spaced_points.begin(), spaced_points.end());
			}
			gcodegen_.ooze_prevention.standby_points = temp_points;
		}
	}
}


/*
 *	写入End commands到GCode文件中
 */
void GCodeExporter::ExportEndCommands(std::ofstream& fout) {
	//回抽喷头
	fout << gcodegen_.retract();	
	//qDebug() << gcodegen_.retract().c_str();
	//停止风扇
	fout << gcodegen_.writer.set_fan(0);		//G107
	//qDebug() << gcodegen_.writer.set_fan(0).c_str();
	//添加end_gcode
	for (auto end_gcode : printconfig_.end_filament_gcode.values) {
		fout << gcodegen_.placeholder_parser->process(end_gcode).c_str() << "\n";
		//qDebug() << gcodegen_.placeholder_parser->process(end_gcode).c_str() << endl;
	}
	fout << gcodegen_.placeholder_parser->process(printconfig_.end_gcode) << "\n";
	fout << gcodegen_.writer.update_progress(gcodegen_.layer_count, gcodegen_.layer_count, 1);
	fout << gcodegen_.writer.postamble();

// 	qDebug() << gcodegen_.placeholder_parser->process(printconfig_.end_gcode).c_str() << endl;
// 	qDebug() << gcodegen_.writer.update_progress(gcodegen_.layer_count, gcodegen_.layer_count, 1).c_str();
// 	qDebug() << gcodegen_.writer.postamble().c_str();

	//写入filament status
	print_->ClearFilamentStats();
	print_->total_used_filament = 0;
	print_->total_extruded_volume = 0;
	print_->total_weight = 0;
	print_->total_cost = 0;

	for (auto extruder_map : gcodegen_.writer.extruders) {
		auto& extruder = extruder_map.second;

		double used_filament = extruder.used_filament();
		double extruded_volume = extruder.extruded_volume();
		double filament_weight = extruded_volume * extruder.filament_density() / 1000;
		double filament_cost = filament_weight * (extruder.filament_cost() / 1000);
		print_->SetFilamentStats(extruder_map.first, used_filament);

		fout << "; filament used = " << used_filament << " mm (" << extruded_volume / 1000 << "cm3)" << "\n";
		//qDebug() << "; filament used = " << used_filament << " mm (" << extruded_volume / 1000 << "cm3)" << "\n";
		if (filament_weight > 0) {
			print_->total_weight += filament_weight;
			fout << "; filament used = " << filament_weight << "g" << "\n";
			//qDebug() << "; filament used = " << filament_weight << "g" << "\n";

			if (filament_cost > 0) {
				print_->total_cost += filament_cost;
				fout << "; filament cost = " << filament_cost << "\n";
				//qDebug() << "; filament cost = " << filament_cost << "\n";
			}
		}

		print_->total_used_filament += used_filament;
		print_->total_extruded_volume += extruded_volume;
	}
	fout << "; total filament cost = " << print_->total_cost << "\n";
	//qDebug() << "; total filament cost = " << print_->total_cost << "\n";

	//添加配置信息
	fout << "\n";
	for (auto& opt_key : printconfig_.keys()) {
		if (printconfig_.option(opt_key) != nullptr) {
			fout << "; " << opt_key << " = " << printconfig_.serialize(opt_key) << "\n";
			//qDebug() << "; " << opt_key.c_str() << " = " << printconfig_.serialize(opt_key).c_str();
		}
	}
	for (auto& opt_key : print_->default_object_config.keys()) {
		if (printconfig_.option(opt_key) != nullptr) {
			fout << "; " << opt_key << " = " << printconfig_.serialize(opt_key) << "\n";
			//qDebug() << "; " << opt_key.c_str() << " = " << printconfig_.serialize(opt_key).c_str();
		}
	}
	for (auto& opt_key : print_->default_region_config.keys()) {
		if (printconfig_.option(opt_key) != nullptr) {
			fout << "; " << opt_key << " = " << printconfig_.serialize(opt_key) << "\n";
			//qDebug() << "; " << opt_key.c_str() << " = " << printconfig_.serialize(opt_key).c_str();
		}
	}
}

void GCodeExporter::FlushFilters(std::ofstream& fout) {
	//fout << FilterGCode()
}



/*
 *	写入打印机相关参数到GCode文件中
 */
void GCodeExporter::ExportParameter(std::ofstream& fout) {
	//写入相关设置参数
	PrintObject* first_object = objects_[0];
	float layer_height = first_object->config.layer_height;

	for (int region_id = 0; region_id < print_->regions.size(); region_id++) {
		PrintRegion* region = print_->regions[region_id];

		// width and volume speed of external perimeter extrusion
		{
			Flow experimeter_flow = region->flow(frExternalPerimeter, layer_height, 0, 0, -1, *first_object);
			auto volume_speed = experimeter_flow.mm3_per_mm() * region->config.get_abs_value("external_perimeter_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}

			fout << "; external perimeters extrusion width = "
				<< experimeter_flow.width
				<< " ("
				<< volume_speed
				<< "mm^3)\n";
// 			qDebug() << "; external perimeters extrusion width = "
// 				<< experimeter_flow.width
// 				<< " ("
// 				<< volume_speed
// 				<< "mm^3)" << endl;
		}

		// width and volume speed of perimeter extrusion
		{
			Flow peri_flow = region->flow(frPerimeter, layer_height, 0, 0, -1, *first_object);
			auto volume_speed = peri_flow.mm3_per_mm() * region->config.get_abs_value("perimeter_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; perimeters extrusion width = " 
				<< peri_flow.width 
				<< " (" 
				<< volume_speed
				<< "mm^3)\n";
	/*		qDebug() << "; perimeters extrusion width = "
				<< peri_flow.width
				<< " ("
				<< volume_speed
				<< "mm^3)" << endl;*/
		}

		// width and volume speed of infill extrusion
		{
			Flow infill_flow = region->flow(frInfill, layer_height, 0, 0, -1, *first_object);
			auto volume_speed = infill_flow.mm3_per_mm() * region->config.get_abs_value("infill_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; infill extrusion width = " 
				<< infill_flow.width
				<< " (" 
				<< volume_speed
				<< "mm^3)\n";
			//qDebug() << "; infill extrusion width = "
			//	<< infill_flow.width
			//	<< " ("
			//	<< volume_speed
			//	<< "mm^3)" << endl;
		}

		// width and volume speed of solid infill extrusion
		{
			Flow solid_infill_flow = region->flow(frSolidInfill, layer_height, 0, 0, -1, *first_object);
			auto volume_speed = solid_infill_flow.mm3_per_mm() * region->config.get_abs_value("infill_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; solid infill extrusion width = " 
				<< solid_infill_flow.width
				<< " (" 
				<< volume_speed
				<< "mm^3)\n";
// 			qDebug() << "; solid infill extrusion width = "
// 				 << solid_infill_flow.width
// 				<< " ("
// 				<< volume_speed
// 				<< "mm^3)\n";
		}


		//width and volume speed of top solid infill extrusion
		{
			Flow top_solid_infill_flow = region->flow(frTopSolidInfill, layer_height, 0, 0, -1, *first_object);
			auto volume_speed = top_solid_infill_flow.mm3_per_mm() * region->config.get_abs_value("infill_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; top solid infill extrusion width = " 
				<< top_solid_infill_flow.width
				<< " (" 
				<< volume_speed
				<< "mm^3)\n";
// 			qDebug() << "; top solid infill extrusion width = "
// 				<< top_solid_infill_flow.width
// 				<< " ("
// 				<< volume_speed
// 				<< "mm^3)" << endl;
		}


		//width and volume speed of support material extrusion
		if (print_->has_support_material()) {
			auto first_object = objects_[0];
			float support_flow_width = first_object->config.support_material_extrusion_width
				|| first_object->config.extrusion_width;
			FlowRole support_role = frSupportMaterial;
			int support_extruder = first_object->config.support_material_extruder;
			float support_nozzle_diameter = print_->config.nozzle_diameter.get_at(support_extruder);
			float layer_height = first_object->config.layer_height;
			Flow support_flow = Flow::new_from_config_width(support_role, ConfigOptionFloatOrPercent(support_flow_width, false),
				support_nozzle_diameter, layer_height, 0);

			auto volume_speed = support_flow.mm3_per_mm() * objects_[0]->config.get_abs_value("support_material_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; support material extrusion width = " 
				<< support_flow.width
				<< " (" 
				<< volume_speed
				<< "mm^3)\n";
// 			qDebug() << "; support material extrusion width = "
// 				<< support_flow.width
// 				<< " ("
// 				<< volume_speed
// 				<< "mm^3)" << endl;
		}

		// width and volume speed of first layer extrusion
		if (print_->config.first_layer_extrusion_width) {
			Flow first_layer_flow =  region->flow(frPerimeter, layer_height, 0, 1, -1, *objects_[0]);
			auto volume_speed = first_layer_flow.mm3_per_mm() * region->config.get_abs_value("perimeter_speed");
			if (printconfig_.max_volumetric_speed > 0) {
				volume_speed = std::min(volume_speed, (double)printconfig_.max_volumetric_speed);
			}
			fout << "; first layer extrusion width = "
				<< first_layer_flow.width
				<< " ("
				<< volume_speed
				<< "mm^3)\n";
// 			qDebug() << "; first layer extrusion width = "
// 				<< first_layer_flow.width
// 				<< " ("
// 				<< volume_speed
// 				<< "mm^3)" << endl;
		}
		//fout << endl;
		fout << "\n";
	}
}


/*
 *	打印第一层时喷头温度
 */
void GCodeExporter::PrintFirstLayerTemp(std::ofstream& fout,bool wait) {

	std::set<size_t> extruders = print_->extruders();

	for (auto t : extruders) {
		auto temp = printconfig_.first_layer_temperature.get_at(t);
		if (printconfig_.ooze_prevention) {
			temp += printconfig_.standby_temperature_delta;
		}

		if (temp > 0) {
			fout << gcodegen_.writer.set_temperature(temp, wait, t);
			//qDebug() << gcodegen_.writer.set_temperature(temp, wait, t).c_str();
		}
	}
}


/*
 *	分别对每一层进行单独处理，将其打印路径写入GCode文件中
 */
void GCodeExporter::ProcessLayer(Layer* layer, Points& copies, std::ofstream& fout) {
	std::string gcode = "";

	auto object = layer->object();
	gcodegen_.config.apply(object->config, true);

	//TODO: spiral_vase

	//此处不考虑spiral_vase, 所以直接设置为true
	gcodegen_.enable_loop_clipping = true;

	//设置最合适的volumetric-speed
	InitAutoSpeed(layer);

	//添加layer的相关信息，包括切换温度、before_layer_gcode, per_layer_gcode
	gcode += PreProcessGCode(layer);

	//extrude skirt
	gcode += ExtrudeSkirt(layer);

	//extrude brim
	gcode += ExtrudeBrim(layer);

	for (auto& copy : copies) {
		if (last_obj_copy_.x != copy.x || last_obj_copy_.y != copy.y) {
			gcodegen_.avoid_crossing_perimeters.use_external_mp = true;
		}
		last_obj_copy_ = copy;

		gcodegen_.set_origin(Pointf(unscale(copy.x), unscale(copy.y)));
		
		//extrude support material before other things,原因是其pint_z可能更小一些
		//而且这样做可以避免在打印support material时移动到other things上
		if (typeid(layer) == typeid(SupportLayer)){
			SupportLayer* support_layer = dynamic_cast<SupportLayer*>(layer);
			//extrude support-interface-fills
			gcode += ExtrudeSupportMaterial(support_layer);
		}

		//打印perimeter和infill的策略，
		// 1. 根据extruder对extrusions进行分组，从而可以minimize toolpath
		// 2. 从last used extruder开始
		// 3. 对每一个extruder, 按照island对extrusions进行分组
		// 4. 对每一个island,先extrude perimeters, 除非用户设置先extrude infill
		// 5. 需要追踪每一个regions，

		// <region_id, perimeters>或<region_id, infill>
		typedef std::unordered_map<int, ExtrusionEntityCollection> entities_by_region;
		// <"perimeter", perimeters>或<"infill",infills>：每一个Island上的打印区域
		typedef std::unordered_map<std::string, entities_by_region> island;
		//该层上的所有islands
		typedef std::vector<island> islands;
		//<extruder_id, [island0, island1,island2...]>: 每个extruder对应的islands
		std::unordered_map<int, islands> by_extruder;
		
		//计算layer_slices的bounding-box
		std::vector<BoundingBox> layer_slices_bb;
		for (auto expolygon: layer->slices.expolygons) {
			layer_slices_bb.push_back(expolygon.contour.bounding_box());
		}

		int slices_count = layer->slices.count() - 1;
		for (int region_id = 0; region_id < print_->regions.size(); region_id++) {
			if(region_id >= layer->regions.size())	continue;
			LayerRegion* layer_region = layer->regions[region_id];
			PrintRegion* print_region = print_->get_region(region_id);
			
			//TODO: perimeter和infill的类型？
			//处理Perimeters
			{
				int extruder_id = print_region->config.perimeter_extruder - 1;
				//如果使用该extruder的话，则初始化该extruder对应的数据项
				if (by_extruder[extruder_id].empty()) {
					by_extruder[extruder_id].resize(slices_count + 1);
				}
				for (auto* perimeter_coll : layer_region->perimeters.entities) {
					if (const ExtrusionLoop* loop = dynamic_cast<ExtrusionLoop*>(perimeter_coll)) {
						for (int i = 0; i <= slices_count; i++) {
							if (i == slices_count ||
								(layer_slices_bb[i].contains(loop->first_point())
									&& layer->slices.expolygons[i].contour.contains(loop->first_point()))) {
								by_extruder[extruder_id][i]["perimeter"][region_id].append(*loop);
								break;
							}
						}
					}
					else {
						ExtrusionEntityCollection* perimeter_collection = dynamic_cast<ExtrusionEntityCollection*>(perimeter_coll);
						if (perimeter_collection->empty())
							continue;
						for (int i = 0; i <= slices_count; i++) {
							if (i == slices_count ||
								(layer_slices_bb[i].contains(perimeter_collection->first_point())
									&& layer->slices.expolygons[i].contour.contains(perimeter_collection->first_point()))) {
								by_extruder[extruder_id][i]["perimeter"][region_id].append(*perimeter_collection);
								break;
							}
						}
					}
				}
			}

			//处理infill
			// layer_region->fills是一个ExtrusionPathCollection集合，
			//该集合中的每一项都包含了一组infill surface
			for (ExtrusionEntity* entity : layer_region->fills.entities) {
				bool is_solid_infill = false;
				ExtrusionEntityCollection* entity_collection = dynamic_cast<ExtrusionEntityCollection*>(entity);
				if (const ExtrusionPath* infill_path = dynamic_cast<ExtrusionPath*>(entity_collection->entities[0])) {
					is_solid_infill = infill_path->is_solid_infill();
				}
				else if (const ExtrusionLoop* infill_loop = dynamic_cast<ExtrusionLoop*>(entity_collection->entities[0])) {
					is_solid_infill = infill_loop->is_solid_infill();
				}
				int extruder_id = is_solid_infill ?
					print_region->config.solid_infill_extruder - 1 :
					print_region->config.infill_extruder - 1;
				if (by_extruder[extruder_id].empty()) {
					by_extruder[extruder_id].resize(slices_count + 1);
				}
				for (int i = 0; i <= slices_count; i++) {
					if (i == slices_count ||
						(layer_slices_bb[i].contains(entity_collection->first_point())
							&& layer->slices.expolygons[i].contour.contains(entity_collection->first_point()))) {
						by_extruder[extruder_id][i]["infill"][region_id].append(*entity_collection);
						break;
					}
				}
			}
		}

		std::vector<int> extruder_ids;
		for (auto val : by_extruder) {
			extruder_ids.push_back(val.first);
		}
		std::sort(extruder_ids.begin(), extruder_ids.end());
		extruder_ids.erase(std::unique(extruder_ids.begin(), extruder_ids.end()), extruder_ids.end());

		if (by_extruder.size() > 1) {
			int last_extruder_id = gcodegen_.writer.extruder()->id;
			if (by_extruder.find(last_extruder_id) != by_extruder.end()) {
				extruder_ids.erase(std::remove(extruder_ids.begin(), extruder_ids.end(), last_extruder_id), 
					extruder_ids.end());
				extruder_ids.insert(extruder_ids.begin(), last_extruder_id);
			}
		}

		for (int extruder_id : extruder_ids) {
			gcode += gcodegen_.set_extruder(extruder_id);

			for (auto& print_island : by_extruder[extruder_id]) {
				if (print_island.empty()) {
					continue;
				}
				if (print_->config.infill_first) {
					gcode += ExtrudeInfills(print_island["infill"]);
					gcode += ExtrudePerimeters(print_island["perimeter"]);
				}else {
					gcode += ExtrudePerimeters(print_island["perimeter"]);
					gcode += ExtrudeInfills(print_island["infill"]);
				}
			}
			
		}

	}

// 	//TODO: spiral_vase
	if (cooling_buffer_ != nullptr) {
		int object_id = (intptr_t)layer->object();
		std::string layer_ptr;
		if (typeid(layer) == typeid(SupportLayer)) {
			layer_ptr = "support_layer";
		}
		else {
			layer_ptr = "layer";
		}

		gcode = cooling_buffer_->append(gcode,
			std::to_string(object_id) + layer_ptr,
			layer->id(),
			layer->print_z);
	}
	fout << gcode;
}

/*
 *	初始化auto speed,即设置一个最合适的volumetric-speed
 */
void GCodeExporter::InitAutoSpeed(Layer* layer) {
	std::vector<double> mm3_per_mm;

	//获取该层上的minimum cross-section(横截面)
	for (int region_id = 0; region_id < print_->regions.size(); region_id++) {
		if (layer->regions.empty()) {
			break;
		}
		auto region = print_->get_region(region_id);
		LayerRegion* layer_region = layer->get_region(region_id);

		if (region->config.get_abs_value("perimeter_speed") == 0
			|| region->config.get_abs_value("small_perimeter_speed") == 0
			|| region->config.get_abs_value("external_perimeter_speed") == 0
			|| region->config.get_abs_value("bridge_speed") == 0) {
			mm3_per_mm.push_back(layer_region->perimeters.min_mm3_per_mm());
		}

		if (region->config.get_abs_value("infill_speed") == 0
			|| region->config.get_abs_value("solid_infill_speed") == 0
			|| region->config.get_abs_value("top_solid_infill_speed") == 0
			|| region->config.get_abs_value("bridge_speed") == 0
			|| region->config.get_abs_value("gap_fill_speed") == 0) {
			mm3_per_mm.push_back(layer_region->fills.min_mm3_per_mm());
		}
	}

	PrintObject* object = layer->object();
	if (typeid(layer) == typeid(SupportLayer)) {
		SupportLayer* support_layer = dynamic_cast<SupportLayer*>(layer);
		if (object->config.get_abs_value("support_material_speed") == 0
			|| object->config.get_abs_value("support_material_interface_speed") == 0) {
			mm3_per_mm.push_back(support_layer->support_fills.min_mm3_per_mm());
			mm3_per_mm.push_back(support_layer->support_interface_fills.min_mm3_per_mm());
		}
	}

	//删除掉too thin segments
	mm3_per_mm.erase(std::remove_if(mm3_per_mm.begin(), mm3_per_mm.end(),
		[](double d) {return d <= 0.01; }), 
		mm3_per_mm.end());

	if (!mm3_per_mm.empty()) {
		double min_mm3 = *(std::min_element(mm3_per_mm.begin(), mm3_per_mm.end()));

		//最合适的volumetric-speed是指用最大的速度打印的最小cross-section
		double volumetric_speed = min_mm3 * printconfig_.max_print_speed;

		//volumetric-speed的最大值为配置参数里的max-volumetric-speed
		if (printconfig_.max_volumetric_speed > 0) {
			volumetric_speed = std::min(volumetric_speed, (double)printconfig_.max_volumetric_speed);
		}
		gcodegen_.volumetric_speed = volumetric_speed;
	}
}

std::string GCodeExporter::PreProcessGCode(Layer* layer) {
	std::string gcode;
	//如果当前是第二层，则需要改变喷头温度和热床温度
	if (!second_layer_done_ && layer->id() == 1) {
		for (auto extruder : gcodegen_.writer.extruders) {
			auto temp = printconfig_.temperature.get_at(extruder.first);
			if (temp != 0 && temp != printconfig_.first_layer_temperature.get_at(extruder.first)) {
				gcode += gcodegen_.writer.set_temperature(temp, 0, extruder.first);
			}
		}

		//设置喷头温度和热床温度
		if (printconfig_.has_heatbed && print_->config.first_layer_bed_temperature != 0
			&& printconfig_.bed_temperature != printconfig_.first_layer_bed_temperature) {
			gcode += gcodegen_.writer.set_bed_temperature(print_->config.bed_temperature);
		}
		second_layer_done_ = true;
	}

	//TODO: 修改为shared_pointer
	// 添加before_layer_gcode
	if (!print_->config.before_layer_gcode.value.empty()) {
		std::shared_ptr<PlaceholderParser> pp(new PlaceholderParser(*gcodegen_.placeholder_parser));
		pp->set("layer_num", gcodegen_.layer_index + 1);
		pp->set("layer_z", layer->print_z);
		gcode += pp->process(printconfig_.before_layer_gcode);
		gcode += "\n";
	}
	//设置新的layer
	gcode += gcodegen_.change_layer(*layer);
	//添加per_layer_gcode
	if (!printconfig_.layer_gcode.value.empty()) {
		std::shared_ptr<PlaceholderParser> pp(new PlaceholderParser(*gcodegen_.placeholder_parser));
		pp->set("layer_num", gcodegen_.layer_index);
		pp->set("layer_z", layer->print_z);
		gcode += pp->process(printconfig_.layer_gcode);
		gcode += "\n";
	}
	return gcode;
}

std::string GCodeExporter::ExtrudeBrim(Layer* layer) {
	std::string gcode;
	if (!brim_done_) {
		auto object = layer->object();
		gcode += gcodegen_.set_extruder(print_->brim_extruder() - 1);
		gcodegen_.set_origin(Pointf(0, 0));
		gcodegen_.avoid_crossing_perimeters.use_external_mp = true;
		for (auto brim_entity : print_->brim.entities) {
			ExtrusionLoop* path = dynamic_cast<ExtrusionLoop*>(brim_entity);
			gcode += gcodegen_.extrude(*path, "brim", object->config.support_material_speed);
		}

		brim_done_ = true;
		gcodegen_.avoid_crossing_perimeters.use_external_mp = false;
		//允许直接移动到first object point
		gcodegen_.avoid_crossing_perimeters.disable_once = true;
	}
	return gcode;
}

std::string GCodeExporter::ExtrudeSkirt(Layer* layer) {
	std::string gcode;
	auto object = layer->object();
	if ((skirt_done_.size() < printconfig_.skirt_height || print_->has_infinite_skirt())
		&& (skirt_done_.find(layer->print_z) == skirt_done_.end())
		&& ((typeid(layer) != typeid(SupportLayer)) || (layer->id() < object->config.raft_layers))) {

		gcodegen_.set_origin(Pointf(0, 0));
		gcodegen_.avoid_crossing_perimeters.use_external_mp = true;
		std::vector<unsigned int> extruder_ids;
		for (auto extruder : gcodegen_.writer.extruders) {
			extruder_ids.push_back(extruder.first);
		}
		gcode += gcodegen_.set_extruder(extruder_ids[0]);

		//如果brim足够大的话，则取消skirt
		if (layer->id() < printconfig_.skirt_height || print_->has_infinite_skirt()) {
			Flow skirt_flow = print_->skirt_flow();

			//在所有的extruders外围distribute skirt loops
			std::vector<ExtrusionEntity*> skirt_loops = print_->skirt.entities;
			for (int i = 0; i < skirt_loops.size(); i++) {
				//当printing layers > 0时，忽视min-skirt-length,只使用skirts参数
				if (layer->id() > 0 && i >= print_->config.skirts) {
					break;
				}
				unsigned int extruder_id = extruder_ids[(i / extruder_ids.size()) % extruder_ids.size()];
				if (layer->id() == 0) {
					gcode += gcodegen_.set_extruder(extruder_id);
				}


				ExtrusionEntity* loop = skirt_loops[i]->clone();
				Flow layer_skirt_flow = skirt_flow;
				layer_skirt_flow.height = layer->height;
				double mm3_per_mm = layer_skirt_flow.mm3_per_mm();
				ExtrusionLoop* casted_loop = dynamic_cast<ExtrusionLoop*>(loop);
				for (auto& path : casted_loop->paths) {
					path.height = layer->height;
					path.mm3_per_mm = mm3_per_mm;
				}

				gcode += gcodegen_.extrude(*casted_loop, "skirt", object->config.support_material_speed);
			}
		}

		skirt_done_[layer->print_z] = 1;
		gcodegen_.avoid_crossing_perimeters.use_external_mp = false;

		//如果是first layer，则允许其直接运行到first object point
		if (layer->id() == 0) {
			gcodegen_.avoid_crossing_perimeters.disable_once = true;
		}

	}
	return gcode;
}

std::string GCodeExporter::ExtrudeSupportMaterial(SupportLayer* support_layer) {
	std::string gcode;
	auto object = support_layer->object();
	if (support_layer->support_interface_fills.count() > 0) {
		gcode += gcodegen_.set_extruder(object->config.support_material_interface_extruder - 1);

		//ExtrusionEntityCollection* entity_collection = new ExtrusionEntityCollection();
		std::shared_ptr<ExtrusionEntityCollection> entity_collection(new ExtrusionEntityCollection);
		support_layer->support_interface_fills.chained_path_from(gcodegen_.last_pos(), entity_collection.get(), 0);

		for (auto entity : entity_collection->entities) {
			ExtrusionPath* path = dynamic_cast<ExtrusionPath*>(entity);
			gcode += gcodegen_.extrude_path(*path, "support material interface",
				object->config.get_abs_value("support_material_interface_speed"));
		}
		
	}

	//extrude support-fills
	if (support_layer->support_fills.count() > 0) {
		gcode += gcodegen_.set_extruder(object->config.support_material_extruder - 1);

		//ExtrusionEntityCollection* entity_collection = new ExtrusionEntityCollection();
		std::shared_ptr<ExtrusionEntityCollection> entity_collection(new ExtrusionEntityCollection);
		support_layer->support_fills.chained_path_from(gcodegen_.last_pos(), entity_collection.get(), 0);

		for (auto entity : entity_collection->entities) {
			ExtrusionPath* path = dynamic_cast<ExtrusionPath*>(entity);
			gcode += gcodegen_.extrude_path(*path, "support material",
				object->config.get_abs_value("support_material_speed"));
		}
	}
	return gcode;
}

std::string GCodeExporter::ExtrudePerimeters(std::unordered_map<int, ExtrusionEntityCollection>& peri_by_region) {
	std::string gcode = "";
	std::vector<int> region_ids;
	for (auto& val : peri_by_region) {
		region_ids.push_back(val.first);
	}

	std::sort(region_ids.begin(), region_ids.end());

	for (int region_id : region_ids) {

		gcodegen_.config.apply(print_->get_region(region_id)->config);

		ExtrusionEntityCollection& entity_collection = peri_by_region[region_id];
// 		for (ExtrusionEntity* entity : entity_collection.entities) {
// 			gcode += gcodegen_.extrude(*entity, "perimeter", -1);
// 		}
		for (ExtrusionEntity* entity : entity_collection.entities) {
			if (const ExtrusionPath* path = dynamic_cast<ExtrusionPath*>(entity)) {
				gcode += gcodegen_.extrude(*path, "perimeter", -1);
			}
			else if (const ExtrusionLoop* loop = dynamic_cast<ExtrusionLoop*>(entity)) {
				gcode += gcodegen_.extrude(*loop, "perimeter", -1);
			}
			else {
				ExtrusionEntityCollection* casted_collection = dynamic_cast<ExtrusionEntityCollection*>(entity);
				for (ExtrusionEntity* casted_entity : casted_collection->entities) {
					gcode += gcodegen_.extrude(*casted_entity, "perimeter", -1);
				}
			}
		}
	}
	return gcode;
}


std::string GCodeExporter::ExtrudeInfills(std::unordered_map<int, ExtrusionEntityCollection>& infill_by_region) {
	std::string gcode;
	std::vector<int> region_ids;
	for (auto& val : infill_by_region) {
		region_ids.push_back(val.first);
	}
	std::sort(region_ids.begin(), region_ids.end());

	for (int region_id : region_ids) {
		PrintRegion* print_region = print_->get_region(region_id);
		gcodegen_.config.apply(print_region->config);

		ExtrusionEntityCollection infill_collection = infill_by_region[region_id];
		ExtrusionEntityCollection* chained_collection = new ExtrusionEntityCollection;
		infill_collection.chained_path_from(gcodegen_.last_pos(), chained_collection, 0);
		for (ExtrusionEntity* entity : chained_collection->entities) {
			if (const ExtrusionPath* path = dynamic_cast<ExtrusionPath*>(entity)) {
				gcode += gcodegen_.extrude(*path, "infill", -1);
			}
			else if (const ExtrusionLoop* loop = dynamic_cast<ExtrusionLoop*>(entity)) {
				gcode += gcodegen_.extrude(*loop, "infill", -1);
			}
			else {
				ExtrusionEntityCollection* further_chained_collection = new ExtrusionEntityCollection;
				ExtrusionEntityCollection* entity_collection = dynamic_cast<ExtrusionEntityCollection*>(entity);
				entity_collection->chained_path_from(gcodegen_.last_pos(), further_chained_collection, 0);
				for (ExtrusionEntity* path : further_chained_collection->entities) {
					gcode += gcodegen_.extrude(*path, "infill", -1);
				}
			}
		}
		//delete chained_collection;
	}
	return gcode;
}

