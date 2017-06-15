//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

// Dimensions of each image size.

extern int image_dimensions_list[IMAGE_SIZES];

#ifdef STREAMING_MEDIA

// Semaphore for pixmap image updating.

extern void *image_updated_semaphore;

#endif

// Externally visible functions.

cache_entry *
get_cache_entry(pixmap *pixmap_ptr, int brightness_index);

bool
create_image_caches(void);

void
purge_image_caches(int delta_frames);

void
delete_image_caches(void);

int
get_size_index(int texture_width, int texture_height);

void
set_size_indices(texture *texture_ptr);

bool
add_span(int sy, edge *left_edge_ptr, edge *right_edge_ptr, pixmap *pixmap_ptr,
		 pixel colour_pixel, int brightness_index, bool is_popup);

void
add_movable_span(int sy, edge *left_edge_ptr, edge *right_edge_ptr, 
				 pixmap *pixmap_ptr, pixel colour_pixel, int brightness_index);
