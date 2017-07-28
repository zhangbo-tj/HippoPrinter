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
 *	���캯��
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
 *	�ж�֧�Žṹ��top surfaces: contact = overhangs - clearance + margin
 *	����Ľ����support material����֧��object�ĽӴ��棨contact surfaces��,
 *  �����ù��ĸ�contact surfaces�·��Ľṹ����ι�����build����
 */
void SupportMaterial::ContactArea(PrintObject& object, std::map<double, Polygons>& contact_map,
	std::map<double, Polygons>& overhang_map) {

	//����û����õ��ǽǶ�ֵ������ת��Ϊ����
	// +1���Ա�֤ת�����ֵ����ԭֵ��Ӧ�Ļ���ֵ
	double threshold_rad = Geometry::deg2rad(object_config_->support_material_threshold + 1);

	//�ж��Ƿ�ֻ��build plate�Ϲ���supprt material, ��������Ļ�����top surfaces���浽buildplate_only_top_surfaces�У�Ȼ����
	//��contact surfaces�м�ȥbuildplate_only_top_surfaces�������Ͳ����ڱ�top_surfaces֧�ŵ�contact surface
	//����������������֧�Žṹ
	bool buildplate_only = (object_config_->support_material_buildplate_only||object_config_->support_material_enforce_layers)
		&& object_config_->support_material_buildplate_only;
	
	Polygons buildplate_only_top_surfaces;

	
	//����contact areas �� overhang areas
	for (int layer_id = 0; layer_id < object.layer_count(); layer_id++) {
	//for(int layer_id = object.layer_count()-1; layer_id >= 0; layer_id--){

		//ע�⣺��raft_layers����0ʱ��layer_id ��һ������layer->id
		//�˴�layer_id = 0��ʾ����first object layer
		//��layer->id == 0��ʾfirst print layer(����raft layers)
		
		//���û��raft,����������layer 0,������layer 1
		if (object_config_->raft_layers == 0 && layer_id == 0) {
			continue;
		}

		//���������support material���ߵ�ǰlayer�Ѿ������˱������ɵ�support layers������
		//���˳�ѭ��
		if (layer_id > 0 &&
			!object_config_->support_material && 
			(layer_id >= object_config_->support_material_enforce_layers)) {
			break;
		}
		
		Layer* layer = object.get_layer(layer_id);

		//������model�����support material
		if (buildplate_only) {
			//����layer�ϵ����е�top surfaces�ϲ���һ��
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

			//����ǰ���top surfaces��֮ǰ����Layers�ϵ�top surfaces���кϲ���
			//������ӵ�top surfaces���а�ȫƫ�ƣ��Ա�֤������֮ǰ����layers�ϵ�top surfaces����������һ��
			//���ǲ������ںϲ��Ĺ����н��а�ȫƫ�ƣ���Ϊ��ᵼ��build_only_top_surfacesԽ��Խ��
			if (!projection_new.empty()) {
				projection_new = offset(projection_new, scale_(0.01));

				buildplate_only_top_surfaces.insert(buildplate_only_top_surfaces.end(),
					projection_new.begin(), projection_new.end());

				buildplate_only_top_surfaces = union_(buildplate_only_top_surfaces, 0);
			}
		}


		//�����Ҫ����֧�ŵ�overhangs��contacts
		Polygons contact;
		Polygons overhang;

		
		if (layer_id == 0) {
			//����first object player����Ҫ����raft��ռ�õĿռ䣬�˴�ֻ����contour��������holes
			for (ExPolygon& expolygon : layer->slices.expolygons) {
				overhang.push_back(expolygon.contour);
			}
			//Polygons offseted_overhang = offset(overhang, scale_(+SUPPORT_MATERIAL_MARGIN));
			//contact.insert(contact.end(), offseted_overhang.begin(), offseted_overhang.end());
			contact = offset(overhang, scale_(+SUPPORT_MATERIAL_MARGIN));
		}
		else {
			Layer* lower_layer = object.get_layer(layer_id - 1);

			//��ǰ�����lower layers�ϵ�polygons���Ա���������Ҫ
			Polygons lower_slice_polygons;
			for (ExPolygon& expolygon : lower_layer->slices.expolygons) {
				lower_slice_polygons.push_back(expolygon.contour);
				lower_slice_polygons.insert(lower_slice_polygons.end(),
					expolygon.holes.begin(), expolygon.holes.end());
			}

			//�ֱ��ÿһ��LayerRegion���д���
			for (LayerRegion* layer_region : layer->regions) {
				coord_t flow_width = layer_region->flow(frExternalPerimeter).scaled_width();
				
				//diff��ʾlayer��perimeter��centerline��lower layer��boundary֮���diff, Ҳ����overhang������
				Polygons diff_polygons;		
				
				Polygons layer_region_polygons;
				for (Surface& surface : layer_region->slices.surfaces) {
					layer_region_polygons.push_back(surface.expolygon.contour);
					layer_region_polygons.insert(layer_region_polygons.end(),
						surface.expolygon.holes.begin(), surface.expolygon.holes.end());
				}


				//���ָ����threshold angle,����Ҫʹ�ò�ͬ���߼������overhangs
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

					//layer_region�ϵ�slices��offset�󣬺�lower_layer�ϵ�slices��diff
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

					//ɾ����Щ�ر�С������					
					diff_polygons = offset2(diff_polygons, -flow_width / 10, +flow_width / 10);
					
					//diff_polygons����lower_slices�ı߽����һ���overhanging regions��perimeter��centerline��ɵ�ring��stripe
					//diffΪ�����ʾ�ò㲻����perimeter��centerline��lower_slice boundary֮��������Ҳ��û��overhang����
				}

				//����Ҫ֧��Bridge�ṹ
				if (object_config_->dont_support_bridges == true) {
					//����bridging perimeters������
					Polygons bridged_perimeters;
					{
						Flow bridge_flow = layer_region->flow(frPerimeter, 1);

						//��ȡlower layer��slices������������grow half the nozzle diameter
						//ԭ���Ǽ�ʹhalf nozzle diameter�Ŀ������lower layer�����棬��Ȼ��Ϊ�ò��Ǳ�lower layer֧�ŵ�
						double nozzle_diameter = print_config_->nozzle_diameter.get_at(
							layer_region->region()->config.perimeter_extruder - 1);
						Polygons lower_grown_slices = offset(lower_slice_polygons, scale_(+nozzle_diameter / 2));
						
						//��ȡPolylines��ʽ��perimeters
						ExtrusionEntityCollection* perimeters_collection = new ExtrusionEntityCollection;
						layer_region->perimeters.flatten(perimeters_collection);

						Polylines overhang_perimeters;
						for (ExtrusionEntity* entity : perimeters_collection->entities) {
							overhang_perimeters.push_back(entity->as_polyline());
						}
						for (Polyline& polyline : overhang_perimeters) {
							polyline.points[0].translate(1, 0);
						}

						//ֻ����perimeters��overhang����
						overhang_perimeters = diff_pl(overhang_perimeters, lower_grown_slices);

						//ֻ����strtaight overhangs
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

						//ֻ�����յ���layer slices�ڵ�overhangs
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


						//ͨ����չpolyline�Ŀ�ȣ���bridging polylinesת��Ϊpolygons
						{
							//����bridges�����ܼ������Ŀ�ȴ���spacing,ԭ���������Ǹ���non-bridging perimeters spacing
							//��ȷ��λ�õ�
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

					
					//�Ƴ�����bridges,ֻ֧����Щunsupport edges
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
					//��Ҫ��top surfaces��support overhangs
					//���������Ҫ��ͨ��grow overhang region����contact surface֮ǰ���
					diff_polygons = diff(diff_polygons, buildplate_only_top_surfaces);
				}

				if (diff_polygons.empty()) {
					continue;
				}

				overhang.insert(overhang.end(), diff_polygons.begin(), diff_polygons.end());

				//ʹ��gap�����ֵ��half the upper extrusion width)����contact area, ��ʹ�����õĲ���ֵ�������extend
				//��extend contact areaʱ���õ�����extend�ķ�ʽ��ԭ���Ǳ���֧�Žṹ��object����һ�����overflow�����
				Polygons lower_polygons;
				for (ExPolygon& expolygon : lower_layer->slices.expolygons) {
					lower_polygons.push_back(expolygon.contour);
					lower_polygons.insert(lower_polygons.end(),
						expolygon.holes.begin(), expolygon.holes.end());
				}
				Polygons slice_margin = offset(lower_polygons, +flow_width / 2);
				if (buildplate_only) {
					//ͬ��ʹ��top surfaces�޼�contact surfaces
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


		//���潫contact areaӦ�õ���Ӧ��layer��
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

			//�����layer is too low,����Ӹ�layer
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
 *	����object��top surfaces,��Ҫ�øò������ж�support material��layer_height,
 *  ͬʱ����ɾ��object�����µ�֧�ţ�����������õ�λ��
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


			//����top layer�Ϸ���contact areas��projection
			//����Ҫ��new contact areas��ӵ�current projection��
			//new�ĺ��壺���е���last top layer������
			double min_top;
			if (!top_map.empty()) {
				min_top = top_map.begin()->first;
			}
			else {
				min_top = (--contact_map.end())->first;
			}

			//ʹ��<=��Ϊ�˱��������Щ��top layer��Zֵ��ͬ��contact regions
			for (auto& val : contact_map) {
				if (val.first > layer->print_z && val.first <= min_top) {
					projection.insert(projection.end(), val.second.begin(), val.second.end());
				}
			}

			//�����Ƿ���projection�䵽��ǰ���top surface��
			Polygons top_polygons;
			for (Surface* surface : top_surfaces) {
				top_polygons.push_back(surface->expolygon.contour);
				top_polygons.insert(top_polygons.end(), 
					surface->expolygon.holes.begin(), surface->expolygon.holes.end());
			}


			Polygons touching_polygons = intersection(projection, top_polygons);

			if (!touching_polygons.empty()) {
				//grow tio surfaces�Ӷ����Ա�֤interface��support���Ժ�object֮����һ���ļ�϶
				top_map[layer->print_z] = offset(touching_polygons, flow_->scaled_width());
			}

			//�Ƴ�touched ����Ȼ���������һ�� lower top surfaces����projection
			projection = diff(projection, touching_polygons);
		}
	}
}


/*
 *	�Ѿ�֪���˴�ӡ��������±߽磬�����м��(intermediate layers)
 */
void SupportMaterial::SupportLayersZ(PrintObject& object, std::vector<double>& contact_z,
	std::vector<double>& top_z,double max_object_layer_height,std::vector<double>& z_vec) {
	//��¼������Zֵ�Ƿ�ΪΪtop surface 
	std::map<double, int> top_map;
	for (double val : top_z) {
		top_map[val] = 1;
	}

	//�ж�����non-contact layers��layer height,ʹ��max()��Ŀ���Ǳ��������Щ�����Ĳ�
	double nozzle_diameter = print_config_->nozzle_diameter.get_at(object_config_->support_material_extruder - 1);
	double support_material_height = std::max(max_object_layer_height, nozzle_diameter*0.75);
	double contact_distance = ContactDistance(support_material_height, nozzle_diameter);

	//��ʼ����֪�Ĺ̶���support layers
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

	//���first layer height
	double first_layer_height = object_config_->get_abs_value("first_layer_height");
	while (!z_vec.empty() && z_vec[0] <= first_layer_height) {
		z_vec.erase(z_vec.begin());
	}
	z_vec.insert(z_vec.begin(), first_layer_height);


	//ͨ����first layer��first contact layer֮��ľ�����ȷֶεķ������raft layers
	if (object_config_->raft_layers > 1 && z_vec.size() >= 2) {

		//z[1]������raft layer��Ҳ��first layer object��contact layer
		double height = (z_vec[1] - z_vec[0]) / (object_config_->raft_layers - 1);


		//��Ȼ�Ѿ���������raft layers��������ٲ���raft_layers-2��rafts
		std::vector<double> raft_layer_heights;
		for (int i = 1; i <= object_config_->raft_layers - 2;i++) {
			raft_layer_heights.push_back(z_vec[0] + height * i);
		}
		auto iter = z_vec.begin();
		//z_vec.insert(z_vec.begin() + 2, raft_layer_heights.begin(), raft_layer_heights.end());
		z_vec.insert(z_vec.begin(), raft_layer_heights.begin(), raft_layer_heights.end());
	}
	

	//��������layer,��Ϊ�Ѿ������raft layers,������������
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
 *	��contact layers���´�������interface layers
 */
void SupportMaterial::GenerateInterfaceLayers(std::vector<double>& support_z,std::map<double, Polygons>& contact_map,
	std::map<double,Polygons>& top_map, std::map<int, Polygons>& interface_map) {
	int interface_layers_num = object_config_->support_material_interface_layers;

	//��contact areas�·�����interface layers
	for (int layer_id = 0; layer_id < support_z.size(); layer_id++) {
		double z_val = support_z[layer_id];

		Polygons this_polygons;
		if (contact_map.find(z_val) == contact_map.end()) {
			continue;
		}
		else {
			this_polygons = contact_map[z_val];
		}

		//��contact layer����interface layer
		for (int i = layer_id - 1; i >= 0 && i > layer_id - interface_layers_num; i--) {
			double z_val = support_z[i];
			std::vector<int> overlapping_layers = OverlappingLayers(i, support_z);
			std::vector<double> overlapping_z;
			for (auto& val : overlapping_layers) {
				overlapping_z.push_back(support_z[val]);
			}

			//��ǰ���interface areaΪupper contact area����upper interface area����layer slices֮���diff����
			//diff���������������support material��top surfaces
			Polygons diff_polygons1;
			//��ǰcontact region�б�ɾ����ӳ��(clipped projection)
			diff_polygons1.insert(diff_polygons1.end(), this_polygons.begin(), this_polygons.end());
			if (interface_map.find(i) != interface_map.end()) {
				//�ò��Ѿ�����interface region
				diff_polygons1.insert(diff_polygons1.end(), interface_map[i].begin(), interface_map[i].end());
			}

			Polygons diff_polygons2;
			for (double d : overlapping_z) {
				if (top_map.find(d) != top_map.end()) {
					//�ò��top slices
					diff_polygons2.insert(diff_polygons2.end(), top_map[d].begin(), top_map[d].end());
				}
			}

			for (double d : overlapping_z) {
				//�ò��contact regions
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
 *	���base support layer����һ������reverse interfaces, ԭ������������object top surfaces��
 */
void SupportMaterial::GenerateBottomInterfacesLayers(std::vector<double>& support_z, std::map<int, Polygons>& base_map,
	std::map<double, Polygons>& top_map, std::map<int, Polygons>& interface_map) {
	
	//�������������interface layers���������κ�bottom interface layers
	if (object_config_->support_material_interface_layers == 0) {
		return;
	}


	coord_t area_threshold = interface_flow_->scaled_spacing() * interface_flow_->scaled_spacing();

	//����object��top surfaces
	for (auto& val : top_map) {
		Polygons& this_polygons = val.second;

		//��¼Ϊ��top surface���ɵ�interface layers����
		int interface_layers = 0;
		
		//�������е�support layers��ֱ���ҵ�һ��������top surface�Ϸ���support layer
		for (int layer_id = 0; layer_id < support_z.size(); layer_id++) {
			double z = support_z[layer_id];
			if (z <= val.first) {
				continue;
			}

			if (base_map.find(layer_id) != base_map.end()) {
				//����Ӧ��Ϊinterface area��support material area
				Polygons interface_area_polygons = intersection(base_map[layer_id], this_polygons);

				//ɾ�������С������
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

				//���µ�interface area���浽interface��
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
 *	��contact layers��interface layers���´��ݣ�����main support layers
 */
void SupportMaterial::GenerateBaseLayers(std::vector<double>& support_z, std::map<double,Polygons>& contact_map,
	std::map<double,Polygons>& top_map,std::map<int,Polygons>& interface_map, std::map<int,Polygons>& base_map) {

	//��interface�·�����support layers
	for (int i = support_z.size() - 1; i >= 0; i--) {
		double z_val = support_z[i];
		std::vector<int> overlapping_layers = OverlappingLayers(i, support_z);
		std::vector<double> overlapping_z;
		for (int val : overlapping_layers) {
			overlapping_z.push_back(support_z[val]);
		}

		//Ϊ�˱���û��interface layer���������Ҫ�۲���upper layer
		//һ��interace layer��ʾֻ��һ��contact layer,����interace[i+1]Ϊ��
		Polygons upper_contact;
		if (object_config_->support_material_interface_layers <= 1) {
			double temp_z_val = support_z[i + 1];
			if (contact_map.find(temp_z_val) != contact_map.end()) {
				upper_contact = contact_map[temp_z_val];
			}
		}

		Polygons diff_polygons1;
		//upper layer�ϵ�support regions
		if (base_map.find(i + 1) != base_map.end()) {
			diff_polygons1.insert(diff_polygons1.end(), base_map[i + 1].begin(),base_map[i+1].end());
		}
		//upper layer�ϵ�interface regions
		if (interface_map.find(i + 1) != interface_map.end()) {
			diff_polygons1.insert(diff_polygons1.end(), interface_map[i + 1].begin(), interface_map[i + 1].end());
		}
		//upper layer�ϵ�contact regions
		diff_polygons1.insert(diff_polygons1.end(), upper_contact.begin(), upper_contact.end());


		Polygons diff_polygons2;
		//top slices��contact regions on this layer
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

		//layer->slices����current layer�ϵ�full shape, ���Ҳ����perimeter's width
		//support ������support material��full shape, ���Ҳ������Ҫ��extrusion
		//��������б�����full extrusion width
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
 *	�ֱ��ÿһ��֧�Žṹ���д�������������һ��
 */
void SupportMaterial::ProcessLayer(PrintObject& object,int layer_id, std::map<double, Polygons>& overhang_map,
	std::map<double, Polygons>& contact_map, std::map<int, Polygons>& interface_map, std::map<int, Polygons>& base_map) {
	int contact_loops = 1;
	
	//circle: contact area����״	
	coord_t circle_radius = 1.5 * interface_flow_->scaled_width();
	coord_t circle_distance = 3 * circle_radius;
	Points circle_points = { Point(circle_radius* cos(5 * PI / 3),circle_radius*sin(5 * PI / 3)),
		Point(circle_radius* cos(4 * PI / 3),circle_radius*sin(4 * PI / 3)),
		Point(circle_radius* cos(PI),circle_radius*sin(PI)),
		Point(circle_radius* cos(2 * PI / 3),circle_radius*sin(2 * PI / 3)),
		Point(circle_radius* cos(PI / 3),circle_radius*sin(PI / 3)),
		Point(circle_radius* cos(0),circle_radius*sin(0)) };
	Polygon circle(circle_points);

	//֧�Žṹ�����ģʽ
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

	//֧�Žṹ�Ĳ���
	int interface_angle = object_config_->support_material_angle + 90;
	float interface_spacing = object_config_->support_material_interface_spacing + interface_flow_->spacing();
	double interface_density = interface_spacing == 0 ? 1 : interface_flow_->spacing() / interface_spacing;
	float support_spacing = object_config_->support_material_spacing + flow_->spacing();
	float support_density = support_spacing == 0 ? 1 : flow_->spacing() / support_spacing;

	//support layer
	SupportLayer* support_layer = object.support_layers[layer_id];
	double z = support_layer->print_z;

	//flow�Ĳ���
	Flow flow = *flow_;
	flow.height = support_layer->height;
	Flow interface_flow = *interface_flow_;
	interface_flow.height = support_layer->height;

	Polygons overhang_polygons;
	Polygons contact_polygons;
	Polygons interface_polygons;
	Polygons base_polygons;
	
	//��ȡ��ӡ�߶�z��Ӧ��overhang area, contact area, interface area, base area
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

	//islands,�����Խ��д�ӡ������,��contact area, interface area, base area����union���
	Polygons islands_polygons = contact_polygons;
	islands_polygons.insert(islands_polygons.end(), interface_polygons.begin(), interface_polygons.end());
	islands_polygons.insert(islands_polygons.end(), base_polygons.begin(), base_polygons.end());
	support_layer->support_islands.append(union_ex(islands_polygons));

	//contact�ṹ
	Polygons contact_infill_polygons;
	if (object_config_->support_material_interface_layers == 0) {
		//�������Ҫinterface area,��contact area����һ���base area���д���
		base_polygons.insert(base_polygons.end(), contact_polygons.begin(), contact_polygons.end());
	}
	else if (!contact_polygons.empty() && contact_loops > 0) {
		//��������Ȧ��ѭ��

		
		//��������Ȧ���νṹ�����ߣ�centerline��
		contact_polygons = offset(contact_polygons, -interface_flow.scaled_width() / 2);

		//��������໷״�ṹ��������
		Polygons external_loops_polygons = contact_polygons;

		//ֻ���ǽӴ�overhang�Ļ�״�ṹ��loops facing the overhang��
		Polygons overhangs_with_margin = offset(overhang_polygons, +interface_flow.scaled_width() / 2);
		Polylines splitted_overhang_polylines;
		for(Polygon& polygon: external_loops_polygons){
			splitted_overhang_polylines.push_back(polygon.split_at_first_point());
		}
		Polylines external_loops_polylines = intersection_pl(splitted_overhang_polylines, overhangs_with_margin);

		//ʹ�����õ����ģʽ�������νṹ
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

		//ɾ��һ����
		Polylines splitted_loops_polylines;
		for (Polygon& loop : loops_polygons) {
			splitted_loops_polylines.push_back(loop.split_at_first_point());
		}

		Polylines intersected_loops_polylines = intersection_pl(splitted_loops_polylines, 
			offset(overhang_polygons, +scale_(SUPPORT_MATERIAL_MARGIN)));
		
		//���contact infill area��interface area
		//grow loops by circle_radius��Ŀ����ȷ��û��tiny extrusions����circle���棬
		//�����������ᵼ��loops ��contact infill֮����ֺܴ��gap,���Կ����ұ�ķ���
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

		//��loopsת��Ϊextrusionpath objects
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

		//����interface area�ⲿ��״�ṹ��������
		interface_polygons = offset2(interface_polygons, +SCALED_EPSILON, -(SCALED_EPSILON + interface_flow.scaled_width() / 2));
		
		//ͨ��offset regions���ϲ����ǣ��Դ���ȷ�����ǿ��Ժϲ���һ��
		interface_polygons.insert(interface_polygons.end(), contact_infill_polygons.begin(), contact_infill_polygons.end());
		interface_polygons = offset(interface_polygons, SCALED_EPSILON);

		//��base area�ṹλ��Holes�ṹ����ʱ��������ת��ΪInterface area�ṹ
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

		//��base area�м�ȥinterface area
		base_polygons = diff(base_polygons, interface_polygons);
		
		//��Interface pathת��Ϊextrusionpath object
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


	//base support��flange(��Ե)
	if (!base_polygons.empty()) {
		support_filler->angle = Geometry::deg2rad(angles[layer_id%angles.size()]);
		//��ʹ��base_flow��ԭ������Ҫһ��constant spacing value, �Ӷ�ȷ�����е�layers������ȷ�ض��루aligned��
		support_filler->min_spacing = flow.spacing();

		//�ҵ��ⲿ������������
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
			//�Ե�һ��ʹ����ȷ�ļ����spacing��,ԭ���ǲ���Ҫ�������ģʽ��������һ��
			interface_filler->min_spacing = base_flow.spacing();

			//��ȥbrim�ṹ
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
			//��֧�Žṹ����䲿�֣�support infill���ⲿ���perimeter
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
 * ���object top surfaces�ϵ�first support layer
 */
void SupportMaterial::GeneratePillarsShape(std::map<double,Polygons>& contact_map,std::vector<double> support_z,
	std::map<int,Polygons>& shape_map) {
	
	//�����ÿյ㼯����BoundingBox
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
	
	//��pillar��ӵ�ÿһ����
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


		//ÿ�δ���һ��pillar,������capitals֮����ֺϲ������
		//����Ҫͬʱ��¼��capital֧�ŵ�contact area, ԭ������Ҫȷ��nothing is left
		Polygons contact_supported_by_capitals;
		for (auto& capital : capitals) {
			//����capital tops
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


		//capitalsһ������²��ø�������contact area,��Ϊ�ܻ���ʣ���û�б����ǵĲ��֣�
		//���潫unsupported areasӳ�䵽ground, ����ʹ��normal supportһ��
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
		//��Ҫclip bottom layers���Ӷ���������continuous base flange����Ҳ����clip raft layers
		int layer_id = val.first;
		if (layer_id == 0 || layer_id < object_config_->raft_layers) {
			continue;
		}
		val.second = intersection(val.second, shape_map[layer_id]);
	}

}

/*
 *	���������layer���غϵ�layer������
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

