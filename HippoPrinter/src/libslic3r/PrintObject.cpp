#include "Print.hpp"
#include "BoundingBox.hpp"
#include "ClipperUtils.hpp"
#include "Geometry.hpp"
#include <algorithm>
#include <vector>
#include <map>
#include <QDebug>

namespace Slic3r {

PrintObject::PrintObject(Print* print, ModelObject* model_object, const BoundingBoxf3 &modobj_bbox)
:   typed_slices(false),
	_print(print),
	_model_object(model_object)
{
	// Compute the translation to be applied to our meshes so that we work with smaller coordinates
	{
		// Translate meshes so that our toolpath generation algorithms work with smaller
		// XY coordinates; this translation is an optimization and not strictly required.
		// A cloned mesh will be aligned to 0 before slicing in _slice_region() since we
		// don't assume it's already aligned and we don't alter the original position in model.
		// We store the XY translation so that we can place copies correctly in the output G-code
		// (copies are expressed in G-code coordinates and this translation is not publicly exposed).
		this->_copies_shift = Point(
			scale_(modobj_bbox.min.x), scale_(modobj_bbox.min.y));

		// Scale the object size and store it
		Pointf3 size = modobj_bbox.size();
		this->size = Point3(scale_(size.x), scale_(size.y), scale_(size.z));
	}
	
	this->reload_model_instances();
	this->layer_height_ranges = model_object->layer_height_ranges;
}

PrintObject::~PrintObject()
{
}

Print*
PrintObject::print()
{
	return this->_print;
}

Points
PrintObject::copies() const
{
	return this->_copies;
}

bool
PrintObject::add_copy(const Pointf &point)
{
	Points points = this->_copies;
	points.push_back(Point::new_scale(point.x, point.y));
	return this->set_copies(points);
}

bool
PrintObject::delete_last_copy()
{
	Points points = this->_copies;
	points.pop_back();
	return this->set_copies(points);
}

bool
PrintObject::delete_all_copies()
{
	Points points;
	return this->set_copies(points);
}

bool
PrintObject::set_copies(const Points &points)
{
	this->_copies = points;
	
	// order copies with a nearest neighbor search and translate them by _copies_shift
	this->_shifted_copies.clear();
	this->_shifted_copies.reserve(points.size());
	
	// order copies with a nearest-neighbor search
	std::vector<Points::size_type> ordered_copies;
	Slic3r::Geometry::chained_path(points, ordered_copies);
	
	for (std::vector<Points::size_type>::const_iterator it = ordered_copies.begin(); it != ordered_copies.end(); ++it) {
		Point copy = points[*it];
		copy.translate(this->_copies_shift);
		this->_shifted_copies.push_back(copy);
	}
	
	bool invalidated = false;
	if (this->_print->invalidate_step(psSkirt)) invalidated = true;
	if (this->_print->invalidate_step(psBrim)) invalidated = true;
	return invalidated;
}

bool
PrintObject::reload_model_instances()
{
	Points copies;
	for (ModelInstancePtrs::const_iterator i = this->_model_object->instances.begin(); i != this->_model_object->instances.end(); ++i) {
		copies.push_back(Point::new_scale((*i)->offset.x, (*i)->offset.y));
	}
	return this->set_copies(copies);
}

BoundingBox
PrintObject::bounding_box() const
{
	// since the object is aligned to origin, bounding box coincides with size
	Points pp;
	pp.push_back(Point(0,0));
	pp.push_back(this->size);
	return BoundingBox(pp);
}

// returns 0-based indices of used extruders
std::set<size_t>
PrintObject::extruders() const
{
	std::set<size_t> extruders = this->_print->extruders();
	std::set<size_t> sm_extruders = this->support_material_extruders();
	extruders.insert(sm_extruders.begin(), sm_extruders.end());
	return extruders;
}

// returns 0-based indices of used extruders
std::set<size_t>
PrintObject::support_material_extruders() const
{
	std::set<size_t> extruders;
	if (this->has_support_material()) {
		extruders.insert(this->config.support_material_extruder - 1);
		extruders.insert(this->config.support_material_interface_extruder - 1);
	}
	return extruders;
}

void
PrintObject::add_region_volume(int region_id, int volume_id)
{
	region_volumes[region_id].push_back(volume_id);
}

/*  This is the *total* layer count (including support layers)
	this value is not supposed to be compared with Layer::id
	since they have different semantics */
size_t
PrintObject::total_layer_count() const
{
	return this->layer_count() + this->support_layer_count();
}

size_t
PrintObject::layer_count() const
{
	return this->layers.size();
}

void
PrintObject::clear_layers()
{
	for (int i = this->layers.size()-1; i >= 0; --i)
		this->delete_layer(i);
}

Layer*
PrintObject::add_layer(int id, coordf_t height, coordf_t print_z, coordf_t slice_z)
{
	Layer* layer = new Layer(id, this, height, print_z, slice_z);
	layers.push_back(layer);
	return layer;
}

void
PrintObject::delete_layer(int idx)
{
	LayerPtrs::iterator i = this->layers.begin() + idx;
	delete *i;
	this->layers.erase(i);
}

size_t
PrintObject::support_layer_count() const
{
	return this->support_layers.size();
}

void
PrintObject::clear_support_layers()
{
	for (int i = this->support_layers.size()-1; i >= 0; --i)
		this->delete_support_layer(i);
}

SupportLayer*
PrintObject::add_support_layer(int id, coordf_t height, coordf_t print_z)
{
	SupportLayer* layer = new SupportLayer(id, this, height, print_z, -1);
	support_layers.push_back(layer);
	return layer;
}

void
PrintObject::delete_support_layer(int idx)
{
	SupportLayerPtrs::iterator i = this->support_layers.begin() + idx;
	delete *i;
	this->support_layers.erase(i);
}

bool
PrintObject::invalidate_state_by_config(const PrintConfigBase &config)
{
	const t_config_option_keys diff = this->config.diff(config);
	
	std::set<PrintObjectStep> steps;
	bool all = false;
	
	// this method only accepts PrintObjectConfig and PrintRegionConfig option keys
	for (const t_config_option_key &opt_key : diff) {
		if (opt_key == "layer_height"
			|| opt_key == "first_layer_height"
			|| opt_key == "xy_size_compensation"
			|| opt_key == "raft_layers") {
			steps.insert(posSlice);
		} else if (opt_key == "support_material_contact_distance") {
			steps.insert(posSlice);
			steps.insert(posPerimeters);
			steps.insert(posSupportMaterial);
		} else if (opt_key == "support_material") {
			steps.insert(posPerimeters);
			steps.insert(posSupportMaterial);
		} else if (opt_key == "support_material_angle"
			|| opt_key == "support_material_extruder"
			|| opt_key == "support_material_extrusion_width"
			|| opt_key == "support_material_interface_layers"
			|| opt_key == "support_material_interface_extruder"
			|| opt_key == "support_material_interface_spacing"
			|| opt_key == "support_material_interface_speed"
			|| opt_key == "support_material_buildplate_only"
			|| opt_key == "support_material_pattern"
			|| opt_key == "support_material_spacing"
			|| opt_key == "support_material_threshold"
			|| opt_key == "dont_support_bridges") {
			steps.insert(posSupportMaterial);
		} else if (opt_key == "interface_shells"
			|| opt_key == "infill_only_where_needed") {
			steps.insert(posPrepareInfill);
		} else if (opt_key == "seam_position"
			|| opt_key == "support_material_speed") {
			// these options only affect G-code export, so nothing to invalidate
		} else {
			// for legacy, if we can't handle this option let's invalidate all steps
			all = true;
			break;
		}
	}
	
	if (!diff.empty())
		this->config.apply(config, true);
	
	bool invalidated = false;
	if (all) {
		invalidated = this->invalidate_all_steps();
	} else {
		for (const PrintObjectStep &step : steps)
			if (this->invalidate_step(step))
				invalidated = true;
	}
	
	return invalidated;
}

bool
PrintObject::invalidate_step(PrintObjectStep step)
{
	bool invalidated = this->state.invalidate(step);
	
	// propagate to dependent steps
	if (step == posPerimeters) {
		this->invalidate_step(posPrepareInfill);
		this->_print->invalidate_step(psSkirt);
		this->_print->invalidate_step(psBrim);
	} else if (step == posDetectSurfaces) {
		this->invalidate_step(posPrepareInfill);
	} else if (step == posPrepareInfill) {
		this->invalidate_step(posInfill);
	} else if (step == posInfill) {
		this->_print->invalidate_step(psSkirt);
		this->_print->invalidate_step(psBrim);
	} else if (step == posSlice) {
		this->invalidate_step(posPerimeters);
		this->invalidate_step(posDetectSurfaces);
		this->invalidate_step(posSupportMaterial);
	} else if (step == posSupportMaterial) {
		this->_print->invalidate_step(psSkirt);
		this->_print->invalidate_step(psBrim);
	}
	
	return invalidated;
}

bool
PrintObject::invalidate_all_steps()
{
	// make a copy because when invalidating steps the iterators are not working anymore
	std::set<PrintObjectStep> steps = this->state.started;
	
	bool invalidated = false;
	for (std::set<PrintObjectStep>::const_iterator step = steps.begin(); step != steps.end(); ++step) {
		if (this->invalidate_step(*step)) invalidated = true;
	}
	return invalidated;
}

bool
PrintObject::has_support_material() const
{
	return this->config.support_material
		|| this->config.raft_layers > 0
		|| this->config.support_material_enforce_layers > 0;
}

void
PrintObject::detect_surfaces_type()
{
	// prerequisites
	// this->slice();
	
	if (this->state.is_done(posDetectSurfaces)) return;
	this->state.set_started(posDetectSurfaces);
	
	parallelize<Layer*>(
		std::queue<Layer*>(std::deque<Layer*>(this->layers.begin(), this->layers.end())),  // cast LayerPtrs to std::queue<Layer*>
		boost::bind(&Slic3r::Layer::detect_surfaces_type, _1),
		this->_print->config.threads.value
	);
	
	this->typed_slices = true;
	this->state.set_done(posDetectSurfaces);
}

void
PrintObject::process_external_surfaces()
{
	parallelize<Layer*>(
		std::queue<Layer*>(std::deque<Layer*>(this->layers.begin(), this->layers.end())),  // cast LayerPtrs to std::queue<Layer*>
		boost::bind(&Slic3r::Layer::process_external_surfaces, _1),
		this->_print->config.threads.value
	);
}

/* This method applies bridge flow to the first internal solid layer above
   sparse infill */
void
PrintObject::bridge_over_infill()
{
	FOREACH_REGION(this->_print, region) {
		const size_t region_id = region - this->_print->regions.begin();
		
		// skip bridging in case there are no voids
		if ((*region)->config.fill_density.value == 100) continue;
		
		// get bridge flow
		const Flow bridge_flow = (*region)->flow(
			frSolidInfill,
			-1,     // layer height, not relevant for bridge flow
			true,   // bridge
			false,  // first layer
			-1,     // custom width, not relevant for bridge flow
			*this
		);
		
		// get the average extrusion volume per surface unit
		const double mm3_per_mm  = bridge_flow.mm3_per_mm();
		const double mm3_per_mm2 = mm3_per_mm / bridge_flow.width;
		
		FOREACH_LAYER(this, layer_it) {
			// skip first layer
			if (layer_it == this->layers.begin()) continue;
			
			Layer* layer        = *layer_it;
			LayerRegion* layerm = layer->get_region(region_id);
			
			// extract the stInternalSolid surfaces that might be transformed into bridges
			Polygons internal_solid;
			layerm->fill_surfaces.filter_by_type(stInternalSolid, &internal_solid);
			if (internal_solid.empty()) continue;
			
			// check whether we should bridge or not according to density
			{
				// get the normal solid infill flow we would use if not bridging
				const Flow normal_flow = layerm->flow(frSolidInfill, false);
				
				// Bridging over sparse infill has two purposes:
				// 1) cover better the gaps of internal sparse infill, especially when
				//    printing at very low densities;
				// 2) provide a greater flow when printing very thin layers where normal
				//    solid flow would be very poor.
				// So we calculate density threshold as interpolation according to normal flow.
				// If normal flow would be equal or greater than the bridge flow, we can keep
				// a low threshold like 25% in order to bridge only when printing at very low
				// densities, when sparse infill has significant gaps.
				// If normal flow would be equal or smaller than half the bridge flow, we
				// use a higher threshold like 50% in order to bridge in more cases.
				// We still never bridge whenever fill density is greater than 50% because
				// we would overstuff.
				const float min_threshold = 25.0;
				const float max_threshold = 50.0;
				const float density_threshold = std::max(
					std::min<float>(
						min_threshold
							+ (max_threshold - min_threshold)
							* (normal_flow.mm3_per_mm() - mm3_per_mm)
							/ (mm3_per_mm/2 - mm3_per_mm),
						max_threshold
					),
					min_threshold
				);
				
				if ((*region)->config.fill_density.value > density_threshold) continue;
			}
			
			// check whether the lower area is deep enough for absorbing the extra flow
			// (for obvious physical reasons but also for preventing the bridge extrudates
			// from overflowing in 3D preview)
			ExPolygons to_bridge;
			{
				Polygons to_bridge_pp = internal_solid;
				
				// Only bridge where internal infill exists below the solid shell matching
				// these two conditions:
				// 1) its depth is at least equal to our bridge extrusion diameter;
				// 2) its free volume (thus considering infill density) is at least equal
				//    to the volume needed by our bridge flow.
				double excess_mm3_per_mm2 = mm3_per_mm2;
				
				// iterate through lower layers spanned by bridge_flow
				const double bottom_z = layer->print_z - bridge_flow.height;
				for (int i = (layer_it - this->layers.begin()) - 1; i >= 0; --i) {
					const Layer* lower_layer = this->layers[i];
					
					// subtract the void volume of this layer
					excess_mm3_per_mm2 -= lower_layer->height * (100 - (*region)->config.fill_density.value)/100;
					
					// stop iterating if both conditions are matched
					if (lower_layer->print_z < bottom_z && excess_mm3_per_mm2 <= 0) break;
					
					// iterate through regions and collect internal surfaces
					Polygons lower_internal;
					FOREACH_LAYERREGION(lower_layer, lower_layerm_it)
						(*lower_layerm_it)->fill_surfaces.filter_by_type(stInternal, &lower_internal);
					
					// intersect such lower internal surfaces with the candidate solid surfaces
					to_bridge_pp = intersection(to_bridge_pp, lower_internal);
				}
				
				// don't bridge if the volume condition isn't matched
				if (excess_mm3_per_mm2 > 0) continue;
				
				// there's no point in bridging too thin/short regions
				{
					const double min_width = bridge_flow.scaled_width() * 3;
					to_bridge_pp = offset2(to_bridge_pp, -min_width, +min_width);
				}
				
				if (to_bridge_pp.empty()) continue;
				
				// convert into ExPolygons
				to_bridge = union_ex(to_bridge_pp);
			}
			
			#ifdef SLIC3R_DEBUG
			printf("Bridging %zu internal areas at layer %zu\n", to_bridge.size(), layer->id());
			#endif
			
			// compute the remaning internal solid surfaces as difference
			const ExPolygons not_to_bridge = diff_ex(internal_solid, to_polygons(to_bridge), true);
			
			// build the new collection of fill_surfaces
			{
				Surfaces new_surfaces;
				for (Surfaces::const_iterator surface = layerm->fill_surfaces.surfaces.begin(); surface != layerm->fill_surfaces.surfaces.end(); ++surface) {
					if (surface->surface_type != stInternalSolid)
						new_surfaces.push_back(*surface);
				}
				
				for (ExPolygons::const_iterator ex = to_bridge.begin(); ex != to_bridge.end(); ++ex)
					new_surfaces.push_back(Surface(stInternalBridge, *ex));
				
				for (ExPolygons::const_iterator ex = not_to_bridge.begin(); ex != not_to_bridge.end(); ++ex)
					new_surfaces.push_back(Surface(stInternalSolid, *ex));
				
				layerm->fill_surfaces.surfaces = new_surfaces;
			}
			
			/*
			# exclude infill from the layers below if needed
			# see discussion at https://github.com/alexrj/Slic3r/issues/240
			# Update: do not exclude any infill. Sparse infill is able to absorb the excess material.
			if (0) {
				my $excess = $layerm->extruders->{infill}->bridge_flow->width - $layerm->height;
				for (my $i = $layer_id-1; $excess >= $self->get_layer($i)->height; $i--) {
					Slic3r::debugf "  skipping infill below those areas at layer %d\n", $i;
					foreach my $lower_layerm (@{$self->get_layer($i)->regions}) {
						my @new_surfaces = ();
						# subtract the area from all types of surfaces
						foreach my $group (@{$lower_layerm->fill_surfaces->group}) {
							push @new_surfaces, map $group->[0]->clone(expolygon => $_),
								@{diff_ex(
									[ map $_->p, @$group ],
									[ map @$_, @$to_bridge ],
								)};
							push @new_surfaces, map Slic3r::Surface->new(
								expolygon       => $_,
								surface_type    => S_TYPE_INTERNALVOID,
							), @{intersection_ex(
								[ map $_->p, @$group ],
								[ map @$_, @$to_bridge ],
							)};
						}
						$lower_layerm->fill_surfaces->clear;
						$lower_layerm->fill_surfaces->append($_) for @new_surfaces;
					}
					
					$excess -= $self->get_layer($i)->height;
				}
			}
			*/
		}
	}
}

// adjust the layer height to the next multiple of the z full-step resolution
coordf_t PrintObject::adjust_layer_height(coordf_t layer_height) const
{
	coordf_t result = layer_height;
	if(this->_print->config.z_steps_per_mm > 0) {
		coordf_t min_dz = 1 / this->_print->config.z_steps_per_mm * 4;
		result = int(layer_height / min_dz + 0.5) * min_dz;
	}

	return result > 0 ? result : layer_height;
}

// generate a vector of print_z coordinates in object coordinate system (starting with 0) but including
// the first_layer_height if provided.
std::vector<coordf_t> PrintObject::generate_object_layers(coordf_t first_layer_height) {

	std::vector<coordf_t> result;

	coordf_t min_nozzle_diameter = 1.0;
	std::set<size_t> object_extruders = this->_print->object_extruders();
	for (std::set<size_t>::const_iterator it_extruder = object_extruders.begin(); it_extruder != object_extruders.end(); ++ it_extruder) {
		min_nozzle_diameter = std::min(min_nozzle_diameter, this->_print->config.nozzle_diameter.get_at(*it_extruder));
	}
	coordf_t layer_height = std::min(min_nozzle_diameter, this->config.layer_height.getFloat());
	layer_height = this->adjust_layer_height(layer_height);
	this->config.layer_height.value = layer_height;
	if(first_layer_height) {
		result.push_back(first_layer_height);
	}

	coordf_t print_z = first_layer_height;
	coordf_t height = first_layer_height;
	// loop until we have at least one layer and the max slice_z reaches the object height
	while (print_z < unscale(this->size.z)) {
		height = layer_height;

		// look for an applicable custom range
		for (t_layer_height_ranges::const_iterator it_range = this->layer_height_ranges.begin(); it_range != this->layer_height_ranges.end(); ++ it_range) {
			if(print_z >= it_range->first.first && print_z <= it_range->first.second) {
				if(it_range->second > 0) {
					height = it_range->second;
				}
			}
		}

		print_z += height;

		result.push_back(print_z);
	}

	// Reduce or thicken the top layer in order to match the original object size.
	// This is not actually related to z_steps_per_mm but we only enable it in case
	// user provided that value, as it means they really care about the layer height
	// accuracy and we don't provide unexpected result for people noticing the last
	// layer has a different layer height.
	if (this->_print->config.z_steps_per_mm > 0 && result.size() > 1) {
		coordf_t diff = result.back() - unscale(this->size.z);
		int last_layer = result.size()-1;

		if (diff < 0) {
			// we need to thicken last layer
			coordf_t new_h = result[last_layer] - result[last_layer-1];
			new_h = std::min(min_nozzle_diameter, new_h - diff); // add (negativ) diff value
			std::cout << new_h << std::endl;
			result[last_layer] = result[last_layer-1] + new_h;
		} else {
			// we need to reduce last layer
			coordf_t new_h = result[last_layer] - result[last_layer-1];
			if(min_nozzle_diameter/2 < new_h) { //prevent generation of a too small layer
				new_h = std::max(min_nozzle_diameter/2, new_h - diff); // subtract (positive) diff value
				std::cout << new_h << std::endl;
				result[last_layer] = result[last_layer-1] + new_h;
			}
		}
	}
	return result;
}

// 1) Decides Z positions of the layers,
// 2) Initializes layers and their regions
// 3) Slices the object meshes
// 4) Slices the modifier meshes and reclassifies the slices of the object meshes by the slices of the modifier meshes
// 5) Applies size compensation (offsets the slices in XY plane)
// 6) Replaces bad slices by the slices reconstructed from the upper/lower layer
// Resulting expolygons of layer regions are marked as Internal.
//
// this should be idempotent
void PrintObject::_slice()
{

	coordf_t raft_height = 0; // 	coordf_t print_z = 0;// 	coordf_t height  = 0;
	coordf_t first_layer_height = this->config.first_layer_height.get_abs_value(this->config.layer_height.value);


	// take raft layers into account
	int id = 0;

	if (this->config.raft_layers > 0) {
		id = this->config.raft_layers;

		coordf_t min_support_nozzle_diameter = 1.0;
		std::set<size_t> support_material_extruders = this->_print->support_material_extruders();
		for (std::set<size_t>::const_iterator it_extruder = support_material_extruders.begin(); it_extruder != support_material_extruders.end(); ++ it_extruder) {
			min_support_nozzle_diameter = std::min(min_support_nozzle_diameter, this->_print->config.nozzle_diameter.get_at(*it_extruder));
		}
		coordf_t support_material_layer_height = 0.75 * min_support_nozzle_diameter;

		// raise first object layer Z by the thickness of the raft itself
		// plus the extra distance required by the support material logic
		raft_height += first_layer_height;
		raft_height += support_material_layer_height * (this->config.raft_layers - 1);

		// reset for later layer generation
		first_layer_height = 0;

		// detachable support
		if(this->config.support_material_contact_distance > 0) {
			first_layer_height = min_support_nozzle_diameter;
			raft_height += this->config.support_material_contact_distance;

		}
	}

	// Initialize layers and their slice heights.
	std::vector<float> slice_zs;
	{
		this->clear_layers();
		// All print_z values for this object, without the raft.
		std::vector<coordf_t> object_layers = this->generate_object_layers(first_layer_height);
		// Reserve object layers for the raft. Last layer of the raft is the contact layer.
		slice_zs.reserve(object_layers.size());
		Layer *prev = nullptr;
		coordf_t lo = raft_height;
		coordf_t hi = lo;
		for (size_t i_layer = 0; i_layer < object_layers.size(); i_layer++) {
			lo = hi;  // store old value
			hi = object_layers[i_layer] + raft_height;
			coordf_t slice_z = 0.5 * (lo + hi) - raft_height;
			Layer *layer = this->add_layer(id++, hi - lo, hi, slice_z);
			slice_zs.push_back(float(slice_z));
			if (prev != nullptr) {
				prev->upper_layer = layer;
				layer->lower_layer = prev;
			}
			// Make sure all layers contain layer region objects for all regions.
			for (size_t region_id = 0; region_id < this->_print->regions.size(); ++ region_id)
				layer->add_region(this->print()->regions[region_id]);
			prev = layer;
		}
	}

	if (this->print()->regions.size() == 1) {
		// Optimized for a single region. Slice the single non-modifier mesh.
		std::vector<ExPolygons> expolygons_by_layer = this->_slice_region(0, slice_zs, false);
		for (size_t layer_id = 0; layer_id < expolygons_by_layer.size(); ++ layer_id)
			this->layers[layer_id]->regions.front()->slices.append(std::move(expolygons_by_layer[layer_id]), stInternal);
	} else {
		// Slice all non-modifier volumes.
		for (size_t region_id = 0; region_id < this->print()->regions.size(); ++ region_id) {
			std::vector<ExPolygons> expolygons_by_layer = this->_slice_region(region_id, slice_zs, false);
			for (size_t layer_id = 0; layer_id < expolygons_by_layer.size(); ++ layer_id)
				this->layers[layer_id]->regions[region_id]->slices.append(std::move(expolygons_by_layer[layer_id]), stInternal);
		}
		// Slice all modifier volumes.
		for (size_t region_id = 0; region_id < this->print()->regions.size(); ++ region_id) {
			std::vector<ExPolygons> expolygons_by_layer = this->_slice_region(region_id, slice_zs, true);
			// loop through the other regions and 'steal' the slices belonging to this one
			for (size_t other_region_id = 0; other_region_id < this->print()->regions.size(); ++ other_region_id) {
				if (region_id == other_region_id)
					continue;
				for (size_t layer_id = 0; layer_id < expolygons_by_layer.size(); ++ layer_id) {
					Layer       *layer = layers[layer_id];
					LayerRegion *layerm = layer->regions[region_id];
					LayerRegion *other_layerm = layer->regions[other_region_id];
					if (layerm == nullptr || other_layerm == nullptr)
						continue;
					Polygons other_slices = to_polygons(other_layerm->slices);
					ExPolygons my_parts = intersection_ex(other_slices, to_polygons(expolygons_by_layer[layer_id]));
					if (my_parts.empty())
						continue;
					// Remove such parts from original region.
					other_layerm->slices.set(diff_ex(other_slices, to_polygons(my_parts)), stInternal);
					// Append new parts to our region.
					layerm->slices.append(std::move(my_parts), stInternal);
				}
			}
		}
	}

	// remove last layer(s) if empty
	bool done = false;
	while (! this->layers.empty()) {
		const Layer *layer = this->layers.back();
		for (size_t region_id = 0; region_id < this->print()->regions.size(); ++ region_id)
			if (layer->regions[region_id] != nullptr && ! layer->regions[region_id]->slices.empty()) {
				done = true;
				break;
			}
		if(done) {
			break;
		}
		this->delete_layer(int(this->layers.size()) - 1);
	}

	for (size_t layer_id = 0; layer_id < layers.size(); ++ layer_id) {
		Layer *layer = this->layers[layer_id];
		// Apply size compensation and perform clipping of multi-part objects.
		float delta = float(scale_(this->config.xy_size_compensation.value));
		bool  scale = delta != 0.f;
		if (layer->regions.size() == 1) {
			if (scale) {
				// Single region, growing or shrinking.
				LayerRegion *layerm = layer->regions.front();
				layerm->slices.set(offset_ex(to_expolygons(std::move(layerm->slices.surfaces)), delta), stInternal);
			}
		} else if (scale) {
			// Multiple regions, growing, shrinking or just clipping one region by the other.
			// When clipping the regions, priority is given to the first regions.
			Polygons processed;
			for (size_t region_id = 0; region_id < layer->regions.size(); ++ region_id) {
				LayerRegion *layerm = layer->regions[region_id];
				ExPolygons slices = to_expolygons(std::move(layerm->slices.surfaces));
				if (scale)
					slices = offset_ex(slices, delta);
				if (region_id > 0)
					// Trim by the slices of already processed regions.
					slices = diff_ex(to_polygons(std::move(slices)), processed);
				if (region_id + 1 < layer->regions.size())
					// Collect the already processed regions to trim the to be processed regions.
					processed += to_polygons(slices);
				layerm->slices.set(std::move(slices), stInternal);
			}
		}
		// Merge all regions' slices to get islands, chain them by a shortest path.
		layer->make_slices();
	}
}

// called from slice()
std::vector<ExPolygons>
PrintObject::_slice_region(size_t region_id, std::vector<float> z, bool modifier)
{
	std::vector<ExPolygons> layers;
	std::vector<int> &region_volumes = this->region_volumes[region_id];
	if (region_volumes.empty()) return layers;
	
	ModelObject &object = *this->model_object();
	
	// compose mesh
	TriangleMesh mesh;
	for (std::vector<int>::const_iterator it = region_volumes.begin();
		it != region_volumes.end(); ++it) {
		
		const ModelVolume &volume = *object.volumes[*it];
		if (volume.modifier != modifier) continue;
		
		mesh.merge(volume.mesh);
	}
	if (mesh.facets_count() == 0) return layers;

	// transform mesh
	// we ignore the per-instance transformations currently and only 
	// consider the first one
	object.instances[0]->transform_mesh(&mesh, true);

	// align mesh to Z = 0 (it should be already aligned actually) and apply XY shift
	mesh.translate(
		-unscale(this->_copies_shift.x),
		-unscale(this->_copies_shift.y),
		-object.bounding_box().min.z
	);
	
	// perform actual slicing
	TriangleMeshSlicer<Z>(&mesh).slice(z, &layers);
	return layers;
}

void
PrintObject::_make_perimeters()
{
	if (this->state.is_done(posPerimeters)) return;
	this->state.set_started(posPerimeters);
	
	//main_statusbar_->showMessage("Start make perimeters");

	// merge slices if they were split into types
	// This is not currently taking place because since merge_slices + detect_surfaces_type
	// are not truly idempotent we are invalidating posSlice here (see the Perl part of 
	// this method).
	if (this->typed_slices) {
		// merge_slices() undoes detect_surfaces_type()
		FOREACH_LAYER(this, layer_it)
			(*layer_it)->merge_slices();
		this->typed_slices = false;
		this->state.invalidate(posDetectSurfaces);
	}
	
	// compare each layer to the one below, and mark those slices needing
	// one additional inner perimeter, like the top of domed objects-
	
	// this algorithm makes sure that at least one perimeter is overlapping
	// but we don't generate any extra perimeter if fill density is zero, as they would be floating
	// inside the object - infill_only_where_needed should be the method of choice for printing
	// hollow objects
	FOREACH_REGION(this->_print, region_it) {
		size_t region_id = region_it - this->_print->regions.begin();
		const PrintRegion &region = **region_it;
		
		if (!region.config.extra_perimeters
			|| region.config.perimeters == 0
			|| region.config.fill_density == 0
			|| this->layer_count() < 2) continue;
		
		for (size_t i = 0; i <= (this->layer_count()-2); ++i) {
			LayerRegion &layerm                     = *this->get_layer(i)->get_region(region_id);
			const LayerRegion &upper_layerm         = *this->get_layer(i+1)->get_region(region_id);
			
			// In order to avoid diagonal gaps (GH #3732) we ignore the external half of the upper
			// perimeter, since it's not truly covering this layer.
			const Polygons upper_layerm_polygons = offset(
				upper_layerm.slices,
				-upper_layerm.flow(frExternalPerimeter).scaled_width()/2
			);
			
			// Filter upper layer polygons in intersection_ppl by their bounding boxes?
			// my $upper_layerm_poly_bboxes= [ map $_->bounding_box, @{$upper_layerm_polygons} ];
			double total_loop_length = 0;
			for (Polygons::const_iterator it = upper_layerm_polygons.begin(); it != upper_layerm_polygons.end(); ++it)
				total_loop_length += it->length();
			
			const coord_t perimeter_spacing     = layerm.flow(frPerimeter).scaled_spacing();
			const Flow ext_perimeter_flow       = layerm.flow(frExternalPerimeter);
			const coord_t ext_perimeter_width   = ext_perimeter_flow.scaled_width();
			const coord_t ext_perimeter_spacing = ext_perimeter_flow.scaled_spacing();
			
			for (Surfaces::iterator slice = layerm.slices.surfaces.begin();
				slice != layerm.slices.surfaces.end(); ++slice) {
				while (true) {
					// compute the total thickness of perimeters
					const coord_t perimeters_thickness = ext_perimeter_width/2 + ext_perimeter_spacing/2
						+ (region.config.perimeters-1 + slice->extra_perimeters) * perimeter_spacing;
					
					// define a critical area where we don't want the upper slice to fall into
					// (it should either lay over our perimeters or outside this area)
					const coord_t critical_area_depth = perimeter_spacing * 1.5;
					const Polygons critical_area = diff(
						offset(slice->expolygon, -perimeters_thickness),
						offset(slice->expolygon, -(perimeters_thickness + critical_area_depth))
					);
					
					// check whether a portion of the upper slices falls inside the critical area
					const Polylines intersection = intersection_pl(
						upper_layerm_polygons,
						critical_area
					);
					
					// only add an additional loop if at least 30% of the slice loop would benefit from it
					{
						double total_intersection_length = 0;
						for (Polylines::const_iterator it = intersection.begin(); it != intersection.end(); ++it)
							total_intersection_length += it->length();
						if (total_intersection_length <= total_loop_length*0.3) break;
					}
					
					/*
					if (0) {
						require "Slic3r/SVG.pm";
						Slic3r::SVG::output(
							"extra.svg",
							no_arrows   => 1,
							expolygons  => union_ex($critical_area),
							polylines   => [ map $_->split_at_first_point, map $_->p, @{$upper_layerm->slices} ],
						);
					}
					*/
					
					slice->extra_perimeters++;
				}
				
				#ifdef DEBUG
					if (slice->extra_perimeters > 0)
						printf("  adding %d more perimeter(s) at layer %zu\n", slice->extra_perimeters, i);
				#endif
			}
		}
	}
	
	parallelize<Layer*>(
		std::queue<Layer*>(std::deque<Layer*>(this->layers.begin(), this->layers.end())),  // cast LayerPtrs to std::queue<Layer*>
		boost::bind(&Slic3r::Layer::make_perimeters, _1),
		this->_print->config.threads.value
	);
	
	/*
		simplify slices (both layer and region slices),
		we only need the max resolution for perimeters
	### This makes this method not-idempotent, so we keep it disabled for now.
	###$self->_simplify_slices(&Slic3r::SCALED_RESOLUTION);
	*/
	
	this->state.set_done(posPerimeters);
}

void
PrintObject::_infill()
{
	if (this->state.is_done(posInfill)) return;
	this->state.set_started(posInfill);
	
	//main_statusbar_->showMessage("Start infill");

	parallelize<Layer*>(
		std::queue<Layer*>(std::deque<Layer*>(this->layers.begin(), this->layers.end())),  // cast LayerPtrs to std::queue<Layer*>
		boost::bind(&Slic3r::Layer::make_fills, _1),
		this->_print->config.threads.value
	);
	
	/*  we could free memory now, but this would make this step not idempotent
	### $_->fill_surfaces->clear for map @{$_->regions}, @{$object->layers};
	*/
	
	this->state.set_done(posInfill);
}

}



/*
 *	以下为用户自己添加的函数，在原项目中是以perl代码形式存在的
 */


/*
 *	对三维模型进行切分
 *  对于有切分缺陷的部分，使用其上方和下方的切分区域对其进行替换
 *  删除为空的层
 */
void PrintObject::Slice() {
	if (state.is_done(posSlice)) return;

	state.set_started(posSlice);

	//main_statusbar_->showMessage("Start slicing model");

	//对打印对象进行切分
	_slice();

	//遍历所有切分后的层，如果某一层存在slicing error，则使用其上一层和下一层的切分区域进行替换
	// 检测slicing errors
	for (auto layer_id = 0; layer_id < layer_count(); layer_id++) {
		Layer* layer = get_layer(layer_id);

		if (!layer->slicing_errors) continue;

		for (auto region_id = 0; region_id < layer->region_count(); region_id++) {
			LayerRegion* region = layer->regions[region_id];

			SurfaceCollection upper_surfaces;
			SurfaceCollection lower_surfaces;

			//获取其上方没有slicing error的layer的surfaces
			for (auto upper_layer_id = layer_id + 1; upper_layer_id < layer_count(); upper_layer_id++) {
				if (!get_layer(upper_layer_id)->slicing_errors) {
					upper_surfaces = get_layer(upper_layer_id)->regions[region_id]->slices;
					break;
				}
			}

			//获取下方没有slicing error的layer的surfaces
			for (auto lower_layer_id = layer_id - 1; lower_layer_id >= 0; lower_layer_id--) {
				if (!get_layer(lower_layer_id)->slicing_errors) {
					lower_surfaces = get_layer(lower_layer_id)->regions[region_id]->slices;
					break;
				}
			}

			Surfaces source_surfaces(upper_surfaces.surfaces.begin(), upper_surfaces.surfaces.end());
			source_surfaces.insert(source_surfaces.end(), lower_surfaces.surfaces.begin(), lower_surfaces.surfaces.end());
			//std::merge(upper_slices.surfaces.begin(), upper_slices.surfaces.end(),
// 						lower_slices.surfaces.begin(), lower_slices.surfaces.end(),
// 						back_inserter(source_surfaces));

			
			//对所有的contour进行union,然后再对holes进行difference，即得到该层的slices
			Polygons source_contours;
			for (auto& surface : source_surfaces) {
				//source_contours.insert(source_contours.end(), surface.expolygon.contour);
				source_contours.push_back(surface.expolygon.contour);
			}
			ExPolygons union_contours = union_ex(source_contours);

			
			Polygons source_holes;
			for (auto& surface : source_surfaces) {
				source_holes.insert(source_holes.end(), surface.expolygon.holes.begin(), surface.expolygon.holes.end());
			}
			ExPolygons diff_polygons = diff_ex(to_polygons(union_contours), source_holes);

			region->slices.clear();
			region->slices.append(diff_polygons, stInternal);
		}
		layer->make_slices();
	}


	//如果某一层没有切分区域，则将其移除，并修改其他层的id
	while (!layers.empty() && get_layer(0)->slices.expolygons.size() == 0) {
		delete_layer(0);
		for (auto i = 0; i < layer_count(); i++) {
			//get_layer(i)->set_id(get_layer(i)->id() - 1);
			int temp_id = get_layer(i)->id() - 1;
			get_layer(i)->set_id(temp_id);
		}
	}

	//如果需要，则对slices进行simplify
	if (_print->config.resolution) {
		SimplySlices(scale_(print()->config.resolution));
	}
	if (layers.empty()) {
		qDebug() << "No layers were detected";
	}

	//当layer->region被分类为top/bottom/internal时，typed_slices为真
	//因此下一次再调用make_perimters的话会在计算loops之前进行union操作
	typed_slices = false;
	state.set_done(posSlice);
}


/*
 *	为打印对象生成Perimeters结构
 */
void PrintObject::MakePerimeters() {
	if (state.is_done(posPerimeters)) return;

	if (typed_slices) {
		invalidate_step(posSlice);
	}

	Slice();
	_make_perimeters();
}


/*
 *	将LayerRegion中的每一个Slice都分类为top/internal/bottom
 *  同时还将LayRegion中的每一个填充区域从expolygon转换为top/internal/bottom类型的surfaces collection
 */
void PrintObject::DetectSurfaceType() {
	Slice();

	detect_surfaces_type();
}


/*
 *	对打印对象进行填充，首先要进行填充前的准备工作
 */
void PrintObject::Infill() {
	//预处理
	PrepareInfill();

	//进行填充
	_infill();
}


/*
 *	准备进行填充工作
 */
void PrintObject::PrepareInfill() {
	if (state.is_done(posPrepareInfill)) { return; }

	
	invalidate_step(posPerimeters);
	MakePerimeters();

	//设置打印状态
	state.set_started(posPrepareInfill);
	//main_statusbar_->showMessage("Start preparing infill");

	//将所有layer region上的surface分类为top/internal/bottom
	//根据某一曾上方或下方是否存在别的层进行判断
	DetectSurfaceType();

	//根据配置判断是否需要：
	//1. 将bottom/top转换为internal
	//2. 将thin internal转换为internal solid
	for (auto& layer : layers) {
		for (auto& layer_region : layer->regions) {
			layer_region->prepare_fill_surfaces();
		}
	}

	//使用BridgeDetector检测Bridges结构以及unsupported bridge region
	//而且重新定义top/internal/bottom区域，在最下面一层可能也会有top区域
	process_external_surfaces();

	//将top/bottom/bridge附近的Internal转换为Internal solid,保证了在solid层中，
	//solid下方的internal转变为internal solid
	DiscoverHorizontalShells();

	//将solid区域（bottom/top/perimeters）下方的internal转换为internal solid
	ClipFillSurfaces();

	//检测该层为internal solid而其下方的层为internal类型的区域
	//将其一部分由internal solid转换为internal bridge,确保internal solid不会塌陷
	bridge_over_infill();

	//将不同层的填充区域根据配置参数进行合并
	CombineInfill();

	state.set_done(posPrepareInfill);
}

/*
 *	检测external layers附近的fill surfaces
 *  将其由internal重新分类为internal 和 internal solid
 */
void PrintObject::DiscoverHorizontalShells() {
	for (auto region_id = 0; region_id < _print->regions.size(); region_id++) {
		for (auto layer_id = 0; layer_id < layer_count(); layer_id++) {
			LayerRegion* layer_region = get_layer(layer_id)->get_region(region_id);

			if (layer_region->region()->config.solid_infill_every_layers
				&& layer_region->region()->config.fill_density > 0
				&& (layer_id % layer_region->region()->config.bottom_solid_layers) == 0) {
			
				SurfaceType type = layer_region->region()->config.fill_density == 100 ?
					stInternalSolid : stInternalBridge;
				
				SurfacesPtr internal_surfaces;
				internal_surfaces = layer_region->fill_surfaces.filter_by_type(stInternal);
				for (Surface* surface : internal_surfaces) {
					surface->surface_type = type;
				}
			}


			// 在current layer上查找特定类型的slices
			// 不适用fill_slices而使用slices的原因是后者同时包含了perimeter area，而这部分也将被扩充到shell
			for (SurfaceType type : {stTop, stBottom, stBottomBridge}) {
				//当前layer的current solid 区域，同时包含slices内的solid区域和solid infill区域
				Polygons solid_polygons;
				SurfacesPtr solid_slices = layer_region->slices.filter_by_type(type);
// 				for (Surface* surface : solid_slices) {
// 					//solid_polygons.push_back(surface->operator Slic3r::Polygons);
// 					solid_polygons.insert(solid_polygons.end(),
// 						surface->operator Slic3r::Polygons().begin(), surface->operator Slic3r::Polygons().end());
// 				}
				for (Surface* surface : solid_slices) {
					solid_polygons.push_back(surface->expolygon.contour);
					for (auto& hole : surface->expolygon.holes) {
						solid_polygons.push_back(hole);
					}
				}


				SurfacesPtr solid_fill_surfaces = layer_region->fill_surfaces.filter_by_type(type);
// 				for (Surface* surface : solid_fill_surfaces) {
// 					//solid_polygons.push_back(surface->operator Slic3r::Polygons);
// 					solid_polygons.insert(solid_polygons.end(), 
// 						surface->operator Slic3r::Polygons().begin(), surface->operator Slic3r::Polygons().end());
// 				}
				for (Surface* surface : solid_fill_surfaces) {
					solid_polygons.push_back(surface->expolygon.contour);
					for (auto& hole : surface->expolygon.holes) {
						solid_polygons.push_back(hole);
					}
				}
				
				//如果当前层没有solid区域的话，则跳到下一个类型
				if (solid_polygons.empty()) { continue; }
				qDebug() << "Layer " << layer_id << " has " << (type == stTop ? "top" : "bottom") << " surfaces";

				//当前层所处的solid层数
				int solid_count = (type == stTop) ?
					layer_region->region()->config.top_solid_layers :
					layer_region->region()->config.bottom_solid_layers;

				//neighbor_id表示与当前layer处于同一个solid 的layer
				for (auto neighbor_id = (type == stTop) ? layer_id - 1 : layer_id + 1;
					std::abs(neighbor_id - layer_id) <= solid_count - 1;
					(type == stTop) ? neighbor_id-- : neighbor_id++) {
					
					if(neighbor_id < 0 || neighbor_id >= layer_count()) continue;
					qDebug() << "Looking for neighbirs on layer " << neighbor_id;

					//neighbor layer上处于同一个layer region的部分
					LayerRegion* neighbor_layer_region = get_layer(neighbor_id)->regions[region_id];
					SurfaceCollection neighbor_fill_surfaces = neighbor_layer_region->fill_surfaces;

					//neighbor层上的internal 和internal solid填充区域
					Polygons neighbor_polygons;
					for (Surface& surface : neighbor_fill_surfaces.surfaces) {
						if (surface.surface_type == stInternal || surface.surface_type == stInternalSolid) {
							//neighbor_polygons.push_back(surface.operator Slic3r::Polygons);
// 							neighbor_polygons.insert(neighbor_polygons.end(),
// 								surface.operator Slic3r::Polygons().begin(), surface.operator Slic3r::Polygons().end());
							neighbor_polygons.push_back(surface.expolygon.contour);
							for (auto& hole : surface.expolygon.holes) {
								neighbor_polygons.push_back(hole);
							}
						}
					}

					// 计算current layer上的solid区域，与 neighbor layer上的internal & internal solid区域的Intersection
					// 即为当前层上的new internal solid区域，同时这一部分也是neighbor layer上的new internal solid部分
					Polygons new_internal_solid = intersection(solid_polygons, neighbor_polygons,1);

					// 如果这一层上不需要internal solid的话，需要根据用户设置的参数判读是否需要继续在查找neighbor layer
					if (new_internal_solid.empty()) {
						// 如果用户希望object是中空的，则不再搜索neighbor layer，此时只生产external solid shell，
						// 从而会导致打印出的物体中perimeter之间有hole，internal solid shells都是previous layer上
						// shell的子集
						if (layer_region->region()->config.fill_density == 0) {
							break;
						}
						else {
							//如果需要internal infill, 则可以自由得生成所需要的internal solid shell
							continue;
						}
					}

					// 如果打印的是一个中空物体的话，则需要丢弃任何比perimeter更thin的solid shell
					if (layer_region->region()->config.fill_density == 0) {
						
						// solid shell的厚度临界值
						coord_t margin = neighbor_layer_region->flow(frExternalPerimeter).scaled_width();

						Polygons offseted = offset2(new_internal_solid, -margin, +margin,CLIPPER_OFFSET_SCALE, jtMiter, 5);
						
						// 计算得到的厚度过小的区域
						Polygons too_narrow = diff(new_internal_solid, offseted, 1);

						// 如果存在厚度过小的情况的话，就需要从current layer的solid区域剔除这一部分
						// 同时要更新当前layer上的internal solid区域
						if (!too_narrow.empty()) {
							new_internal_solid = diff(new_internal_solid, too_narrow);
							solid_polygons = new_internal_solid;
						}
					}


					// 由于internal solid区域可能会被collapsed，因此要确保current layer上的new internal solid区域足够宽
					{
						// new internal solid区域的宽度边界，用于计算too narrow的区域
						coord_t margin = 3 * layer_region->flow(frSolidInfill).scaled_width();

						Polygons offseted = offset2(new_internal_solid, -margin, +margin, CLIPPER_OFFSET_SCALE, jtMiter, 5);
						Polygons too_narrow = diff(new_internal_solid, offseted, 1);

						// grow collapsing部分，并在neighbor layer和original layer上添加一部分区域，
						// 从而可以保证下一层的shell也能得到支撑
						if (!too_narrow.empty()) {

							// 将too_narrow区域(即collapsing区域)向外扩展，并与neighbor layer上的internal & bridge区域做intersection
							// 从而得到需要进行grow的区域，并将其添加到new internal solid部分
							Polygons internal_bridge_polygons;
							for (Surface& surface : neighbor_fill_surfaces.surfaces) {
								if (surface.is_internal() || surface.is_bridge()) {
									//internal_bridge_polygons.push_back(surface.operator Slic3r::Polygons);
// 									internal_bridge_polygons.insert(internal_bridge_polygons.end(),
// 										surface.operator Slic3r::Polygons().begin(), surface.operator Slic3r::Polygons().end());
									internal_bridge_polygons.push_back(surface.expolygon.contour);
									for (auto& hole : surface.expolygon.holes) {
										internal_bridge_polygons.push_back(hole);
									}
								}
							}

							Polygons offseted_margin = offset(too_narrow, +margin);
							Polygons grown = intersection(offseted_margin, internal_bridge_polygons);

							new_internal_solid.insert(new_internal_solid.begin(), grown.begin(), grown.end());
							solid_polygons = new_internal_solid;
						}

					}

					// 将neighbor层上的internal solid区域合并到当前层上的new internal solid区域
					// 作为neighbor 层上的internal solid区域
					Polygons neighbor_internal_solid;
					for (Surface& surface : neighbor_fill_surfaces.surfaces) {
						if (surface.surface_type == stInternalSolid) {
							//neighbor_internal_solid.push_back(surface.operator Slic3r::Polygons);
// 							neighbor_internal_solid.insert(neighbor_internal_solid.end(),
// 								surface.operator Slic3r::Polygons().begin(), surface.operator Slic3r::Polygons().end());
							neighbor_internal_solid.push_back(surface.expolygon.contour);
							for (auto& hole : surface.expolygon.holes) {
								neighbor_internal_solid.push_back(hole);
							}
						}
					}
					neighbor_internal_solid.insert(neighbor_internal_solid.end(), 
						new_internal_solid.begin(), new_internal_solid.end());
					ExPolygons internal_solid = union_ex(neighbor_internal_solid);

					// 此时neighbor layer上的internal区域即为原本的internal 区域减去internal solid区域
					ExPolygons neighbor_internal;
					for (Surface& surface : neighbor_fill_surfaces.surfaces) {
						if (surface.surface_type == stInternal) {
							neighbor_internal.push_back(surface.expolygon);
						}
					}
					ExPolygons internal = diff_ex(neighbor_internal, internal_solid, 1);

					//将neighbor fill surfaces清空，并将internal和internal solid区域添加进去
					neighbor_fill_surfaces.clear();

					neighbor_fill_surfaces.append(internal, stInternal);
					neighbor_fill_surfaces.append(internal_solid, stInternalSolid);

					// 将neighbor layer上的top surfaces和bottom surfaces都添加进去
					Surfaces top_bottom_surfaces;
					for (Surface& surface : neighbor_layer_region->fill_surfaces.surfaces) {
						if (surface.surface_type == stTop || surface.is_bottom()) {
							top_bottom_surfaces.push_back(surface);
						}
					}

					// 先对所有的top/bottom surfaces进行分组，然后再分别将每一组合并之后，再减去internal & internal solid区域，
					// 剩余的部分就是新的top/bottom surfaces
					SurfaceCollection top_bottom_surface_collection(top_bottom_surfaces);
					std::vector<SurfacesConstPtr> grouped_surfaces;
					top_bottom_surface_collection.group(&grouped_surfaces);
					for (SurfacesConstPtr surfaces : grouped_surfaces) {
						ExPolygons all_surface_expolygons;
						for (auto& surface : surfaces) {all_surface_expolygons.push_back(surface->expolygon);}

// 						ExPolygons internal_intersolid_expolygons;
// 						std::merge(internal.begin(), internal.end(),
// 							internal_solid.begin(), internal_solid.end(),
// 							back_inserter(internal_intersolid_expolygons));

						ExPolygons internal_intersolid_expolygons(internal.begin(), internal.end());
						internal_intersolid_expolygons.insert(internal_intersolid_expolygons.end(), 
							internal_solid.begin(), internal_solid.end());

						ExPolygons result_solid_expolygons = diff_ex(all_surface_expolygons, internal_intersolid_expolygons, 1);
						
						neighbor_fill_surfaces.append(result_solid_expolygons, surfaces[0]->surface_type);
					}

					//用计算出的新的neighbor layer上的fill surfaces替换
					neighbor_layer_region->fill_surfaces = neighbor_fill_surfaces;
				}
				
			}
		}
	}
}


/*
 *	将internal solid（包括perimeter）下一层的internal和internal void surfaces
 *  转换为internal surfaces
 */
void PrintObject::ClipFillSurfaces() {
	bool precodition = config.infill_only_where_needed;
	if (!precodition) return;
	for (PrintRegion* region : _print->regions) {
		precodition = precodition || region->config.fill_density > 0;
	}
	if (!precodition) return;

	//由上向下进行处理，忽视最底层
	Polygons upper_internal;
	for (int layer_id = layer_count() - 1; layer_id >= 1; layer_id--) {
		Layer* cur_layer = get_layer(layer_id);
		Layer* lower_layer = get_layer(layer_id - 1);

		//检测overhang区域，即solid infill surfaces
		Polygons overhangs;
		for (LayerRegion* region : cur_layer->regions) {
			for (Surface& surface : region->fill_surfaces.surfaces) {
				if (surface.is_solid()) {
					//overhangs.push_back(surface.operator Slic3r::Polygons);
// 					overhangs.insert(overhangs.end(),
// 						surface.operator Slic3r::Polygons().begin(),
// 						surface.operator Slic3r::Polygons().end());
					overhangs.push_back(surface.expolygon.contour);
					for (auto& hole : surface.expolygon.holes) {
						overhangs.push_back(hole);
					}
				}
			}
		}

		// 如果存在一个没有被支撑perimeter loop的话，同样需要对其进行支撑
		{

			// 当前层的perimeters为slices surfaces - fill surfaces
// 			Polygons cur_slices;
// 			cur_slices = cur_layer->slices.operator Slic3r::Polygons();
// 			Polygons cur_fill;
// // 			for (LayerRegion* region : cur_layer->regions) {
// // 				//cur_fill.push_back(region->fill_surfaces.operator Slic3r::Polygons);
// // // 				cur_fill.insert(cur_fill.end(),
// // // 					region->fill_surfaces.operator Slic3r::Polygons().begin(), 
// // // 					region->fill_surfaces.operator Slic3r::Polygons().end());
// // 				cur_fill.push_back(region->fill_surfaces.)
// // 			}
			Polygons cur_slices;
			for (ExPolygon& ex:cur_layer->slices.expolygons) {
				cur_slices.push_back(ex.contour);
				for (auto& hole : ex.holes) {
					cur_slices.push_back(hole);
				}
			}

			Polygons cur_fill;
			for (LayerRegion* region : cur_layer->regions) {
				for (Surface& surface : region->fill_surfaces.surfaces) {
					cur_fill.push_back(surface.expolygon.contour);
					for (auto& hole : surface.expolygon.holes) {
						cur_fill.push_back(hole);
					}
				}
			}


			Polygons cur_perimeter = diff(cur_slices, cur_fill);

			// 再计算cur layer的perimeter与下一层的infill surface之间的diff
			// 即在cur layer的perimeter中减去被下一层的perimeter支撑的部分
			Polygons lower_infill;
// 			for (LayerRegion* region : lower_layer->regions) {
// 				//lower_infill.push_back(region->fill_surfaces.operator Slic3r::Polygons);
// 				lower_infill.insert(lower_infill.end(),
// 					region->fill_surfaces.operator Slic3r::Polygons().begin(),
// 					region->fill_surfaces.operator Slic3r::Polygons().end());
// 			}
			for (LayerRegion* region : lower_layer->regions) {
				for (Surface& surface : region->fill_surfaces.surfaces) {
					lower_infill.push_back(surface.expolygon.contour);
					for (auto& hole : surface.expolygon.holes) {
						lower_infill.push_back(hole);
					}
				}
			}


			cur_perimeter = intersection(cur_perimeter, lower_infill, 1);

			// 只考虑那些宽度超过extrusion width的perimeter
			double width_threshold = 0;
			for (LayerRegion* region : cur_layer->regions) {
				width_threshold = std::min(width_threshold, (double)region->flow(frPerimeter).scaled_width());
			}
			cur_perimeter = offset2(cur_perimeter, -width_threshold, +width_threshold);

			// 将perimeters中没有被低层perimeter支撑的部分添加到overhang中
			overhangs.insert(overhangs.end(), cur_perimeter.begin(), cur_perimeter.end());
		}


		// 计算lower layer的new internal区域
		// 将cur layer的需要支撑的区域（包括overhang和其上方的internal区域）
		// 与lower layer的internal和internal void区域做intersection
		
		// 将当前层的overhang与上一层的internal合并
// 		Polygons cur_overhangs_upperinternal;
// 		std::merge(overhangs.begin(), overhangs.end(),
// 			upper_internal.begin(), upper_internal.end(),
// 			back_inserter(cur_overhangs_upperinternal));

		Polygons cur_overhangs_upperinternal(overhangs.begin(), overhangs.end());
		cur_overhangs_upperinternal.insert(cur_overhangs_upperinternal.end(),
			upper_internal.begin(), upper_internal.end());

		// 下一层的internal和internal void区域
		Polygons lower_internal_internalvoid;
		for (LayerRegion* region : lower_layer->regions) {
			for (Surface& surface : region->fill_surfaces.surfaces) {
				if (surface.surface_type == stInternal || surface.surface_type == stInternalVoid) {
					//lower_internal_internalvoid.push_back(surface.operator Slic3r::Polygons);
// 					lower_internal_internalvoid.insert(lower_internal_internalvoid.end(), 
// 						surface.operator Slic3r::Polygons().begin(),
// 						surface.operator Slic3r::Polygons().end() );
					lower_internal_internalvoid.push_back(surface.expolygon.contour);
					for (auto& hole : surface.expolygon.holes) {
						lower_internal_internalvoid.push_back(hole);
					}
				}
			}
		}

		//二者之间的intersection即为新的internal区域
		Polygons new_internal = intersection(cur_overhangs_upperinternal, lower_internal_internalvoid);
		upper_internal = new_internal;		//更新upper internal


		// 将lower layer的new internal区域应用到每一个lower layer的每一个layer region上
		// 方法是将new internal与每一个layer region的internal & internal void区域做intersection作为新的internal
		// 而new internal与每一个layer region的internal & internal void区域的difference作为新的internal void
		for (LayerRegion* layer_region : lower_layer->regions) {
			if (layer_region->region()->config.fill_density == 0) return;

			// lower layer上的internal surfaces和其他的surfaces
			Polygons lower_internal_polygons;
			Surfaces lower_other_surfaces;

			for (Surface surface : layer_region->fill_surfaces.surfaces) {
				if (surface.surface_type == stInternal || surface.surface_type == stInternalVoid) {
					//lower_internal_polygons.push_back(surface.operator Slic3r::Polygons);
// 					lower_internal_polygons.insert(lower_internal_polygons.end(),
// 						surface.operator Slic3r::Polygons().begin(), surface.operator Slic3r::Polygons().end() );
					lower_internal_polygons.push_back(surface.expolygon.contour);
					for (auto& hole : surface.expolygon.holes) {
						lower_internal_polygons.push_back(hole);
					}
				}
				else {
					lower_other_surfaces.push_back(surface);
				}
			}

			// 将new internal与internal & internal void区域的intersection作为新的internal区域
			Surfaces lower_internal_surfaces;
			ExPolygons added_lower_internal_expolygons = intersection_ex(lower_internal_polygons, new_internal, 1);
			for (ExPolygon& expolygon : added_lower_internal_expolygons) {
				lower_internal_surfaces.push_back(Surface(stInternal,expolygon));
			}

			// 将new internal与internal & internal void区域的difference作为internal void区域
			ExPolygons added_lower_other_expolygons = diff_ex(lower_internal_polygons, new_internal, 1);
			for (ExPolygon& expolygon : added_lower_other_expolygons) {
				lower_other_surfaces.push_back(Surface(stInternalVoid, expolygon));
			}

			//将重新计算得到的internal和other fill surfaces添加到lower layer的fill surfaces中
			layer_region->fill_surfaces.clear();
			layer_region->fill_surfaces.append(lower_internal_surfaces);
			layer_region->fill_surfaces.append(lower_other_surfaces);
		}



	}
}



/*
 *	根据用户设置参数合并不同layer之间的fill surfaces
 */
void PrintObject::CombineInfill() {


	//对每一个Print Region分别进行操作，对应的是每一个Print Region
	for (int region_id = 0; region_id < _print->regions.size(); region_id++) {
		PrintRegion* print_region = _print->get_region(region_id);
		int infills_every_count = print_region->config.infill_every_layers;
		if (!(infills_every_count > 1 && print_region->config.fill_density > 0))
			continue;


		//限制合并后的Layer的最大高度为用户设置的nozzle所允许的最大高度
		double nozzle_diameter = std::min(
				_print->config.nozzle_diameter.get_at(print_region->config.infill_extruder - 1),
				_print->config.nozzle_diameter.get_at(print_region->config.solid_infill_extruder - 1)
			);
		
		// 根据合并后所允许的最大高度计算层之间的合并信息
		// combine<int, int>为<layer_id，layer_id下方要合并的层数>
		std::map<int, int> combine;
		{
			double current_height = 0;
			int layers = 0;
			
			//从layer id = 1开始，因为layer id = 0时下方并没有layer，所以也就不用进行合并
			for (int layer_id = 1; layer_id < layer_count(); layer_id++) {
				Layer* layer = get_layer(layer_id);

				double height = layer->height;
				// 合并后的层高不会超过max_layer_height，而且合并的层数不会超过max combined layer count
				if (current_height + height >= nozzle_diameter + EPSILON || layers >= infills_every_count) {
					combine[layer_id - 1] = layers;
					current_height = 0;
					layers = 0;
				}
				current_height += height;
				layers++;
			}
			//添加最上面一层的合并信息
			combine[layer_count() - 1] = layers;
		}


		// 遍历所有要进行合并的Layer
		for (auto layer_combine : combine) {
			int layer_id = layer_combine.first;
			if (layer_id <= 1)
				return;
			
			//获取所有要进行combine的layer region，这些layer region来自所有要合并的layer
			LayerRegionPtrs layer_regions;
			for (int id = layer_id - combine[layer_id] + 1; id <= layer_id; id++) {
				layer_regions.push_back(get_layer(id)->get_region(region_id));
			}

			//只合并internal infill部分
			for (SurfaceType type : {stInternal}) {
				
				SurfacesPtr internal_fill_surfaces = layer_regions[0]->fill_surfaces.filter_by_type(type);
				
				//使用lowest layer来初始化intersection
				ExPolygons intersection;
				for (Surface* surface : internal_fill_surfaces) {
					intersection.push_back(surface->expolygon);
				}

				//从第二层开始，将其fill surfaces与intersection进行intersection操作
				for (int id = 1; id < layer_regions.size(); id++) {
					SurfacesPtr temp_fill_surfaces = layer_regions[id]->fill_surfaces.filter_by_type(type);
					ExPolygons temp_intersection;
					for (Surface* surface : temp_fill_surfaces) {
						temp_intersection.push_back(surface->expolygon);
					}

					intersection = intersection_ex(intersection, temp_intersection);
				}

				//在intersection中所有area小于临界值的部分都将被剔除
				double area_threshold = layer_regions[0]->infill_area_threshold();
				for (auto iterator = intersection.begin(); iterator != intersection.end();) {
					if (iterator->area() < area_threshold) {
						intersection.erase(iterator);
					}
					else {
						++iterator;
					}
				}

				// 如果intersection为空，则进行下一次循环，即没有合格的intersection
				if (intersection.empty()) {
					continue;
				}

				// intersection包含可能被所有的layers合并的region，所以需要从所有的layers中移除这一部分
				// layer_regions[-1] ???
				std::vector<InfillPattern> patterns{ipRectilinear, ipGrid, ipHoneycomb};
				ExPolygons intersection_with_clearance = offset_ex(intersection,
					layer_regions[0]->flow(frInfill).scaled_width() / 2 +
					layer_regions[0]->flow(frPerimeter).scaled_width() / 2 +
					( (type == stInternalSolid) ||
					  (std::find(patterns.begin(), patterns.end(), print_region->config.fill_pattern)!= patterns.end())
					) ?
					layer_regions[0]->flow(frSolidInfill).scaled_width() : 0
					// 由于在稍后的步骤中rectlinear和honeycomb的填充区域将会grow，并与perimeters进行overlap，所以需要抵消这一部分
				);
				

				//当前合并区域的层高
				double sum_height = 0;
				for (LayerRegion* region : layer_regions) {
					sum_height += region->layer()->height;
				}

				// 对所有的layer region进行操作
				for (LayerRegion* layer_region : layer_regions) {
					ExPolygons this_type_expolygons;
					Surfaces other_type_surfaces;
					for (Surface& surface : layer_region->fill_surfaces.surfaces) {
						if (surface.surface_type != type) {
							this_type_expolygons.push_back(surface.expolygon);
						}
						else {
							other_type_surfaces.push_back(surface);
						}
					}

					//将internal infill与intersection with clearance做difference，得到new this type expolygons
					ExPolygons new_this_type_expolygons = diff_ex(this_type_expolygons, intersection_with_clearance);
					
					//将internal infill减去intersection with clearance后剩下的部分作为新的internal infill
					Surfaces new_this_type_surfaces;
					for (auto& expolygon : new_this_type_expolygons) {
						new_this_type_surfaces.push_back(Surface(type, expolygon));
					}

				
					// 将调整过高度之后的surfaces添加的最上层的layer
					if (layer_region->layer()->id() == get_layer(layer_id)->id()) {
						for (ExPolygon& expolygon : intersection) {
							Surface new_surface(type, expolygon);
							new_surface.thickness = sum_height;
							new_surface.thickness_layers = layer_regions.size();
							new_this_type_surfaces.push_back(new_surface);
						}
					}
					else {		//对于其他层则为void层
						// this type expolygons与intersection with clearance的intersection作为internal void区域
						// 原因是该区域都被转换到顶层的高度上了
						ExPolygons void_expolygons = intersection_ex(this_type_expolygons, intersection_with_clearance);
						for (ExPolygon& expolygon : void_expolygons) {
							new_this_type_surfaces.push_back(Surface(stInternalVoid, expolygon));
						}
					}

					// 将计算后的fill surfaces添加到该layer region上
					layer_region->fill_surfaces.clear();
					layer_region->fill_surfaces.append(new_this_type_surfaces);
					layer_region->fill_surfaces.append(other_type_surfaces);
				}
			}

		}

	}
}


/*
 *	对Slices进行简化，只有在需要的时候才进行
 */
void PrintObject::SimplySlices(double distance) {
	for (auto& layer : layers) {
		layer->slices.simplify(distance);
		for (auto& region : layer->regions) {
			region->slices.simplify(distance);
		}
	}
}


/*
 *	生成支撑结构Supppoer Material
 */
void PrintObject::GenerateSupportMaterial() {
	//预处理
	Slice();

	if (state.is_done(posSupportMaterial))
		return;

	state.set_started(posSupportMaterial);

	clear_support_layers();

	if ((!config.support_material && config.raft_layers == 0) ||
		(layer_count() < 2)) {
	
		state.set_done(posSupportMaterial);
		return;
	}
}


/*
 *	设置主窗口的状态栏
 */
void PrintObject::SetStatusbar(QStatusBar* statusbar) {
	main_statusbar_ = statusbar;
}