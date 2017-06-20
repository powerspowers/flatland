//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All rights reserved.
//******************************************************************************

// Spot parsing function.

void 
parse_spot_file(char *spot_URL, char *spot_file_path);

// Spot saving function.

void
save_spot_file(const char *spot_file_path);

// Generic read string function.

bool
read_string(FILE *fp, char *buffer, int max_buffer_length);

// Configuration file functions.

void
load_config_file(void);

void
save_config_file(void);

// Blockset cache file functions.
 
void
delete_cached_blockset_list(void);

cached_blockset *
new_cached_blockset(const char *path, const char *href, int size, time_t updated);

bool
load_cached_blockset_list(void);

void
save_cached_blockset_list(void);

void
create_cached_blockset_list(void);

cached_blockset *
find_cached_blockset(const char *href);

bool
delete_cached_blockset(const char *href);

bool
check_for_blockset_update(const char *version_file_URL, const char *blockset_name,
						  unsigned int blockset_version);

// Rover version file parsing function.

bool
parse_rover_version_file(unsigned int &version_number, string &message);

// Spot directory file functions.

void
destroy_spot_dir_list(spot_dir_entry *spot_dir_list);

void
load_spot_directory(void);
