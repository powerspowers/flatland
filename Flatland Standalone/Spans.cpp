//******************************************************************************
// Copyright (C) 2018 Flatland Online Inc., Philip Stephens, Michael Powers.
// This code is licensed under the MIT license (see LICENCE file for details).
//******************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Classes.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Render.h"

// Image cache list.

static cache *image_cache_list[IMAGE_SIZES];

// Dimensions of each image size.

int image_dimensions_list[IMAGE_SIZES] = { 
	2048, 1024, 512, 256, 128, 64, 32, 16, 8, 4, 2
};

#ifdef STREAMING_MEDIA

// Semaphore for pixmap image updating.

void *image_updated_semaphore;

#endif

//------------------------------------------------------------------------------
// Create the image caches.
//------------------------------------------------------------------------------

bool
create_image_caches(void)
{
	for (int size_index = 0; size_index < IMAGE_SIZES; size_index++)
		if ((image_cache_list[size_index] = new cache(size_index)) == NULL)
			return(false);
	return(true);
}

//------------------------------------------------------------------------------
// Delete the image caches.
//------------------------------------------------------------------------------

void
delete_image_caches(void)
{
	for (int size_index = 0; size_index < IMAGE_SIZES; size_index++)
		if (image_cache_list[size_index])
			delete image_cache_list[size_index];
}

//------------------------------------------------------------------------------
// Get the size index for the given texture size.
//------------------------------------------------------------------------------

int
get_size_index(int texture_width, int texture_height)
{
	int max_dimensions;
	int size_index;

	// Chose the smallest buffer that the texture image can fit into.

	max_dimensions = FMAX(texture_width, texture_height);
	for (size_index = 0; size_index < IMAGE_SIZES; size_index++) {
		if (max_dimensions > image_dimensions_list[size_index])
			break;
	}
	size_index--;
	return(size_index);
}

//------------------------------------------------------------------------------
// Compute and set the size indices for each pixmap in the given texture.  This
// is used to select the approapiate cache entry list when caching the pixmap.
//------------------------------------------------------------------------------

void
set_size_indices(texture *texture_ptr)
{
	int size_index;
	pixmap *pixmap_list;
	int pixmap_no;

	// Get the size index for this texture.

	size_index = get_size_index(texture_ptr->width, texture_ptr->height);

	// Step through all the pixmaps in the texture, and set the size index for
	// each.

	pixmap_list = texture_ptr->pixmap_list;
	for (pixmap_no = 0; pixmap_no < texture_ptr->pixmaps; pixmap_no++)
			pixmap_list[pixmap_no].size_index = size_index;	
}

//------------------------------------------------------------------------------
// Get the next free cache entry for the given image size.
//------------------------------------------------------------------------------

static cache_entry *
get_free_cache_entry(int size_index)
{
	cache *image_cache_ptr;
	cache_entry *cache_entry_ptr;
	cache_entry *free_cache_entry_ptr;
	cache_entry *oldest_cache_entry_ptr;
	int oldest_frame_no;

	// Search for the first free texture cache entry, and the oldest entry that
	// wasn't created in this frame.

	image_cache_ptr = image_cache_list[size_index];
	free_cache_entry_ptr = NULL;
	oldest_cache_entry_ptr = NULL;
	oldest_frame_no = frames_rendered;
	cache_entry_ptr = image_cache_ptr->cache_entry_list;
	while (cache_entry_ptr != NULL) {
		if (cache_entry_ptr->pixmap_ptr == NULL) {
			free_cache_entry_ptr = cache_entry_ptr;
			break;
		}
		if (cache_entry_ptr->frame_no < oldest_frame_no) {
			oldest_cache_entry_ptr = cache_entry_ptr;
			oldest_frame_no = cache_entry_ptr->frame_no;
		}
		cache_entry_ptr = cache_entry_ptr->next_cache_entry_ptr;
	}

	// If there is no free cache entry...

	if (free_cache_entry_ptr == NULL) {

		// If there is no oldest cache entry, add a new cache entry to the list.
		// If this fails, reuse the first cache entry in the list.

		if (oldest_cache_entry_ptr == NULL && (free_cache_entry_ptr = 
			image_cache_list[size_index]->add_cache_entry()) == NULL)
			oldest_cache_entry_ptr = image_cache_ptr->cache_entry_list;

		// If we're reusing a cache entry, remove it's reference from the pixmap
		// that has been using it.

		if (oldest_cache_entry_ptr != NULL) {
			pixmap *oldest_pixmap_ptr;
			int oldest_brightness_index;
			
			oldest_pixmap_ptr = oldest_cache_entry_ptr->pixmap_ptr;
			oldest_brightness_index = oldest_cache_entry_ptr->brightness_index;
			oldest_pixmap_ptr->cache_entry_list[oldest_brightness_index] = NULL;
			free_cache_entry_ptr = oldest_cache_entry_ptr;
		}
	}

	// Return a pointer to the free cache entry.

	return(free_cache_entry_ptr);
}

//------------------------------------------------------------------------------
// Return a cache entry for the given pixmap at the given brightness index.
//------------------------------------------------------------------------------

cache_entry *
get_cache_entry(pixmap *pixmap_ptr, int brightness_index)
{
	cache_entry *cache_entry_ptr;
	int size_index;
	int image_dimensions;

	// If the cache entry already exists, return a pointer to it, after
	// updating the frame number.

	cache_entry_ptr = pixmap_ptr->cache_entry_list[brightness_index];
	if (cache_entry_ptr != NULL) {

#ifdef STREAMING_MEDIA

		// If this pixmap image was updated, refresh the cache entry.  This
		// only occurs on video pixmaps.

		raise_semaphore(image_updated_semaphore);
		bool image_updated = pixmap_ptr->image_updated[brightness_index];
		pixmap_ptr->image_updated[brightness_index] = false;
		lower_semaphore(image_updated_semaphore);
		if (image_updated) {
			size_index = pixmap_ptr->size_index;
			image_dimensions = image_dimensions_list[size_index];
			if (hardware_acceleration)
				hardware_set_texture(cache_entry_ptr);
			else
				set_lit_image(cache_entry_ptr, image_dimensions);
		}

#endif

		// Update the frame number and return a pointer to the cache entry.

		cache_entry_ptr->frame_no = frames_rendered;
		return(cache_entry_ptr);
	}

	// Get a free cache entry for this lit image, and initialise it.

	size_index = pixmap_ptr->size_index;
	cache_entry_ptr = get_free_cache_entry(size_index);
	image_dimensions = image_dimensions_list[size_index];
	cache_entry_ptr->lit_image_mask = (image_dimensions - 1) << FRAC_BITS;
	cache_entry_ptr->lit_image_shift = FRAC_BITS - (IMAGE_SIZES - size_index);
	cache_entry_ptr->pixmap_ptr = pixmap_ptr;
	cache_entry_ptr->brightness_index = brightness_index;
	cache_entry_ptr->frame_no = frames_rendered;

	// Store the pointer to the cache entry in the associated pixmap.

	pixmap_ptr->cache_entry_list[brightness_index] = cache_entry_ptr;

	// If hardware acceleration is enabled, set the hardware texture, otherwise
	// create the lit image for this cache entry.

	if (hardware_acceleration)
		hardware_set_texture(cache_entry_ptr);
	else
		set_lit_image(cache_entry_ptr, image_dimensions);

	// Return the pointer to the cache entry.

	return(cache_entry_ptr);
}

//------------------------------------------------------------------------------
// Insert a span into a span buffer row.
//------------------------------------------------------------------------------

static void
insert_span(span_row *span_row_ptr, span *prev_span_ptr, span *new_span_ptr,
			pixmap *pixmap_ptr)
{

	// If we have a solid colour span (no texture), the pixmap is opaque
	// (no transparent colour index), or we have a sky span (1/tz = 0)...

	if (pixmap_ptr == NULL || pixmap_ptr->transparent_index == -1 ||
		new_span_ptr->start_span.one_on_tz == 0.0f) {

		// If there is a previous span, add the new span after the
		// previous span.

		if (prev_span_ptr != NULL) {
			new_span_ptr->next_span_ptr = prev_span_ptr->next_span_ptr;
			prev_span_ptr->next_span_ptr = new_span_ptr;
		} 

		// If there is no previous span, add the new span to the head
		// of the opaque span list.

		else {
			new_span_ptr->next_span_ptr	= span_row_ptr->opaque_span_list;
			span_row_ptr->opaque_span_list = new_span_ptr;
		}
	}
	
	// If the texture has a transparent colour index, add the new span to the
	// head of the transparent span list (which makes it the backmost
	// transparent span so far in this row).

	else {
		new_span_ptr->next_span_ptr = span_row_ptr->transparent_span_list;
		span_row_ptr->transparent_span_list = new_span_ptr;
	}
}

//------------------------------------------------------------------------------
// Add a polygon span to the span buffer.  It is assumed that the span will be
// behind all other spans currently in the buffer.  The return value indicates
// whether the span was inserted or rejected.
//------------------------------------------------------------------------------

bool
add_span(int sy, edge *left_edge_ptr, edge *right_edge_ptr, pixmap *pixmap_ptr,
		 pixel colour_pixel, int brightness_index, bool is_popup)
{
	float left_sx, right_sx;
	float delta_sx, one_on_delta_sx;
	span new_span;
	span_row *span_row_ptr;
	span *prev_span_ptr, *curr_span_ptr, *new_span_ptr;
	bool span_inserted;

	// If the right screen x coordinate is less than or equal to the left
	// screen x coordinate, switch the edges.

	if (right_edge_ptr->sx <= left_edge_ptr->sx) {
		edge *temp_edge_ptr = left_edge_ptr;
		left_edge_ptr = right_edge_ptr;
		right_edge_ptr = temp_edge_ptr;
	}

	// Compute the left and right screen x coordinate + 1.  If they are equal,
	// the span has zero width and can be discarded.

	left_sx = FCEIL(left_edge_ptr->sx);
	right_sx = FCEIL(right_edge_ptr->sx);
	if (left_sx == right_sx)
		return(false);

	// Clamp the left and right screen x coordinates to the display.

	if (left_sx < 0.0)
		left_sx = 0.0;
	if (right_sx > frame_buffer_width)
		right_sx = frame_buffer_width;

	// Compute the delta values for the new span.  Note that these deltas are
	// computed from the original left and right screen x coordinates, not the
	// adjusted ones.

	delta_sx = right_edge_ptr->sx - left_edge_ptr->sx;
	one_on_delta_sx = 1.0f / delta_sx;
	new_span.delta_span.one_on_tz = 
		(right_edge_ptr->one_on_tz - left_edge_ptr->one_on_tz) * 
		one_on_delta_sx;
	new_span.delta_span.u_on_tz = 
		(right_edge_ptr->u_on_tz - left_edge_ptr->u_on_tz) * one_on_delta_sx;
	new_span.delta_span.v_on_tz = 
		(right_edge_ptr->v_on_tz - left_edge_ptr->v_on_tz) * one_on_delta_sx;

	// Determine the initial values for the new span.  This includes taking
	// into account a left screen x coordinate that is to the right of the
	// span's left edge.
	
	new_span.start_span.one_on_tz = left_edge_ptr->one_on_tz;
	new_span.start_span.u_on_tz = left_edge_ptr->u_on_tz;
	new_span.start_span.v_on_tz = left_edge_ptr->v_on_tz;
	delta_sx = left_sx - left_edge_ptr->sx;
	if (delta_sx > 0.0) {
		new_span.start_span.one_on_tz += new_span.delta_span.one_on_tz *
			delta_sx;
		new_span.start_span.u_on_tz += new_span.delta_span.u_on_tz * delta_sx;
		new_span.start_span.v_on_tz += new_span.delta_span.v_on_tz * delta_sx;
	}

	// Initialise the span.

	new_span.sy = sy;
	new_span.start_sx = (int)left_sx;
	new_span.end_sx = (int)right_sx;
	new_span.is_popup = is_popup;
	new_span.pixmap_ptr = pixmap_ptr;
	new_span.colour_pixel = colour_pixel;
	new_span.brightness_index = brightness_index;

	// If the opaque span list is empty, just insert the new span and return.

	span_row_ptr = (*span_buffer_ptr)[sy];
	if (span_row_ptr->opaque_span_list == NULL) {
		span *new_span_ptr = dup_span(&new_span);
		insert_span(span_row_ptr, NULL, new_span_ptr, pixmap_ptr);
		return(true);
	}

	// Start at the first opaque span.

	prev_span_ptr = NULL;
	curr_span_ptr = span_row_ptr->opaque_span_list;
	span_inserted = false;
	do {

		// If the new span ends before the current span starts, insert the
		// new span before the current span and return.

		if (new_span.end_sx <= curr_span_ptr->start_sx) {
			new_span_ptr = dup_span(&new_span);
			insert_span(span_row_ptr, prev_span_ptr, new_span_ptr, pixmap_ptr);
			return(true);
		}

		// If the new span starts after the current span ends, move onto the
		// next span in the row.

		if (new_span.start_sx >= curr_span_ptr->end_sx) {
			prev_span_ptr = curr_span_ptr;
			curr_span_ptr = curr_span_ptr->next_span_ptr;
			continue;
		}

		// If the new span starts before the current span starts, insert a new
		// span segment before the current span that starts where the new span
		// starts and ends where the current span starts.

		if (new_span.start_sx < curr_span_ptr->start_sx) {
			new_span_ptr = dup_span(&new_span);
			new_span_ptr->end_sx = curr_span_ptr->start_sx;
			insert_span(span_row_ptr, prev_span_ptr, new_span_ptr, pixmap_ptr);
			span_inserted = true;
		}

		// If the new span ends before the current span ends, there is nothing
		// more to do.

		if (new_span.end_sx <= curr_span_ptr->end_sx)
			return(span_inserted);

		// Advance the start of the new span to be equal to the end of the
		// current span, then move onto the next span in the row.

		new_span.adjust_start(curr_span_ptr->end_sx);
		prev_span_ptr = curr_span_ptr;
		curr_span_ptr = curr_span_ptr->next_span_ptr;
	} while (curr_span_ptr != NULL);

	// Insert the new span at the end of the span row.

	new_span_ptr = dup_span(&new_span);
	insert_span(span_row_ptr, prev_span_ptr, new_span_ptr, pixmap_ptr);
	return(true);
}

//------------------------------------------------------------------------------
// Insert a movable span into a span buffer row.
//------------------------------------------------------------------------------

static void
insert_movable_span(span_row *span_row_ptr, span *prev_span_ptr, 
					span *new_span_ptr, pixmap *pixmap_ptr)
{
	span *span_ptr, *add_span_ptr;

	// If we have a solid colour span (no texture) or the pixmap is opaque
	// (no transparent colour index)...

	if (pixmap_ptr == NULL || pixmap_ptr->transparent_index == -1) {

		// If there is a previous span, add the new span after the
		// previous span.

		if (prev_span_ptr != NULL) {
			new_span_ptr->next_span_ptr = prev_span_ptr->next_span_ptr;
			prev_span_ptr->next_span_ptr = new_span_ptr;
		} 

		// If there is no previous span, add the new span to the head
		// of the opaque span list.

		else {
			new_span_ptr->next_span_ptr	= span_row_ptr->opaque_span_list;
			span_row_ptr->opaque_span_list = new_span_ptr;
		}

		// Check for overlapping spans in the transparent span list, and adjust
		// or remove those that are behind the new span.

		prev_span_ptr = NULL;
		span_ptr = span_row_ptr->transparent_span_list;
		while (span_ptr != NULL) {

			// If the new span overlaps this transparent span, and is in front
			// of it...

			if (!(new_span_ptr->start_sx >= span_ptr->end_sx ||
				new_span_ptr->end_sx <= span_ptr->start_sx) &&
				!new_span_ptr->span_in_front(span_ptr)) {

				// If the this transparent span is completely hidden by the new
				// span, remove it and move onto the next transparent span in
				// the list.

				if (new_span_ptr->start_sx <= span_ptr->start_sx &&
					new_span_ptr->end_sx >= span_ptr->end_sx) {
					if (prev_span_ptr != NULL) {
						span_ptr = del_span(span_ptr);
						prev_span_ptr->next_span_ptr = span_ptr;
					} else {
						span_ptr = del_span(span_row_ptr->transparent_span_list);
						span_row_ptr->transparent_span_list = span_ptr;
					}
					continue;
				}

				// If this transparent span is split by the new span, shorten 
				// the left segment, and add a new right segment.

				else if (new_span_ptr->start_sx > span_ptr->start_sx &&
					new_span_ptr->end_sx < span_ptr->end_sx) {
					add_span_ptr = dup_span(span_ptr);
					add_span_ptr->adjust_start(new_span_ptr->end_sx);
					add_span_ptr->next_span_ptr = span_ptr->next_span_ptr;
					span_ptr->end_sx = new_span_ptr->start_sx;
					span_ptr->next_span_ptr = add_span_ptr;
					prev_span_ptr = span_ptr;
					span_ptr = add_span_ptr;
				}

				// If the start of the current span is overlapped by the new
				// span, adjust the start of the current span to avoid the
				// overlap.

				else if (new_span_ptr->end_sx < span_ptr->end_sx)
					span_ptr->adjust_start(new_span_ptr->end_sx);

				// If the end of the current span is overlapped by the new
				// span, adjust the end of the current span to avoid the
				// overlap.

				else if (new_span_ptr->start_sx > span_ptr->start_sx)
					span_ptr->end_sx = new_span_ptr->start_sx;
			}

			// Move onto the next transparent span in the list.

			prev_span_ptr = span_ptr;
			span_ptr = span_ptr->next_span_ptr;
		}
	}
	
	// If the texture has a transparent colour index, insert the new span into
	// the transparent span list before the first overlapping span that is in
	// front of the new span.  If no spans overlap, the new span is added to
	// the end of the list.

	else {
		prev_span_ptr = NULL;
		span_ptr = span_row_ptr->transparent_span_list;
		while (span_ptr != NULL) {
			if (!(new_span_ptr->start_sx >= span_ptr->end_sx ||
				new_span_ptr->end_sx <= span_ptr->start_sx) &&
				new_span_ptr->span_in_front(span_ptr))
				break;
			prev_span_ptr = span_ptr;
			span_ptr = span_ptr->next_span_ptr;
		}
		if (prev_span_ptr != NULL) {
			new_span_ptr->next_span_ptr = prev_span_ptr->next_span_ptr;
			prev_span_ptr->next_span_ptr = new_span_ptr;
		} else {
			new_span_ptr->next_span_ptr	= span_row_ptr->transparent_span_list;
			span_row_ptr->transparent_span_list = new_span_ptr;
		}
	}
}

//------------------------------------------------------------------------------
// Remove a span from a span buffer row, and return a pointer to the next
// span.
//------------------------------------------------------------------------------

static span *
remove_span(span_row *span_row_ptr, span *prev_span_ptr)
{
	span *next_span_ptr;

	// Delete the span and adjust the previous span's next span
	// pointer, or the span row's opaque span list pointer.

	if (prev_span_ptr != NULL) {
		next_span_ptr = del_span(prev_span_ptr->next_span_ptr);
		prev_span_ptr->next_span_ptr = next_span_ptr;
	} else {
		next_span_ptr = del_span(span_row_ptr->opaque_span_list);
		span_row_ptr->opaque_span_list = next_span_ptr;
	}

	// Return a pointer to the span after the one deleted.

	return(next_span_ptr);
}

//------------------------------------------------------------------------------
// Add a movable span to the completed span buffer.
//------------------------------------------------------------------------------

void
add_movable_span(int sy, edge *left_edge_ptr, edge *right_edge_ptr, 
				 pixmap *pixmap_ptr, pixel colour_pixel, int brightness_index)
{
	float left_sx, right_sx;
	float delta_sx, one_on_delta_sx;
	span_row *span_row_ptr;
	span *prev_span_ptr;
	span *first_span_ptr, *last_span_ptr;
	span new_span;

	// If the right screen x coordinate is less than or equal to the left
	// screen x coordinate, switch the edges.

	if (right_edge_ptr->sx <= left_edge_ptr->sx) {
		edge *temp_edge_ptr = left_edge_ptr;
		left_edge_ptr = right_edge_ptr;
		right_edge_ptr = temp_edge_ptr;
	}

	// Compute the left and right screen x coordinate + 1.  If they are equal,
	// the span has zero width and can be discarded.

	left_sx = FCEIL(left_edge_ptr->sx);
	right_sx = FCEIL(right_edge_ptr->sx);
	if (left_sx == right_sx)
		return;

	// Clamp the left and right screen x coordinates to the display.

	if (left_sx < 0.0)
		left_sx = 0.0;
	if (right_sx > frame_buffer_width)
		right_sx = frame_buffer_width;

	// Compute the delta values for the new span.  Note that these deltas are
	// computed from the original left and right screen x coordinates, not the
	// adjusted ones.

	delta_sx = right_edge_ptr->sx - left_edge_ptr->sx;
	one_on_delta_sx = 1.0f / delta_sx;
	new_span.delta_span.one_on_tz = 
		(right_edge_ptr->one_on_tz - left_edge_ptr->one_on_tz) * 
		one_on_delta_sx;
	new_span.delta_span.u_on_tz = 
		(right_edge_ptr->u_on_tz - left_edge_ptr->u_on_tz) * one_on_delta_sx;
	new_span.delta_span.v_on_tz = 
		(right_edge_ptr->v_on_tz - left_edge_ptr->v_on_tz) * one_on_delta_sx;

	// Determine the initial values for the new span.  This includes taking
	// into account a left screen x coordinate that is to the right of the
	// span's left edge.
	
	new_span.start_span.one_on_tz = left_edge_ptr->one_on_tz;
	new_span.start_span.u_on_tz = left_edge_ptr->u_on_tz;
	new_span.start_span.v_on_tz = left_edge_ptr->v_on_tz;
	delta_sx = left_sx - left_edge_ptr->sx;
	if (delta_sx > 0.0) {
		new_span.start_span.one_on_tz += new_span.delta_span.one_on_tz *
			delta_sx;
		new_span.start_span.u_on_tz += new_span.delta_span.u_on_tz * delta_sx;
		new_span.start_span.v_on_tz += new_span.delta_span.v_on_tz * delta_sx;
	}

	// Initialise the span.

	new_span.sy = sy;
	new_span.start_sx = (int)left_sx;
	new_span.end_sx = (int)right_sx;
	new_span.is_popup = false;
	new_span.pixmap_ptr = pixmap_ptr;
	new_span.colour_pixel = colour_pixel;
	new_span.brightness_index = brightness_index;

	// Find the first span that overlaps the new span (i.e. does not end
	// before the new span begins).  We also remember the previous span.

	span_row_ptr = (*span_buffer_ptr)[sy];
	prev_span_ptr = NULL;
	first_span_ptr = span_row_ptr->opaque_span_list;
	while (first_span_ptr != NULL && new_span.start_sx >= first_span_ptr->end_sx) {
		prev_span_ptr = first_span_ptr;
		first_span_ptr = first_span_ptr->next_span_ptr;
	}
	if (first_span_ptr == NULL) {
		return;
	}

	// Find the last span that overlaps the new span (i.e. does not end before
	// the new span ends).

	last_span_ptr = first_span_ptr;
	while (last_span_ptr != NULL && last_span_ptr->end_sx < new_span.end_sx)
		last_span_ptr = last_span_ptr->next_span_ptr;
	if (last_span_ptr == NULL) {
		return;
	}

	// If the first and last overlapping span is one and the same...

	if (first_span_ptr == last_span_ptr) {

		// If the new span is in front of the overlapping span...

		if (first_span_ptr->span_in_front(&new_span)) {
			span *new_span_ptr;

			// Insert the new span after the first overlapping span.

			new_span_ptr = dup_span(&new_span);
			insert_movable_span(span_row_ptr, first_span_ptr, new_span_ptr,
				pixmap_ptr);
	
			// If the new span is a solid colour or it's pixmap is opaque
			// (no transparent colour index)...

			if (!pixmap_ptr || pixmap_ptr->transparent_index == -1) {

				// If the first overlapping span ends after the new span, insert
				// a new last span after the new span.

				if (first_span_ptr->end_sx > new_span_ptr->end_sx) {
					last_span_ptr = dup_span(first_span_ptr);
					last_span_ptr->adjust_start(new_span_ptr->end_sx);
					insert_span(span_row_ptr, new_span_ptr, last_span_ptr, NULL);
				}

				// Shorten the first span so that it no longer overlaps the new
				// span, or remove it altogether if it becomes zero length.

				first_span_ptr->end_sx = new_span_ptr->start_sx;
				if (first_span_ptr->start_sx == first_span_ptr->end_sx)
					remove_span(span_row_ptr, prev_span_ptr);
			}
		}
	}

	// If the first and last overlapping spans are different...

	else {
		span new_segment;
		span *next_span_ptr;

		// Get a pointer to the span after the first overlapping span.

		next_span_ptr = first_span_ptr->next_span_ptr;

		// If the new span is in front of the first overlapping span...

		if (first_span_ptr->span_in_front(&new_span)) {

			// Initialise the new segment to be the new span ending where the
			// first overlapping span ends.

			new_segment = new_span;
			new_segment.end_sx = first_span_ptr->end_sx;

			// If the new span is opaque...

			if (pixmap_ptr == NULL || pixmap_ptr->transparent_index == -1) {

				// Shorten the first span so that it no longer overlaps the
				// new span, or remove it altogether if it becomes zero length.
				// The first span becomes the previous span if it wasn't removed.

				first_span_ptr->end_sx = new_segment.start_sx;
				if (first_span_ptr->start_sx == first_span_ptr->end_sx)
					remove_span(span_row_ptr, prev_span_ptr);
				else
					prev_span_ptr = first_span_ptr;
			} else
				prev_span_ptr = first_span_ptr;

			// Adjust start of the new span to start where the new segment ends.

			new_span.adjust_start(new_segment.end_sx);
		}

		// If the new span is behind the first overlapping span...

		else {

			// Adjust the start of the new span to start where the first
			// overlapping span ends.

			new_span.adjust_start(first_span_ptr->end_sx);

			// Initialise the new segment to be the new span with zero length.

			new_segment = new_span;
			new_segment.end_sx = new_segment.start_sx;

			// The previous span is now the first span.

			prev_span_ptr = first_span_ptr;
		}

		// While the next overlapping span is not the last overlapping span...

		while (next_span_ptr != last_span_ptr) {

			// If the new span is in front of the overlapping span...

			if (next_span_ptr->span_in_front(&new_span)) {
		
				// Extend the new segment so that it ends where the overlapping
				// span ends.

				new_segment.end_sx = next_span_ptr->end_sx;
	
				// Adjust the start of the new span to start where the
				// overlapping span ends.

				new_span.adjust_start(next_span_ptr->end_sx);

				// If the new span is opaque, remove the overlapping span.  Then
				// move to the next overlapping span.

				if (pixmap_ptr == NULL || pixmap_ptr->transparent_index == -1)
					next_span_ptr = remove_span(span_row_ptr, prev_span_ptr);
				else {
					prev_span_ptr = next_span_ptr;
					next_span_ptr = next_span_ptr->next_span_ptr;
				}
			}

			// If the new span is behind the overlapping span...

			else {

				// If the new segment is not zero length, insert it before the
				// overlapping span.

				if (new_segment.start_sx != new_segment.end_sx) {
					span *new_span_ptr = dup_span(&new_segment);
					insert_movable_span(span_row_ptr, prev_span_ptr,
						new_span_ptr, NULL);
				}

				// Adjust the start of the new span to start where the 
				// overlapping span ends.

				new_span.adjust_start(next_span_ptr->end_sx);

				// Initialise the new segment to be the new span with zero 
				// length.

				new_segment = new_span;
				new_segment.end_sx = new_segment.start_sx;

				// Move to the next overlapping span.

				prev_span_ptr = next_span_ptr;
				next_span_ptr = next_span_ptr->next_span_ptr;
			}
		}

		// If the new span is in front of the last overlapping span...

		if (last_span_ptr->span_in_front(&new_span)) {
			span *new_span_ptr;

			// Extend the new segment so that it ends where the new span ends.

			new_segment.end_sx = new_span.end_sx;

			// Insert the new segment before the last overlapping span.

			new_span_ptr = dup_span(&new_segment);
			insert_movable_span(span_row_ptr, prev_span_ptr, new_span_ptr,
				pixmap_ptr);

			// If the new span is opaque, adjust the start of last overlapping
			// span so that it's at the end of the new span, and if it becomes
			// zero length remove it.

			if (pixmap_ptr == NULL || pixmap_ptr->transparent_index == -1) {
				last_span_ptr->adjust_start(new_span.end_sx);
				if (last_span_ptr->start_sx == last_span_ptr->end_sx)
					remove_span(span_row_ptr, new_span_ptr);
			}
		}

		// If the new span is behind the last overlapping span...

		else {

			// If the new segment is not zero length, insert it before the
			// last overlapping span.

			if (new_segment.start_sx != new_segment.end_sx) {
				span *new_span_ptr = dup_span(&new_segment);
				insert_movable_span(span_row_ptr, prev_span_ptr, new_span_ptr,
					pixmap_ptr);
			}
		}
	}
}
