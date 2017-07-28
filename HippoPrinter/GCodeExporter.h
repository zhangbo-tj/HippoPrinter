#pragma once

#include <string>
#include <fstream>
#include <unordered_map>

#include <src/libslic3r/Print.hpp>
#include <src/libslic3r/GCode.hpp>
#include <src/libslic3r/GCode/CoolingBuffer.hpp>
#include <src/libslic3r/PlaceholderParser.hpp>


class GCodeExporter{
public:
	GCodeExporter(Print* print);
	~GCodeExporter();

	void Export(char* file_path);
	void ExportParameter(std::ofstream& fout);
	void InitMotionPlanner();
	void CalWipingPoints();
	void ExportEndCommands(std::ofstream& fout);
	
	void PrintFirstLayerTemp(std::ofstream& fout, bool wait);
	void FlushFilters(std::ofstream& fout);

	void ProcessLayer(Layer* layer, Points& copies,std::ofstream& fout);
	void InitAutoSpeed(Layer* layer);
	std::string PreProcessGCode(Layer* layer);
	std::string ExtrudeSkirt(Layer* layer);
	std::string ExtrudeBrim(Layer* layer);
	std::string ExtrudeSupportMaterial(SupportLayer* support_layer);
	std::string ExtrudePerimeters(std::unordered_map<int, ExtrusionEntityCollection>& peri_by_region);
	std::string ExtrudeInfills(std::unordered_map<int, ExtrusionEntityCollection>& infill_by_region);
	

public:
	Print* print_;
	PrintObjectPtrs objects_;
	PlaceholderParser placeholder_parser_;
	PrintConfig printconfig_;

	GCode gcodegen_;
	CoolingBuffer* cooling_buffer_;


	std::map<double, int> skirt_done_;
	bool brim_done_;
	bool second_layer_done_;
	Point last_obj_copy_;
	bool auto_speed_;
};

