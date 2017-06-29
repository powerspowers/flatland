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
	pixmap_ptr->image_is_16_bit = false;
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
	byte header[6];
	bool has_global_colourmap;
	int index;

	// Initialise the number of pixmaps loaded, the transparent flag, and the
	// loop flag.

	pixmaps = 0;
	transparent = false;
	texture_loops = false;

	// Initialise the pointer to the RGB palette.

	RGB_palette = (RGBcolour *)global_colourmap;

	// Read and verify the header.

	read_block(header, 6);
	if (_strnicmp((char *)header, id87, 6) &&
	    _strnicmp((char *)header, id89, 6))
		image_error("Not a GIF file");
	
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

	// Initialise the number of pixmaps loaded, the transparent flag, and the
	// loop flag.

	pixmaps = 0;
	transparent = false;
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

		buffer_size = image_width * image_height * 2;
		NEWARRAY(buffer_ptr, imagebyte, buffer_size);
		if (buffer_ptr == NULL)
			image_memory_error("JPEG image");

		// Read the pixel components one scan line at a time, and convert them
		// to 16-bit pixels in texture pixel format.

		image_ptr = buffer_ptr;
		for (row = 0; row < image_height; row++) {
			jpeg_read_scanlines(&cinfo, scan_line, 1);
			for (col = 0; col < image_width; col++) {
				if (cinfo.jpeg_color_space == JCS_GRAYSCALE)
					colour.set_RGB(scan_line[0][col], scan_line[0][col],
						scan_line[0][col]);
				else
					colour.set_RGB(scan_line[0][col * 3], 
						scan_line[0][col * 3 + 1], scan_line[0][col * 3 + 2]);
				*(word *)image_ptr = RGB_to_texture_pixel(colour);
				image_ptr += 2;
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

	pixmap_list->image_is_16_bit = true;
	pixmap_list->image_ptr = buffer_ptr;
	pixmap_list->image_size = buffer_size;
	pixmap_list->width = image_width;
	pixmap_list->height = image_height;
	pixmap_list->transparent_index = -1;
	pixmap_list->delay_ms = 1000;
	pixmaps++;
}

//------------------------------------------------------------------------------
// Save the frame buffer as a JPEG file of the given size.
//------------------------------------------------------------------------------

bool
save_frame_buffer_to_JPEG(int width, int height, const char *file_path)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	byte *image_buffer, *image_row_ptr;
	JSAMPROW scan_line[1];
	FILE *fp;
	int row;

	// Set up our own error handler for fatal errors.

	cinfo.err = jpeg_std_error(&jerr);
	jerr.error_exit = my_error_exit;
	image_buffer = NULL;
	try {

		// Allocate and initialise a JPEG compression object.

		jpeg_create_compress(&cinfo);

		// Set the destination to be a standard file.

		if ((fp = fopen(file_path, "wb")) == NULL)
			error("Unable to open file %s for writing", file_path);
		jpeg_stdio_dest(&cinfo, fp);

		// Set the image info.

		cinfo.image_width = width;
		cinfo.image_height = height;
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_RGB;
		jpeg_set_defaults(&cinfo);

		// Allocate the image buffer.

		NEWARRAY(image_buffer, byte, width * height * 3);
		if (image_buffer == NULL)
			memory_error("JPEG image"); 

		// Convert the frame buffer into a 24-bit image buffer.

		if (!save_frame_buffer(image_buffer, width, height))
			error("Unable to take snapshot of frame buffer");

		// Write the image buffer to the JPEG file.

		jpeg_start_compress(&cinfo, TRUE);
		image_row_ptr = image_buffer;
		for (row = 0; row < height; row++) {
			scan_line[0] = image_row_ptr;
			image_row_ptr += width * 3;
			jpeg_write_scanlines(&cinfo, scan_line, 1);
		}

		// Clean up after decompression.

		jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);
		DELARRAY(image_buffer, byte, width * height * 3);
		fclose(fp);
		return(true);
	}
	catch (char *message) {
		jpeg_destroy_compress(&cinfo);
		if (image_buffer != NULL)
			DELARRAY(image_buffer, byte, width * height * 3);
		image_error(message);
	}
	return(false);
}

//==============================================================================
// Load an image.
//==============================================================================

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

	// Attempt to load the file as a GIF image.  If this fails, rewind the file
	// and attempt to load it as a JPG image.  If this also fails, generate a
	// warning message and return a failure status.

	try {
		load_GIF();
		texture_ptr->is_16_bit = false;
	}
	catch (char *) {
		try {
			rewind_file();
			load_JPEG();
			texture_ptr->is_16_bit = true;
		}
		catch (char *) {
			pop_file();
			if (URL)
				warning("URL %s is not a GIF or JPEG image", URL);
			else
				warning("File %s is not a GIF or JPEG image", file_path);
			return(false);
		}
	}

	// Do everything else in a try block, so that we can catch errors and
	// report them as warnings instead.

	try {

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
		if (image_width <= 256 && image_height <= 256)
			set_size_indices(texture_ptr);

		// If this is an 8-bit texture, create the RGB palette, and one of 
		// either the texture palette or display palette.

		if (!texture_ptr->is_16_bit) {
			if (!texture_ptr->create_RGB_palette(colours, BRIGHTNESS_LEVELS, 
				RGB_palette))
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

//------------------------------------------------------------------------------
// Load a GIF image from the topmost open file, returning a new texture object.
//------------------------------------------------------------------------------

texture *
load_GIF_image(void)
{
	texture *texture_ptr;

	// Load the GIF.

	texture_ptr = NULL;
	try {
		int index;
		
		// Load the GIF.

		load_GIF();

		// Create the texture object and initialise it.

		NEW(texture_ptr, texture);
		if (texture_ptr == NULL)
			image_memory_error("texture");
		texture_ptr->is_16_bit = false;
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
		if (image_width <= 256 && image_height <= 256)
			set_size_indices(texture_ptr);

		// Create the RGB palette.

		if (!texture_ptr->create_RGB_palette(colours, 1, RGB_palette))
			image_memory_error("texture RGB palette");

		// Return the pointer to the texture.

		return(texture_ptr);
	}
	
	// If an error occurred, just delete the texture object if it exists and
	// return a NULL pointer.

	catch (char *) {
		if (texture_ptr != NULL)
			DEL(texture_ptr, texture);
		return(NULL);
	}
}