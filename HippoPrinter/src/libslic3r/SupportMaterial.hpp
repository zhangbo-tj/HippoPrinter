#ifndef slic3r_SupportMaterial_hpp_
#define slic3r_SupportMaterial_hpp_

#include <src/libslic3r/PrintConfig.hpp>
#include <src/libslic3r/Flow.hpp>
#include <src/libslic3r/Print.hpp>


namespace Slic3r {

// how much we extend support around the actual contact area
constexpr coordf_t SUPPORT_MATERIAL_MARGIN = 1.5;


class SupportMaterial {
public:
	SupportMaterial(PrintConfig* print_config, PrintObjectConfig* print_object_config,
		Flow* first_layer_flow, Flow* flow, Flow* interface_flow);

	void Generate(PrintObject& object);

	void ContactArea(PrintObject& object, std::map<double, Polygons>& contact_map,
		std::map<double, Polygons>& overhang_map);

	void ObjectTop(PrintObject& object, std::map<double, Polygons>& contact_map,
		std::map<coordf_t, Polygons>& top_map);

	void SupportLayersZ(PrintObject& object, std::vector<double>& contact_z,
		std::vector<double>& top_z, double max_object_layer_height,
		std::vector<double>& z_vec);

	void GenerateInterfaceLayers(std::vector<double>& support_z, std::map<double, Polygons>& contact_map,
		std::map<double, Polygons>& top_map, std::map<int, Polygons>& interface_map);

	void GenerateBottomInterfacesLayers(std::vector<double>& support_z, std::map<int, Polygons>& base_map,
		std::map<double, Polygons>& top_map, std::map<int, Polygons>& interface_map);

	void GenerateBaseLayers(std::vector<double>& support_z, 
		std::map<double, Polygons>& contact_map,std::map<double, Polygons>& top_map,
		std::map<int, Polygons>& interface_map, std::map<int, Polygons>& base_map);

	void ClipWithObject(PrintObject& object, std::map<int, Polygons>& interface_map,
		std::vector<double>& support_z);

	void GenerateToolPaths(PrintObject& object, std::map<double, Polygons>& overhang_map,
		std::map<double, Polygons>& contact_map, std::map<int, Polygons>& interface_map, 
		std::map<int, Polygons>& base_map);

	void ProcessLayer(PrintObject& object, int layer_id, std::map<double, Polygons>& overhang_map,
		std::map<double, Polygons>& contact_map, std::map<int, Polygons>& interface_map, 
		std::map<int, Polygons>& base_map);

	void GeneratePillarsShape(std::map<double, Polygons>& contact_map, std::vector<double> support_z,
		std::map<int, Polygons>& shape_map);

	void ClipWithShape(std::map<int, Polygons>& support, std::map<int, Polygons>& shape_map);
	std::vector<int> OverlappingLayers(int i, const std::vector<double>&);

	double ContactDistance(double layer_height, double nozzle_diamter);



public:
	PrintConfig* print_config_;
	PrintObjectConfig* object_config_;
	Flow* flow_;
	Flow* first_layer_flow_;
	Flow* interface_flow_;
};

}

#endif
