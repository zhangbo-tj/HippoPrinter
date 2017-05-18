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
	void Generate();
	void ContactArea(PrintObject& object);
	void ObjectTop();
	void SupportLayersZ();
	void GenerateInterfaceLayers();
	void GenerateBottomInterfacesLayers();
	void GenerateBaseLayers();
	void ClipWithObject();
	void GenerateToolPaths();
	void GeneratePollarsShape();
	void ClipWithShape();
	void OverlappingLayers();
	double ContactDistance(double layer_height, double nozzle_diamter);

public:
	PrintConfig print_config_;
	PrintObjectConfig object_config_;
	Flow flow_;
	Flow first_layer_flow_;
	Flow interface_flow_;
	double scale_ = 1;
};

}

#endif
