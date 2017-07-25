//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc. 
// All Rights Reserved. 
//******************************************************************************

//==============================================================================
// GIF and JPG loader.
//
// GIF code based in part on:
//
// gif2ras.c - Converts from a Compuserve GIF (tm) image to a Sun Raster image.
// Copyright (c) 1988, 1989 by Patrick J. Naughton
//
// Author: Patrick J. Naughton
// naughton@wind.sun.com
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation.
//
// This file is provided AS IS with no warranties of any kind.  The author
// shall have no liability with respect to the infringement of copyrights,
// trade secrets or any patents by this file or any part thereof.  In no
// event will the author be liable for any lost revenue or profits or
// other special, indirect and consequential damages.
//
// The Graphics Interchange Format(c) is the Copyright property of CompuServe
// Incorporated.  GIF(sm) is a Service Mark property of CompuServe
// Incorporated.
//==============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

extern "C" {
#include "Jpeg\jinclude.h"
#include "Jpeg\jpeglib.h"
#include "Jpeg\jerror.h"
#include "LibPNG\png.h"
}

#include "Classes.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Spans.h"

//------------------------------------------------------------------------------
// Common variables.
//------------------------------------------------------------------------------

// Error message buffer.

static char error_msg[BUFSIZ];

// RGB palette.

static int colours;
static RGBcolour *RGB_palette;	

// File pointer.

static void *fp;

// Image width and height.

static int image_width, image_height;

// Array of pixmaps.

#define MAX_PIXMAPS	256
static pixmap pixmap_list[MAX_PIXMAPS];

// Global image data.

static int pixmaps;
static bool transparent;
static int bytes_per_pixel;

// The global colourmap.

static RGBcolour global_colourmap[256];

//------------------------------------------------------------------------------
// GIF loader definitions and variables.
//------------------------------------------------------------------------------

// Signature bytes.

#define IMAGE_SEPERATOR		0x2c
#define GRAPHIC_EXT			0xf9
#define PLAINTEXT_EXT		0x01
#define APPLICATION_EXT		0xff
#define COMMENT_EXT			0xfe
#define START_EXT			0x21
#define GIF_TRAILER			0x3b

// Value masks.

#define INTERLACE_MASK		0x40
#define COLOURMAP_MASK		0x80
#define DISPOSAL_MASK		0x1c
#define DISPOSAL_SHIFT		2
#define TRANSPARENT_MASK	0x01

// Disposal methods.

#define	NO_DISPOSAL_METHOD	0
#define	LEAVE_IN_PLACE		1
#define RESTORE_BG_COLOUR	2
#define RESTORE_PREV_IMAGE	3

static int
    BitOffset = 0,				// Bit Offset of next code.
    XC = 0, YC = 0,				// Output X and Y coords of current pixel.
    Pass = 0,					// Used by output routine if interlaced pic.
    OutCount = 0,				// Decompressor output 'stack count'.
    Width, Height,				// Image dimensions.
	BufferWidth, BufferHeight,	// Image buffer dimensions.
    LeftOffset, TopOffset,		// Image offset.
    BitsPerPixel,				// Bits per pixel, read from GIF header.
    CodeSize,					// Code size, read from GIF header.
    InitCodeSize,				// Starting code size, used during Clear.
    Code,						// Value returned by ReadCode.
    MaxCode,					// Limiting value for current code size.
    ClearCode,					// GIF clear code.
    EOFCode,					// GIF end-of-information code.
    CurCode, OldCode, InCode,	// Decompressor variables.
    FirstFree,					// First free code, generated per GIF spec.
    FreeCode,					// Decompressor, next free slot in hash table.
    FinChar,					// Decompressor variable.
    BitMask;					// AND mask for data size.

// The image data arrays.

static bool Interlaced;			// Interlaced image flag.
static imagebyte *ImagePtr;		// Pointer to current image array.
static int ImageSize;			// Size of current image array.			

// The hash table used by the decompressor.

static int Prefix[4096];
static int Suffix[4096];

// An output array used by the decompressor.

static int OutCode[1025];

// The legal GIF headers.

static const char *id87 = "GIF87a";
static const char *id89 = "GIF89a";

// Last byte read.

static byte ch;

// The background index.

static byte background_index;

// Last graphic control extension seen.

static int disposal_method, prev_disposal_method;
static bool has_transparent_index;
static int delay_time_ms;
static int total_time_ms;
static int transparent_index;

// Loop flag.

static bool texture_loops;

// Block buffer, size, index and number of bits in current block byte.

static byte block[255];
static byte block_size;
static byte block_index;
static int bits;

//------------------------------------------------------------------------------
// JPG loader definitions and variables.
//------------------------------------------------------------------------------

// Expanded data source object for file input.

typedef struct {
  struct jpeg_source_mgr pub;	// Public fields.
  JOCTET *buffer;				// Start of buffer.
  boolean start_of_file;		// Flag indicating if data has been gotten.
} my_source_mgr;

typedef my_source_mgr *my_src_ptr;

// Input buffer size.

#define INPUT_BUF_SIZE  4096

//==============================================================================
// Common functions.
//==============================================================================

//------------------------------------------------------------------------------
// Delete all loaded images.
//------------------------------------------------------------------------------

static void
image_cleanup(void)
{
	for (int index = 0; index < pixmaps; index++)
		if (pixmap_list[index].image_ptr != NULL) {
			DELBASEARRAY(pixmap_list[index].image_ptr, imagebyte, 
				pixmap_list[index].image_size);
			pixmap_list[index].image_ptr = NULL;
		}
}

//------------------------------------------------------------------------------
// Throw an image error message after cleaning up.
//------------------------------------------------------------------------------

static void
image_error(const char *format, ...)
{
	va_list arg_ptr;

	// Clean up.

	image_cleanup();

	// Construct the error message, then throw it.

	va_start(arg_ptr, format);
	vbprintf(error_msg, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);
	throw (char *)error_msg;
}

//------------------------------------------------------------------------------
// Throw an image memory error after cleaning up.
//------------------------------------------------------------------------------

static void
image_memory_error(const char *object)
{
	// Clean up.

	image_cleanup();

	// Generate the error message, then throw it.

	bprintf(error_msg, BUFSIZ, "Insufficient memory to allocate %s", object);
	throw (char *)error_msg;
}

//==============================================================================
// GIF loader functions.
//==============================================================================

static bool
is_GIF_file(byte *header, int header_size)
{
	return header_size == 6 && (!_strnicmp((char *)header, id87, 6) || !_strnicmp((char *)header, id89, 6));
}

//------------------------------------------------------------------------------
// Clear an image to either the transparent or background colour.
//------------------------------------------------------------------------------

static void
clear_image(byte *image_ptr, int size)
{
	if (has_transparent_index) 
		memset(image_ptr, transparent_index, size);
	else
		memset(image_ptr, background_index, size);
}

//------------------------------------------------------------------------------
// Read one byte from the GIF file.
//------------------------------------------------------------------------------

static byte
read_byte(void)
{
	byte buffer;

	if (read_file(&buffer, 1) != 1)
		image_error("Error reading GIF file");
	return(buffer);
}

//------------------------------------------------------------------------------
// Read one word from the GIF file.
//------------------------------------------------------------------------------

static word
read_word(void)
{
	word buffer;

	if (read_file((byte *)&buffer, 2) != 2)
		image_error("Error reading GIF file");
	return(buffer);
}

//------------------------------------------------------------------------------
// Read a variable-length block from the GIF file.
//------------------------------------------------------------------------------

static void
read_block(byte *buffer_ptr, int bytes)
{
	if (read_file(buffer_ptr, bytes) != bytes)
		image_error("Error reading GIF file");
}

//------------------------------------------------------------------------------
// Read the next code bit from the table-based image data.
//------------------------------------------------------------------------------

static int
read_code_bit(int bit_pos)
{
	int code_bit;

	// If we have reached the end of the current block, read the next block
	// and reset the block index and number of bits in current byte.

	if (block_index == block_size) {
		block_size = read_byte();
		read_block(block, block_size);
		block_index = 0;
		bits = 8;
	}

	// Extract the next code bit and shift it to the requested bit position.

	code_bit = (int)(block[block_index] & 1) << bit_pos;

	// Shift the bits in the current block byte right one position; if the byte
	// is empty, move onto the next.

	bits--;
	if (bits > 0)
		block[block_index] >>= 1;
	else {
		block_index++;
		bits = 8;
	}

	// Return code bit.

	return(code_bit);
}

//------------------------------------------------------------------------------
// Read the next code from the table-based image data.
//------------------------------------------------------------------------------

static int
read_code(void)
{
	int code, bit_pos;

	code = 0;
	for (bit_pos = 0; bit_pos < CodeSize; bit_pos++)
		code |= read_code_bit(bit_pos);
	return(code);
}

//------------------------------------------------------------------------------
// Add a pixel to the image buffer.
//------------------------------------------------------------------------------

static void
AddToPixel(byte Index)
{
	// Sanity check the pixel coordinates.

	if (TopOffset + YC >= BufferHeight ||
		LeftOffset + XC >= BufferWidth)
		return;

	// Store the pixel value at (XC + LeftOffset, YC + TopOffset), provided it
	// isn't the transparent colour.

	if ((int)Index != transparent_index)
		*(ImagePtr + ((TopOffset + YC) * BufferWidth) + (LeftOffset + XC)) = 
			Index;

	// Update the X-coordinate, and if it overflows, update the
	// Y-coordinate.  If a non-interlaced picture, just increment YC to the
	// next scan line. If it's interlaced, deal with the interlace as
	// described in the GIF spec.  Put the decoded scan line out to the
	// image buffer if we haven't gone past the bottom of it.

	XC++;
	if (XC == Width) {
		XC = 0;
		if (!Interlaced)
			YC++;
		else {
			switch (Pass) {
			case 0:
				YC += 8;
				if (YC >= Height) {
					Pass++;
					YC = 4;
				}
				break;
			case 1:
				YC += 8;
				if (YC >= Height) {
					Pass++;
					YC = 2;
				}
				break;
			case 2:
				YC += 4;
				if (YC >= Height) {
					Pass++;
					YC = 1;
				}
				break;
			case 3:
				YC += 2;
	    	}
		}
    }
}

//------------------------------------------------------------------------------
// Read the GIF extension blocks before an image.
//------------------------------------------------------------------------------

static bool
read_GIF_extensions(void)
{
	// Parse any extension blocks that we might be interested in, and skip over
	// the rest.

	for (ch = read_byte(); ch == START_EXT; ch = read_byte()) {
		byte buffer[255];

		// Parse the extension based upon the type.  Currently we're only
		// interested in the graphic control extension, and the comment
		// extension before the first image.

		switch (ch = read_byte()) {

		// Parse the graphic extension block.  A delay time of zero is invalid,
		// so we bump it to 1 ms.

		case GRAPHIC_EXT:
			read_byte();
			ch = read_byte();
			prev_disposal_method = disposal_method;
			disposal_method = (ch & DISPOSAL_MASK) >> DISPOSAL_SHIFT;
			has_transparent_index = (ch & TRANSPARENT_MASK) ? true : false;
			delay_time_ms = read_word() * 10;
			if (delay_time_ms == 0)
				delay_time_ms++;
			transparent_index = read_byte();
			if (!has_transparent_index)
				transparent_index = -1;
			break;

		// Parse the application extension block.

		case APPLICATION_EXT:

			// If the first block contains the application name "NETSCAPE2.0",
			// this is a loop extension.

			ch = read_byte();
			read_block(buffer, ch);
			if (!_strnicmp((char *)buffer, "NETSCAPE2.0", 11))
				texture_loops = true;
	  	}

		// Skip over any unparsed data sub-blocks in the extension.

		while ((ch = read_byte()) != 0)
			read_block(buffer, ch);
	}

	// If the trailer byte appears next, there is no image to follow so return
	// false.

	if (ch == GIF_TRAILER)
		return(false);

	// Verify the existence of the image seperator.

	if (ch != IMAGE_SEPERATOR)
		image_error("Expected image seperator (got %02x instead)",
			ch);
	return(true);
}

//------------------------------------------------------------------------------
// Read one GIF image into a pixmap.
//------------------------------------------------------------------------------

static void
read_GIF_image(void)
{
	pixmap *pixmap_ptr;
	byte *prev_image_ptr;
	int index;

	// Get a pointer to the current pixmap.

	pixmap_ptr = &pixmap_list[pixmaps];

	// Initialise global variables.

	XC = 0;
	YC = 0;
	Pass = 0;
	OutCount = 0;
	block_size = 0;
	block_index = 0;
	
	// Set the transparent index and delay time for this pixmap.  The 
	// transparent flag is also set to indicate at least one pixmap has 
	// transparent pixels.

	if (has_transparent_index) {
		pixmap_ptr->transparent_index = transparent_index;
		transparent = true;
	} else
		pixmap_ptr->transparent_index = -1;
	pixmap_ptr->delay_ms = delay_time_ms;

	// Now read in values from the image descriptor.

	LeftOffset = read_word();
	TopOffset = read_word();
	Width = read_word();
	Height = read_word();
	ch = read_byte();
	Interlaced = ((ch & INTERLACE_MASK) ? true : false);

	// Determine the dimensions of the image buffer.

	BufferWidth = image_width;
	BufferHeight = image_height;

	// Verify that this image fits inside the buffer.

	if (LeftOffset + Width > BufferWidth ||
		TopOffset + Height > BufferHeight)
		image_error("GIF image #%d is larger than %dx%d", pixmaps + 1, 
			BufferWidth, BufferHeight);
	
	// If there is a local colour map present, flag this as an error; we don't
	// handle them.

	if (ch & COLOURMAP_MASK)
		image_error("Cannot handle a GIF with a local colourmap");

	// Allocate the final image buffer.

	ImageSize = BufferWidth * BufferHeight;
	NEWARRAY(ImagePtr, imagebyte, ImageSize);
	if (ImagePtr == NULL)
		image_memory_error("GIF image");

	// Initialise the pixmap.

	pixmap_ptr->width = BufferWidth;
	pixmap_ptr->height = BufferHeight;
	pixmap_ptr->bytes_per_pixel = 1;
	pixmap_ptr->image_ptr = ImagePtr;
	pixmap_ptr->image_size = ImageSize;

	// Initialise the pixmap image according to the disposal method
	// specified for the previous pixmap image.

	switch (prev_disposal_method) {
	case NO_DISPOSAL_METHOD:
	case RESTORE_BG_COLOUR:
		clear_image(ImagePtr, ImageSize);
		break;
	case LEAVE_IN_PLACE:
		if (pixmaps < 1)
			clear_image(ImagePtr, ImageSize);
		else {
			prev_image_ptr = pixmap_list[pixmaps - 1].image_ptr;
			memcpy(ImagePtr, prev_image_ptr, ImageSize);
		}
		break;
	case RESTORE_PREV_IMAGE:
		if (pixmaps < 2)
			clear_image(ImagePtr, ImageSize);
		else {
			prev_image_ptr = pixmap_list[pixmaps - 1].image_ptr;
			memcpy(ImagePtr, prev_image_ptr, ImageSize);
		}
	}

	// Start reading the table-based image data. First we get the intial code
	// size and compute decompressor constant values, based on this code size.

	CodeSize = read_byte();
	ClearCode = 1 << CodeSize;
	EOFCode = ClearCode + 1;
	FirstFree = ClearCode + 2;
	FreeCode = FirstFree;

	// The GIF spec has it that the code size used to compute the above
	// values is the code size given in the file, but the code size used
	// in compression/decompression is one more than this. (thus the ++).

	CodeSize++;
	InitCodeSize = CodeSize;
	MaxCode = 1 << CodeSize;

	// Decompress the file, continuing until you see the GIF EOF code.
	// One obvious enhancement is to add checking for corrupt files here.

	Code = read_code();
	while (Code != EOFCode) {

		// Clear code sets everything back to its initial value, then
		// reads the immediately subsequent code as uncompressed data.

		if (Code == ClearCode) {
			CodeSize = InitCodeSize;
			MaxCode  = 1 << CodeSize;
			FreeCode = FirstFree;
			Code = read_code();
			CurCode = Code;
			OldCode = Code;
			FinChar = CurCode & BitMask;
			AddToPixel((byte)FinChar);

			// XXX -- This is a sanity check in case an EOF code is not found.

			if (YC == Height)
				break;
		}

		// If not a clear code, then must be data: save same as CurCode
		// and InCode.

		 else {
			InCode = Code;
			CurCode = Code;

			// If greater or equal to FreeCode, not in the hash
			// table yet; repeat the last character decoded.

			if (CurCode >= FreeCode) {
				CurCode = OldCode;
				OutCode[OutCount++] = FinChar;
			}

			// Unless this code is raw data, pursue the chain
			// pointed to by CurCode through the hash table to its
			// end; each code in the chain puts its associated
			// output code on the output queue.
	
	    	while (CurCode > BitMask) {
				if (OutCount > 1024)
					image_error("Corrupt GIF file");
				OutCode[OutCount++] = Suffix[CurCode];
				CurCode = Prefix[CurCode];
			}

			// The last code in the chain is treated as raw data.

			FinChar = CurCode & BitMask;
			OutCode[OutCount++] = FinChar;

			// Now we put the data out to the Output routine.
			// It's been stacked LIFO, so deal with it that way...
			// XXX -- I'm checking for YC reaching it's maximum value as a
			// sanity check, in case an EOF code is not seen.

			for (index = OutCount - 1; index >= 0; index--) {
				AddToPixel((byte)OutCode[index]);
				if (YC == Height)
					break;
			}
			if (YC == Height)
				break;
			OutCount = 0;

			// Build the hash table on-the-fly. No table is stored
			// in the file.

			Prefix[FreeCode] = OldCode;
			Suffix[FreeCode] = FinChar;
			OldCode = InCode;

			// Point to the next slot in the table.  If we exceed
			// the current MaxCode value, increment the code size
			// unless it's already 12.  If it is, do nothing: the 
			// next code decompressed better be CLEAR.

			FreeCode++;
			if (FreeCode >= MaxCode && CodeSize < 12) {
				CodeSize++;
				MaxCode *= 2;
			}
		}
		Code = read_code();
	}

	// Skip over any remaining data blocks that were not parsed.
	// XXX -- This is a sanity check; there should only be a zero-length
	// block here.

	while ((ch = read_byte()) != 0)
		read_block(block, ch);
}

//------------------------------------------------------------------------------
// Function to load a GIF image.
//------------------------------------------------------------------------------

static void
load_GIF(void)
{
	bool has_global_colourmap;
	int index;

	// Initialise the number of pixmaps loaded, the transparent flag, and the
	// loop flag.

	pixmaps = 0;
	transparent = false;
	bytes_per_pixel = 1;
	texture_loops = false;

	// Initialise the pointer to the RGB palette.

	RGB_palette = (RGBcolour *)global_colourmap;

	// Read screen dimensions from GIF screen descriptor.

	image_width = read_word();
	image_height = read_word();

	// If the GIF has no global colourmap then exit with failure.  Otherwise
	// compute the number of bits per pixel.

	ch = read_byte();
	has_global_colourmap = ((ch & COLOURMAP_MASK) ? true : false);
	if (!has_global_colourmap)
		image_error("Cannot handle a GIF without a global colourmap");
	BitsPerPixel = (ch & 7) + 1;

	// Compute the colourmap size and bit mask.

	colours = 1 << BitsPerPixel;
	BitMask = colours - 1;

	// Read the background colour index.

	background_index = read_byte();

	// Skip over the pixel aspect ratio.

	read_byte();
	
	// Read in the global colourmap.

	for (index = 0; index < colours; index++) {
		RGB_palette[index].red = (float)read_byte();
		RGB_palette[index].green = (float)read_byte();
		RGB_palette[index].blue = (float)read_byte();
	}

	// Initialise the graphic control extension settings (no disposal method,
	// no transparent index, default delay of 1/100th of a second).

	disposal_method = NO_DISPOSAL_METHOD;
	has_transparent_index = false;
	transparent_index = -1;
	delay_time_ms = 1;
	total_time_ms = 0;

	// Read each image until we reach the GIF trailer byte.

	while (read_GIF_extensions()) {

		// If we've already read the maximum number of pixmaps allowed, flag
		// this as an error.

		if (pixmaps == MAX_PIXMAPS)
			image_error("No more than %d images permitted in a GIF", 
				MAX_PIXMAPS);

		// Read the next pixmap.

		read_GIF_image();
 
		// Update the total time and increment the number of pixmaps loaded.

		total_time_ms += delay_time_ms;
		pixmaps++;
	}

	// If no pixmaps were present in the GIF, then flag this as an error.

	if (pixmaps == 0)
		image_error("GIF file did not contain any images");
}

//==============================================================================
// JPG loader functions.
//==============================================================================

//------------------------------------------------------------------------------
// Handle fatal errors by simply throwing the error message.
//------------------------------------------------------------------------------

static void
my_error_exit(j_common_ptr cinfo)
{
	(*cinfo->err->format_message)(cinfo, error_msg);
	throw error_msg;
}

//------------------------------------------------------------------------------
// Initialize source --- called by jpeg_read_header before any data is actually
// read. 
//------------------------------------------------------------------------------

static void
init_source(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

	// We reset the empty-input-file flag for each image, but we don't clear
	// the input buffer.  This is correct behavior for reading a series of
	// images from one source.

	src->start_of_file = TRUE;
}

//------------------------------------------------------------------------------
// Fill the input buffer --- called whenever buffer is emptied.
//------------------------------------------------------------------------------

static boolean
fill_input_buffer(j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;
	size_t nbytes;

	nbytes = read_file(src->buffer, INPUT_BUF_SIZE);
	if (nbytes <= 0) {

		// Treat empty input file as fatal error.

		if (src->start_of_file)	
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
		WARNMS(cinfo, JWRN_JPEG_EOF);

		// Insert a fake EOI marker.

		src->buffer[0] = (JOCTET)0xFF;
		src->buffer[1] = (JOCTET)JPEG_EOI;
		nbytes = 2;
	}
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;
	return(TRUE);
}

//------------------------------------------------------------------------------
// Skip data --- used to skip over a potentially large amount of
// uninteresting data (such as an APPn marker).
//------------------------------------------------------------------------------

static void
skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr)cinfo->src;

	// Just a dumb implementation for now.  Could use fseek() except
	// it doesn't work on pipes.  Not clear that being smart is worth
	// any trouble anyway --- large skips are infrequent.

	if (num_bytes > 0) {
		while (num_bytes > (long)src->pub.bytes_in_buffer) {
			num_bytes -= (long)src->pub.bytes_in_buffer;

			// Note we assume that fill_input_buffer will never return FALSE,
			// so suspension need not be handled.

			(void)fill_input_buffer(cinfo);
		}
		src->pub.next_input_byte += (size_t)num_bytes;
		src->pub.bytes_in_buffer -= (size_t)num_bytes;
	}
}

//------------------------------------------------------------------------------
// Terminate source --- called by jpeg_finish_decompress
// after all data has been read.  Often a no-op.
//
// NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
// application must deal with any cleanup that should happen even
// for error exit.
//------------------------------------------------------------------------------

static void
term_source(j_decompress_ptr cinfo)
{
}

//------------------------------------------------------------------------------
// Prepare for input from a file stream.  The caller must have already opened
// the stream, and is responsible for closing it after finishing decompression.
//------------------------------------------------------------------------------

static void
jpeg_src(j_decompress_ptr cinfo)
{
	my_src_ptr src;

  // The source object and input buffer are made permanent so that a series
  // of JPEG images can be read from the same file by calling jpeg_stdio_src
  // only before the first one.  (If we discarded the buffer at the end of
  // one image, we'd likely lose the start of the next one.)
  // This makes it unsafe to use this manager and a different source
  // manager serially with the same JPEG object.  Caveat programmer.

	if (cinfo->src == NULL) {
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)
			((j_common_ptr)cinfo, JPOOL_PERMANENT, SIZEOF(my_source_mgr));
		src = (my_src_ptr)cinfo->src;
		src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)
			((j_common_ptr) cinfo, JPOOL_PERMANENT, 
			INPUT_BUF_SIZE * SIZEOF(JOCTET));
	}

	// Set up the public interface.
	
	src = (my_src_ptr)cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;
}

//------------------------------------------------------------------------------
// Load a JPEG file.
//------------------------------------------------------------------------------

static void
load_JPEG(void)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	JSAMPARRAY scan_line;
	imagebyte *buffer_ptr, *image_ptr;
	RGBcolour colour;
	int buffer_size;
	int row, col;

	// Initialise the number of pixmaps loaded, the transparent flag, the bytes per pixel, and the loop flag.

	pixmaps = 0;
	transparent = false;
	bytes_per_pixel = hardware_acceleration ? 4 : 2;
	texture_loops = false;

	// Initialise the total time for animation.

	total_time_ms = 1;

	// Initialise the number of colours to zero, since there is no palette.

	colours = 0;

	// Set up our own error handler for fatal errors.

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = my_error_exit;
	try {

		// Allocate and initialise a JPEG decompression object.

		jpeg_create_decompress(&cinfo);

		// Specify the source of the compressed data.

		jpeg_src(&cinfo);	

		// Obtain image info.

		jpeg_read_header(&cinfo, TRUE);
		image_width = cinfo.image_width;
		image_height = cinfo.image_height;

		// Select decompression parameters
		
		cinfo.quantize_colors = FALSE;

		// Begin decompression.

		jpeg_start_decompress(&cinfo);

		// Allocate the scan line buffer.

		if (cinfo.jpeg_color_space == JCS_GRAYSCALE)
			scan_line = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, 
				JPOOL_IMAGE, image_width, 1);
		else
			scan_line = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, 
				JPOOL_IMAGE, image_width * 3, 1);

		// Allocate the image buffer.

		buffer_size = image_width * image_height * bytes_per_pixel;
		NEWARRAY(buffer_ptr, imagebyte, buffer_size);
		if (buffer_ptr == NULL)
			image_memory_error("JPEG image");

		// Read the pixel components one scan line at a time.

		image_ptr = buffer_ptr;
		for (row = 0; row < image_height; row++) {
			jpeg_read_scanlines(&cinfo, scan_line, 1);
			for (col = 0; col < image_width; col++) {
				if (bytes_per_pixel == 2) {
					if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
						colour.set_RGB(scan_line[0][col], scan_line[0][col], scan_line[0][col]);
					} else {
						colour.set_RGB(scan_line[0][col * 3], scan_line[0][col * 3 + 1], scan_line[0][col * 3 + 2]);
					}
					*(word *)image_ptr = RGB_to_texture_pixel(colour);
					image_ptr += 2;
				} else {
					if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
						pixel grayscale_pixel = scan_line[0][col];
						*(pixel *)image_ptr = 0xFF000000 | (grayscale_pixel << 16) | (grayscale_pixel << 8) | grayscale_pixel;
					} else {
						*(pixel *)image_ptr = 0xFF000000 | (scan_line[0][col * 3] << 16) | (scan_line[0][col * 3 + 1] << 8) | scan_line[0][col * 3 + 2]; 
					}
					image_ptr += 4;
				}
			}
		}

		// Clean up after decompression.

		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
	}
	catch (char *message) {
		jpeg_destroy_decompress(&cinfo);
		image_error(message);
	}

	// Initialise a pixmap containing the image just read.

	pixmap_list->bytes_per_pixel = bytes_per_pixel;
	pixmap_list->image_ptr = buffer_ptr;
	pixmap_list->image_size = buffer_size;
	pixmap_list->width = image_width;
	pixmap_list->height = image_height;
	pixmap_list->transparent_index = -1;
	pixmap_list->delay_ms = 1;
	pixmaps++;
}

//==============================================================================
// PNG loader functions.
//==============================================================================

void read_png_file(png_structp png_ptr, png_bytep data, png_size_t length)
{
	if (read_file(data, length) != length) {
		png_error(png_ptr, "Unable to read PNG file\n");
	}
}

void
load_PNG()
{
	RGBcolour colour;

	// Initialise the number of pixmaps loaded, the transparent flag, the loop flag, and the number of colours.

	pixmaps = 0;
	transparent = true;
	bytes_per_pixel = hardware_acceleration ? 4 : 2;
	texture_loops = false;
	total_time_ms = 1;
	colours = 0;

	// Initialise the pointers to various structures.

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_infop end_info_ptr = NULL;
	imagebyte *image_data = NULL;
	png_bytep *row_pointers = NULL;

	// Create the structures needed to read the PNG file.

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		goto got_error;
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		goto got_error;
	}
	end_info_ptr = png_create_info_struct(png_ptr);
	if (!end_info_ptr) {
		goto got_error;
	}

	// Create the error handler.

	if (setjmp(png_jmpbuf(png_ptr))) {
		goto got_error;
	}

	// Set up the read function.

	png_set_read_fn(png_ptr, NULL, read_png_file);

	// Tell libpng that we've already read the header.

	png_set_sig_bytes(png_ptr, 8);

	// Read the PNG image info.

	png_read_info(png_ptr, info_ptr);
	image_width = png_get_image_width(png_ptr, info_ptr);
	image_height = png_get_image_height(png_ptr, info_ptr);
	int colour_type = png_get_color_type(png_ptr, info_ptr);
	int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	// Set various transformations in order to obtain a 32-bit RGBA image.

	if (bit_depth == 16) {
		png_set_strip_16(png_ptr);
	}
	if (colour_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}
	if (colour_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png_ptr);
	}
	if (colour_type == PNG_COLOR_TYPE_RGB || colour_type == PNG_COLOR_TYPE_GRAY || colour_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}
	if (colour_type == PNG_COLOR_TYPE_GRAY || colour_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png_ptr);
	}
	if (colour_type == PNG_COLOR_TYPE_RGB ||
		colour_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		png_set_bgr(png_ptr);
	}
	png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	// Create a buffer to hold the transformed image data, and an array of row pointers to point into the image data,
	// then read the image.

	int row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	NEWARRAY(image_data, imagebyte, image_width * image_height * 4);
	if (image_data == NULL) {
		goto got_error;
	}
	NEWARRAY(row_pointers, png_bytep, image_height);
	if (row_pointers == NULL) {
		goto got_error;
	}
	for (int y = 0; y < image_height; y++) {
		row_pointers[y] = image_data + y * row_bytes;
	}
	png_read_image(png_ptr, row_pointers);

	// If hardware acceleration is not active, create a buffer to hold a 16-bit image, 
	// and transform the 32-bit image to 16-bit.

	if (bytes_per_pixel == 2) {
		imagebyte *buffer_ptr;
		NEWARRAY(buffer_ptr, imagebyte, image_width * image_height * 2);
		if (buffer_ptr == NULL) {
			goto got_error;
		}
		imagebyte *source_image_ptr = image_data;
		imagebyte *target_image_ptr = buffer_ptr;
		for (int row = 0; row < image_height; row++) {
			for (int col = 0; col < image_width; col++) {
				imagebyte blue = *source_image_ptr++;
				imagebyte green = *source_image_ptr++;
				imagebyte red = *source_image_ptr++;
				imagebyte alpha = *source_image_ptr++;
				colour.set_RGB(red, green, blue, alpha);
				*(word *)target_image_ptr = RGB_to_texture_pixel(colour);
				target_image_ptr += 2;
			}
		}
		DELARRAY(image_data, imagebyte, image_width * image_height * 4);
		image_data = buffer_ptr;
	}

	// Initialise a pixmap containing the image just read.  Note we set a transparent index
	// to ensure that the software renderer uses transparent spans.

	pixmap_list->bytes_per_pixel = bytes_per_pixel;
	pixmap_list->image_ptr = image_data;
	pixmap_list->image_size = image_width * image_height * bytes_per_pixel;
	pixmap_list->width = image_width;
	pixmap_list->height = image_height;
	pixmap_list->transparent_index = 0;
	pixmap_list->delay_ms = 1;
	pixmaps++;

	// Free all temporary data.

	if (row_pointers) {
		DELARRAY(row_pointers, png_bytep, image_height);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
	return;

got_error:
	if (image_data) {
		DELARRAY(image_data, imagebyte, image_width * image_height * 4);
	}
	if (row_pointers) {
		DELARRAY(row_pointers, png_bytep, image_height);
	}
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info_ptr);
	image_error("Unable to read PNG file");
}

//==============================================================================
// Load an image.
//==============================================================================

void
scale_pixmap(pixmap *pixmap_ptr, int new_image_width, int new_image_height, float scale)
{
	// Allocate a new image, and copy a scaled version of the old image to it.

	int new_image_size = new_image_width * new_image_height * pixmap_ptr->bytes_per_pixel;
	imagebyte *new_image;
	NEWARRAY(new_image, imagebyte, new_image_size);
	imagebyte *old_image_ptr = pixmap_ptr->image_ptr, *new_image_ptr = new_image;
	float y = 0.0f;
	for (int row = 0; row < new_image_height; row++, y += scale) {
		imagebyte *row_ptr = old_image_ptr + (int)y * pixmap_ptr->width * pixmap_ptr->bytes_per_pixel;
		float x = 0.0f;
		for (int column = 0; column < new_image_width; column++, x += scale) {
			switch (pixmap_ptr->bytes_per_pixel) {
			case 1:
				*new_image_ptr++ = *(row_ptr + (int)x);
				break;
			case 2:
				*((word *)new_image_ptr) = *((word *)row_ptr + (int)x);
				new_image_ptr += 2;
				break;
			case 4:
				*((pixel *)new_image_ptr) = *((pixel *)row_ptr + (int)x);
				new_image_ptr += 4;
			}
		}
	}

	// Delete the old image, and store the new image data in the pixmap.

	DELARRAY(pixmap_ptr->image_ptr, imagebyte, pixmap_ptr->image_size);
	pixmap_ptr->image_ptr = new_image;
	pixmap_ptr->image_size = new_image_size;
	pixmap_ptr->width = new_image_width;
	pixmap_ptr->height = new_image_height;
}

//------------------------------------------------------------------------------
// Load an image into an existing texture object from the given URL or local
// file.  If the load fails, return FALSE.
//------------------------------------------------------------------------------

bool
load_image(const char *URL, const char *file_path, texture *texture_ptr)
{
	int index;

	// Attempt to open the image file.  If there is no URL specified, it is
	// assumed this is a image file found in a currently open blockset.

	if (URL != NULL) {
		if (!push_file(file_path, URL, false)) {
			warning("Unable to load image %s: File not found", URL);
			return(false);
		}
	} else {
		if (!push_zip_file(file_path, false)) {
			warning("Unable to load image file %s: File not found", file_path);
			return(false);
		}
	}

	// If the file begins with a GIF header, load the rest of the file as a GIF.
	// If it begins with a PNG header, load the rest of the file as a PNG.
	// Otherwise rewind the file and attmept to load the file as a JPEG.

	try {
		byte header[8];
		int header_size;

		header_size = read_file(header, 6);
		if (is_GIF_file(header, header_size)) {
			load_GIF();
		} else {
			header_size += read_file(header + 6, 2);
			if (!png_sig_cmp(header, 0, header_size)) {
				load_PNG();
			} else {
				rewind_file();
				load_JPEG();
			}
		}
		texture_ptr->bytes_per_pixel = bytes_per_pixel;
	}
	catch (char *) {
		pop_file();
		if (URL)
			warning("URL %s is not a GIF, PNG or JPEG image", URL);
		else
			warning("File %s is not a GIF, PNG or JPEG image", file_path);
		return(false);
	}

	// Now process the image(s) read from the file.

	try {

		// If the image exceeds the maximum texture size, scale all of the pixmaps to fit.

		if (image_width > max_texture_size || image_height > max_texture_size) {
			float scale;
			int new_image_width, new_image_height;
			if (image_width >= image_height) {
				scale = (float)image_width / (float)max_texture_size;
				new_image_width = max_texture_size;
				new_image_height = (int)(image_height / scale);
			} else {
				scale = (float)image_height / (float)max_texture_size;
				new_image_width = (int)(image_width / scale);
				new_image_height = max_texture_size;
			}
			for (int i = 0; i < pixmaps ; i++) {
				scale_pixmap(&pixmap_list[i], new_image_width, new_image_height, scale);
			}
			image_width = new_image_width;
			image_height = new_image_height;
		}

		// Initialise the texture object.

		texture_ptr->width = image_width;
		texture_ptr->height = image_height;
		texture_ptr->pixmaps = pixmaps;
		texture_ptr->colours = colours;
		texture_ptr->transparent = transparent;
		texture_ptr->loops = texture_loops;
		texture_ptr->total_time_ms = total_time_ms;

		// Create the pixmap list for the texture.

		NEWARRAY(texture_ptr->pixmap_list, pixmap, pixmaps);
		if (texture_ptr->pixmap_list == NULL)
			image_memory_error("texture pixmap list");

		// Copy the pixmaps into the texture object, then set the size indices
		// for the pixmaps.

		for (index = 0; index < pixmaps; index++) {
			texture_ptr->pixmap_list[index] = pixmap_list[index];
			pixmap_list[index].image_ptr = NULL;
		}
		set_size_indices(texture_ptr);

		// If this is an 8-bit texture, create the RGB palette, and one of 
		// either the texture palette or display palette.

		if (texture_ptr->bytes_per_pixel == 1) {
			if (!texture_ptr->create_RGB_palette(colours, BRIGHTNESS_LEVELS, RGB_palette))
				image_memory_error("texture RGB palette");
			if (hardware_acceleration) {
				if (!texture_ptr->create_texture_palette_list())
					image_memory_error("texture palette");
			} else {
				if (!texture_ptr->create_display_palette_list())
					image_memory_error("display palette");
			}
		}
		
		// Close the image file.

		pop_file();
		return(true);
	}

	// If an error occurred, write the error message to the error log as a
	// warning, then return FALSE.

	catch (char *message) {
		pop_file();
		if (URL)
			warning("Unable to load image URL %s: %s", URL, message);
		else
			warning("Unable to load image file %s: %s", file_path, message);
		return(false);
	}
}