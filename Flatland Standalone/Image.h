//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

// Externally visible functions.

bool
save_frame_buffer_to_JPEG(int width, int height, const char *file_path);

bool
load_image(const char *URL, const char *file_path, texture *texture_ptr, 
		   bool unlimited_size);

texture *
load_GIF_image(void);
