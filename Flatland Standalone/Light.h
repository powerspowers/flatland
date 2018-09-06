//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

// The limit on the number of active lights allowed.

#define ACTIVE_LIGHTS_LIMIT	8

// Maximum active lights.

extern int max_active_lights;

// Externally visible functions.

void
get_ambient_light(float *brightness, RGBcolour *colour);

void
set_ambient_light(float brightness, RGBcolour colour);

void
set_master_intensity(float brightness);

void
set_active_lights(light_ref *active_light_list, vertex *vertex_ptr);

void
compute_vertex_colour(light_ref *active_light_list, vertex *vertex_ptr, 
					  vector *normal_ptr, RGBcolour *colour_ptr);

float
compute_vertex_brightness(light_ref *active_light_list, vertex *vertex_ptr, 
						  vector *normal_ptr);
