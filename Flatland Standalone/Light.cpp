//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "Classes.h"
#include "Fileio.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Spans.h"
#include "Utils.h"

// Some useful constants.

#define	ONE_OVER_765		0.001307189f

// Maximum active lights.

int max_active_lights;

// Ambient brightness and unlit ambient colour.

static float ambient_brightness;
static RGBcolour unlit_ambient_colour;

// Ambient colour, and master intensity and colour.

static float ambient_red, ambient_green, ambient_blue;
static float master_intensity;
static float master_red, master_green, master_blue;

// Distance list used to determine closest lights.

static float distance_list[ACTIVE_LIGHTS_LIMIT];

//------------------------------------------------------------------------------
// Get the ambient light.
//------------------------------------------------------------------------------

void
get_ambient_light(float *brightness, RGBcolour *colour)
{
	*brightness = ambient_brightness;
	*colour = unlit_ambient_colour;
}

//------------------------------------------------------------------------------
// Set the ambient light.
//------------------------------------------------------------------------------

void
set_ambient_light(float brightness, RGBcolour colour)
{
	ambient_brightness = brightness;
	unlit_ambient_colour = colour;
	ambient_red = colour.red * brightness;
	ambient_green = colour.green * brightness;
	ambient_blue = colour.blue * brightness;
}

//------------------------------------------------------------------------------
// Set the master intensity and colour (brightness must be between -1 and 1).
//------------------------------------------------------------------------------

void
set_master_intensity(float brightness)
{
	master_intensity = brightness;
	master_red = 255.0f * brightness;
	master_green = 255.0f * brightness;
	master_blue = 255.0f * brightness;
}

//------------------------------------------------------------------------------
// Search a given list of lights for the closest to the given vertex.
//------------------------------------------------------------------------------

void
find_closest_lights_in_light_list(light_ref *active_light_list, 
								  light *light_list, vertex *translation_ptr, 
								  vertex *vertex_ptr)
{
	light *light_ptr;
	int	j, k;
	bool done;
	vertex light_pos;
	vector rel_pos;
	float distance;

	light_ptr = light_list;
	while (light_ptr != NULL) {

		// Compute the distance from the light to the vertex.  If a translation
		// has been supplied, add it to the light's position, otherwise use the
		// map coordinates in the light itself.

		light_pos = light_ptr->position;
		if (translation_ptr != NULL)
			light_pos = light_pos + *translation_ptr;
		else {
			vertex translation;
			translation.set_map_translation(light_ptr->map_coords.column, light_ptr->map_coords.row, light_ptr->map_coords.level);
			light_pos = light_pos + translation;
		}
		rel_pos = light_pos - *vertex_ptr;
		distance = (rel_pos.dx * rel_pos.dx) + (rel_pos.dy * rel_pos.dy) + 
			(rel_pos.dz * rel_pos.dz);

		// If this light is closer than any in the list, insert it into the
		// list.  The list is kept sorted from nearest to furthest.

		j = 0;
		done = false;
		while (j < max_active_lights && !done) {
			if (distance_list[j] < 0.0 || distance < distance_list[j]) {
				if (j < max_active_lights - 1) {
					for (k = max_active_lights - 1; k > j; k--) {
						distance_list[k] = distance_list[k - 1];
						active_light_list[k] = active_light_list[k - 1];
					}
				}
				distance_list[j] = distance;
				active_light_list[j].light_ptr = light_ptr;
				active_light_list[j].light_pos = light_pos;
				done = true;
			}
			j++;
		}

		// Check the next light.

		light_ptr = light_ptr->next_light_ptr;
	}
}

//------------------------------------------------------------------------------
// Search a given list of blocks for the closest lights to the given vertex.
//------------------------------------------------------------------------------

void
find_closest_lights_in_block_list(light_ref *active_light_list, 
								  block *block_list, vertex *vertex_ptr)
{
	block *block_ptr = block_list;
	while (block_ptr != NULL) {
		if (block_ptr->light_list != NULL)
			find_closest_lights_in_light_list(active_light_list,
				block_ptr->light_list, &block_ptr->translation, vertex_ptr);
		block_ptr = block_ptr->next_block_ptr;
	}
}

//------------------------------------------------------------------------------
// Determine the active lights for the given block, which are the closest
// lights to the given vertex.
//------------------------------------------------------------------------------

void
set_active_lights(light_ref *active_light_list, vertex *vertex_ptr)
{
	int index;
	vertex translation;

	// Initialise the closest light list, and the distance list.

	for (index = 0; index < max_active_lights; index++) {
		active_light_list[index].light_ptr = NULL;
		distance_list[index] = -1.0f;
	}

	// Search through the global list of lights, as well as any lights in the
	// movable and fixed block list, and remember at most the three closest.

	find_closest_lights_in_light_list(active_light_list, global_light_list, 
		NULL, vertex_ptr);
	find_closest_lights_in_block_list(active_light_list, movable_block_list, 
		vertex_ptr);
	find_closest_lights_in_block_list(active_light_list, fixed_block_list, 
		vertex_ptr);
}

//------------------------------------------------------------------------------
// Compute the lit colour of a vertex using the given active light list.
//------------------------------------------------------------------------------

void
compute_vertex_colour(light_ref *active_light_list, vertex *vertex_ptr, 
					  vector *normal_ptr, RGBcolour *colour_ptr)
{
	int index;
	light *light_ptr;
	vector light_vector;
	float red, green, blue;
	float dot, dot2, distance;
	float intensity;

	// Initialise the additive polygon colour to be the ambient colour.

	red = ambient_red;
	green = ambient_green;
	blue = ambient_blue;

	// If an orb light has been defined, add it to the polygon colour.
	// The intensity of a directional light is based upon the cosine of
	// the angle between the light ray and the normal vector of the
	// polygon.  If the polygon is facing away from the light, it is
	// not affected.

	if (orb_light_ptr != NULL) {
		dot = -(orb_light_ptr->dir & *normal_ptr);
		if (FGT(dot, 0.0)) {
			red += orb_light_ptr->lit_colour.red * dot;
			green += orb_light_ptr->lit_colour.green * dot;
			blue += orb_light_ptr->lit_colour.blue * dot;
		}
	}

	// Now step through the list of active lights.

	for (index = 0; index < max_active_lights; index++) {
 		light_ptr = active_light_list[index].light_ptr;
		if (light_ptr == NULL)
			continue;

		// Work out the light intensity based upon it's style.

		switch(light_ptr->style) {
		case STATIC_POINT_LIGHT:
		case PULSATING_POINT_LIGHT:

			// The intensity of a point light is based upon the cosine of the
			// angle between the light ray and the normal vector of the polygon,
			// multiplied by the fraction of the distance to the light's
			// radius if it's not a flood light. If the polygon is facing away
			// from the light, it is not affected.

			light_vector = active_light_list[index].light_pos - *vertex_ptr;
			distance = light_vector.normalise();
			dot = light_vector & *normal_ptr;
			if (distance <= light_ptr->radius && dot > 0.0f) {
				if (light_ptr->flood)
					intensity = light_ptr->intensity;
				else
					intensity = light_ptr->intensity * dot *
						((light_ptr->radius - distance) * 
						light_ptr->one_on_radius);
				red += light_ptr->colour.red * intensity;
				green += light_ptr->colour.green * intensity;
				blue += light_ptr->colour.blue * intensity;
			}
			break;

		case STATIC_SPOT_LIGHT:
		case REVOLVING_SPOT_LIGHT:
		case SEARCHING_SPOT_LIGHT:

			// The intensity of a spot light is a combination of the cosine of
			// the angle between the light ray and the cone, and the light ray
			// and the normal vector of the polygon, multiplied by the fraction
			// of the distance to the light's radius if it's not a flood light.
			// If the polygon is facing away from the light, it is not affected.

			light_vector = -(active_light_list[index].light_pos - *vertex_ptr);
			distance = light_vector.normalise();
			dot = light_vector & light_ptr->dir;
			dot2 = -(light_vector & *normal_ptr);
			if (distance <= light_ptr->radius && dot2 > 0.0f &&
				dot > light_ptr->cos_cone_angle) {
				if (light_ptr->flood)
					intensity = light_ptr->intensity;
				else 
					intensity = light_ptr->intensity * dot2 *
						((dot - light_ptr->cos_cone_angle) *
						light_ptr->cone_angle_M) * 
						((light_ptr->radius - distance) * 
						light_ptr->one_on_radius);
				red += light_ptr->colour.red * intensity;
				green += light_ptr->colour.green * intensity;
				blue += light_ptr->colour.blue * intensity;
			}
			break;
		}
	}

	// Adjust the final vertex colour by the master intensity, making sure the
	// component values stay within 0 and 255.

	red += master_red;
	if (FLT(red, 0.0f))
		red = 0.0f;
	else if (FGT(red, 255.0f))
		red = 255.0f;
	green += master_green;
	if (FLT(green, 0.0f))
		green = 0.0f;
	else if (FGT(green, 255.0f))
		green = 255.0f;
	blue += master_blue;
	if (FLT(blue, 0.0f))
		blue = 0.0f;
	else if (FGT(blue, 255.0f))
		blue = 255.0f;

	// Set the final vertex colour.

	colour_ptr->red = red;
	colour_ptr->green = green;
	colour_ptr->blue = blue;
}

//------------------------------------------------------------------------------
// Compute the brightness of a vertex using the given active light list.
//------------------------------------------------------------------------------

float
compute_vertex_brightness(light_ref *active_light_list, vertex *vertex_ptr, 
						  vector *normal_ptr)
{
	RGBcolour vertex_colour;

	compute_vertex_colour(active_light_list, vertex_ptr, normal_ptr, 
		&vertex_colour);
	return((vertex_colour.red + vertex_colour.blue + 
		vertex_colour.green) * ONE_OVER_765);
}
