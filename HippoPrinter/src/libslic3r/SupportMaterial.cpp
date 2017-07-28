#include "SupportMaterial.hpp"

#include <map>
#include <algorithm>

#include <src/libslic3r/Geometry.hpp>
#include <src/libslic3r/ClipperUtils.hpp>

#include "src/libslic3r/Fill/Fill.hpp"
#include <src/libslic3r/libslic3r.h>

namespace Slic3r {
}


/*
 *	构造函数
 */
SupportMaterial::SupportMaterial(PrintConfig* print_config, PrintObjectConfig* object_config,
	Flow* first_layer_flow,Flow* flow,Flow* interface_flow) 
	:print_config_(print_config),object_config_(object_config),
	first_layer_flow_(first_layer_flow),flow_(flow),interface_flow_(interface_flow){
	

}


void SupportMaterial::Generate(PrintObject& object) {
	std::map<double, Polygons> contact_map;		//<print_z,polygons>
	std::map<double, Polygons> overhang_map;	//<print_z, polygons>
	std::map<double, Polygons> top_map;		//<print_z,polygons>
	std::map<int, Polygons> interface_map;	//<layer_id, polygons>
	std::map<int, Polygons> base_map;		//<layer_id, polygons>
	
	std::vector<double> support_z;

	ContactArea(object,contact_map, overhang_map);

	ObjectTop(object, contact_map, top_map);

	std::vector<double> contact_z;
	for (auto& val : contact_map) {
		contact_z.push_back(val.first);
	}

	std::vector<double> top_z;
	for (auto& val : top_map) {
		top_z.push_back(val.first);
	}

	double max_layer_height = 0;
	for (auto& layer : object.layers) {
		max_layer_height = max_layer_height > layer->height ? max_layer_height : layer->height;
	}

	SupportLayersZ(object,contact_z,top_z,max_layer_height,support_z);

	std::map<int, Polygons> pillars_shape;
	if (object_config_->support_material_pattern == smpPillars) {
		GeneratePillarsShape(contact_map, support_z, pillars_shape);
	}
	GenerateInterfaceLayers(support_z, contact_map, top_map, interface_map);
	ClipWithObject(object, base_map, support_z);
	ClipWithShape(interface_map, pillars_shape);


	GenerateBaseLayers(support_z, contact_map, top_map, interface_map, base_map);
	ClipWithObject(object, base_map, support_z);
	ClipWithShape(base_map, pillars_shape);

	GenerateBottomInterfacesLayers(support_z, base_map, top_map, interface_map);

	for (int i = 0; i < support_z.size(); i++) {
		object.add_support_layer(i,		//id
			(i == 0) ? support_z[i] : (support_z[i] - support_z[i - 1]),	//height
			support_z[i]);		//print_z

		if (i >= 1) {
			object.support_layers[i-1]->upper_layer = object.support_layers[i];
			object.support_layers[i]->lower_layer = object.support_layers[i-1];
		}
	}
	
	GenerateToolPaths(object, overhang_map, contact_map, interface_map, base_map);
}


/*
 *	判断支撑结构的top surfaces: contact = overhangs - clearance + margin
 *	计算的结果是support material用来支撑object的接触面（contact surfaces）,
 *  而不用关心该contact surfaces下方的结构是如何构建（build）的
 */
void SupportMaterial::ContactArea(PrintObject& object, std::map<double, Polygons>& contact_map,
	std::map<double, Polygons>& overhang_map) {

	//如果用户设置的是角度值，则将其转换为弧度
	// +1可以保证转换后的值包含原值对应的弧度值
	double threshold_rad = Geometry::deg2rad(object_config_->support_material_threshold + 1);

	//判断是否只在build plate上构建supprt material, 如果这样的话，则将top surfaces保存到buildplate_only_top_surfaces中，然后再
	//从contact surfaces中减去buildplate_only_top_surfaces，这样就不存在被top_surfaces支撑的contact surface
	//即不在物体上生成支撑结构
	bool buildplate_only = (object_config_->support_material_buildplate_only||object_config_->support_material_enforce_layers)
		&& object_config_->support_material_buildplate_only;
	
	Polygons buildplate_only_top_surfaces;

	
	//计算contact areas 和 overhang areas
	for (int layer_id = 0; layer_id < object.layer_count(); layer_id++) {
	//for(int layer_id = object.layer_count()-1; layer_id >= 0; layer_id--){

		//注意：当raft_layers大于0时，layer_id 不一定等于layer->id
		//此处layer_id = 0表示这是first object layer
		//而layer->id == 0表示first print layer(包括raft layers)
		
		//如果没有raft,而且正处理layer 0,则跳到layer 1
		if (object_config_->raft_layers == 0 && layer_id == 0) {
			continue;
		}

		//如果不生成support material或者当前layer已经超过了必须生成的support layers数量，
		//则退出循环
		if (layer_id > 0 &&
			!object_config_->support_material && 
			(layer_id >= object_config_->support_material_enforce_layers)) {
			break;
		}
		
		Layer* layer = object.get_layer(layer_id);

		//不能在model上添加support material
		if (buildplate_only) {
			//将该layer上的所有的top surfaces合并到一起
			Polygons projection_new;
			for (LayerRegion* layer_region : layer->regions) {
				for (Surface& surface : layer_region->slices.surfaces) {
					if (surface.surface_type == stTop) {
						projection_new.push_back(surface.expolygon.contour);
						projection_new.insert(projection_new.end(), 
							surface.expolygon.holes.begin(), surface.expolygon.holes.end());
					}
				}
			}

			//将当前层的top surfaces和之前所有Layers上的top surfaces进行合并，
			//对新添加的top surfaces进行安全偏移，以保证它们与之前所有layers上的top surfaces可以连接在一起
			//但是并不会在合并的过程中进行安全偏移，因为这会导致build_only_top_surfaces越来越大
			if (!projection_new.empty()) {
				projection_new = offset(projection_new, scale_(0.01));

				buildplate_only_top_surfaces.insert(buildplate_only_top_surfaces.end(),
					projection_new.begin(), projection_new.end());

				buildplate_only_top_surfaces = union_(buildplate_only_top_surfaces, 0);
			}
		}


		//检测需要进行支撑的overhangs和contacts
		Polygons contact;
		Polygons overhang;

		
		if (layer_id == 0) {
			//对于first object player，需要计算raft所占用的空间，此处只考虑contour而不考虑holes
			for (ExPolygon& expolygon : layer->slices.expolygons) {
				overhang.push_back(expolygon.contour);
			}
			//Polygons offseted_overhang = offset(overhang, scale_(+SUPPORT_MATERIAL_MARGIN));
			//contact.insert(contact.end(), offseted_overhang.begin(), offseted_overhang.end());
			contact = offset(overhang, scale_(+SUPPORT_MATERIAL_MARGIN));
		}
		else {
			Layer* lower_layer = object.get_layer(layer_id - 1);

			//提前计算好lower layers上的polygons，以便后面计算需要
			Polygons lower_slice_polygons;
			for (ExPolygon& expolygon : lower_layer->slices.expolygons) {
				lower_slice_polygons.push_back(expolygon.contour);
				lower_slice_polygons.insert(lower_slice_polygons.end(),
					expolygon.holes.begin(), expolygon.holes.end());
			}

			//分别对每一个LayerRegion进行处理
			for (LayerRegion* layer_region : layer->regions) {
				coord_t flow_width = layer_region->flow(frExternalPerimeter).scaled_width();
				
				//diff表示layer的perimeter的centerline与lower layer的boundary之间的diff, 也就是overhang的区域
				Polygons diff_polygons;		
				
				Polygons layer_region_polygons;
				for (Surface& surface : layer_region->slices.surfaces) {
					layer_region_polygons.push_back(surface.expolygon.contour);
					layer_region_polygons.insert(layer_region_polygons.end(),
						surface.expolygon.holes.begin(), surface.expolygon.holes.end());
				}


				//如果指定了threshold angle,则需要使用不同的逻辑来检测overhangs
				if ((object_config_->support_material && threshold_rad != 0) ||
					(layer_id <= object_config_->support_material_enforce_layers) ||
					(object_config_->raft_layers > 0 && layer_id == 0)) {
					
					double layer_threshold_rad = threshold_rad;
					coord_t d = 0;
					if (layer_id <= object_config_->support_material_enforce_layers) {
						layer_threshold_rad = Geometry::deg2rad(89);
					}
					if (layer_threshold_rad != 0) {
						d = scale_(lower_layer->height * (cos(layer_threshold_rad) / sin(layer_threshold_rad)));
					}

					//layer_region上的slices做offset后，和lower_layer上的slices做diff
					Polygons region_slice_polygons;
					for (Surface& surface : layer_region->slices.surfaces) {
						ExPolygon& expolygon = surface.expolygon;
						region_slice_polygons.push_back(expolygon.contour);
						region_slice_polygons.insert(region_slice_polygons.end(),
							expolygon.holes.begin(), expolygon.holes.end());
					}

					Polygons lower_polygons;
					for (ExPolygon& expolygon : lower_layer->slices.expolygons) {
						lower_polygons.push_back(expolygon.contour);
						lower_polygons.insert(lower_polygons.end(),
							expolygon.holes.begin(), expolygon.holes.end());
					}

					diff_polygons = diff(offset(region_slice_polygons, -d), lower_polygons);

					if (d > (flow_width / 2)) {
						diff_polygons = diff(offset(diff_polygons, (d - flow_width / 2)), lower_polygons);
					}
				}
				else{
					double offset_distance = object_config_->get_abs_value("support_material_threshold", flow_width);

					diff_polygons = diff(layer_region_polygons, offset(lower_slice_polygons, offset_distance));

					//删除那些特别小的区域					
					diff_polygons = offset2(diff_polygons, -flow_width / 10, +flow_width / 10);
					
					//diff_polygons包含lower_slices的边界和这一层的overhanging regions的perimeter的centerline组成的ring或stripe
					//diff为空则表示该层不存在perimeter的centerline在lower_slice boundary之外的情况，也就没有overhang区域
				}

				//不需要支撑Bridge结构
				if (object_config_->dont_support_bridges == true) {
					//计算bridging perimeters的区域
					Polygons bridged_perimeters;
					{
						Flow bridge_flow = layer_region->flow(frPerimeter, 1);

						//获取lower layer的slices，并将其向外grow half the nozzle diameter
						//原因是即使half nozzle diameter的宽度落在lower layer的外面，仍然认为该层是被lower layer支撑的
						double nozzle_diameter = print_config_->nozzle_diameter.get_at(
							layer_region->region()->config.perimeter_extruder - 1);
						Polygons lower_grown_slices = offset(lower_slice_polygons, scale_(+nozzle_diameter / 2));
						
						//获取Polylines形式的perimeters
						ExtrusionEntityCollection* perimeters_collection = new ExtrusionEntityCollection;
						layer_region->perimeters.flatten(perimeters_collection);

						Polylines overhang_perimeters;
						for (ExtrusionEntity* entity : perimeters_collection->entities) {
							overhang_perimeters.push_back(entity->as_polyline());
						}
						for (Polyline& polyline : overhang_perimeters) {
							polyline.points[0].translate(1, 0);
						}

						//只考虑perimeters的overhang区域
						overhang_perimeters = diff_pl(overhang_perimeters, lower_grown_slices);

						//只考虑strtaight overhangs
// 						Polylines temp_overhang_perimeters;
// 						for (Polyline& polyline : overhang_perimeters) {
// 							if (polyline.is_straight()) {
// 								temp_overhang_perimeters.push_back(polyline);
// 							}
// 						}
// 						std::swap(temp_overhang_perimeters, overhang_perimeters);
						overhang_perimeters.erase(std::remove_if(overhang_perimeters.begin(),overhang_perimeters.end(),
							[](Polyline& polyline) {return !polyline.is_straight(); }),
							overhang_perimeters.end());

						//只考虑终点在layer slices内的overhangs
						for (Polyline& polyline : overhang_perimeters) {
							polyline.extend_start(flow_width);
							polyline.extend_end(flow_width);
						}
						
// 						temp_overhang_perimeters.clear();
// 						for (Polyline& polyline : overhang_perimeters) {
// 							if (layer->slices.contains(polyline.first_point()) && layer->slices.contains(polyline.last_point())) {
// 								temp_overhang_perimeters.push_back(polyline);
// 							}
// 						}
// 						swap(temp_overhang_perimeters, overhang_perimeters);

						overhang_perimeters.erase(std::remove_if(overhang_perimeters.begin(),overhang_perimeters.end(),
							[&](Polyline& polyline) {
							return !layer->slices.contains(polyline.first_point()) || !layer->slices.contains(polyline.last_point());
							}),
							overhang_perimeters.end());


						//通过扩展polyline的宽度，将bridging polylines转换为polygons
						{
							//对于bridges，不能假设它的宽度大于spacing,原因是它们是根据non-bridging perimeters spacing
							//才确定位置的
							double width = std::max({ bridge_flow.scaled_width(),
								bridge_flow.scaled_spacing(),
								flow_width,
								layer_region->flow(frPerimeter).scaled_width() });

							Polygons grown_perimeters;
							for (Polyline& polyline : overhang_perimeters) {
								Polygons polygons = offset(polyline, (width / 2 + 10));
								grown_perimeters.insert(grown_perimeters.end(),
									polygons.begin(), polygons.end());
							}

							bridged_perimeters = union_(grown_perimeters);
						}

					}	//end for computing bridged_perimeters

					
					//移除整个bridges,只支撑那些unsupport edges
					Polygons bridge_polygons;
					for (Surface& surface : layer_region->fill_surfaces.surfaces) {
						if (surface.surface_type == stBottomBridge &&  surface.bridge_angle != -1) {
							bridge_polygons.push_back(surface.expolygon.contour);
							bridge_polygons.insert(bridge_polygons.end(),
								surface.expolygon.holes.begin(), surface.expolygon.holes.end());
							//bridged_perimeters.push_back(surface.expolygon.contour);
							//bridged_perimeters.insert(bridged_perimeters.end(),
							//	bridge_polygons.begin(), bridge_polygons.end());
						}
					}
					
					bridged_perimeters.insert(bridged_perimeters.end(), 
						bridge_polygons.begin(), bridge_polygons.end());
					diff_polygons = diff(diff_polygons, bridged_perimeters, 1);

					Polygons grown_bridged_edges;
					for (Polyline& polyline : layer_region->unsupported_bridge_edges.polylines) {
						Polygons grown = offset(polyline, scale_(SUPPORT_MATERIAL_MARGIN));

						grown_bridged_edges.insert(grown_bridged_edges.end(),
							grown.begin(), grown.end());
					}
						
					Polygons intersection_polygons = intersection(grown_bridged_edges, bridge_polygons);
					diff_polygons.insert(diff_polygons.end(), intersection_polygons.begin(), intersection_polygons.end());
						
				}	//if(dont_support_bridges)


				if (buildplate_only) {
					//不要在top surfaces上support overhangs
					//这个步骤需要在通过grow overhang region计算contact surface之前完成
					diff_polygons = diff(diff_polygons, buildplate_only_top_surfaces);
				}

				if (diff_polygons.empty()) {
					continue;
				}

				overhang.insert(overhang.end(), diff_polygons.begin(), diff_polygons.end());

				//使用gap的最大值（half the upper extrusion width)计算contact area, 并使用配置的参数值对其进行extend
				//在extend contact area时采用的是逐步extend的方式，原因是避免支撑结构在object的另一侧出现overflow的情况
				Polygons lower_polygons;
				for (ExPolygon& expolygon : lower_layer->slices.expolygons) {
					lower_polygons.push_back(expolygon.contour);
					lower_polygons.insert(lower_polygons.end(),
						expolygon.holes.begin(), expolygon.holes.end());
				}
				Polygons slice_margin = offset(lower_polygons, +flow_width / 2);
				if (buildplate_only) {
					//同样使用top surfaces修剪contact surfaces
					slice_margin.insert(slice_margin.end(), 
						buildplate_only_top_surfaces.begin(), buildplate_only_top_surfaces.end());
					slice_margin = union_(slice_margin);
				}

				for (coord_t i = 0; i <= 3; i++) {
					if (i == 0) {
						coord_t temp_offset = flow_width / 2;
						diff_polygons = diff(offset(diff_polygons, +temp_offset), slice_margin);
					}
					else {
						coord_t temp_offset = scale_(SUPPORT_MATERIAL_MARGIN/3);
						diff_polygons = diff(offset(diff_polygons, +temp_offset), slice_margin);
					}
					//diff_polygons = diff(offset(diff_polygons, flow_width / 2), slice_margin);
				}

				contact.insert(contact.end(), diff_polygons.begin(), diff_polygons.end());

			}
		
		}	//end for layer_id != 0

		if(contact.empty())	continue;


		//下面将contact area应用到对应的layer上
		{
			std::vector<double> nozzles;
			for (LayerRegion* layer_region : layer->regions) {
				double peri_nozzle_diameter = print_config_->nozzle_diameter.get_at(layer_region->region()->config.perimeter_extruder - 1);
				double infill_nozzle_diameter = print_config_->nozzle_diameter.get_at(layer_region->region()->config.infill_extruder - 1);
				double solid_nozzle_diameter = print_config_->nozzle_diameter.get_at(layer_region->region()->config.solid_infill_extruder - 1);

				nozzles.push_back(peri_nozzle_diameter);
				nozzles.push_back(infill_nozzle_diameter);
				nozzles.push_back(solid_nozzle_diameter);
			}

			double nozzle_diamter = std::accumulate(nozzles.begin(), nozzles.end(), 0.0) / nozzles.size();

			coordf_t contact_z = layer->print_z - ContactDistance(layer->height, nozzle_diamter);

			//如果该layer is too low,则忽视该layer
			double first_layer_height = object_config_->get_abs_value("first_layer_height") - EPSILON;
			if (contact_z < first_layer_height) {
				continue;
			}

			contact_map[contact_z] = contact;
			overhang_map[contact_z] = overhang;
		}
		
	}

}



/*
 *	计算object的top surfaces,需要用该参数来判断support material的layer_height,
 *  同时可以删除object轮廓下的支撑，并检测它放置的位置
 */
void SupportMaterial::ObjectTop(PrintObject& object, std::map<double,Polygons>& contact_map, 
	std::map<coordf_t,Polygons>& top_map) {

	if (object_config_->support_material_buildplate_only) {
		return;
	}

	Polygons projection;
	for (int layer_id = object.layer_count() - 1; layer_id >= 0; --layer_id) {
	//for(int layer_id = 0; layer_id < object.layer_count();++layer_id){
		Layer* layer = object.get_layer(layer_id);

		for (LayerRegion* layer_region : layer->regions) {
			SurfacesPtr& top_surfaces = layer_region->slices.filter_by_type(stTop);
			if (top_surfaces.empty()) {
				continue;
			}


			//计算top layer上方的contact areas的projection
			//首先要将new contact areas添加到current projection中
			//new的含义：所有低于last top layer的区域
			double min_top;
			if (!top_map.empty()) {
				min_top = top_map.begin()->first;
			}
			else {
				min_top = (--contact_map.end())->first;
			}

			//使用<=是为了避免忽视那些和top layer的Z值相同的contact regions
			for (auto& val : contact_map) {
				if (val.first > layer->print_z && val.first <= min_top) {
					projection.insert(projection.end(), val.second.begin(), val.second.end());
				}
			}

			//计算是否有projection落到当前层的top surface上
			Polygons top_polygons;
			for (Surface* surface : top_surfaces) {
				top_polygons.push_back(surface->expolygon.contour);
				top_polygons.insert(top_polygons.end(), 
					surface->expolygon.holes.begin(), surface->expolygon.holes.end());
			}


			Polygons touching_polygons = intersection(projection, top_polygons);

			if (!touching_polygons.empty()) {
				//grow tio surfaces从而可以保证interface和support可以和object之间有一定的间隙
				top_map[layer->print_z] = offset(touching_polygons, flow_->scaled_width());
			}

			//移除touched 区域，然后继续在下一层 lower top surfaces进行projection
			projection = diff(projection, touching_polygons);
		}
	}
}


/*
 *	已经知道了打印对象的上下边界，生成中间层(intermediate layers)
 */
void SupportMaterial::SupportLayersZ(PrintObject& object, std::vector<double>& contact_z,
	std::vector<double>& top_z,double max_object_layer_height,std::vector<double>& z_vec) {
	//记录给定的Z值是否为为top surface 
	std::map<double, int> top_map;
	for (double val : top_z) {
		top_map[val] = 1;
	}

	//判断所有non-contact layers的layer height,使用max()的目的是避免插入那些过薄的层
	double nozzle_diameter = print_config_->nozzle_diameter.get_at(object_config_->support_material_extruder - 1);
	double support_material_height = std::max(max_object_layer_height, nozzle_diameter*0.75);
	double contact_distance = ContactDistance(support_material_height, nozzle_diameter);

	//初始化已知的固定的support layers
	std::vector<double> temp_top_z = top_z;
	//std::transform(temp_top_z.begin(), temp_top_z.begin(), temp_top_z.end(),std::bind2nd(std::plus<double>(),contact_distance));
	for (auto& val : temp_top_z) {
		val += contact_distance;
	}

	//std::vector<double> z_vec;
	z_vec.insert(z_vec.end(), contact_z.begin(), contact_z.end());
	z_vec.insert(z_vec.end(), top_z.begin(), top_z.end());
	z_vec.insert(z_vec.end(), temp_top_z.begin(), temp_top_z.end());
	std::sort(z_vec.begin(), z_vec.end());
	z_vec.erase(unique(z_vec.begin(), z_vec.end()), z_vec.end());

	//添加first layer height
	double first_layer_height = object_config_->get_abs_value("first_layer_height");
	while (!z_vec.empty() && z_vec[0] <= first_layer_height) {
		z_vec.erase(z_vec.begin());
	}
	z_vec.insert(z_vec.begin(), first_layer_height);


	//通过将first layer和first contact layer之间的距离均匀分段的方法添加raft layers
	if (object_config_->raft_layers > 1 && z_vec.size() >= 2) {

		//z[1]是最后的raft layer，也是first layer object的contact layer
		double height = (z_vec[1] - z_vec[0]) / (object_config_->raft_layers - 1);


		//既然已经有了两层raft layers，下面就再插入raft_layers-2层rafts
		std::vector<double> raft_layer_heights;
		for (int i = 1; i <= object_config_->raft_layers - 2;i++) {
			raft_layer_heights.push_back(z_vec[0] + height * i);
		}
		auto iter = z_vec.begin();
		//z_vec.insert(z_vec.begin() + 2, raft_layer_heights.begin(), raft_layer_heights.end());
		z_vec.insert(z_vec.begin(), raft_layer_heights.begin(), raft_layer_heights.end());
	}
	

	//创建其他layer,因为已经完成了raft layers,所以跳过它们
	for (int i = z_vec.size()-1; i >= object_config_->raft_layers; i--) {
		double target_height = support_material_height;
		if (i > 0 && top_map.find(z_vec[i-1]) != top_map.end()) {
			target_height = nozzle_diameter;
		}

		if ((i == 0 && z_vec[i] > target_height + first_layer_height) ||
			(i > 0)&&(z_vec[i] - z_vec[i - 1] > target_height + EPSILON)) {
			z_vec.insert(z_vec.begin() + i, z_vec[i] - target_height);
			i++;
		}
	}

	std::sort(z_vec.begin(), z_vec.end());
	z_vec.erase(unique(z_vec.begin(), z_vec.end()), z_vec.end());
}


/*
 *	将contact layers向下传递生成interface layers
 */
void SupportMaterial::GenerateInterfaceLayers(std::vector<double>& support_z,std::map<double, Polygons>& contact_map,
	std::map<double,Polygons>& top_map, std::map<int, Polygons>& interface_map) {
	int interface_layers_num = object_config_->support_material_interface_layers;

	//在contact areas下方生成interface layers
	for (int layer_id = 0; layer_id < support_z.size(); layer_id++) {
		double z_val = support_z[layer_id];

		Polygons this_polygons;
		if (contact_map.find(z_val) == contact_map.end()) {
			continue;
		}
		else {
			this_polygons = contact_map[z_val];
		}

		//将contact layer当做interface layer
		for (int i = layer_id - 1; i >= 0 && i > layer_id - interface_layers_num; i--) {
			double z_val = support_z[i];
			std::vector<int> overlapping_layers = OverlappingLayers(i, support_z);
			std::vector<double> overlapping_z;
			for (auto& val : overlapping_layers) {
				overlapping_z.push_back(support_z[val]);
			}

			//当前层的interface area为upper contact area（或upper interface area）和layer slices之间的diff运算
			//diff结果的作用是连接support material和top surfaces
			Polygons diff_polygons1;
			//当前contact region中被删除的映射(clipped projection)
			diff_polygons1.insert(diff_polygons1.end(), this_polygons.begin(), this_polygons.end());
			if (interface_map.find(i) != interface_map.end()) {
				//该层已经有了interface region
				diff_polygons1.insert(diff_polygons1.end(), interface_map[i].begin(), interface_map[i].end());
			}

			Polygons diff_polygons2;
			for (double d : overlapping_z) {
				if (top_map.find(d) != top_map.end()) {
					//该层的top slices
					diff_polygons2.insert(diff_polygons2.end(), top_map[d].begin(), top_map[d].end());
				}
			}

			for (double d : overlapping_z) {
				//该层的contact regions
				if (contact_map.find(d) != contact_map.end()) {
					diff_polygons2.insert(diff_polygons2.end(), contact_map[d].begin(), contact_map[d].end());
				}
			}

			Polygons diff_polygons = diff(diff_polygons1, diff_polygons2, 1);
			this_polygons = diff_polygons;
			interface_map[i] = diff_polygons;
		}
	}
}


/*
 *	检测base support layer的那一部分是reverse interfaces, 原因是它们落在object top surfaces上
 */
void SupportMaterial::GenerateBottomInterfacesLayers(std::vector<double>& support_z, std::map<int, Polygons>& base_map,
	std::map<double, Polygons>& top_map, std::map<int, Polygons>& interface_map) {
	
	//如果不允许生成interface layers，则生成任何bottom interface layers
	if (object_config_->support_material_interface_layers == 0) {
		return;
	}


	coord_t area_threshold = interface_flow_->scaled_spacing() * interface_flow_->scaled_spacing();

	//遍历object的top surfaces
	for (auto& val : top_map) {
		Polygons& this_polygons = val.second;

		//记录为该top surface生成的interface layers数量
		int interface_layers = 0;
		
		//遍历所有的support layers，直到找到一个正好在top surface上方的support layer
		for (int layer_id = 0; layer_id < support_z.size(); layer_id++) {
			double z = support_z[layer_id];
			if (z <= val.first) {
				continue;
			}

			if (base_map.find(layer_id) != base_map.end()) {
				//计算应该为interface area的support material area
				Polygons interface_area_polygons = intersection(base_map[layer_id], this_polygons);

				//删除面积过小的区域
// 				Polygons temp_interface_area;
// 				for (Polygon& polygon : interface_area_polygons) {
// 					if (std::abs(polygon.area()) >= area_threshold) {
// 						temp_interface_area.push_back(polygon);
// 					}
// 				}
// 				std::swap(temp_interface_area, interface_area_polygons);
				interface_area_polygons.erase(std::remove_if(interface_area_polygons.begin(),
					interface_area_polygons.end(),
					[&](Polygon& polygon) {return std::abs(polygon.area()) < area_threshold; }),
					interface_area_polygons.end());

				base_map[layer_id] = diff(base_map[layer_id], interface_area_polygons);

				//将新的interface area保存到interface中
				interface_map[layer_id].insert(interface_map[layer_id].end(),
					interface_area_polygons.begin(), interface_area_polygons.end());
			}
			interface_layers++;

			if (interface_layers == object_config_->support_material_interface_layers) {
				break;
			}
		}
	}
}

/*
 *	将contact layers和interface layers向下传递，生成main support layers
 */
void SupportMaterial::GenerateBaseLayers(std::vector<double>& support_z, std::map<double,Polygons>& contact_map,
	std::map<double,Polygons>& top_map,std::map<int,Polygons>& interface_map, std::map<int,Polygons>& base_map) {

	//在interface下方生成support layers
	for (int i = support_z.size() - 1; i >= 0; i--) {
		double z_val = support_z[i];
		std::vector<int> overlapping_layers = OverlappingLayers(i, support_z);
		std::vector<double> overlapping_z;
		for (int val : overlapping_layers) {
			overlapping_z.push_back(support_z[val]);
		}

		//为了避免没有interface layer的情况，需要观察其upper layer
		//一个interace layer表示只有一个contact layer,所以interace[i+1]为空
		Polygons upper_contact;
		if (object_config_->support_material_interface_layers <= 1) {
			double temp_z_val = support_z[i + 1];
			if (contact_map.find(temp_z_val) != contact_map.end()) {
				upper_contact = contact_map[temp_z_val];
			}
		}

		Polygons diff_polygons1;
		//upper layer上的support regions
		if (base_map.find(i + 1) != base_map.end()) {
			diff_polygons1.insert(diff_polygons1.end(), base_map[i + 1].begin(),base_map[i+1].end());
		}
		//upper layer上的interface regions
		if (interface_map.find(i + 1) != interface_map.end()) {
			diff_polygons1.insert(diff_polygons1.end(), interface_map[i + 1].begin(), interface_map[i + 1].end());
		}
		//upper layer上的contact regions
		diff_polygons1.insert(diff_polygons1.end(), upper_contact.begin(), upper_contact.end());


		Polygons diff_polygons2;
		//top slices和contact regions on this layer
		for (double z : overlapping_z) {
			if (top_map.find(z) != top_map.end()) {
				diff_polygons2.insert(diff_polygons2.end(), top_map[z].begin(), top_map[z].end());
			}

			if (contact_map.find(z) != contact_map.end()) {
				diff_polygons2.insert(diff_polygons2.end(), contact_map[z].begin(), contact_map[z].end());
			}
		}

		//interface layers on this layer
		for (int i : overlapping_layers) {
			if (interface_map.find(i) != interface_map.end()) {
				diff_polygons2.insert(diff_polygons2.end(), interface_map[i].begin(), interface_map[i].end());
			}
		}

		base_map[i] = diff(diff_polygons1, diff_polygons2, 1);
	}
}

void SupportMaterial::ClipWithObject(PrintObject& object, std::map<int,Polygons>& support_map,
		std::vector<double>& support_z) {

	for (auto& val : support_map) {
		if (val.second.empty()) {
			continue;
		}

		double z_max = support_z[val.first];
		double z_min = (val.first == 0) ? 0 : support_z[val.first - 1];

		//layer->slices包含current layer上的full shape, 因此也包含perimeter's width
		//support 包含了support material的full shape, 因此也包含主要的extrusion
		//计算过程中保留了full extrusion width
		Polygons layer_slice_polygons;
		for (Layer* layer : object.layers) {
			if (layer->print_z > z_min && (layer->print_z - layer->height) < z_max) {
				for (ExPolygon& expolygon : layer->slices.expolygons) {
					layer_slice_polygons.push_back(expolygon.contour);
					layer_slice_polygons.insert(layer_slice_polygons.end(), 
						expolygon.holes.begin(), expolygon.holes.end());
				}
			}
		}

		support_map[val.first] = diff(val.second, offset(layer_slice_polygons, +flow_->scaled_width()));
	}
}

void SupportMaterial::GenerateToolPaths(PrintObject& object,std::map<double,Polygons>& overhang_map,
	std::map<double,Polygons>& contact_map,std::map<int,Polygons>& interface_map,std::map<int,Polygons>& base_map) {
	parallelize<int>(
		0,
		object.support_layers.size()-1,
		boost::bind(&SupportMaterial::ProcessLayer,this,std::ref(object),_1,overhang_map,contact_map,interface_map,base_map),
		print_config_->threads.value
	);

}


/*
 *	分别对每一层支撑结构进行处理，最后再组合在一起
 */
void SupportMaterial::ProcessLayer(PrintObject& object,int layer_id, std::map<double, Polygons>& overhang_map,
	std::map<double, Polygons>& contact_map, std::map<int, Polygons>& interface_map, std::map<int, Polygons>& base_map) {
	int contact_loops = 1;
	
	//circle: contact area的形状	
	coord_t circle_radius = 1.5 * interface_flow_->scaled_width();
	coord_t circle_distance = 3 * circle_radius;
	Points circle_points = { Point(circle_radius* cos(5 * PI / 3),circle_radius*sin(5 * PI / 3)),
		Point(circle_radius* cos(4 * PI / 3),circle_radius*sin(4 * PI / 3)),
		Point(circle_radius* cos(PI),circle_radius*sin(PI)),
		Point(circle_radius* cos(2 * PI / 3),circle_radius*sin(2 * PI / 3)),
		Point(circle_radius* cos(PI / 3),circle_radius*sin(PI / 3)),
		Point(circle_radius* cos(0),circle_radius*sin(0)) };
	Polygon circle(circle_points);

	//支撑结构的填充模式
	SupportMaterialPattern support_pattern = object_config_->support_material_pattern;
	InfillPattern support_infill_pattern;
	int angle = object_config_->support_material_angle;
	std::vector<double> angles(1, angle);

	if (support_pattern == smpRectilinearGrid) {
		support_infill_pattern = ipRectilinear;
		angles.push_back(angles[0] + 90);
	}
	else if (support_pattern == smpPillars) {
		support_infill_pattern = ipHoneycomb;
	}

	//支撑结构的参数
	int interface_angle = object_config_->support_material_angle + 90;
	float interface_spacing = object_config_->support_material_interface_spacing + interface_flow_->spacing();
	double interface_density = interface_spacing == 0 ? 1 : interface_flow_->spacing() / interface_spacing;
	float support_spacing = object_config_->support_material_spacing + flow_->spacing();
	float support_density = support_spacing == 0 ? 1 : flow_->spacing() / support_spacing;

	//support layer
	SupportLayer* support_layer = object.support_layers[layer_id];
	double z = support_layer->print_z;

	//flow的参数
	Flow flow = *flow_;
	flow.height = support_layer->height;
	Flow interface_flow = *interface_flow_;
	interface_flow.height = support_layer->height;

	Polygons overhang_polygons;
	Polygons contact_polygons;
	Polygons interface_polygons;
	Polygons base_polygons;
	
	//获取打印高度z对应的overhang area, contact area, interface area, base area
	if (overhang_map.find(z) != overhang_map.end()) {
		overhang_polygons = overhang_map[z];
	}
	if (contact_map.find(z) != contact_map.end()) {
		contact_polygons = contact_map[z];
	}
	if (interface_map.find(layer_id) != interface_map.end()) {
		interface_polygons = interface_map[layer_id];
	}
	if (base_map.find(layer_id) != base_map.end()) {
		base_polygons = base_map[layer_id];
	}

	//islands,即可以进行打印的区域,对contact area, interface area, base area进行union获得
	Polygons islands_polygons = contact_polygons;
	islands_polygons.insert(islands_polygons.end(), interface_polygons.begin(), interface_polygons.end());
	islands_polygons.insert(islands_polygons.end(), base_polygons.begin(), base_polygons.end());
	support_layer->support_islands.append(union_ex(islands_polygons));

	//contact结构
	Polygons contact_infill_polygons;
	if (object_config_->support_material_interface_layers == 0) {
		//如果不需要interface area,则将contact area当做一般的base area进行处理
		base_polygons.insert(base_polygons.end(), contact_polygons.begin(), contact_polygons.end());
	}
	else if (!contact_polygons.empty() && contact_loops > 0) {
		//生成最外圈的循环

		
		//计算最外圈环形结构的中线（centerline）
		contact_polygons = offset(contact_polygons, -interface_flow.scaled_width() / 2);

		//轮廓的外侧环状结构的中心线
		Polygons external_loops_polygons = contact_polygons;

		//只考虑接触overhang的环状结构（loops facing the overhang）
		Polygons overhangs_with_margin = offset(overhang_polygons, +interface_flow.scaled_width() / 2);
		Polylines splitted_overhang_polylines;
		for(Polygon& polygon: external_loops_polygons){
			splitted_overhang_polylines.push_back(polygon.split_at_first_point());
		}
		Polylines external_loops_polylines = intersection_pl(splitted_overhang_polylines, overhangs_with_margin);

		//使用设置的填充模式创建环形结构
		Points positions;
		for (Polyline& polyline : external_loops_polylines) {
			Points spaced_points = Polygon(polyline.points).equally_spaced_points(circle_distance);
			positions.insert(positions.end(), spaced_points.begin(), spaced_points.end());
		}

		
		Polygons translated_circles_polygons;
		for(Point& p:positions){
			Polygon cloned_circle = circle;
			cloned_circle.translate(p);
			translated_circles_polygons.push_back(cloned_circle);
		}

		//external loops - interface polygons(translated cicles)
		Polygons loops0 = diff(external_loops_polygons, translated_circles_polygons);

		//make more loops
		Polygons loops_polygons = loops0;
		for (int i = 2; i < contact_loops; i++) {
			coord_t d = (i - 1)*interface_flow.scaled_spacing();
			loops0 = offset2(loops0, -d - 0.5*interface_flow.scaled_spacing(),
				0.5*interface_flow.scaled_spacing());
			loops_polygons.insert(loops_polygons.end(), loops0.begin(), loops0.end());
		}

		//删除一部分
		Polylines splitted_loops_polylines;
		for (Polygon& loop : loops_polygons) {
			splitted_loops_polylines.push_back(loop.split_at_first_point());
		}

		Polylines intersected_loops_polylines = intersection_pl(splitted_loops_polylines, 
			offset(overhang_polygons, +scale_(SUPPORT_MATERIAL_MARGIN)));
		
		//添加contact infill area到interface area
		//grow loops by circle_radius的目的是确保没有tiny extrusions留在circle外面，
		//但是这样做会导致loops 和contact infill之间出现很大的gap,所以可以找别的方法
		Polygons grown_loops_polygons;
		for (Polyline& polyline : intersected_loops_polylines) {
			Polygons grown_polygons = offset(polyline, circle_radius*1.1);
			grown_loops_polygons.insert(grown_loops_polygons.end(),
				grown_polygons.begin(), grown_polygons.end());

		}
		contact_infill_polygons = diff(contact_polygons, grown_loops_polygons);


		double mm3_per_mm = interface_flow.mm3_per_mm();

		ExtrusionPath loop_path(erSupportMaterialInterface);
		
		loop_path.mm3_per_mm = mm3_per_mm;
		loop_path.width = interface_flow.width;
		loop_path.height = support_layer->height;

		//将loops转换为extrusionpath objects
		ExtrusionPaths loops_paths;
		for (Polyline& polyline : intersected_loops_polylines) {
			loop_path.polyline = polyline;
			loops_paths.push_back(loop_path);
		}
		
		support_layer->support_interface_fills.append(loops_paths);
	}//end for contact_polygons


	Fill* interface_filler = Fill::new_from_type("rectilinear");
	Fill* support_filler = Fill::new_from_type(support_infill_pattern);

	interface_filler->bounding_box = object.bounding_box();
	support_filler->bounding_box = object.bounding_box();

	//interface and contact infill
	if (!interface_polygons.empty() || !contact_infill_polygons.empty()) {
		interface_filler->angle = Geometry::deg2rad(interface_angle);
		interface_filler->min_spacing = interface_flow.spacing();

		//计算interface area外部环状结构的中心线
		interface_polygons = offset2(interface_polygons, +SCALED_EPSILON, -(SCALED_EPSILON + interface_flow.scaled_width() / 2));
		
		//通过offset regions来合并它们，以此来确保它们可以合并在一起
		interface_polygons.insert(interface_polygons.end(), contact_infill_polygons.begin(), contact_infill_polygons.end());
		interface_polygons = offset(interface_polygons, SCALED_EPSILON);

		//当base area结构位于Holes结构部分时，将它们转换为Interface area结构
		Polygons temp_interface_polygons;
		for (Polygon& p : interface_polygons) {
			if (p.is_clockwise()) {
				Polygon p2 = p;
				p2.make_counter_clockwise();
				Polygons diff_polygons = diff(Polygons(p2), base_polygons, 1);
				if (diff_polygons.empty()) {
					continue;
				}
			}
			temp_interface_polygons.push_back(p);
		}
		std::swap(temp_interface_polygons, interface_polygons);

		//从base area中减去interface area
		base_polygons = diff(base_polygons, interface_polygons);
		
		//将Interface path转换为extrusionpath object
		ExtrusionPaths paths;
		ExPolygons unioned_expolygons = union_ex(interface_polygons);
		for (ExPolygon& expolygon : unioned_expolygons) {
			Surface temp_fill_surface(stInternal,expolygon);
			interface_filler->density = interface_density;
			interface_filler->complete = 1;
			Polylines filled_polylines = interface_filler->fill_surface(temp_fill_surface);

			ExtrusionPath support_interface_path = ExtrusionPath(erSupportMaterialInterface, interface_flow.mm3_per_mm(),
				interface_flow.width, interface_flow.height);
			for (Polyline& polyline : filled_polylines) {
				support_interface_path.polyline = polyline;
				paths.push_back(support_interface_path);
			}
		}

		support_layer->support_interface_fills.append(paths);
	}


	//base support和flange(轮缘)
	if (!base_polygons.empty()) {
		support_filler->angle = Geometry::deg2rad(angles[layer_id%angles.size()]);
		//不使用base_flow的原因是需要一个constant spacing value, 从而确保所有的layers可以正确地对齐（aligned）
		support_filler->min_spacing = flow.spacing();

		//找到外部轮廓的中心线
		ExPolygons to_infill_expolygons;
		Polygons to_infill_polygons = offset2(base_polygons, SCALED_EPSILON, -(SCALED_EPSILON + flow.scaled_width()/2));
		ExtrusionPaths paths;

		double density = 0.5;
		Flow& base_flow = flow;

		//base flange
		if (layer_id == 0) {
			interface_filler->angle = Geometry::deg2rad(object_config_->support_material_angle + 90);
			double density = 0.5;
			Flow& base_flow = *first_layer_flow_;
			//对第一层使用正确的间隔（spacing）,原因是不需要它的填充模式与其它层一样
			interface_filler->min_spacing = base_flow.spacing();

			//减去brim结构
			if (print_config_->brim_width > 0) {
				coord_t d = scale_(print_config_->brim_width * 2);
				Polygons layer0_polygons;
				for (ExPolygon& expolygon : object.get_layer(0)->slices.expolygons) {
					layer0_polygons.push_back(expolygon.contour);
					layer0_polygons.insert(layer0_polygons.end(), 
						expolygon.holes.begin(), expolygon.holes.end());
				}
				to_infill_expolygons = diff_ex(to_infill_polygons, 
					offset(layer0_polygons, d));
			}
			else {
				to_infill_expolygons = union_ex(to_infill_polygons);
			}
		}
		else {
			//在支撑结构的填充部分（support infill）外部添加perimeter
			ExtrusionPath path = ExtrusionPath(erSupportMaterial, flow.mm3_per_mm(), flow.width, flow.height);

			for (Polygon& polygon : to_infill_polygons) {
				path.polyline = polygon.split_at_first_point();
				paths.push_back(path);
			}

			to_infill_expolygons = offset_ex(to_infill_polygons, -flow.scaled_spacing());
		}


		for (ExPolygon& expolygon : to_infill_expolygons) {
			support_filler->density = density;
			support_filler->complete = 1;
			Surface temp_fill_surface = Surface(stInternal, expolygon);
			Polylines filler_polylines = support_filler->fill_surface(temp_fill_surface);
			ExtrusionPath path = ExtrusionPath(erSupportMaterial, base_flow.mm3_per_mm(), base_flow.width, base_flow.height);

			for (Polyline& polyline : filler_polylines) {
				path.polyline = polyline;
				paths.push_back(path);
			}
		}
		support_layer->support_fills.append(paths);
	}


}

/*
 * 检测object top surfaces上的first support layer
 */
void SupportMaterial::GeneratePillarsShape(std::map<double,Polygons>& contact_map,std::vector<double> support_z,
	std::map<int,Polygons>& shape_map) {
	
	//避免用空点集构造BoundingBox
	if (contact_map.empty()) {
		return;
	}

	coord_t pillar_size = scale_(2.5);
	coord_t pillar_spacing = scale_(10);

	Points pillar_points = { Point((coord_t)0,(coord_t)0),Point((coord_t)0,pillar_size),
		Point(pillar_size,pillar_size),Point(pillar_size,(coord_t)0) };
	Polygon pillar = Polygon(pillar_points);


	Points temp_bbox_points;
	for (auto& val : contact_map) {
		for (auto& polygon : val.second) {
			temp_bbox_points.insert(temp_bbox_points.end(), polygon.points.begin(), polygon.points.end());
		}
	}
	Polygons pillars;
	BoundingBox bbox(temp_bbox_points);
	for (coord_t x = bbox.min.x; x <= bbox.max.x - pillar_size; x += pillar_spacing) {
		for (coord_t y = bbox.min.y; y <= bbox.max.y - pillar_size; y += pillar_spacing) {
			Polygon p = pillar;
			p.translate(x, y);
			pillars.push_back(p);
		}
	}
	Polygons grid = union_(pillars);
	
	//将pillar添加到每一层上
	for (int i = 0; i < support_z.size(); i++) {
		shape_map[i] = grid;
	}

	//build capitals
	for (int i = 0; i < support_z.size(); i++) {
		double z_val = support_z[i];

		Polygons capitals;
		if (contact_map.find(z_val) != contact_map.end()) {
			capitals = intersection(grid, contact_map[z_val]);
		}
		else {
			capitals = intersection(grid, Polygons());
		}


		//每次处理一个pillar,来避免capitals之间出现合并的情况
		//但是要同时记录被capital支撑的contact area, 原因是需要确保nothing is left
		Polygons contact_supported_by_capitals;
		for (auto& capital : capitals) {
			//扩张capital tops
			Polygons offseted_polygons = offset(Polygons(capital), +(pillar_spacing - pillar_size) / 2);
			capital = offseted_polygons[0];
			contact_supported_by_capitals.push_back(capital);

			for (int j = i - 1; j >= 0; j--) {
				double j_z_val = support_z[j];
				Polygons temp_polygons = offset(Polygons(capital), -interface_flow_->scaled_width() / 2);
				if (temp_polygons.empty()) {
					continue;
				}
				capital = temp_polygons[0];

				if (capitals.empty()) {
					break;
				}
				shape_map[j].push_back(capital);
			}
		}


		//capitals一般情况下不好覆盖整个contact area,因为总会有剩余的没有被覆盖的部分，
		//下面将unsupported areas映射到ground, 就像使用normal support一样
		Polygons contact_not_supported;
		if (contact_map.find(z_val) != contact_map.end()) {
			contact_not_supported = diff(contact_map[z_val], contact_supported_by_capitals);
		}
		else {
			contact_not_supported = diff(Polygons(), contact_supported_by_capitals);
		}
		
		if (!contact_not_supported.empty()) {
			for (int j = i - 1; j >= 0; j--) {
				shape_map[j].insert(shape_map[j].end(), 
					contact_not_supported.begin(), contact_not_supported.end());
			}
		}
	}
}


void SupportMaterial::ClipWithShape(std::map<int,Polygons>& support, std::map<int,Polygons>& shape_map) {
	for (auto& val : support) {
		//不要clip bottom layers，从而可以生成continuous base flange而且也不用clip raft layers
		int layer_id = val.first;
		if (layer_id == 0 || layer_id < object_config_->raft_layers) {
			continue;
		}
		val.second = intersection(val.second, shape_map[layer_id]);
	}

}

/*
 *	返回与给定layer相重合的layer的索引
 */
std::vector<int> SupportMaterial::OverlappingLayers(int index,const std::vector<double>& support_z) {

	double z_max = support_z[index];
	double z_min = (index == 0) ? 0 : support_z[index - 1];

	std::vector<int> res;

	for (int i = 0; i < support_z.size(); i++) {
		double z_max2 = support_z[i];
		double z_min2 = (i == 0) ? 0 : support_z[i - 1];

		if (z_max > z_min2 && z_min < z_max2) {
			res.push_back(i);
		}
	}
	return res;

}


/*
 *	
 */
double SupportMaterial::ContactDistance(double layer_height, double nozzle_diameter) {
	double extra = object_config_->support_material_contact_distance;

	if (extra == 0) {
		return layer_height;
	}
	else {
		return nozzle_diameter + extra;
	}
}

