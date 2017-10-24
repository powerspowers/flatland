//******************************************************************************
// $Header$
// Copyright (C) 1998-2002 Flatland Online Inc.
// All Rights Reserved. 
//******************************************************************************

#define STRICT
#define INITGUID
#define D3D_OVERLOADS

#include <windows.h>
#include <windowsx.h>
#include "resource.h"

#if SYMBOLIC_DEBUG
#include "Dbghelp.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <process.h>
#include <direct.h>

#include <commdlg.h>
#include <commctrl.h>
#include <objbase.h>
#include <cguid.h>
#include <Urlmon.h>
#include <Wininet.h>

#include <d3d11.h>
#include <DirectXColors.h>
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <d3dcompiler.h>
#include <ddraw.h>
#include <dsound.h>

using namespace DirectX;

#ifdef STREAMING_MEDIA
#include <mmstream.h>
#include <amstream.h>
#include <ddstream.h>
#endif
#include "Classes.h"
#include "Fileio.h"
#include "Image.h"
#include "Light.h"
#include "Main.h"
#include "Memory.h"
#include "Parser.h"
#include "Platform.h"
#include "Plugin.h"
#include "Render.h"
#include "Spans.h"
#include "Utils.h"

//==============================================================================
// Global definitions.
//==============================================================================

//------------------------------------------------------------------------------
// Event class.
//------------------------------------------------------------------------------

// Default constructor initialises event handle and value.

event::event()
{
	event_handle = NULL;
	event_value = false;
}

// Default destructor does nothing.

event::~event()
{
}

// Method to create the event handle.

void
event::create_event(void)
{
	event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
}

// Method to destroy the event handle.

void
event::destroy_event(void)
{
	if (event_handle != NULL)
		CloseHandle(event_handle);
}

// Method to send an event.

void
event::send_event(bool value)
{
	event_value = value;
	SetEvent(event_handle);
}

// Method to reset an event.

void
event::reset_event(void)
{
	ResetEvent(event_handle);
	event_value = false;
}

// Method to check if an event has been sent.

bool
event::event_sent(void)
{
	return(WaitForSingleObject(event_handle, 0) != WAIT_TIMEOUT);
}

// Method to wait for an event.

bool
event::wait_for_event(void)
{
	WaitForSingleObject(event_handle, INFINITE);
	return(event_value);
}

//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------

// Application directory.

string app_dir;

// Display, builder and texture pixel formats.

pixel_format display_pixel_format;
pixel_format builder_pixel_format;
pixel_format texture_pixel_format;

// Largest texture size permitted.

int max_texture_size;

// Display properties.

int window_width;
int window_height;
float half_window_width;
float half_window_height;

// Flag indicating whether the main window is ready.

bool main_window_ready;

// Flag indicating whether sound is on.

bool sound_on;

// // Selected cached blockset and loaded blockset (for builder window).

cached_blockset *selected_cached_blockset_ptr;
blockset *loaded_blockset_ptr;

//==============================================================================
// Local definitions.
//==============================================================================

//------------------------------------------------------------------------------
// Cursor class.
//------------------------------------------------------------------------------

// Cursor class definition.

struct cursor {
	HCURSOR handle;
	bitmap *mask_bitmap_ptr;
	bitmap *image_bitmap_ptr;
	int hotspot_x, hotspot_y;

	cursor();
	~cursor();
};

// Default constructor initialises the bitmap pointers.

cursor::cursor()
{
	mask_bitmap_ptr = NULL;
	image_bitmap_ptr = NULL;
}

// Default destructor deletes the bitmaps, if they exist.

cursor::~cursor()
{
	if (mask_bitmap_ptr != NULL)
		DEL(mask_bitmap_ptr, bitmap);
	if (image_bitmap_ptr != NULL)
		DEL(image_bitmap_ptr, bitmap);
}

//------------------------------------------------------------------------------
// Icon class.
//------------------------------------------------------------------------------

// Icon class definition.

struct icon {
	texture *texture0_ptr;
	texture *texture1_ptr;
	int width, height;

	icon();
	~icon();
};

// Default constructor initialises the texture pointers.

icon::icon()
{
	texture0_ptr = NULL;
	texture1_ptr = NULL;
}

// Default destructor deletes the textures, if they exist.

icon::~icon()
{
	if (texture0_ptr != NULL)
		DEL(texture0_ptr, texture);
	if (texture1_ptr != NULL)
		DEL(texture1_ptr, texture);
}

//------------------------------------------------------------------------------
// Miscellaneous classes.
//------------------------------------------------------------------------------

// Hardware texture class.

struct hardware_texture {
	int image_dimensions;
	ID3D11Texture2D *d3d_texture_ptr;
	ID3D11ShaderResourceView *d3d_shader_resource_view_ptr;

	hardware_texture()
	{
		d3d_texture_ptr = NULL;
		d3d_shader_resource_view_ptr = NULL;
	}

	~hardware_texture()
	{
		if (d3d_shader_resource_view_ptr) {
			d3d_shader_resource_view_ptr->Release();
		}
		if (d3d_texture_ptr) {
			d3d_texture_ptr->Release();
		}
	}
};

// Hardware vertex class.

struct hardware_vertex {
	XMFLOAT3 position;
	XMFLOAT2 texture_coords;
	XMFLOAT4 diffuse_colour;

	hardware_vertex(float x, float y, float z, float u, float v, RGBcolour *diffuse_colour_ptr, float diffuse_alpha) {
		position = XMFLOAT3(x, y, z);
		texture_coords = XMFLOAT2(u, v);
		diffuse_colour = XMFLOAT4(diffuse_colour_ptr->red, diffuse_colour_ptr->green, diffuse_colour_ptr->blue, diffuse_alpha);
	}
};

// Hardware constant buffers.

struct hardware_fog_constant_buffer {
	int fog_style;
	float fog_start;
	float fog_end;
	float fog_density;
	XMFLOAT4 fog_colour;
};

struct hardware_matrix_constant_buffer {
	XMMATRIX projection;
};

// Structure to hold the colour palette.

struct MYLOGPALETTE {
	WORD palVersion;
	WORD palNumEntries; 
    PALETTEENTRY palPalEntry[256];
};

// Structure to hold bitmap info for a DIB section.

struct MYBITMAPINFO {
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[256];
};

//------------------------------------------------------------------------------
// Vertex and pixel shaders.
//------------------------------------------------------------------------------

char *colour_vertex_shader_source =
	"cbuffer fog_constant_buffer : register(b0) {\n"
	"	int fog_style;\n"
	"	float fog_start;\n"
	"	float fog_end;\n"
	"	float fog_density;\n"
	"	float4 fog_colour;\n"
	"};\n"
	"cbuffer matrix_constant_buffer : register(b1) {\n"
	"	matrix projection;\n"
	"};\n"
	"struct VS_INPUT {\n"
	"	float4 pos : POSITION;\n"
	"	float2 tex : TEXCOORD0;\n"
	"   float4 colour : COLOUR;\n"
	"};\n"
	"struct PS_INPUT {\n"
	"	float4 pos : SV_POSITION;\n"
	"   float4 colour : COLOUR;\n"
	"	float fog_factor : FOG;\n"
	"};\n"
	"PS_INPUT VS(VS_INPUT input) {\n"
	"	PS_INPUT output = (PS_INPUT)0;\n"
	"	output.pos = mul(input.pos, projection);\n"
	"   output.colour = input.colour;\n"
	"	if (fog_style) {\n"
	"		output.fog_factor = saturate((fog_end - length(input.pos)) / (fog_end - fog_start));\n"
	"	}\n"
	"	return output;\n"
	"}\n";

char *colour_pixel_shader_source =
	"cbuffer fog_constant_buffer : register(b0) {\n"
	"	int fog_style;\n"
	"	float fog_start;\n"
	"	float fog_end;\n"
	"	float fog_density;\n"
	"	float4 fog_colour;\n"
	"};\n"
	"struct PS_INPUT {\n"
	"	float4 pos : SV_POSITION;\n"
	"   float4 colour : COLOUR;\n"
	"	float fog_factor : FOG;\n"
	"};\n"
	"float4 PS(PS_INPUT input) : SV_Target {\n"
	"   if (fog_style && input.colour.a > 0.0) {\n"
	"		return (input.fog_factor * input.colour) + ((1.0 - input.fog_factor) * fog_colour);\n"
	"	}\n"
	"	return input.colour;\n"
	"}\n";

char *texture_vertex_shader_source =
	"cbuffer fog_constant_buffer : register(b0) {\n"
	"	int fog_style;\n"
	"	float fog_start;\n"
	"	float fog_end;\n"
	"	float4 fog_colour;\n"
	"	float fog_density;\n"
	"};\n"
	"cbuffer matrix_constant_buffer : register(b1) {\n"
	"	matrix projection;\n"
	"};\n"
	"struct VS_INPUT {\n"
	"	float4 pos : POSITION;\n"
	"	float2 tex : TEXCOORD0;\n"
	"   float4 colour : COLOUR;\n"
	"};\n"
	"struct PS_INPUT {\n"
	"	float4 pos : SV_POSITION;\n"
	"	float2 tex : TEXCOORD0;\n"
	"   float4 colour : COLOUR;\n"
	"	float fog_factor : FOG;\n"
	"};\n"
	"PS_INPUT VS(VS_INPUT input) {\n"
	"	PS_INPUT output = (PS_INPUT)0;\n"
	"	output.pos = mul(input.pos, projection);\n"
	"	output.tex = input.tex;\n"
	"   output.colour = input.colour;\n"
	"	if (fog_style) {\n"
	"		output.fog_factor = saturate((fog_end - length(input.pos)) / (fog_end - fog_start));\n"
	"	}\n"
	"	return output;\n"
	"}\n";

char *texture_pixel_shader_source =
	"cbuffer fog_constant_buffer : register(b0) {\n"
	"	int fog_style;\n"
	"	float fog_start;\n"
	"	float fog_end;\n"
	"	float4 fog_colour;\n"
	"	float fog_density;\n"
	"};\n"
	"Texture2D tx_diffuse : register(t0);\n"
	"SamplerState sam_linear : register(s0);\n"
	"struct PS_INPUT {\n"
	"	float4 pos : SV_POSITION;\n"
	"	float2 tex : TEXCOORD0;\n"
	"   float4 colour : COLOUR;\n"
	"	float fog_factor : FOG;\n"
	"};\n"
	"float4 PS(PS_INPUT input) : SV_Target {\n"
	"	float4 texture_colour = tx_diffuse.Sample(sam_linear, input.tex) * input.colour;\n"
	"   if (fog_style && texture_colour.a > 0.0) {\n"
	"		return (input.fog_factor * texture_colour) + ((1.0 - input.fog_factor) * fog_colour);\n"
	"	}\n"
	"	return texture_colour;\n"
	"}\n";

//------------------------------------------------------------------------------
// Local variables.
//------------------------------------------------------------------------------

// Display depth.

static int display_depth;

// Pixel mask table for component sizes of 1 to 8 bits.

static int pixel_mask_table[8] = {
	0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF
};

// Dither tables.

static byte *dither_table[4];
static byte *dither00, *dither01, *dither10, *dither11;

// Lighting tables.

static pixel *light_table[BRIGHTNESS_LEVELS];

// The standard colour palette, and the table used to get the index of a colour in the 6x6x6 colour cube.

static HPALETTE standard_palette_handle;
static RGBcolour standard_RGB_palette[256];
static byte colour_index[216];

// Colour component masks.

static pixel red_comp_mask;
static pixel green_comp_mask;
static pixel blue_comp_mask;
static pixel alpha_comp_mask;

// Flag indicating whether a label is currently visible, the label text,
// the texture to hold it, and the width of the current label.

static bool label_visible;
static string label_text;
static texture *label_texture_ptr;
static int label_width;

// Title text, and texture to hold it.

static string title_text;
static texture *title_texture_ptr;

// Movement cursor IDs.

#define MOVEMENT_CURSORS	8
static int movement_cursor_ID_list[MOVEMENT_CURSORS] = { 
	IDC_ARROW_N,	IDC_ARROW_NE,	IDC_ARROW_E,	IDC_ARROW_SE,
	IDC_ARROW_S,	IDC_ARROW_SW,	IDC_ARROW_W,	IDC_ARROW_NW
};

// Handles to available cursors.

static HCURSOR movement_cursor_handle_list[MOVEMENT_CURSORS];
static HCURSOR hand_cursor_handle;
static HCURSOR arrow_cursor_handle;
static HCURSOR crosshair_cursor_handle;

// Pointers to available cursors.

static cursor *movement_cursor_ptr_list[MOVEMENT_CURSORS];
static cursor *hand_cursor_ptr;
static cursor *arrow_cursor_ptr;
static cursor *crosshair_cursor_ptr;

// Pointer to current cursor.

static cursor *curr_cursor_ptr;

// DirectDraw data.

static LPDIRECTDRAW ddraw_object_ptr;
static LPDIRECTDRAWSURFACE ddraw_primary_surface_ptr;
static LPDIRECTDRAWSURFACE ddraw_frame_buffer_surface_ptr;
static LPDIRECTDRAWCLIPPER ddraw_clipper_ptr;

// Direct3D data.

static IDXGISwapChain *d3d_swap_chain_ptr;
static ID3D11Device *d3d_device_ptr;
static ID3D11DeviceContext *d3d_device_context_ptr;
static bool main_render_target_selected;
static ID3D11RenderTargetView *d3d_main_render_target_view_ptr;		// Render target view for main window.
static ID3D11Texture2D *d3d_main_depth_stencil_texture_ptr;			// Depth/stencil texture for main window.
static ID3D11DepthStencilView *d3d_main_depth_stencil_view_ptr;		// Depth/stencil view for main window.
static D3D11_VIEWPORT d3d_main_viewport;							// Viewport for main window.
static ID3D11Texture2D *d3d_builder_render_target_texture_ptr;		// Render target texture used to create builder icons.
static ID3D11RenderTargetView *d3d_builder_render_target_view_ptr;	// Render target view used to create builder icons.
static ID3D11Texture2D *d3d_builder_staging_texture_ptr;			// Staging texture used to create builder icons.
static ID3D11Texture2D *d3d_builder_depth_stencil_texture_ptr;		// Depth/stencil texture used to create builder icons.
static ID3D11DepthStencilView *d3d_builder_depth_stencil_view_ptr;	// Depth/stencil view used to create builder icons.
static D3D11_VIEWPORT d3d_builder_viewport;							// Viewport used to create builder icons.
static ID3D11DepthStencilState *d3d_3D_depth_stencil_state_ptr;
static ID3D11DepthStencilState *d3d_2D_depth_stencil_state_ptr;
static ID3D11VertexShader *d3d_colour_vertex_shader_ptr;
static ID3D11VertexShader *d3d_texture_vertex_shader_ptr;
static ID3D11InputLayout *d3d_vertex_layout_ptr;
#define MAX_VERTICES 256
static ID3D11Buffer *d3d_vertex_buffer_ptr;
static ID3D11PixelShader *d3d_colour_pixel_shader_ptr;
static ID3D11PixelShader *d3d_texture_pixel_shader_ptr;
static ID3D11BlendState *d3d_blend_state_ptr;
static ID3D11RasterizerState *d3d_rasterizer_state_ptr;
static ID3D11SamplerState *d3d_sampler_state_ptr;
static ID3D11Buffer *d3d_constant_buffer_list[2];

// Intermediate 16-bit frame buffer used by 8-bit display depth.

static byte *intermediate_frame_buffer_ptr;

// 32-bit frame buffer used to create builder icons.

static byte *builder_frame_buffer_ptr;

// Current frame buffer being used by the software renderer,
// along with its row pitch (in bytes), it's pixel depth (in bites),
// and a pointer to its pixel format.

static byte *frame_buffer_ptr;
static int frame_buffer_row_pitch;
static int frame_buffer_depth;
static pixel_format *frame_buffer_pixel_format_ptr;

// Builder variables.

static blockset *selected_blockset_ptr;

// Private sound data.

static LPDIRECTSOUND dsound_object_ptr;

#ifdef STREAMING_MEDIA

// Private streaming media data.

#define PLAYER_UNAVAILABLE		1
#define	STREAM_UNAVAILABLE		2
#define	STREAM_STARTED			3

static int unscaled_video_width, unscaled_video_height;
static int video_pixel_format;
static event stream_opened;
static event terminate_streaming_thread;
static event wmp_download_requested;

// Private streaming media data specific to WMP.

#define	AUDIO_PACKET_SIZE			5000
#define MAX_AUDIO_PACKETS			10
#define HALF_AUDIO_PACKETS			5
#define SOUND_BUFFER_SIZE			AUDIO_PACKET_SIZE * MAX_AUDIO_PACKETS

static IMultiMediaStream *global_stream_ptr;
static HANDLE end_of_stream_handle;
static bool streaming_video_available;
static IMediaStream *primary_video_stream_ptr;
static IDirectDrawMediaStream *ddraw_stream_ptr;
static IDirectDrawStreamSample *video_sample_ptr;
static LPDIRECTDRAWSURFACE video_surface_ptr;
static DDPIXELFORMAT ddraw_video_pixel_format;
static event video_frame_available;

// Streaming thread handle.

static unsigned long streaming_thread_handle;

#endif // STREAMING MEDIA

// App window data.

#define SPOT_URL_LABEL_WIDTH	100
#define SPOT_URL_BAR_HEIGHT		20

static HINSTANCE app_instance_handle;
static HWND app_window_handle;
static HWND spot_URL_label_handle;
static HWND spot_URL_edit_box_handle;
static HWND status_bar_handle;
static void (*quit_callback_ptr)();

// Main window data.

static HWND main_window_handle;
static void (*key_callback_ptr)(bool key_down, byte key_code);
static void (*mouse_callback_ptr)(int x, int y, int button_code);
static void (*timer_callback_ptr)(void);
static void (*resize_callback_ptr)(void *window_handle, int width, int height);
static void (*display_callback_ptr)(void);

// New spot window data.

static HWND new_spot_window_handle;

// Light window data.

static HWND light_window_handle;
static HWND light_slider_handle;
static void (*light_callback_ptr)(float brightness, bool window_closed);

// Options window data.

static HWND options_window_handle;
static HWND view_radius_slider_handle;
static HWND move_rate_slider_handle;
static HWND turn_rate_slider_handle;
static void (*options_callback_ptr)(int option_ID, int option_value);

// About and help window data.

static HWND about_window_handle;
static HWND help_window_handle;
static HFONT bold_font_handle;
static HFONT symbol_font_handle;

// Blockset manager window data.

static HWND blockset_manager_window_handle;
static HWND blockset_list_view_handle;
static HWND blockset_update_button_handle;
static HWND blockset_delete_button_handle;
static HWND update_period_spin_control_handle;

// Builder window data.

static HWND builder_window_handle;

// Macro for converting a point size into pixel units

#define	POINTS_TO_PIXELS(point_size) \
	MulDiv(point_size, GetDeviceCaps(dc_handle, LOGPIXELSY), 72)

// Macro to pass a mouse message to a handler.

#define HANDLE_MOUSE_MSG(window_handle, message, fn) \
	(fn)((window_handle), (message), LOWORD(lParam), HIWORD(lParam), \
		   (UINT)(wParam))

// Macros to pass a keyboard message to a handler.

#define HANDLE_KEYDOWN_MSG(window_handle, message, fn) \
    (fn)((window_handle), (UINT)(wParam), TRUE, (lParam & 0x40000000), \
		 (UINT)HIWORD(lParam))

#define HANDLE_KEYUP_MSG(window_handle, message, fn) \
    (fn)((window_handle), (UINT)(wParam), FALSE, FALSE, (UINT)HIWORD(lParam))

// Virtual key to key code table.

struct vk_to_keycode {
	UINT virtual_key;
	byte key_code;
};

static vk_to_keycode key_code_table[KEY_CODES] = {
	{VK_ESCAPE, ESC_KEY},
	{VK_SHIFT, SHIFT_KEY},
	{VK_CONTROL, CONTROL_KEY},
	{VK_MENU, ALT_KEY},
	{VK_SPACE, SPACE_BAR_KEY},
	{VK_BACK, BACK_SPACE_KEY},
	{VK_RETURN, ENTER_KEY},
	{VK_INSERT, INSERT_KEY},
	{VK_DELETE, DELETE_KEY},
	{VK_HOME, HOME_KEY},
	{VK_END, END_KEY},
	{VK_PRIOR, PAGE_UP_KEY},
	{VK_NEXT, PAGE_DOWN_KEY},
	{VK_UP, UP_KEY},
	{VK_DOWN, DOWN_KEY},
	{VK_LEFT, LEFT_KEY},
	{VK_RIGHT, RIGHT_KEY},
	{VK_NUMPAD0, NUMPAD_0_KEY},
	{VK_NUMPAD1, NUMPAD_1_KEY},
	{VK_NUMPAD2, NUMPAD_2_KEY},
	{VK_NUMPAD3, NUMPAD_3_KEY},
	{VK_NUMPAD4, NUMPAD_4_KEY},
	{VK_NUMPAD5, NUMPAD_5_KEY},
	{VK_NUMPAD6, NUMPAD_6_KEY},
	{VK_NUMPAD7, NUMPAD_7_KEY},
	{VK_NUMPAD8, NUMPAD_8_KEY},
	{VK_NUMPAD9, NUMPAD_9_KEY},
	{VK_ADD, NUMPAD_ADD_KEY},
	{VK_SUBTRACT, NUMPAD_SUBTRACT_KEY},
	{VK_MULTIPLY, NUMPAD_MULTIPLY_KEY},
	{VK_DIVIDE, NUMPAD_DIVIDE_KEY},
	{VK_DECIMAL, NUMPAD_PERIOD_KEY}
};

// Width of a linearly interpolated texture span.

#define SPAN_WIDTH			32
#define SPAN_SHIFT			5

// Two constants used to load immediate floating point values into floating
// point registers.

const float const_1 = 1.0;
const float fixed_shift = 65536.0;

// Assembly macro for fast float to fixed point conversion for texture (u,v)
// coordinates.

#define COMPUTE_UV(u,v,u_on_tz,v_on_tz,end_tz) __asm \
{ \
	__asm fld	fixed_shift \
	__asm fmul	end_tz \
	__asm fld	st(0) \
	__asm fmul	u_on_tz \
	__asm fistp	DWORD PTR u \
	__asm fmul  v_on_tz \
	__asm fistp DWORD PTR v \
}

// Assembly macro for drawing a possibly transparent 16-bit pixel.

#define DRAW_TRANSPARENT_PIXEL16(label) __asm \
{ \
	__asm mov edi, ebx \
	__asm and edi, ecx \
	__asm shr edi, FRAC_BITS \
	__asm mov eax, edx \
	__asm and eax, ecx \
	__asm and eax, INT_MASK \
	__asm shr eax, cl \
	__asm or  edi, eax \
	__asm mov eax, image_ptr \
	__asm mov ax, [eax + edi * 2] \
	__asm test ax, transparency_mask16 \
	__asm jz label \
	__asm mov [esi], ax \
} __asm { \
label: \
	__asm add ebx, delta_u \
	__asm add edx, delta_v \
	__asm add esi, 2 \
}

// Assembly macro for drawing a possibly transparent 24-bit pixel.

#define DRAW_TRANSPARENT_PIXEL24(label) __asm \
{ \
	__asm mov edi, ebx \
	__asm and edi, ecx \
	__asm shr edi, FRAC_BITS \
	__asm mov eax, edx \
	__asm and eax, ecx \
	__asm and eax, INT_MASK \
	__asm shr eax, cl \
	__asm or  edi, eax \
	__asm mov eax, image_ptr \
	__asm mov eax, [eax + edi * 4] \
	__asm test eax, transparency_mask24 \
	__asm jz label \
	__asm mov edi, [esi] \
	__asm and edi, 0xff000000 \
	__asm or edi, eax \
	__asm mov [esi], edi \
} __asm { \
label: \
	__asm add ebx, delta_u \
	__asm add edx, delta_v \
	__asm add esi, 3 \
}

// Assembly macro for drawing a possibly transparent 32-bit pixel.

#define DRAW_TRANSPARENT_PIXEL32(label) __asm \
{ \
	__asm mov edi, ebx \
	__asm and edi, ecx \
	__asm shr edi, FRAC_BITS \
	__asm mov eax, edx \
	__asm and eax, ecx \
	__asm and eax, INT_MASK \
	__asm shr eax, cl \
	__asm or  edi, eax \
	__asm mov eax, image_ptr \
	__asm mov eax, [eax + edi * 4] \
	__asm test eax, transparency_mask32 \
	__asm jz label \
	__asm mov [esi], eax \
} __asm { \
label: \
	__asm add ebx, delta_u \
	__asm add edx, delta_v \
	__asm add esi, 4 \
}

//==============================================================================
// Private functions.
//==============================================================================

//------------------------------------------------------------------------------
// Functions to display error messages that begin with common phrases.
//------------------------------------------------------------------------------

static void
failed_to(const char *message)
{
	diagnose("Failed to %s", message);
}

static void
failed_to_create(const char *message)
{
	diagnose("Failed to create the %s", message);
}

//------------------------------------------------------------------------------
// Blit the frame buffer onto the primary surface.  This is only used in
// software rendering mode.
//------------------------------------------------------------------------------

static void
blit_frame_buffer(void)
{
	POINT pos;
	RECT frame_buffer_surface_rect;
	RECT primary_surface_rect;
	HRESULT blt_result;

	// Set the frame buffer surface rectangle, and copy it to the primary
	// surface rectangle.

	frame_buffer_surface_rect.left = 0;
	frame_buffer_surface_rect.top = 0;
	frame_buffer_surface_rect.right = window_width;
	frame_buffer_surface_rect.bottom = window_height;
	primary_surface_rect = frame_buffer_surface_rect;

	// Offset the primary surface rectangle by the position of the main 
	// window's client area.

	pos.x = 0;
	pos.y = 0;
	ClientToScreen(main_window_handle, &pos);
	primary_surface_rect.left += pos.x;
	primary_surface_rect.top += pos.y;
	primary_surface_rect.right += pos.x;
	primary_surface_rect.bottom += pos.y;

	// Blit the frame buffer surface rectangle to the the primary surface.

	while (true) {
		blt_result = ddraw_primary_surface_ptr->Blt(&primary_surface_rect,
			ddraw_frame_buffer_surface_ptr, &frame_buffer_surface_rect, 0, NULL);
		if (blt_result == DD_OK || blt_result != DDERR_WASSTILLDRAWING)
			break;
	}
}

//------------------------------------------------------------------------------
// Create an 8-bit bitmap of the given dimensions, using the given palette.
//------------------------------------------------------------------------------

static bitmap *
create_bitmap(int width, int height, int colours, RGBcolour *RGB_palette,
			  pixel *palette, int transparent_index)
{
	bitmap *bitmap_ptr;
	HDC hdc;
	MYBITMAPINFO bitmap_info;
	int index;

	// Create the bitmap object.

	NEW(bitmap_ptr, bitmap);
	if (bitmap_ptr == NULL)
		return(NULL);

	// Initialise the bitmap info structure.

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = width;
	bitmap_info.bmiHeader.biHeight = -height;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 8;
	bitmap_info.bmiHeader.biSizeImage = 0;
	bitmap_info.bmiHeader.biXPelsPerMeter = 0;
	bitmap_info.bmiHeader.biYPelsPerMeter = 0;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;
	bitmap_info.bmiHeader.biCompression = BI_RGB;
	for (index = 0; index < colours; index++) {
		bitmap_info.bmiColors[index].rgbRed = (byte)RGB_palette[index].red;
		bitmap_info.bmiColors[index].rgbGreen = (byte)RGB_palette[index].green;
		bitmap_info.bmiColors[index].rgbBlue = (byte)RGB_palette[index].blue;
		bitmap_info.bmiColors[index].rgbReserved = 0;
	}

	// Create the bitmap image as a DIB section.

	hdc = GetDC(main_window_handle);
	bitmap_ptr->handle = CreateDIBSection(hdc, (BITMAPINFO *)&bitmap_info,
		DIB_RGB_COLORS, (void **)&bitmap_ptr->pixels, NULL, 0);
	ReleaseDC(main_window_handle, hdc);
	if (bitmap_ptr->handle == NULL) {
		DEL(bitmap_ptr, bitmap);
		return(NULL);
	}

	// If the bitmap image was created, fill in the remainder of the bitmap structure.

	bitmap_ptr->width = width;
	bitmap_ptr->height = height;
	bitmap_ptr->bytes_per_row = (width % 4) == 0 ? width : width + 4 - (width % 4);
	bitmap_ptr->colours = colours;
	bitmap_ptr->RGB_palette = RGB_palette;
	bitmap_ptr->palette = palette;
	bitmap_ptr->transparent_index = transparent_index;
	return(bitmap_ptr);
}

//------------------------------------------------------------------------------
// Create a bitmap and initialize it with the first pixmap of the given texture.
// The bitmap will use 32-bit pixels regardless of the pixel depth of the
// texture.
//------------------------------------------------------------------------------

bitmap *
create_bitmap_from_texture(texture *texture_ptr)
{
	HBITMAP bitmap_handle;
	byte *bitmap_pixels;
	bitmap *bitmap_ptr;
	HDC hdc;
	BITMAPINFO bitmap_info;

	// Initialise the bitmap info structure.

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = texture_ptr->width;
	bitmap_info.bmiHeader.biHeight = -texture_ptr->height;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biSizeImage = 0;
	bitmap_info.bmiHeader.biXPelsPerMeter = 0;
	bitmap_info.bmiHeader.biYPelsPerMeter = 0;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	// Create the bitmap image as a DIB section.

	hdc = GetDC(app_window_handle);
	bitmap_handle = CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, (void **)&bitmap_pixels, NULL, 0);
	ReleaseDC(app_window_handle, hdc);

	// Create the bitmap object.

	NEW(bitmap_ptr, bitmap);
	if (bitmap_ptr == NULL)
		return(NULL);

	// Fill in the bitmap structure (the parts we care about anyhow).

	bitmap_ptr->handle = bitmap_handle;
	bitmap_ptr->pixels = bitmap_pixels;
	bitmap_ptr->width = texture_ptr->width;
	bitmap_ptr->height = texture_ptr->height;
	bitmap_ptr->bytes_per_row = texture_ptr->width * 4;

	// Copy the pixel data from the first pixmap of the texture into the bitmap.  Note we only support 8-bit or 32-bit pixel pixmaps.

	pixmap *pixmap_ptr = texture_ptr->pixmap_list;
	switch (texture_ptr->bytes_per_pixel) {
	case 1:
		{
			byte *source_ptr = pixmap_ptr->image_ptr;
			pixel *target_ptr = (pixel *)bitmap_pixels;
			for (int row = 0; row < texture_ptr->height; row++) {
				for (int col = 0; col < texture_ptr->width; col++) {
					int pixel = *source_ptr++;
					if (pixel == pixmap_ptr->transparent_index) {
						*target_ptr++ = 0;
					} else {
						RGBcolour colour = texture_ptr->RGB_palette[pixel];
						*target_ptr++ = 0xFF000000 | ((byte)colour.red << 16) | ((byte)colour.green << 8) | (byte)colour.blue;
					}
				}
			}
		}
		break;
	case 4:
		{
			pixel *source_ptr = (pixel *)pixmap_ptr->image_ptr;
			pixel *target_ptr = (pixel *)bitmap_pixels;
			for (int row = 0; row < texture_ptr->height; row++) {
				for (int col = 0; col < texture_ptr->width; col++) {
					*target_ptr++ = *source_ptr++;
				}
			}
		}
	}
	return(bitmap_ptr);
}

//------------------------------------------------------------------------------
// Create a bitmap and initialize it with the builder render target texture.
//------------------------------------------------------------------------------

bitmap *
create_bitmap_from_builder_render_target()
{
	HBITMAP bitmap_handle;
	byte *bitmap_pixels;
	bitmap *bitmap_ptr;
	HDC hdc;
	BITMAPINFO bitmap_info;
	D3D11_MAPPED_SUBRESOURCE d3d_mapped_subresource;

	// Initialise the bitmap info structure.

	bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info.bmiHeader.biWidth = BUILDER_ICON_WIDTH;
	bitmap_info.bmiHeader.biHeight = -BUILDER_ICON_HEIGHT;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biSizeImage = 0;
	bitmap_info.bmiHeader.biXPelsPerMeter = 0;
	bitmap_info.bmiHeader.biYPelsPerMeter = 0;
	bitmap_info.bmiHeader.biClrUsed = 0;
	bitmap_info.bmiHeader.biClrImportant = 0;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	// Create the bitmap image as a DIB section.

	hdc = GetDC(app_window_handle);
	bitmap_handle = CreateDIBSection(hdc, &bitmap_info, DIB_RGB_COLORS, (void **)&bitmap_pixels, NULL, 0);
	ReleaseDC(app_window_handle, hdc);

	// Create the bitmap object.

	NEW(bitmap_ptr, bitmap);
	if (bitmap_ptr == NULL)
		return(NULL);

	// Fill in the bitmap structure (the parts we care about anyhow).

	bitmap_ptr->handle = bitmap_handle;
	bitmap_ptr->pixels = bitmap_pixels;
	bitmap_ptr->width = BUILDER_ICON_WIDTH;
	bitmap_ptr->height = BUILDER_ICON_HEIGHT;
	bitmap_ptr->bytes_per_row = BUILDER_ICON_WIDTH * 4;

	// If hardware acceleration is enabled...

	if (hardware_acceleration) {

		// Copy the render texture to the staging texture.

		d3d_device_context_ptr->CopyResource(d3d_builder_staging_texture_ptr, d3d_builder_render_target_texture_ptr);

		// Lock the staging texture surface.

		HRESULT result = d3d_device_context_ptr->Map(d3d_builder_staging_texture_ptr, 0, D3D11_MAP_READ, 0, &d3d_mapped_subresource);
		if (FAILED(result)) {
			DEL(bitmap_ptr, bitmap);
			return NULL;
		}

		// Copy the pixel data from the texture to the bitmap, performing the appropriate pixel conversion.

		byte *source_ptr = (byte *)d3d_mapped_subresource.pData;
		pixel *target_ptr = (pixel *)bitmap_pixels;
		int row_gap = d3d_mapped_subresource.RowPitch - bitmap_ptr->bytes_per_row;
		for (int row = 0; row < BUILDER_ICON_HEIGHT; row++) {
			for (int col = 0; col < BUILDER_ICON_WIDTH; col++) {
				byte red = *source_ptr++;
				byte green = *source_ptr++;
				byte blue = *source_ptr++;
				byte alpha = *source_ptr++;
				*target_ptr++ = (alpha << 24) | (red << 16) | (green << 8) | blue;
			}
			source_ptr += row_gap;
		}

		// Unlock the staging texture surface and return the bitmap.

		d3d_device_context_ptr->Unmap(d3d_builder_staging_texture_ptr, 0);
	}

	// If software rendering is enabled, just copy the builder frame buffer to the bitmap.

	else {
		pixel *source_ptr = (pixel *)builder_frame_buffer_ptr;
		pixel *target_ptr = (pixel *)bitmap_pixels;
		for (int row = 0; row < BUILDER_ICON_HEIGHT; row++) {
			for (int col = 0; col < BUILDER_ICON_WIDTH; col++) {
				*target_ptr++ = *source_ptr++;
			}
		}
	}

	// Return the bitmap.

	return(bitmap_ptr);
}

//------------------------------------------------------------------------------
// Create a bitmap and initialize it with the software frame buffer.
//------------------------------------------------------------------------------

bitmap *
create_bitmap_from_frame_buffer()
{
	return NULL;
}

//------------------------------------------------------------------------------
// Destroy a bitmap handle.
//------------------------------------------------------------------------------

void
destroy_bitmap_handle(void *bitmap_handle)
{
	DeleteBitmap(bitmap_handle);
}

//------------------------------------------------------------------------------
// Create an 8-bit pixmap with a 2-colour palette and assigned it to the given
// texture.  The pixmap will be used for displaying text.
//------------------------------------------------------------------------------

static bool
create_pixmap_for_text(texture *texture_ptr, int width, int height,
					   RGBcolour text_colour, RGBcolour *bg_colour_ptr)
{
	pixmap *pixmap_ptr;
	RGBcolour RGB_palette[2];

	// Create the pixmap object and initialise it.

	NEWARRAY(pixmap_ptr, pixmap, 1);
	if (pixmap_ptr == NULL)
		return(false);
	pixmap_ptr->bytes_per_pixel = 1;
	pixmap_ptr->image_size = width * height;
	pixmap_ptr->size_index = get_size_index(width, height);
	NEWARRAY(pixmap_ptr->image_ptr, imagebyte, pixmap_ptr->image_size);
	if (pixmap_ptr->image_ptr == NULL)
		return(false);
	pixmap_ptr->width = width;
	pixmap_ptr->height = height;
	if (bg_colour_ptr == NULL)
		pixmap_ptr->transparent_index = 2;
	else
		pixmap_ptr->transparent_index = -1;

	// Initialise the pixel with either the background colour or the
	// transparent index.

	if (bg_colour_ptr == NULL)
		memset(pixmap_ptr->image_ptr, 2, pixmap_ptr->image_size); 
	else
		memset(pixmap_ptr->image_ptr, 0, pixmap_ptr->image_size);

	// Initialise the texture.

	texture_ptr->bytes_per_pixel = 1;
	texture_ptr->transparent = (bg_colour_ptr == NULL);
	texture_ptr->width = width;
	texture_ptr->height = height;
	texture_ptr->pixmaps = 1;
	texture_ptr->pixmap_list = pixmap_ptr;

	// Use a two-colour palette containing the background and text colours as
	// the only entries.

	if (bg_colour_ptr != NULL)
		RGB_palette[0].set(*bg_colour_ptr);
	RGB_palette[1].set(text_colour);
	if (!texture_ptr->create_RGB_palette(2, BRIGHTNESS_LEVELS, RGB_palette))
		return(false);

	// Create the palette index table or display palette list for the texture.

	if (display_depth == 8) {
		if (!texture_ptr->create_palette_index_table())
			return(false);
	} else {
		if (!texture_ptr->create_texture_palette_list())
			return(false);
		if (!texture_ptr->create_display_palette_list())
			return(false);
	}

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Draw text onto the pixmap of the given texture.  Note that the pixmap must
// have been created by the above function.  If the function succeeds, the
// width of the text drawn is returned.
//------------------------------------------------------------------------------

static int
draw_text_on_pixmap(texture *texture_ptr, char *text, int text_alignment,
					bool doubled_text)
{
	pixmap *pixmap_ptr;
	bitmap *image_bitmap_ptr;
	int row, col;
	HDC hdc, bitmap_hdc;
	HBITMAP old_bitmap_handle;
	RECT bitmap_rect;
	byte *bitmap_row_ptr;
	byte *pixmap_row_ptr;
	byte colour_index;
	int x_offset, y_offset;
	unsigned int alignment;
	RGBcolour bg_colour, text_colour;
	int text_width;

	// Create an 8-bit bitmap for drawing the text onto.

	if ((image_bitmap_ptr = create_bitmap(texture_ptr->width, 
		texture_ptr->height, texture_ptr->colours, texture_ptr->RGB_palette, 
		texture_ptr->display_palette_list, -1)) == NULL)
		return(0);

	// Set the rectangle representing the bitmap area.

	bitmap_rect.left = 0;
	bitmap_rect.top = 0;
	bitmap_rect.right = texture_ptr->width;
	bitmap_rect.bottom = texture_ptr->height;

	// Create a device context and select the bitmap into it.

	hdc = GetDC(main_window_handle);
	bitmap_hdc = CreateCompatibleDC(hdc);
	ReleaseDC(main_window_handle, hdc);
	old_bitmap_handle = SelectBitmap(bitmap_hdc, image_bitmap_ptr->handle);

	// Initialise the bitmap with either the background colour or the
	// transparent index.

	if (texture_ptr->transparent)
		colour_index = 2;
	else
		colour_index = 0;
	for (row = 0; row < texture_ptr->height; row++) {
		bitmap_row_ptr = image_bitmap_ptr->pixels + 
			row * image_bitmap_ptr->bytes_per_row;
		for (col = 0; col < texture_ptr->width; col++)
			bitmap_row_ptr[col] = colour_index;
	}
	
	// Select the horizontal text alignment mode.

	switch (text_alignment) {
	case TOP_LEFT:
	case LEFT:
	case BOTTOM_LEFT:
		alignment = DT_LEFT;
		break;
	case TOP:
	case CENTRE:
	case BOTTOM:
		alignment = DT_CENTER;
		break;
	case TOP_RIGHT:
	case RIGHT:
	case BOTTOM_RIGHT:
		alignment = DT_RIGHT;
	}

	// Adjust the bitmap rectangle so that it snaps to the vertical size
	// of the text.  Remember the width.

	DrawText(bitmap_hdc, text, strlen(text), &bitmap_rect, 
		alignment | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX | DT_CALCRECT);
	text_width = bitmap_rect.right;

	// Now compute the offset needed to align the bitmap horizontally and
	// vertically, and adjust the bitmap rectangle.

	switch (text_alignment) {
	case TOP_LEFT:
	case LEFT:
	case BOTTOM_LEFT:
		x_offset = 0;
		break;
	case TOP:
	case CENTRE:
	case BOTTOM:
		x_offset = (texture_ptr->width - (bitmap_rect.right - 
			bitmap_rect.left)) / 2;
		break;
	case TOP_RIGHT:
	case RIGHT:
	case BOTTOM_RIGHT:
		x_offset = texture_ptr->width - (bitmap_rect.right - bitmap_rect.left);
	}
	switch (text_alignment) {
	case TOP_LEFT:
	case TOP:
	case TOP_RIGHT:
		y_offset = 0;
		break;
	case LEFT:
	case CENTRE:
	case RIGHT:
		y_offset = (texture_ptr->height - (bitmap_rect.bottom - 
			bitmap_rect.top)) / 2;
		break;
	case BOTTOM_LEFT:
	case BOTTOM:
	case BOTTOM_RIGHT:
		y_offset = texture_ptr->height - (bitmap_rect.bottom - bitmap_rect.top);
	}
	bitmap_rect.left += x_offset;
	bitmap_rect.right += x_offset;
	bitmap_rect.top += y_offset;
	bitmap_rect.bottom += y_offset;

	// Now draw the text for real.  It is drawn in the background colour
	// first if double_text is TRUE, then the text colour offset by one pixel.
	// This creates a nice effect that is easy to read over a texture if the
	// popup and text colours are chosen approapiately.

	SetBkMode(bitmap_hdc, TRANSPARENT);
	if (doubled_text) {
		bg_colour = texture_ptr->RGB_palette[0];
		SetTextColor(bitmap_hdc, RGB(bg_colour.red, bg_colour.green, 
			bg_colour.blue));
		DrawText(bitmap_hdc, text, strlen(text), &bitmap_rect, 
			alignment | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);
		bitmap_rect.left++;
		bitmap_rect.right++;
	}
	text_colour = texture_ptr->RGB_palette[1];
	SetTextColor(bitmap_hdc, RGB(text_colour.red, text_colour.green, 
		text_colour.blue));
	DrawText(bitmap_hdc, text, strlen(text), &bitmap_rect, 
		alignment | DT_WORDBREAK | DT_EXPANDTABS | DT_NOPREFIX);

	// Copy the bitmap back into the pixmap image.

	pixmap_ptr = texture_ptr->pixmap_list;
	for (row = 0; row < texture_ptr->height; row++) {
		pixmap_row_ptr = pixmap_ptr->image_ptr + row * texture_ptr->width;
		bitmap_row_ptr = image_bitmap_ptr->pixels + 
			row * image_bitmap_ptr->bytes_per_row;
		for (col = 0; col < texture_ptr->width; col++)
			pixmap_row_ptr[col] = bitmap_row_ptr[col];
	}

	// Select the default bitmap into the device context before deleting
	// it and the bitmap.

	SelectBitmap(bitmap_hdc, old_bitmap_handle);
	DeleteDC(bitmap_hdc);
	DEL(image_bitmap_ptr, bitmap);

	// Return the width of the text.

	return(text_width);
}

//------------------------------------------------------------------------------
// Create the label texture.
//------------------------------------------------------------------------------

bool
create_label_texture(void)
{
	RGBcolour bg_colour, text_colour;

	// Create the label texture.

	NEW(label_texture_ptr, texture);
	if (label_texture_ptr == NULL)
		return(false);
	bg_colour.set_RGB(0x33, 0x33, 0x33);
	text_colour.set_RGB(0xff, 0xcc, 0x66);
	if (!create_pixmap_for_text(label_texture_ptr, max_texture_size,
		TASK_BAR_HEIGHT, text_colour, &bg_colour)) 
		return(false);

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Destroy the label texture.
//------------------------------------------------------------------------------

void
destroy_label_texture(void)
{
	if (label_texture_ptr != NULL)
		DEL(label_texture_ptr, texture);
}

//------------------------------------------------------------------------------
// Draw a label.
//------------------------------------------------------------------------------

static void
draw_label(void)
{
	pixmap *pixmap_ptr;
	int mouse_x, mouse_y;
	int x, y;

	// Get the current position of the mouse.

	mouse_x = curr_mouse_x.get();
	mouse_y = curr_mouse_y.get();

	// Place the label above or below the mouse cursor, making sure it doesn't
	// get clipped by the right edge of the screen.

	x = mouse_x - curr_cursor_ptr->hotspot_x;
	if (x + label_width >= window_width)
		x = window_width - label_width;
	if (curr_mouse_y.get() >= window_height)
		y = window_height - TASK_BAR_HEIGHT;
	else {
		y = mouse_y - curr_cursor_ptr->hotspot_y + GetSystemMetrics(SM_CYCURSOR);
		if (y + TASK_BAR_HEIGHT >= window_height)
			y = mouse_y - curr_cursor_ptr->hotspot_y - TASK_BAR_HEIGHT;
	}

	// Now draw the label pixmap.

	pixmap_ptr = label_texture_ptr->pixmap_list;
	if (hardware_acceleration) {
		RGBcolour dummy_colour;
		float one_on_dimensions = 1.0f / image_dimensions_list[pixmap_ptr->size_index];
		float u = (float)label_width * one_on_dimensions;
		float v = (float)pixmap_ptr->height * one_on_dimensions;
		hardware_render_2D_polygon(pixmap_ptr, dummy_colour, 1.0f, (float)x, (float)y, (float)label_width, (float)pixmap_ptr->height,
			0.0f, 0.0f, u, v);
	} else {
		draw_pixmap(pixmap_ptr, 0, x, y, label_width, pixmap_ptr->height);
	}
}

//------------------------------------------------------------------------------
// Create a cursor.
//------------------------------------------------------------------------------

static cursor *
create_cursor(HCURSOR cursor_handle) 
{
	cursor *cursor_ptr;
	ICONINFO cursor_info;

	// Create the cursor object, and initialise it's handle.

	if ((cursor_ptr = new cursor) == NULL)
		return(NULL);
	cursor_ptr->handle = cursor_handle;

	// Get the cursor mask and image bitmaps, and the cursor hotspot.

	GetIconInfo(cursor_handle, &cursor_info);
	cursor_ptr->hotspot_x = cursor_info.xHotspot;
	cursor_ptr->hotspot_y = cursor_info.yHotspot;

	// Delete the actual cursor bitmaps.

	DeleteBitmap(cursor_info.hbmMask);
	if (cursor_info.hbmColor)
		DeleteBitmap(cursor_info.hbmColor);

	// Return a pointer to the cursor object.

	return(cursor_ptr);
}

//------------------------------------------------------------------------------
// Function to compute constants for converting a colour component to a value
// that forms part of a pixel value.
//------------------------------------------------------------------------------

static void
set_component(pixel component_mask, pixel &pixel_mask, int &right_shift,
			  int &left_shift)
{
	int component_size;

	// Count the number of zero bits in the component mask, starting from the
	// rightmost bit.  This is the left shift.

	left_shift = 0;
	while ((component_mask & 1) == 0 && left_shift < 32) {
		component_mask >>= 1;
		left_shift++;
	}

	// Count the number of one bits in the component mask, starting from the
	// rightmost bit.  This is the component size.

	component_size = 0;
	while ((component_mask & 1) == 1 && left_shift + component_size < 32) {
		component_mask >>= 1;
		component_size++;
	}
	if (component_size > 8)
		component_size = 8;

	// Compute the right shift as 8 - component size.  Use the component size to
	// look up the pixel mask in a table.

	right_shift = 8 - component_size;
	pixel_mask = pixel_mask_table[component_size - 1];
}

//------------------------------------------------------------------------------
// Set up a pixel format based upon the component masks.
//------------------------------------------------------------------------------

static void
set_pixel_format(pixel_format *pixel_format_ptr, pixel red_comp_mask,
				 pixel green_comp_mask, pixel blue_comp_mask,
				 pixel alpha_comp_mask)
{
	set_component(red_comp_mask, pixel_format_ptr->red_mask,
		pixel_format_ptr->red_right_shift, pixel_format_ptr->red_left_shift);
	set_component(green_comp_mask, pixel_format_ptr->green_mask,
		pixel_format_ptr->green_right_shift, pixel_format_ptr->green_left_shift);
	set_component(blue_comp_mask, pixel_format_ptr->blue_mask,
		pixel_format_ptr->blue_right_shift, pixel_format_ptr->blue_left_shift);
	pixel_format_ptr->alpha_comp_mask = alpha_comp_mask;
}

//------------------------------------------------------------------------------
// Allocate and create the dither tables.
//------------------------------------------------------------------------------

static bool
create_dither_tables(void)
{
	int index, table;
	float source_factor;
	float target_factor;
	float RGB_threshold[4];
	int red, green, blue;

	// Allocate the four dither tables, each containing 32768 palette indices.

	if ((dither_table[0] = new byte[32768]) == NULL ||
		(dither_table[1] = new byte[32768]) == NULL ||
		(dither_table[2] = new byte[32768]) == NULL ||
		(dither_table[3] = new byte[32768]) == NULL)
		return(false);

	// Set up some convienance pointers to each dither table.

	dither00 = dither_table[0];
	dither01 = dither_table[1];
	dither10 = dither_table[2];
	dither11 = dither_table[3];

	// Compute the source and destination RGB factors.

	source_factor = 256.0f / 32.0f;
	target_factor = 256.0f / 6.0f;

	// Compute the RGB threshold values.

	for (table = 0; table < 4; table++)
		RGB_threshold[table] = table * target_factor / 4.0f;

	// Now generate the dither tables.

	index = 0;
	for (red = 0; red < 32; red++)
		for (green = 0; green < 32; green++)
			for (blue = 0; blue < 32; blue++) {
				for (table = 0; table < 4; table++) {
					int r, g, b;

					r = (int)(FMIN(red * source_factor / target_factor, 4.0f));
					if (red * source_factor - r * target_factor >
						RGB_threshold[table])
						r++;
					g = (int)(FMIN(green * source_factor / target_factor, 4.0f));
					if (green * source_factor - g * target_factor >
						RGB_threshold[table])
						g++;
					b = (int)(FMIN(blue * source_factor / target_factor, 4.0f));
					if (blue * source_factor - b * target_factor >
						RGB_threshold[table])
						b++;
					dither_table[table][index] = 
						colour_index[(r * 6 + g) * 6 + b];
				}
				index++;
			}
	return(true);
}

//------------------------------------------------------------------------------
// Allocate and create the light tables.
//------------------------------------------------------------------------------

static bool
create_light_tables(void)
{
	int table, index;
	float red, green, blue;
	float brightness;
	RGBcolour colour;

	// Create a light table for each brightness level.

	for (table = 0; table < BRIGHTNESS_LEVELS; table++) {

		// Create a table of 32768 pixels.

		if ((light_table[table] = new pixel[65536]) == NULL)
			return(false);

		// Choose a brightness factor for this table.

		brightness = (float)(MAX_BRIGHTNESS_INDEX - table) / 
			(float)MAX_BRIGHTNESS_INDEX;

		// Step through the 32768 RGB combinations, and convert each one to a
		// display pixel at the chosen brightness.

		index = 0;
		for (red = 0.0f; red < 256.0f; red += 8.0f)
			for (green = 0.0f; green < 256.0f; green += 8.0f)
				for (blue = 0.0f; blue < 256.0f; blue += 8.0f) {
					colour.set_RGB(red, green, blue); 
					colour.adjust_brightness(brightness);
					light_table[table][index] = RGB_to_display_pixel(colour);
					index++;
				}
	}
	return(true);
}

//------------------------------------------------------------------------------
// Create the standard palette (a 6x6x6 colour cube).
//------------------------------------------------------------------------------

static bool
create_standard_palette()
{
	int index, red, green, blue;

	// For an 8-bit colour depth, create the standard palette from colour
	// entries in the system palette.  This really only works well if the
	// browser has already created a 6x6x6 colour cube in the system palette,
	// otherwise the colours chosen won't match the desired colours very well
	// at all.

	if (display_depth == 8) {
		HDC hdc;
		MYLOGPALETTE palette;
		PALETTEENTRY *palette_entries;
		bool used_palette_entry[256];

		// Get the system palette entries.

		hdc = GetDC(NULL);
		palette_entries = (PALETTEENTRY *)&palette.palPalEntry;
		GetSystemPaletteEntries(hdc, 0, 256, palette_entries);
		ReleaseDC(NULL, hdc);

		// Create a palette matching the system palette.

		palette.palVersion = 0x300;
		palette.palNumEntries = 256;
		if ((standard_palette_handle = CreatePalette((LOGPALETTE *)&palette))
			== NULL)
			return(false);

		// Copy the system palette entries into our standard RGB palette, and
		// initialise the used palette entry flags.

		for (index = 0; index < 256; index++) {
			standard_RGB_palette[index].red = palette_entries[index].peRed;
			standard_RGB_palette[index].green = palette_entries[index].peGreen;
			standard_RGB_palette[index].blue = palette_entries[index].peBlue;
			used_palette_entry[index] = false;
		}

		// Locate all colours in the 6x6x6 colour cube, and set up the colour
		// index table to match.  Used palette entries are flagged.

		index = 0;
		for (red = 0; red < 6; red++)
			for (green = 0; green < 6; green++)
				for (blue = 0; blue < 6; blue++) {
					int palette_index = 
						GetNearestPaletteIndex(standard_palette_handle,
						RGB(0x33 * red, 0x33 * green, 0x33 * blue));
					colour_index[index] = palette_index;
					used_palette_entry[palette_index] = true;
					index++;
				}

		// Choose an unused palette entry as the transparent pixel.

		for (index = 0; index < 256; index++)
			if (!used_palette_entry[index])
				break;
	}

	// For all other colour depths, simply create our own standard palette.

	else {

		// Create the 6x6x6 colour cube and set up the colour index table to
		// match.

		index = 0;
		for (red = 0; red < 6; red++)
			for (green = 0; green < 6; green++)
				for (blue = 0; blue < 6; blue++) {
					standard_RGB_palette[index].red = (byte)(0x33 * red);
					standard_RGB_palette[index].green = (byte)(0x33 * green);
					standard_RGB_palette[index].blue = (byte)(0x33 * blue);
					colour_index[index] = index;
					index++;
				}

		// Assign the COLOR_MENU colour to the next available palette entry.
		
		standard_RGB_palette[index].red = GetRValue(GetSysColor(COLOR_MENU));
		standard_RGB_palette[index].green = GetGValue(GetSysColor(COLOR_MENU));
		standard_RGB_palette[index].blue = GetBValue(GetSysColor(COLOR_MENU));
		index++;

		// Fill the remaining palette entries with black.

		while (index < 256) {
			standard_RGB_palette[index].red = 0;
			standard_RGB_palette[index].green = 0;
			standard_RGB_palette[index].blue = 0;
			index++;
		}
	}

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Set the active cursor.
//------------------------------------------------------------------------------

static void
set_active_cursor(cursor *cursor_ptr)
{
	POINT cursor_pos;

	// If the requested cursor is already the current one, do nothing.
	// Otherwise make it the current cursor.

	if (cursor_ptr == curr_cursor_ptr)
		return;
	curr_cursor_ptr = cursor_ptr;

	// Set the cursor in the class and make it the current cursor, then force a 
	// cursor change by explicitly setting the cursor position.

	SetClassLong(main_window_handle, GCL_HCURSOR, (LONG)curr_cursor_ptr->handle);
	SetCursor(curr_cursor_ptr->handle);
	GetCursorPos(&cursor_pos);
	SetCursorPos(cursor_pos.x, cursor_pos.y);
}

//------------------------------------------------------------------------------
// Start up the DirectSound system.
//------------------------------------------------------------------------------

static bool
start_up_DirectSound(void)
{
	// Create the DirectSound object.

	if (DirectSoundCreate(NULL, &dsound_object_ptr, NULL) != DD_OK)
		return(false);

	// Set the cooperative level for this application.

	return(dsound_object_ptr->SetCooperativeLevel(main_window_handle,
		DSSCL_NORMAL) == DD_OK);
}

//------------------------------------------------------------------------------
// Shut down the DirectSound system.
//------------------------------------------------------------------------------

static void
shut_down_DirectSound(void)
{
	if (dsound_object_ptr != NULL) {
		dsound_object_ptr->Release();
		dsound_object_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Initialise a spin control.
//------------------------------------------------------------------------------

static void
init_spin_control(HWND spin_control_handle, HWND edit_control_handle, 
				  int max_digits, int min_value, int max_value, 
				  int initial_value)
{
	SendMessage(edit_control_handle, EM_SETLIMITTEXT, max_digits, 0);
	SendMessage(spin_control_handle, UDM_SETBUDDY, (WPARAM)edit_control_handle, 0);
	SendMessage(spin_control_handle, UDM_SETRANGE, 0, MAKELONG(max_value, min_value));
	SendMessage(spin_control_handle, UDM_SETPOS, 0, MAKELONG(initial_value, 0));
}

//------------------------------------------------------------------------------
// Initialise a slider control.
//------------------------------------------------------------------------------

static void
init_slider_control(HWND slider_control_handle, int min_value, int max_value, int initial_value, int tick_frequency)
{
	SendMessage(slider_control_handle, TBM_SETTICFREQ, tick_frequency, 0);
	SendMessage(slider_control_handle, TBM_SETRANGE, TRUE, MAKELONG(min_value, max_value));
	SendMessage(slider_control_handle, TBM_SETPOS, TRUE, initial_value);
}

//==============================================================================
// Semaphore functions.
//==============================================================================

//------------------------------------------------------------------------------
// Create a semaphore, and return a handle to it.
//------------------------------------------------------------------------------

void *
create_semaphore(void)
{
	CRITICAL_SECTION *critical_section_ptr;

	if ((critical_section_ptr = 
		(CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION))) == NULL)
		return(NULL);
	InitializeCriticalSection(critical_section_ptr);
	return(critical_section_ptr);
}

//------------------------------------------------------------------------------
// Destroy a semaphore.
//------------------------------------------------------------------------------

void
destroy_semaphore(void *semaphore_handle)
{
	CRITICAL_SECTION *critical_section_ptr = 
		(CRITICAL_SECTION *)semaphore_handle;
	DeleteCriticalSection(critical_section_ptr);
	free(critical_section_ptr);
}

//------------------------------------------------------------------------------
// Raise a semaphore.
//------------------------------------------------------------------------------

void
raise_semaphore(void *semaphore_handle)
{
	EnterCriticalSection((CRITICAL_SECTION *)semaphore_handle);
}

//------------------------------------------------------------------------------
// Lower a semaphore.
//------------------------------------------------------------------------------

void
lower_semaphore(void *semaphore_handle)
{
	LeaveCriticalSection((CRITICAL_SECTION *)semaphore_handle);
}

//==============================================================================
// Plugin window functions.
//==============================================================================

#ifdef SYMBOLIC_DEBUG

//------------------------------------------------------------------------------
// Exception filter.
//------------------------------------------------------------------------------

typedef struct _MY_SYMBOL {
    DWORD SizeOfStruct;
    DWORD Address;
    DWORD Size;
    DWORD Flags;
    DWORD MaxNameLength;
    CHAR Name[256];
} MY_SYMBOL;

static string exception_desc;

static BOOL CALLBACK
handle_exception_report_event(HWND window_handle, UINT message, WPARAM wParam,
							  LPARAM lParam)
{
	HWND control_handle;

	switch (message) {

	// Generate the exception report.

	case WM_INITDIALOG:
		control_handle = GetDlgItem(window_handle, IDC_EXCEPTION_DESC);
		SetWindowText(control_handle, (char *)exception_desc); 
		return(TRUE);

	// Handle the OK and close buttons.

	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL:
				EndDialog(window_handle, 0);
			}
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

static LONG WINAPI
exception_filter(EXCEPTION_POINTERS *exception_ptr)
{
	LDT_ENTRY descriptor;
	DWORD code_base_address;
	DWORD exception_address;
	bool sym_initialised, got_symbol_table;
	HANDLE process_handle, thread_handle;
	DWORD displacement;
	MY_SYMBOL symbol;
	STACKFRAME stack_frame;
	char message[BUFSIZ];
	int stack_frame_count;

	// Determine the base address for the code segment.

	if (!GetThreadSelectorEntry(GetCurrentThread(), 
		exception_ptr->ContextRecord->SegCs, &descriptor))
		fatal_error("Error", "Unable to get descriptor");
	code_base_address = ((DWORD)descriptor.BaseLow | 
		((DWORD)descriptor.HighWord.Bytes.BaseMid << 16) |
		((DWORD)descriptor.HighWord.Bytes.BaseHi << 24)) + 
		(DWORD)app_instance_handle;
	
	// Attempt to obtain the symbol table for the application.  If this fails, we
	// will generate numerical addresses only in our exception report.

	sym_initialised = false;
	got_symbol_table = false;
	process_handle = GetCurrentProcess();
	thread_handle = GetCurrentThread();
	if (SymInitialize(process_handle, app_dir, FALSE)) {
		sym_initialised = true;
		if (SymLoadModule(process_handle, NULL, "Flatland Standalone.exe", NULL,
			(DWORD)app_instance_handle, 0))
			got_symbol_table = true;
	}

	// Display a summary of the exception with the exception address included,
	// with symbolic translation if available.

	switch (exception_ptr->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		exception_desc = "Access violation";
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_PRIV_INSTRUCTION:
		exception_desc = "Illegal instruction";
		break;
	case EXCEPTION_STACK_OVERFLOW:
		exception_desc = "Stack overflow";
		break;
	default:
		exception_desc = "Exception";
	}
	symbol.SizeOfStruct = sizeof(MY_SYMBOL);
	symbol.MaxNameLength= 255;
	exception_address = (DWORD)exception_ptr->ExceptionRecord->ExceptionAddress;
	if (got_symbol_table && SymGetSymFromAddr(process_handle, exception_address,
		&displacement, (IMAGEHLP_SYMBOL *)&symbol))
		bprintf(message, BUFSIZ, " in function %s + %08x\r\n\r\n", symbol.Name, 
			displacement);
	else
		bprintf(message, BUFSIZ, " at address %08x\r\n\r\n", exception_address - 
			code_base_address);
	exception_desc += message;

	// If we have a symbol table, generate a stack trace.

	if (got_symbol_table) {

		// Initialise the stack frame to the current one.

		memset(&stack_frame, 0, sizeof(stack_frame));
		stack_frame.AddrPC.Offset = exception_ptr->ContextRecord->Eip;
		stack_frame.AddrPC.Mode = AddrModeFlat;
		stack_frame.AddrStack.Offset = exception_ptr->ContextRecord->Esp;
		stack_frame.AddrStack.Mode = AddrModeFlat;
		stack_frame.AddrFrame.Offset = exception_ptr->ContextRecord->Ebp;
		stack_frame.AddrFrame.Mode = AddrModeFlat;

		// Now walk the stack.  For sanity, we only walk at most 32 frames,
		// just in case the function goes in an infinite loop.

		exception_desc += "Stack trace:\r\n";
		stack_frame_count = 0;
		while (stack_frame_count < 32 && StackWalk(IMAGE_FILE_MACHINE_I386, 
			process_handle, thread_handle, &stack_frame, exception_ptr, NULL, 
			SymFunctionTableAccess, SymGetModuleBase, NULL)) {
			if (SymGetSymFromAddr(process_handle, stack_frame.AddrPC.Offset, 
				&displacement, (IMAGEHLP_SYMBOL *)&symbol))
				bprintf(message, BUFSIZ, "%s + %08x\r\n", symbol.Name, 
					displacement);
			else
				bprintf(message, BUFSIZ, "%08x\r\n", stack_frame.AddrPC.Offset - 
					code_base_address);
			exception_desc += message;
			stack_frame_count++;
		}

		// We're done, so unload the symbol table.
		
		SymUnloadModule(process_handle, (DWORD)app_instance_handle);
	}

	// If the image helper API was initialised, clean up now.

	if (sym_initialised)
		SymCleanup(process_handle);

	// Show the exception report dialog box.  It will handle the creation of
	// the report.

	DialogBox(app_instance_handle, MAKEINTRESOURCE(IDD_EXCEPTION_REPORT), NULL, 
		handle_exception_report_event);

	// Let the normal exception handler execute.

	return(EXCEPTION_EXECUTE_HANDLER);
}

#endif

//==============================================================================
// Start up/Shut down functions.
//==============================================================================

//------------------------------------------------------------------------------
// About box dialog procedure.
//------------------------------------------------------------------------------

static INT_PTR CALLBACK 
about_box_dialog_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			string version = "Flatland ";
			version += version_number_to_string(ROVER_VERSION_NUMBER);
			SendDlgItemMessage(hDlg, IDC_STATIC_BOLD5, WM_SETTEXT, 0, (LPARAM)(char *)version);
			return (INT_PTR)TRUE;
		}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

//------------------------------------------------------------------------------
// Display a dialog box for selecting a file to save, and return a pointer to
// the file name (or NULL if none was selected).
//------------------------------------------------------------------------------

static char save_file_path[_MAX_PATH];

static char *
get_save_file_name(char *title, char *filter, char *initial_dir_path)
{
	OPENFILENAME save_file_name;

	// Set up the save file dialog and call it.

	*save_file_path = '\0';
	memset(&save_file_name, 0, sizeof(OPENFILENAME));
	save_file_name.lStructSize = sizeof(OPENFILENAME);
	save_file_name.hwndOwner = app_window_handle;
	save_file_name.lpstrFilter = filter;
	save_file_name.lpstrInitialDir = initial_dir_path;
	save_file_name.lpstrFile = save_file_path;
	save_file_name.nMaxFile = _MAX_PATH;
	save_file_name.lpstrTitle = title;
	save_file_name.Flags = OFN_LONGNAMES | OFN_PATHMUSTEXIST | 
		OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
	if (GetSaveFileName(&save_file_name))
		return(save_file_path);
	else
		return(NULL);
}

//------------------------------------------------------------------------------
// Update the sizes of the status bar boxes.
//------------------------------------------------------------------------------

static void
resize_status_bar(RECT &app_window_rect)
{
	INT right_edges[5];

	right_edges[0] = app_window_rect.right - 400;
	right_edges[1] = app_window_rect.right - 310;
	right_edges[2] = app_window_rect.right - 190;
	right_edges[3] = app_window_rect.right - 130;
	right_edges[4] = app_window_rect.right;
	SendMessage(status_bar_handle, SB_SETPARTS, 5, (LPARAM)right_edges);
}

//------------------------------------------------------------------------------
// Application window procedure.
//------------------------------------------------------------------------------

LRESULT CALLBACK app_window_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
		{
			int wmId = LOWORD(wParam);
			switch (wmId)
			{
			case IDM_ABOUT:
				DialogBox(app_instance_handle, MAKEINTRESOURCE(IDD_ABOUT), hWnd, about_box_dialog_proc);
				break;
			case ID_HELP_VIEWHELP:
				open_help_window();
				break;
			case ID_FILE_NEWSPOT:
				open_new_spot_window();	
				break;
			case ID_FILE_OPENSPOTFILE:
				{	
					char file_path[_MAX_PATH];
					if (open_file_dialog(file_path, _MAX_PATH)) {
						spot_URL_to_load = file_path;
						spot_load_requested.send_event(true);
					}
				}
				break;
			case ID_FILE_SAVESPOTFILE:
				{
					saved_spot_file_path = get_save_file_name("Save spot to 3DML file", "3DML file\0*.3dml\0", NULL);
					if (saved_spot_file_path != NULL) {
						const char *ext_ptr = strrchr(saved_spot_file_path, '.');
						if (ext_ptr == NULL || _stricmp(ext_ptr, ".3dml")) {
							strcat(saved_spot_file_path, ".3dml");
						}
						save_3DML_source_requested.send_event(true);
					}
				}
				break;
			case ID_FILE_OPTIONS:
				show_options_window();
				break;
			case ID_FILE_BRIGHTNESS:
				show_light_window();
				break;
			case ID_FILE_MANAGEBLOCKSETS:
				open_blockset_manager_window();
				break;
			case ID_FILE_EDITSPOT:
				open_builder_window();
				break;
			case ID_FILE_VIEW3DMLSOURCE:
				display_file_as_web_page(curr_spot_file_path);
				break;
			case IDM_EXIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProc(hWnd, message, wParam, lParam);
			}
		}
		break;
	case WM_SIZE:
		{
			RECT app_window_rect;
			GetClientRect(app_window_handle, &app_window_rect);
			SendMessage(status_bar_handle, WM_SIZE, wParam, lParam);
			resize_status_bar(app_window_rect);
			MoveWindow(spot_URL_edit_box_handle, SPOT_URL_LABEL_WIDTH, 0, app_window_rect.right - SPOT_URL_LABEL_WIDTH, SPOT_URL_BAR_HEIGHT, TRUE);
			if (wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) {
				resize_main_window();
			}
		}
		break;
	case WM_CAPTURECHANGED:
		mouse_look_mode.set(false);
		break;
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		if (mouse_look_mode.get()) {
			SendMessage(main_window_handle, message, wParam, lParam);
			return 0;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	case WM_DESTROY:
		quit_callback_ptr();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------------------
// Spot URL edit box window procedure.
//------------------------------------------------------------------------------

static WNDPROC old_spot_URL_edit_box_window_proc;

static LRESULT CALLBACK 
spot_URL_edit_box_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			char buffer[_MAX_PATH + 1];
			int size = Edit_GetLine(hwnd, 0, buffer, _MAX_PATH);
			buffer[size] = '\0';
			spot_URL_to_load = buffer;
			spot_load_requested.send_event(true);
			break;
		}
	default:
		return CallWindowProc(old_spot_URL_edit_box_window_proc, hwnd, msg, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------------------
// Start up the platform API.
//------------------------------------------------------------------------------

static LRESULT CALLBACK
handle_main_window_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam);

bool
start_up_platform_API(void *instance_handle, int show_command, void (*quit_callback)())
{
	char buffer[_MAX_PATH];
	char *app_name;
	WNDCLASS window_class;
	RECT app_window_rect;
	NONCLIENTMETRICS ncm;
	int index;
	FILE *fp;

	// Initialise global variables.

	app_instance_handle = (HINSTANCE)instance_handle;
	quit_callback_ptr = quit_callback;

	// Initialise the common control DLL.

	InitCommonControls();

	// Initialise the COM library.

	CoInitialize(NULL);

	// Register the application window class.

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style          = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc    = app_window_proc;
	wcex.cbClsExtra     = 0;
	wcex.cbWndExtra     = 0;
	wcex.hInstance      = app_instance_handle;
	wcex.hIcon          = LoadIcon(app_instance_handle, MAKEINTRESOURCE(IDI_FLATLANDSTANDALONE));
	wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_FLATLANDSTANDALONE);
	wcex.lpszClassName  = "FlatlandAppWindow";
	wcex.hIconSm        = LoadIcon(app_instance_handle, MAKEINTRESOURCE(IDI_SMALL));
	if (!RegisterClassEx(&wcex)) {
		return FALSE;
	}

	// Create the application window.

	string version = "Flatland ";
	version += version_number_to_string(ROVER_VERSION_NUMBER);
	app_window_handle = CreateWindow("FlatlandAppWindow", version, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, app_instance_handle, nullptr);
	if (!app_window_handle) {
		return FALSE;
	}

	// Create the spot URL label and edit box.

	spot_URL_label_handle = CreateWindow("STATIC", "Spot URL or path: ", WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE, 
		0, 0, SPOT_URL_LABEL_WIDTH, SPOT_URL_BAR_HEIGHT, app_window_handle, NULL, app_instance_handle, NULL);
	if (!spot_URL_label_handle) {
		return FALSE;
	}
	GetClientRect(app_window_handle, &app_window_rect);
	spot_URL_edit_box_handle = CreateWindow("EDIT", NULL, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL, 
		SPOT_URL_LABEL_WIDTH, 0, app_window_rect.right - SPOT_URL_LABEL_WIDTH, SPOT_URL_BAR_HEIGHT, app_window_handle, NULL, app_instance_handle, NULL);
	if (!spot_URL_edit_box_handle) {
		return FALSE;
	}

	// Use the same font for the spot URL label and edit box as used by dialog boxes.

	ncm.cbSize = sizeof(ncm);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
	HFONT dialog_font_handle = CreateFontIndirect(&(ncm.lfMessageFont));
	SendMessage(spot_URL_label_handle, WM_SETFONT, (WPARAM)dialog_font_handle, MAKELPARAM(FALSE, 0));
	SendMessage(spot_URL_edit_box_handle, WM_SETFONT, (WPARAM)dialog_font_handle, MAKELPARAM(FALSE, 0));

	// Subclass the spot URL edit box so that I can capture the enter key and trigger a spot load.

	old_spot_URL_edit_box_window_proc = (WNDPROC)SetWindowLongPtr(spot_URL_edit_box_handle, GWLP_WNDPROC, (LONG_PTR)spot_URL_edit_box_window_proc);

	// Add a status bar at the bottom of the main application window, with two parts: a large left
	// part that holds a title, and a small right part that holds a mode.

	status_bar_handle = CreateWindow(STATUSCLASSNAME, NULL, SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
		0, 0, 0, 0, app_window_handle, NULL, app_instance_handle, NULL);
	if (!status_bar_handle) {
		return FALSE;
	}
	resize_status_bar(app_window_rect);

	// Find the path to the executable, and strip out the file name.

	GetModuleFileName(NULL, buffer, _MAX_PATH);
	app_name = strrchr(buffer, '\\') + 1;
	*app_name = '\0';
	app_dir = buffer;

#ifdef SYMBOLIC_DEBUG

	// Set our own unhandled exception filter.

	SetUnhandledExceptionFilter(exception_filter);

#endif

	// Determine the path to the Flatland directory.

	flatland_dir = app_dir + "Flatland\\";

	// Determine the path to various files in the Flatland directory.

	log_file_path = flatland_dir + "log.txt";
	error_log_file_path = flatland_dir + "errlog.html";
	config_file_path = flatland_dir + "config.txt";
	version_file_path = flatland_dir + "version.txt";
	curr_spot_file_path = flatland_dir + "curr_spot.txt";
	cache_file_path = flatland_dir + "cache.txt";
	new_rover_file_path = flatland_dir + "new_rover.txt";

	// Clear the log file.

	if ((fp = fopen(log_file_path, "w")) != NULL)
		fclose(fp);

	// Get handles to all of the required cursors.

	for (index = 0; index < MOVEMENT_CURSORS; index++)
		movement_cursor_handle_list[index] = LoadCursor(app_instance_handle, 
			MAKEINTRESOURCE(movement_cursor_ID_list[index]));
	arrow_cursor_handle = LoadCursor(NULL, IDC_ARROW);
	hand_cursor_handle = LoadCursor(app_instance_handle, MAKEINTRESOURCE(IDC_HAND_CURSOR));
	crosshair_cursor_handle = LoadCursor(NULL, IDC_CROSS);	

	// Register the main window class.

	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = app_instance_handle;
	window_class.lpfnWndProc = handle_main_window_event;
	window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	window_class.hCursor = arrow_cursor_handle;
	window_class.hbrBackground = GetStockBrush(BLACK_BRUSH);
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = "MainWindow";
	if (!RegisterClass(&window_class)) {
		failed_to("register main window class");
		return(false);
	}

	// Reset flag indicating whether the main window is ready.

	main_window_ready = false;

	// Initialise the various window handles.

	main_window_handle = NULL;
	options_window_handle = NULL;
	about_window_handle = NULL;
	help_window_handle = NULL;
	blockset_manager_window_handle = NULL;
	builder_window_handle = NULL;

	// Show and update the app window.

	ShowWindow(app_window_handle, show_command);
	UpdateWindow(app_window_handle);

	// Return sucess status.

	return(true);
}

//------------------------------------------------------------------------------
// Shut down the platform API.
//------------------------------------------------------------------------------

void
shut_down_platform_API(void)
{
	// Unregister the main window class.

	if (app_instance_handle != NULL)
		UnregisterClass("MainWindow", app_instance_handle);

	// Uninitialize the COM library.

	CoUninitialize();
}

//------------------------------------------------------------------------------
// Create a Direct3D texture.
//------------------------------------------------------------------------------

static ID3D11Texture2D *
create_d3d_texture(int width, int height, DXGI_FORMAT format, D3D11_USAGE usage, UINT bind_flags, UINT cpu_access_flags)
{
	ID3D11Texture2D *d3d_texture_ptr;

	// Create the depth stencil texture.

	D3D11_TEXTURE2D_DESC texture_desc;
	ZeroMemory(&texture_desc, sizeof(texture_desc));
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.MipLevels = 1;
	texture_desc.ArraySize = 1;
	texture_desc.Format = format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Usage = usage;
	texture_desc.BindFlags = bind_flags;
	texture_desc.CPUAccessFlags = cpu_access_flags;
	texture_desc.MiscFlags = 0;
	HRESULT result = d3d_device_ptr->CreateTexture2D(&texture_desc, NULL, &d3d_texture_ptr);
	if (FAILED(result)) {
		return NULL;
	}
	return d3d_texture_ptr;
}

//------------------------------------------------------------------------------
// Create a Direct3D depth stencil view for a texture.
//------------------------------------------------------------------------------

static ID3D11DepthStencilView *
create_depth_stencil_view(ID3D11Texture2D *d3d_texture_ptr, DXGI_FORMAT format)
{
	ID3D11DepthStencilView *d3d_depth_stencil_view_ptr;

	// Create the depth stencil view.

	D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc;
	ZeroMemory(&depth_stencil_view_desc, sizeof(depth_stencil_view_desc));
	depth_stencil_view_desc.Format = format;
	depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;
	if (FAILED(d3d_device_ptr->CreateDepthStencilView(d3d_texture_ptr, &depth_stencil_view_desc, &d3d_depth_stencil_view_ptr))) {
		return NULL;
	}
	return d3d_depth_stencil_view_ptr;
}

//------------------------------------------------------------------------------
// Initialise a Direct3D viewport.
//------------------------------------------------------------------------------

static void
init_d3d_viewport(D3D11_VIEWPORT *d3d_viewport_ptr, int width, int height)
{
	d3d_viewport_ptr->Width = (FLOAT)width;
	d3d_viewport_ptr->Height = (FLOAT)height;
	d3d_viewport_ptr->MinDepth = 0.0f;
	d3d_viewport_ptr->MaxDepth = 1.0f;
	d3d_viewport_ptr->TopLeftX = 0;
	d3d_viewport_ptr->TopLeftY = 0;
}

//------------------------------------------------------------------------------
// Start up the hardware accelerated renderer.
//------------------------------------------------------------------------------

static bool
start_up_hardware_renderer(void)
{
	// Initialize the swap chain description.

	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	ZeroMemory(&swap_chain_desc, sizeof(swap_chain_desc));
	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.BufferDesc.Width = window_width;
	swap_chain_desc.BufferDesc.Height = window_height;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
	swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = main_window_handle;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = TRUE;

	// Create the device and swap chain.

	D3D_FEATURE_LEVEL feature_levels_requested = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		NULL, 
		D3D_DRIVER_TYPE_HARDWARE, 
		NULL, 
		0,
		&feature_levels_requested, 
		1,
		D3D11_SDK_VERSION, 
		&swap_chain_desc, 
		&d3d_swap_chain_ptr, 
		&d3d_device_ptr, 
		NULL,
		&d3d_device_context_ptr))) {
		return false;
	}

	// Get a pointer to the back buffer.

	ID3D11Texture2D *back_buffer_ptr;
	if (FAILED(d3d_swap_chain_ptr->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID *)&back_buffer_ptr))) {
		return false;
	}

	// Create the main render target view.

	if (FAILED(d3d_device_ptr->CreateRenderTargetView(back_buffer_ptr, NULL, &d3d_main_render_target_view_ptr))) {
		return false;
	}

	// Create the main depth stencil texture and view.

	d3d_main_depth_stencil_texture_ptr = create_d3d_texture(window_width, window_height, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_USAGE_DEFAULT, 
		D3D11_BIND_DEPTH_STENCIL, 0);
	if (d3d_main_depth_stencil_texture_ptr == NULL) {
		return false;
	}
	d3d_main_depth_stencil_view_ptr = create_depth_stencil_view(d3d_main_depth_stencil_texture_ptr, DXGI_FORMAT_D24_UNORM_S8_UINT);
	if (d3d_main_depth_stencil_view_ptr == NULL) {
		return false;
	}

	// Create the builder render target texture and view.

	d3d_builder_render_target_texture_ptr = create_d3d_texture(BUILDER_ICON_WIDTH, BUILDER_ICON_HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, 
		D3D11_BIND_RENDER_TARGET, 0);
	if (d3d_builder_render_target_texture_ptr == NULL) {
		return false;
	}
	if (FAILED(d3d_device_ptr->CreateRenderTargetView(d3d_builder_render_target_texture_ptr, NULL, &d3d_builder_render_target_view_ptr))) {
		return false;
	}

	// Create the builder staging texture.

	d3d_builder_staging_texture_ptr = create_d3d_texture(BUILDER_ICON_WIDTH, BUILDER_ICON_HEIGHT, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_STAGING,
		0, D3D11_CPU_ACCESS_READ);
	if (d3d_builder_staging_texture_ptr == NULL) {
		return false;
	}

	// Create the builder depth/stencil texture and view.

	d3d_builder_depth_stencil_texture_ptr = create_d3d_texture(BUILDER_ICON_WIDTH, BUILDER_ICON_HEIGHT, DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_USAGE_DEFAULT, 
		D3D11_BIND_DEPTH_STENCIL, 0);
	if (d3d_builder_depth_stencil_texture_ptr == NULL) {
		return false;
	}
	d3d_builder_depth_stencil_view_ptr = create_depth_stencil_view(d3d_builder_depth_stencil_texture_ptr, DXGI_FORMAT_D24_UNORM_S8_UINT);
	if (d3d_builder_depth_stencil_view_ptr == NULL) {
		return false;
	}

	// Create the 3D depth stencil state.

	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc;
	ZeroMemory(&depth_stencil_desc, sizeof(depth_stencil_desc));
	depth_stencil_desc.DepthEnable = TRUE;
	depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
	depth_stencil_desc.StencilEnable = FALSE;
	if (FAILED(d3d_device_ptr->CreateDepthStencilState(&depth_stencil_desc, &d3d_3D_depth_stencil_state_ptr))) {
		return false;
	}

	// Create the 2D depth stencil state.

	depth_stencil_desc.DepthEnable = FALSE;
	if (FAILED(d3d_device_ptr->CreateDepthStencilState(&depth_stencil_desc, &d3d_2D_depth_stencil_state_ptr))) {
		return false;
	}

	// Set up the main and builder viewports.

	init_d3d_viewport(&d3d_main_viewport, window_width, window_height);
	init_d3d_viewport(&d3d_builder_viewport, BUILDER_ICON_WIDTH, BUILDER_ICON_HEIGHT);

	// Create the vertex buffer.

	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.ByteWidth = sizeof(hardware_vertex) * MAX_VERTICES;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	if (FAILED(d3d_device_ptr->CreateBuffer(&bufferDesc, NULL, &d3d_vertex_buffer_ptr))) {
		return false;
	}
	UINT stride = sizeof(hardware_vertex);
	UINT offset = 0;
	d3d_device_context_ptr->IASetVertexBuffers(0, 1, &d3d_vertex_buffer_ptr, &stride, &offset);

	// Compile and create the colour vertex shader.

	ID3DBlob *shader_blob_ptr;
	HRESULT result = D3DCompile(colour_vertex_shader_source, strlen(colour_vertex_shader_source), "colour vertex shader", NULL, NULL,
		"VS", "vs_4_0", 0, 0, &shader_blob_ptr, NULL);
	if (FAILED(result)) {
		return false;
	}
	result = d3d_device_ptr->CreateVertexShader(shader_blob_ptr->GetBufferPointer(), shader_blob_ptr->GetBufferSize(), NULL,
		&d3d_colour_vertex_shader_ptr);
	shader_blob_ptr->Release();
	if (FAILED(result)) {	
		return false;
	}

	// Compile and create the texture vertex shader.

	if (FAILED(D3DCompile(texture_vertex_shader_source, strlen(texture_vertex_shader_source), "texture vertex shader", NULL, NULL, "VS", "vs_4_0",
		0, 0, &shader_blob_ptr, NULL))) {
		return false;
	}
	if (FAILED(d3d_device_ptr->CreateVertexShader(shader_blob_ptr->GetBufferPointer(), shader_blob_ptr->GetBufferSize(), NULL,
		&d3d_texture_vertex_shader_ptr))) {
		shader_blob_ptr->Release();
		return false;
	}

	// Define and create the vertex layout.

	D3D11_INPUT_ELEMENT_DESC vertex_layout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOUR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	UINT num_elements = ARRAYSIZE(vertex_layout);
	result = d3d_device_ptr->CreateInputLayout(vertex_layout, num_elements, shader_blob_ptr->GetBufferPointer(),
		shader_blob_ptr->GetBufferSize(), &d3d_vertex_layout_ptr);
	shader_blob_ptr->Release();
	if (FAILED(result)) {
		return false;
	}

	// Compile and create the colour pixel shader.

	if (FAILED(D3DCompile(colour_pixel_shader_source, strlen(colour_pixel_shader_source), "colour pixel shader", NULL, NULL, "PS", "ps_4_0",
		0, 0, &shader_blob_ptr, NULL))) {
		return false;
	}
	result = d3d_device_ptr->CreatePixelShader(shader_blob_ptr->GetBufferPointer(), shader_blob_ptr->GetBufferSize(), NULL,
		&d3d_colour_pixel_shader_ptr);
	shader_blob_ptr->Release();
	if (FAILED(result)) {
		return false;
	}

	// Compile and create the texture pixel shader.

	if (FAILED(D3DCompile(texture_pixel_shader_source, strlen(texture_pixel_shader_source), "texture pixel shader", NULL, NULL, "PS", "ps_4_0",
		0, 0, &shader_blob_ptr, NULL))) {
		return false;
	}
	result = d3d_device_ptr->CreatePixelShader(shader_blob_ptr->GetBufferPointer(), shader_blob_ptr->GetBufferSize(), NULL,
		&d3d_texture_pixel_shader_ptr);
	shader_blob_ptr->Release();
	if (FAILED(result)) {
		return false;
	}

	// Create the blend state.

	D3D11_BLEND_DESC blend_desc;
	ZeroMemory(&blend_desc, sizeof(blend_desc));
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if (FAILED(d3d_device_ptr->CreateBlendState(&blend_desc, &d3d_blend_state_ptr))) {
		return false;
	}

	// Create the rasterizer state.

	D3D11_RASTERIZER_DESC rasterizer_desc;
	ZeroMemory(&rasterizer_desc, sizeof(rasterizer_desc));
	rasterizer_desc.FillMode = D3D11_FILL_SOLID;
	rasterizer_desc.CullMode = D3D11_CULL_NONE;
	rasterizer_desc.FrontCounterClockwise = FALSE;
	rasterizer_desc.DepthBias = 0;
	rasterizer_desc.DepthBiasClamp = 0.0f;
	rasterizer_desc.SlopeScaledDepthBias = 0.0f;
	rasterizer_desc.DepthClipEnable = TRUE;
	rasterizer_desc.ScissorEnable = FALSE;
	rasterizer_desc.MultisampleEnable = FALSE;
	rasterizer_desc.AntialiasedLineEnable = FALSE;
	if (FAILED(d3d_device_ptr->CreateRasterizerState(&rasterizer_desc, &d3d_rasterizer_state_ptr))) {
		return false;
	}

	// Create the sampler state.

	D3D11_SAMPLER_DESC sampler_state_desc;
	ZeroMemory(&sampler_state_desc, sizeof(sampler_state_desc));
	sampler_state_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_state_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_state_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_state_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampler_state_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampler_state_desc.MinLOD = 0;
	sampler_state_desc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(d3d_device_ptr->CreateSamplerState(&sampler_state_desc, &d3d_sampler_state_ptr))) {
		return false;
	}

	// Create the fog and matrix constant buffers.

	D3D11_BUFFER_DESC constant_buffer_desc;
	ZeroMemory(&constant_buffer_desc, sizeof(constant_buffer_desc));
	constant_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	constant_buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constant_buffer_desc.CPUAccessFlags = 0;
	constant_buffer_desc.ByteWidth = sizeof(hardware_fog_constant_buffer);
	if (FAILED(d3d_device_ptr->CreateBuffer(&constant_buffer_desc, NULL, &d3d_constant_buffer_list[0]))) {
		return false;
	}
	constant_buffer_desc.ByteWidth = sizeof(hardware_matrix_constant_buffer);
	if (FAILED(d3d_device_ptr->CreateBuffer(&constant_buffer_desc, NULL, &d3d_constant_buffer_list[1]))) {
		return false;
	}

	// Select the main render target.

	select_main_render_target();

	// Set the colour component masks.

	red_comp_mask = 0xff0000;
	green_comp_mask = 0x00ff00;
	blue_comp_mask = 0x0000ff;

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Shut down the hardware accelerated renderer.
//------------------------------------------------------------------------------

static void
shut_down_hardware_renderer(void)
{
	if (d3d_device_context_ptr) {
		d3d_device_context_ptr->ClearState();
	}
	if (d3d_constant_buffer_list[1]) {
		d3d_constant_buffer_list[1]->Release();
		d3d_constant_buffer_list[1] = NULL;
	}
	if (d3d_constant_buffer_list[0]) {
		d3d_constant_buffer_list[0]->Release();
		d3d_constant_buffer_list[0] = NULL;
	}
	if (d3d_sampler_state_ptr) {
		d3d_sampler_state_ptr->Release();
		d3d_sampler_state_ptr = NULL;
	}
	if (d3d_rasterizer_state_ptr) {
		d3d_rasterizer_state_ptr->Release();
		d3d_rasterizer_state_ptr = NULL;
	}
	if (d3d_blend_state_ptr) {
		d3d_blend_state_ptr->Release();
		d3d_blend_state_ptr = NULL;
	}
	if (d3d_texture_pixel_shader_ptr) {
		d3d_texture_pixel_shader_ptr->Release();
		d3d_texture_pixel_shader_ptr = NULL;
	}
	if (d3d_colour_pixel_shader_ptr) {
		d3d_colour_pixel_shader_ptr->Release();
		d3d_colour_pixel_shader_ptr = NULL;
	}
	if (d3d_vertex_layout_ptr) {
		d3d_vertex_layout_ptr->Release();
		d3d_vertex_layout_ptr = NULL;
	}
	if (d3d_texture_vertex_shader_ptr) {
		d3d_texture_vertex_shader_ptr->Release();
		d3d_texture_vertex_shader_ptr = NULL;
	}
	if (d3d_colour_vertex_shader_ptr) {
		d3d_colour_vertex_shader_ptr->Release();
		d3d_colour_vertex_shader_ptr = NULL;
	}
	if (d3d_vertex_buffer_ptr != NULL) {
		d3d_vertex_buffer_ptr->Release();
		d3d_vertex_buffer_ptr = NULL;
	}
	if (d3d_2D_depth_stencil_state_ptr) {
		d3d_2D_depth_stencil_state_ptr->Release();
		d3d_2D_depth_stencil_state_ptr = NULL;
	}
	if (d3d_3D_depth_stencil_state_ptr) {
		d3d_3D_depth_stencil_state_ptr->Release();
		d3d_3D_depth_stencil_state_ptr = NULL;
	}
	if (d3d_builder_depth_stencil_view_ptr) {
		d3d_builder_depth_stencil_view_ptr->Release();
		d3d_builder_depth_stencil_view_ptr = NULL;
	}
	if (d3d_builder_depth_stencil_texture_ptr) {
		d3d_builder_depth_stencil_texture_ptr->Release();
		d3d_builder_depth_stencil_texture_ptr = NULL;
	}
	if (d3d_builder_staging_texture_ptr) {
		d3d_builder_staging_texture_ptr->Release();
		d3d_builder_staging_texture_ptr = NULL;
	}
	if (d3d_builder_render_target_view_ptr) {
		d3d_builder_render_target_view_ptr->Release();
		d3d_builder_render_target_view_ptr = NULL;
	}
	if (d3d_builder_render_target_texture_ptr) {
		d3d_builder_render_target_texture_ptr->Release();
		d3d_builder_render_target_texture_ptr = NULL;
	}
	if (d3d_main_depth_stencil_view_ptr) {
		d3d_main_depth_stencil_view_ptr->Release();
		d3d_main_depth_stencil_view_ptr = NULL;
	}
	if (d3d_main_depth_stencil_texture_ptr) {
		d3d_main_depth_stencil_texture_ptr->Release();
		d3d_main_depth_stencil_texture_ptr = NULL;
	}
	if (d3d_main_render_target_view_ptr) {
		d3d_main_render_target_view_ptr->Release();
		d3d_main_render_target_view_ptr = NULL;
	}
	if (d3d_swap_chain_ptr) {
		d3d_swap_chain_ptr->Release();
		d3d_swap_chain_ptr = NULL;
	}
	if (d3d_device_context_ptr) {
		d3d_device_context_ptr->Release();
		d3d_device_context_ptr = NULL;
	}
	if (d3d_device_ptr) {
		d3d_device_ptr->Release();
		d3d_device_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Start up the software renderer.
//------------------------------------------------------------------------------

static bool
start_up_software_renderer(void)
{
	// Create the DirectDraw object, and set the cooperative level.

	if ((FAILED(DirectDrawCreate(NULL, &ddraw_object_ptr, NULL)) ||
		 FAILED(ddraw_object_ptr->SetCooperativeLevel(main_window_handle,
		 DDSCL_NORMAL))))
		return(false);

	// Create the frame buffer.

	if (!create_frame_buffer())
		return(false);

	// If the display has a depth of 16, 24 or 32 bits...

	if (display_depth > 8) {
		DDSURFACEDESC ddraw_surface_desc;

		// Get the description of the primary surface.

		ddraw_surface_desc.dwSize = sizeof(ddraw_surface_desc);
		ddraw_surface_desc.dwFlags = DDSD_PIXELFORMAT;
		if (FAILED(ddraw_primary_surface_ptr->GetSurfaceDesc(
			&ddraw_surface_desc)))
			return(false);

		// Get the colour component masks.

		red_comp_mask = (pixel)ddraw_surface_desc.ddpfPixelFormat.dwRBitMask;
		green_comp_mask = (pixel)ddraw_surface_desc.ddpfPixelFormat.dwGBitMask;
		blue_comp_mask = (pixel)ddraw_surface_desc.ddpfPixelFormat.dwBBitMask;
	}

	// Select the main render target.

	select_main_render_target();

	// Indicate success.

	return(true);
}

//------------------------------------------------------------------------------
// Shut down the software renderer.
//------------------------------------------------------------------------------

static void
shut_down_software_renderer(void)
{
	// Destroy the frame buffer.

	destroy_frame_buffer();

	// Release the DirectDraw object.

	if (ddraw_object_ptr != NULL)
		ddraw_object_ptr->Release();
}

//------------------------------------------------------------------------------
// Application event loop.
//------------------------------------------------------------------------------

int
run_event_loop()
{
	HACCEL hAccelTable = LoadAccelerators(app_instance_handle, MAKEINTRESOURCE(IDC_FLATLANDSTANDALONE));
	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0)) {
		if ((!builder_window_handle || !IsDialogMessage(builder_window_handle, &msg)) && !TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

//==============================================================================
// Thread functions.
//==============================================================================

//------------------------------------------------------------------------------
// Start a thread.
//------------------------------------------------------------------------------

unsigned long
start_thread(void (*thread_func)(void *arg_list))
{
	return(_beginthread(thread_func, 0, NULL));
}

//------------------------------------------------------------------------------
// Wait for a thread to terminate.
//------------------------------------------------------------------------------

void
wait_for_thread_termination(unsigned long thread_handle)
{
	WaitForSingleObject((HANDLE)thread_handle, INFINITE);
}

//------------------------------------------------------------------------------
// Decrease a thread's priority.
//------------------------------------------------------------------------------

void
decrease_thread_priority(void)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
}

//==============================================================================
// Main window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to handle a main window mouse activation message.
//------------------------------------------------------------------------------

static int
handle_activate_message(HWND window_handle, HWND toplevel_window_handle,
					    UINT code_hit_test, UINT msg)
{
	// Set the keyboard focus to this window.

	SetFocus(window_handle);
	return(MA_ACTIVATE);
}

//------------------------------------------------------------------------------
// Function to handle key events in the main window.
//------------------------------------------------------------------------------

static void
handle_key_event(HWND window_handle, UINT virtual_key, BOOL key_down, 
				 BOOL auto_repeat, UINT flags)
{
	int index;

	// If there is no key callback function, or this is an auto-repeated key
	// down event, do nothing.

	if (key_callback_ptr == NULL || (key_down && auto_repeat))
		return;

	// If the virtual key is a letter or digit, pass it directly to the
	// callback function.

	if ((virtual_key >= 'A' && virtual_key <= 'Z') ||
		(virtual_key >= '0' && virtual_key <= '9'))
		(*key_callback_ptr)(key_down ? true : false, virtual_key);

	// Otherwise convert the virtual key code to a platform independent key
	// code, and pass it to the callback function.

	else {
		for (index = 0; index < KEY_CODES; index++)
			if (virtual_key == key_code_table[index].virtual_key) {
				(*key_callback_ptr)(key_down ? true : false, key_code_table[index].key_code);
				return;
			}
	}
}

//------------------------------------------------------------------------------
// Function to handle mouse events in the main window.
//------------------------------------------------------------------------------

static void
handle_mouse_event(HWND window_handle, UINT message, short x, short y,
				   UINT flags)
{
	// Send the mouse data to the callback function, if there is one.

	if (mouse_callback_ptr == NULL)
		return;
	switch (message) {
	case WM_MOUSEMOVE:
		(*mouse_callback_ptr)(x, y, MOUSE_MOVE_ONLY);
		break;
	case WM_LBUTTONDOWN:
		(*mouse_callback_ptr)(x, y, LEFT_BUTTON_DOWN);
		break;
	case WM_LBUTTONUP:
		(*mouse_callback_ptr)(x, y, LEFT_BUTTON_UP);
		break;
	case WM_RBUTTONDOWN:
		(*mouse_callback_ptr)(x, y, RIGHT_BUTTON_DOWN);
		break;
	case WM_RBUTTONUP:
		(*mouse_callback_ptr)(x, y, RIGHT_BUTTON_UP);
	}
}

//------------------------------------------------------------------------------
// Function to handle events in the main window.
//------------------------------------------------------------------------------

static LRESULT CALLBACK
handle_main_window_event(HWND window_handle, UINT message, WPARAM wParam,
						 LPARAM lParam)
{
	switch(message) {
	HANDLE_MSG(window_handle, WM_MOUSEACTIVATE, handle_activate_message);
	case WM_KEYDOWN:
		HANDLE_KEYDOWN_MSG(window_handle, message, handle_key_event);
		break;
	case WM_KEYUP:
		HANDLE_KEYUP_MSG(window_handle, message, handle_key_event);
		break;
	case WM_MOUSEMOVE:
		if (mouse_look_mode.get()) {
			RECT rect;
			GetWindowRect(window_handle, &rect);
			SetCursorPos(rect.left + (int)half_window_width, rect.top + (int)half_window_height);
		} else {
			HANDLE_MOUSE_MSG(window_handle, message, handle_mouse_event);
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		HANDLE_MOUSE_MSG(window_handle, message, handle_mouse_event);
		break;
	case WM_TIMER:
		if (timer_callback_ptr != NULL)
			(*timer_callback_ptr)();
		break;

	case WM_INPUT:
		if (mouse_look_mode.get()) {
			static RAWINPUT *raw_input_ptr = NULL;
			static UINT prev_raw_input_size = 0;
			UINT raw_input_size;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &raw_input_size, sizeof(RAWINPUTHEADER));
			if (raw_input_size != prev_raw_input_size) {
				if (raw_input_ptr != NULL) {
					delete [](BYTE *)raw_input_ptr;
				}
				raw_input_ptr = (RAWINPUT *)new BYTE[raw_input_size];
				prev_raw_input_size = raw_input_size;
			}
			if (raw_input_ptr && GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw_input_ptr, &raw_input_size, sizeof(RAWINPUTHEADER)) == raw_input_size &&
				raw_input_ptr->header.dwType == RIM_TYPEMOUSE) {
				(*mouse_callback_ptr)(raw_input_ptr->data.mouse.lLastX, raw_input_ptr->data.mouse.lLastY, MOUSE_MOVE_ONLY);
			}
			return 0;
		}
		break;

	case WM_SIZE:
		if (resize_callback_ptr != NULL)
			(*resize_callback_ptr)(window_handle, LOWORD(lParam),
				HIWORD(lParam));
		break;
	case WM_DISPLAYCHANGE:
		if (display_callback_ptr != NULL)
			(*display_callback_ptr)();
		break;
	case WM_USER:
		if (lParam & 0x80000000)
			HANDLE_KEYUP_MSG(window_handle, message, handle_key_event);
		else
			HANDLE_KEYDOWN_MSG(window_handle, message, handle_key_event);
	default:
		return DefWindowProc(window_handle, message, wParam, lParam);
	}
	return 0;
}

//------------------------------------------------------------------------------
// Resize the main window.
//------------------------------------------------------------------------------

void
set_main_window_size(int width, int height)
{
	window_width = width;
	window_height = height;
	half_window_width = (float)width / 2.0f;
	half_window_height = (float) height / 2.0f;
}

//------------------------------------------------------------------------------
// Calculate the rectangle of the main window.
//------------------------------------------------------------------------------

static void
calculate_main_window_rect(RECT *rect_ptr)
{
	RECT app_window_rect;
	RECT status_bar_rect;

	// The main window needs to be the same width as the app window, but it
	// needs to left room above for the spot URL label and edit box, and
	// below for the status bar.

	GetClientRect(app_window_handle, &app_window_rect);
	GetClientRect(status_bar_handle, &status_bar_rect);
	rect_ptr->left = 0;
	rect_ptr->right = app_window_rect.right;
	rect_ptr->top = SPOT_URL_BAR_HEIGHT;
	rect_ptr->bottom = app_window_rect.bottom - status_bar_rect.bottom;
}

//------------------------------------------------------------------------------
// Create the main window.
//------------------------------------------------------------------------------

bool
create_main_window(void (*key_callback)(bool key_down, byte key_code),
				   void (*mouse_callback)(int x, int y, int button_code),
				   void (*timer_callback)(void),
				   void (*resize_callback)(void *window_handle, int width,
										   int height),
				   void (*display_callback)(void))
{
	RECT main_window_rect;
	int index;

	// Do nothing if the main window already exists.

	if (main_window_handle != NULL)
		return(false);

	// Determine the main window size.

	calculate_main_window_rect(&main_window_rect);

	// Initialise the global variables.

	ddraw_object_ptr = NULL;
	ddraw_primary_surface_ptr = NULL;
	ddraw_frame_buffer_surface_ptr = NULL;
	ddraw_clipper_ptr = NULL;
	d3d_device_ptr = NULL;
	d3d_swap_chain_ptr = NULL;
	d3d_device_context_ptr = NULL;
	d3d_main_render_target_view_ptr = NULL;
	d3d_main_depth_stencil_texture_ptr = NULL;
	d3d_main_depth_stencil_view_ptr = NULL;
	d3d_builder_render_target_texture_ptr = NULL;
	d3d_builder_render_target_view_ptr = NULL;
	d3d_builder_staging_texture_ptr = NULL;
	d3d_builder_depth_stencil_texture_ptr = NULL;
	d3d_builder_depth_stencil_view_ptr = NULL;
	d3d_3D_depth_stencil_state_ptr = NULL;
	d3d_2D_depth_stencil_state_ptr = NULL;
	d3d_colour_vertex_shader_ptr = NULL;
	d3d_texture_vertex_shader_ptr = NULL;
	d3d_vertex_layout_ptr = NULL;
	d3d_colour_pixel_shader_ptr = NULL;
	d3d_texture_pixel_shader_ptr = NULL;
	d3d_blend_state_ptr = NULL;
	d3d_rasterizer_state_ptr = NULL;
	d3d_sampler_state_ptr = NULL;
	d3d_constant_buffer_list[0] = NULL;
	d3d_constant_buffer_list[1] = NULL;
	frame_buffer_ptr = NULL;
	dsound_object_ptr = NULL;
	label_visible = false;
	light_window_handle = NULL;
	key_callback_ptr = NULL;
	mouse_callback_ptr = NULL;
	timer_callback_ptr = NULL;
	resize_callback_ptr = NULL;
	display_callback_ptr = NULL;
	title_texture_ptr = NULL;
	max_texture_size = MAX_TEXTURE_SIZE;

	// Initialise the dither table pointers.

	for (index = 0; index < 4; index++)
		dither_table[index] = NULL;

	// Initialise the light table pointers.

	for (index = 0; index < BRIGHTNESS_LEVELS; index++)
		light_table[index] = NULL;

	// If the display depth is less than 8 bits, we cannot support it.

	HDC screen = CreateIC("Display", NULL, NULL, NULL);
	display_depth = GetDeviceCaps(screen, BITSPIXEL);
	DeleteDC(screen);
	if (display_depth < 8) {
		fatal_error("Unsupported colour mode", "Flatland does not "
			"support 16-color displays.\n\nTo view this 3DML document, change "
			"your display setting in the Display control panel,\nthen click on "
			"the RELOAD or REFRESH button of your browser.\nSome versions of "
			"Windows may require you to reboot your PC first.");
		return(false);
	}

	// Set the maximum number of active lights.

	max_active_lights = ACTIVE_LIGHTS_LIMIT;

	// Create the main window as a child of the app window.

	main_window_handle = CreateWindow("MainWindow", nullptr, WS_CHILD | WS_VISIBLE,
		main_window_rect.left, main_window_rect.top, 
		main_window_rect.right - main_window_rect.left,
		main_window_rect.bottom - main_window_rect.top,
		app_window_handle, nullptr, app_instance_handle, nullptr);
	if (main_window_handle == NULL) {
		return false;
	}

	// Set the main window size.

	set_main_window_size(main_window_rect.right - main_window_rect.left, main_window_rect.bottom - main_window_rect.top);

	// Set the input focus to the main window, and set a timer to go off 33 times a second.

	SetFocus(main_window_handle);
	SetTimer(main_window_handle, 1, 30, NULL);

	// Save the pointers to the callback functions.

	key_callback_ptr = key_callback;
	mouse_callback_ptr = mouse_callback;
	timer_callback_ptr = timer_callback;
	resize_callback_ptr = resize_callback;
	display_callback_ptr = display_callback;

	// Register the mouse as a raw input device for the main window, so we can do a Minecraft-style
	// mouse look without needing to capture the mouse.

	RAWINPUTDEVICE raw_input_device;
	raw_input_device.usUsagePage = 1;
	raw_input_device.usUsage = 2;
	raw_input_device.dwFlags = 0;
	raw_input_device.hwndTarget = main_window_handle;
	RegisterRawInputDevices(&raw_input_device, 1, sizeof(raw_input_device));

	// Attempt to start up the hardware accelerated renderer unless software rendering is being forced.
	// If hardware acceleration fails to start up, we will fall back on software rendering.

	hardware_acceleration = false;
	if (!force_software_rendering.get()) {
		if (start_up_hardware_renderer()) {
			hardware_acceleration = true;
		} else {
			shut_down_hardware_renderer();
			failed_to("start up 3D accelerated renderer--trying software renderer instead");
		}
	}
	if (!hardware_acceleration && !start_up_software_renderer()) {
		fatal_error("Flatland cannot start up", "Flatland was unable to initialize Direct3D or "
			"DirectDraw. Please make sure you have DirectX 11 installed.");
		return(false);
	}

	// The texture pixel format is 8888 ARGB if 3D acceleration is active, otherwise it's 1555 ARGB.

	if (hardware_acceleration) {
		set_pixel_format(&texture_pixel_format, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	} else {
		set_pixel_format(&texture_pixel_format, 0x7c00, 0x03e0, 0x001f, 0x8000);
	}

	// The builder pixel format is always 8888 ARGB.

	set_pixel_format(&builder_pixel_format, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
	
	// If the display has a depth of 8 bits...

	if (display_depth == 8) {

		// Create the dither tables needed to convert from 16-bit pixels to
		// 8-bit pixels using the palette.

		if (!create_dither_tables()) {
			failed_to_create("dither tables");
			return(false);
		}

		// Use a display pixel format of 1555 ARGB for the frame buffer, which
		// will be dithered to the 8-bit display.

		set_pixel_format(&display_pixel_format, 0x7c00, 0x03e0, 0x001f, 0x8000);
	}

	// If the display has a depth of 16, 24 or 32 bits...

	else {

		// Determine the alpha component mask...

		switch (display_depth) {

		// If the display depth is 16 bits, and one of the components uses 6
		// bits, then reduce it to 5 bits and use the least significant bit of
		// the component as the alpha component mask.  If the pixel format is 
		// 555, then use the most significant bit of the pixel as the alpha
		// component mask.

		case 16:
			if (red_comp_mask == 0xfc00) {
				red_comp_mask = 0xf800;
				alpha_comp_mask = 0x0400;
			} else if (green_comp_mask == 0x07e0) {
				green_comp_mask = 0x07c0;
				alpha_comp_mask = 0x0020;
			} else if (blue_comp_mask == 0x003f) {
				blue_comp_mask = 0x003e;
				alpha_comp_mask = 0x0001;
			} else
				alpha_comp_mask = 0x8000;
			break;

		// If the display depth is 24 bits, reduce the green component to 7 bits
		// and use the least significant green bit as the alpha component mask.

		case 24:
			green_comp_mask = 0x0000fe00;
			alpha_comp_mask = 0x00000100;
			break;
		
		// If the display depth is 32 bits, use the upper 8 bits as the alpha
		// component mask.
		
		case 32:
			alpha_comp_mask = 0xff000000;
		}

		// Set the display pixel format.

		set_pixel_format(&display_pixel_format, red_comp_mask, green_comp_mask,
			blue_comp_mask, alpha_comp_mask);
	}

#ifdef STREAMING_MEDIA

	// Set the video pixel format to be 555 RGB i.e. the same as the texture
	// format, but with no alpha channel.

	ddraw_video_pixel_format.dwSize = sizeof(DDPIXELFORMAT);
	ddraw_video_pixel_format.dwFlags = DDPF_RGB;
	ddraw_video_pixel_format.dwRGBBitCount = 16;
	ddraw_video_pixel_format.dwRBitMask = 0x7c00;
	ddraw_video_pixel_format.dwGBitMask = 0x03e0;
	ddraw_video_pixel_format.dwBBitMask = 0x001f;

#endif

	// Create the standard RGB colour palette.

	if (!create_standard_palette()) {
		failed_to_create("standardised palette");
		return(false);
	}

	// Create the light tables.

	if (!create_light_tables()) {
		failed_to_create("light tables");
		return(false);
	}

	// Create the label texture.

	if (!create_label_texture()) {
		failed_to_create("label texture");
		return(false);
	}

	// Create all of the required cursors.

	for (index = 0; index < MOVEMENT_CURSORS; index++)
		movement_cursor_ptr_list[index] = 
			create_cursor(movement_cursor_handle_list[index]);
	arrow_cursor_ptr = create_cursor(arrow_cursor_handle);
	hand_cursor_ptr = create_cursor(hand_cursor_handle);
	crosshair_cursor_ptr = create_cursor(crosshair_cursor_handle);

	// Make the current cursor the arrow cursor.

	curr_cursor_ptr = arrow_cursor_ptr;

	// Start up DirectSound, if it is available.

	if (!start_up_DirectSound()) {
		shut_down_DirectSound();
		sound_on = false;
	} else
		sound_on = true;

	// Indicate the main window is ready.

	main_window_ready = true;
	return(true);
}

//------------------------------------------------------------------------------
// Resize the main window.
//------------------------------------------------------------------------------

void
resize_main_window()
{
	RECT main_window_rect;

	calculate_main_window_rect(&main_window_rect);
	MoveWindow(main_window_handle, main_window_rect.left, main_window_rect.top,
		main_window_rect.right - main_window_rect.left, 
		main_window_rect.bottom - main_window_rect.top, TRUE);
}

//------------------------------------------------------------------------------
// Destroy the main window.
//------------------------------------------------------------------------------

void
destroy_main_window(void)
{
	int index;

	// Do nothing if the main window doesn't exist.

	if (main_window_handle == NULL)
		return;

	// Shut down DirectSound, if it were started up.

	if (sound_on)
		shut_down_DirectSound();

	// Destroy the label texture.

	destroy_label_texture();

	// Destroy the cursors.

	for (index = 0; index < MOVEMENT_CURSORS; index++)
		if (movement_cursor_ptr_list[index])
			delete movement_cursor_ptr_list[index];
	if (arrow_cursor_ptr != NULL)
		delete arrow_cursor_ptr;
	if (hand_cursor_ptr != NULL)
		delete hand_cursor_ptr;
	if (crosshair_cursor_ptr != NULL)
		delete crosshair_cursor_ptr;

	// Delete the dither tables and the standard palette if the display used an
	// 8-bit colour depth.

	if (display_depth == 8) {
		for (index = 0; index < 4; index++) {
			if (dither_table[index])
				delete []dither_table[index];
		}
		DeletePalette(standard_palette_handle);
	}

	// Delete the light tables.

	for (index = 0; index < BRIGHTNESS_LEVELS; index++) {
		if (light_table[index] != NULL)
			delete []light_table[index];
	}

	// Shut down the hardware accelerated or software renderer.

	if (hardware_acceleration)
		shut_down_hardware_renderer();
	else
		shut_down_software_renderer();

	// Stop the timer.

	KillTimer(main_window_handle, 1);

	// Destroy the main window.

	DestroyWindow(main_window_handle);
	
	// Reset the main window handle and the main window ready flag.

	main_window_handle = NULL;
	main_window_ready = false;
}

//------------------------------------------------------------------------------
// Determine if the app window is currently minimised.
//------------------------------------------------------------------------------

bool
app_window_is_minimised(void)
{
	bool minimised = app_window_handle != NULL && IsIconic(app_window_handle);
	return(minimised);
}

//==============================================================================
// Message functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to display a message on the debugger output window.
//------------------------------------------------------------------------------

void
debug_message(char *format, ...)
{
	va_list arg_ptr;
	char message[BUFSIZ];

	// Create a message string by parsing the variable argument list according
	// to the contents of the format string.

	va_start(arg_ptr, format);
	vbprintf(message, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);
	OutputDebugString(message);
}

//------------------------------------------------------------------------------
// Function to display a fatal error message in a message box.
//------------------------------------------------------------------------------

void
fatal_error(char *title, char *format, ...)
{
	va_list arg_ptr;
	char message[BUFSIZ];

	// Create a message string by parsing the variable argument list according
	// to the contents of the format string.

	va_start(arg_ptr, format);
	vbprintf(message, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);

	// Display this message in a message box, using the exclamation icon.

	MessageBoxEx(app_window_handle, message, title, 
		MB_TASKMODAL | MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST, 
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

//------------------------------------------------------------------------------
// Function to display an informational message in a message box.
//------------------------------------------------------------------------------

void
information(char *title, char *format, ...)
{
	va_list arg_ptr;
	char message[BUFSIZ];

	// Create a message string by parsing the variable argument list according
	// to the contents of the format string.

	va_start(arg_ptr, format);
	vbprintf(message, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);

	// Display this message in a message box, using the information icon.

	MessageBoxEx(app_window_handle, message, title, 
		MB_TASKMODAL | MB_OK | MB_ICONINFORMATION | MB_TOPMOST,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
}

//------------------------------------------------------------------------------
// Function to display a query in a message box, and to return the response.
//------------------------------------------------------------------------------

bool
query(char *title, bool yes_no_format, char *format, ...)
{
	va_list arg_ptr;
	char message[BUFSIZ];
	int options, result;

	// Create a message string by parsing the variable argument list according
	// to the contents of the format string.

	va_start(arg_ptr, format);
	vbprintf(message, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);

	// Display this message in a message box, using the question mark icon,
	// and return TRUE if the YES button was selected, FALSE otherwise.

	if (yes_no_format)
		options = MB_YESNO | MB_ICONQUESTION;
	else
		options = MB_OKCANCEL | MB_ICONINFORMATION;
	result = MessageBoxEx(app_window_handle, message, title, options | MB_TOPMOST | 
		MB_TASKMODAL, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
	return(result == IDYES || result == IDOK);
}

//==============================================================================
// URL functions.
//==============================================================================

//------------------------------------------------------------------------------
// Open a URL in the default external app.
//------------------------------------------------------------------------------

void
open_URL_in_default_app(const char *URL)
{
	ShellExecute(NULL, NULL, URL, 0, 0, SW_SHOWNORMAL);
}

//------------------------------------------------------------------------------
// Download a URL to a file or the cache.
//------------------------------------------------------------------------------

class download_progress : public IBindStatusCallback {
private:
	string URL;

public:
	download_progress(const char *URL) {
		this->URL = URL;
	}

	HRESULT __stdcall QueryInterface(const IID &,void **) { 
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef(void) { 
		return 1;
	}
	ULONG STDMETHODCALLTYPE Release(void) {
		return 1;
	}
	HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dwReserved, IBinding *pib) {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG *pnPriority) {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved) {
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult, LPCWSTR szError) {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *grfBINDF, BINDINFO *pbindinfo) {
		return E_NOTIMPL;
	}
	virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed) {
		return E_NOTIMPL;
	}        
	virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid, IUnknown *punk) {
		return E_NOTIMPL;
	}

	virtual HRESULT __stdcall OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
	{
		switch (ulStatusCode) {
		case BINDSTATUS_CONNECTING:
			set_title("Connecting to %S\n", szStatusText);
			break;
		case BINDSTATUS_DOWNLOADINGDATA:
			if (ulProgressMax > 0) {
				set_title("Downloading %s (%.1f%% complete)", (char *)URL, ulProgress * 100.0f / ulProgressMax);
			} else {
				set_title("Downloading %s", (char *)URL);
			}
			break;
		}
		return URL_cancel_requested.event_sent() ? E_ABORT : S_OK;
	}
};

bool
download_URL_to_file(const char *URL, char *file_path_buffer, bool no_cache)
{
	download_progress progress(URL);

	// If no caching was requested, delete the cache entry if it exists.

	if (no_cache) {
		DeleteUrlCacheEntry(URL);
	}

	// If no file path was provided, download to the cache and return its name.

	if (*file_path_buffer == '\0') {
		return URLDownloadToCacheFile(NULL, URL, file_path_buffer, _MAX_PATH, 0, &progress) == S_OK;
	}

	// If a file path was provided, download directly to that file.

	return URLDownloadToFile(NULL, URL, file_path_buffer, 0, &progress) == S_OK;
}

//==============================================================================
// New spot window.
//==============================================================================

static BOOL CALLBACK
handle_new_spot_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND control_handle;

	switch (message) {
	case WM_INITDIALOG:
		SendDlgItemMessage(window_handle, IDC_SPOT_COLUMNS, EM_SETLIMITTEXT, 5, 0);
		SendDlgItemMessage(window_handle, IDC_SPOT_ROWS, EM_SETLIMITTEXT, 5, 0);
		SendDlgItemMessage(window_handle, IDC_SPOT_LEVELS, EM_SETLIMITTEXT, 5, 0);
		SetDlgItemText(window_handle, IDC_SPOT_COLUMNS, "10");
		SetDlgItemText(window_handle, IDC_SPOT_ROWS, "10");
		SetDlgItemText(window_handle, IDC_SPOT_LEVELS, "1");
		return(TRUE);
	case WM_DESTROY:
		return(FALSE);
	case WM_COMMAND:
		control_handle = (HWND)lParam;
		if (HIWORD(wParam) == BN_CLICKED) {
			switch (LOWORD(wParam)) {
			case IDOK:
				{
					char buffer[6];

					// Get the map dimensions.  If any are <= 0, reset them to their defaults.

					GetDlgItemText(window_handle, IDC_SPOT_COLUMNS, buffer, 6);
					int columns = atoi(buffer);
					if (columns <= 0) {
						columns = 10;
					}
					GetDlgItemText(window_handle, IDC_SPOT_ROWS, buffer, 6);
					int rows = atoi(buffer);
					if (rows <= 0) {
						rows = 10;
					}
					GetDlgItemText(window_handle, IDC_SPOT_LEVELS, buffer, 6);
					int levels = atoi(buffer);
					if (levels <= 0) {
						levels = 1;
					}

					// Initialize the spot file contents with a bare-bones spot with the given dimensions, ground and sky.

					spot_file_contents = "<spot version=\"";
					spot_file_contents += version_number_to_string(ROVER_VERSION_NUMBER);
					spot_file_contents += "\">\n\t<head>\n";
					spot_file_contents += "\t\t<blockset href=\"http://original.flatland.com/blocksets/flatsets/basic.bset\"/>\n";
					spot_file_contents += "\t\t<map dimensions=\"(";
					spot_file_contents += int_to_string(columns);
					spot_file_contents += ", ";
					spot_file_contents += int_to_string(rows);
					spot_file_contents += ", ";
					spot_file_contents += int_to_string(levels);
					spot_file_contents += ")\"/>\n";
					spot_file_contents += "\t\t<ground/>\n\t</head>\n\t<body>\n\t</body>\n</spot>\n";

					// Signal the player thread to open the spot file contents.

					spot_URL_to_load = "";
					spot_load_requested.send_event(true);

					// Close the window.
				
					close_new_spot_window();
					break;
				}

			case IDCANCEL:
				close_new_spot_window();
			}
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the new spot window.
//------------------------------------------------------------------------------

void
open_new_spot_window()
{
	// If the new spot window is already open, do nothing.

	if (new_spot_window_handle != NULL)
		return;

	// Create the new spot window.

	new_spot_window_handle = CreateDialog(app_instance_handle,
		MAKEINTRESOURCE(IDD_NEW_SPOT), app_window_handle, 
		handle_new_spot_event);

	// Show the new spot window.

	ShowWindow(new_spot_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the new spot window.
//------------------------------------------------------------------------------

void
close_new_spot_window(void)
{
	if (new_spot_window_handle) {
		DestroyWindow(new_spot_window_handle);
		new_spot_window_handle = NULL;
	}
}

//==============================================================================
// Open file dialog.
//==============================================================================

bool
open_file_dialog(char *file_path_buffer, int buffer_size)
{
	OPENFILENAME ofn;

	// Clear the file path buffer.

	*file_path_buffer = '\0';

	// Set up the open file name structure.

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = app_window_handle;
	ofn.lpstrFilter = "Spot file (*.3dml)\0*.3dml\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = file_path_buffer;
	ofn.nMaxFile = buffer_size;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = "Open Spot";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = NULL;
	ofn.FlagsEx = 0;
	
	// Show the open file dialog.

	return GetOpenFileName(&ofn) == TRUE;
}

//==============================================================================
// Light window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to handle light control events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_light_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	int brightness;

	switch (message) {
	case WM_INITDIALOG:
		return(TRUE);
	case WM_DESTROY:
		(*light_callback_ptr)(0.0f, true);
		return(FALSE);
	case WM_VSCROLL:
		brightness = SendMessage(light_slider_handle, TBM_GETPOS, 0, 0);
		(*light_callback_ptr)((float)brightness, false);
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the light control.
//------------------------------------------------------------------------------

void
open_light_window(float brightness, void (*light_callback)(float brightness, bool window_closed))
{
	// If the light window is already open, do nothing.

	if (light_window_handle != NULL)
		return;

	// Save the light callback function pointer.

	light_callback_ptr = light_callback;

	// Create the light window.

	light_window_handle = CreateDialog(app_instance_handle,
		MAKEINTRESOURCE(IDD_LIGHT), app_window_handle, 
		handle_light_event);

	// Get the handle to the slider control, and initialise it with the
	// current brightness setting.

	light_slider_handle = GetDlgItem(light_window_handle, IDC_SLIDER1);
	SendMessage(light_slider_handle, TBM_SETTICFREQ, 10, 0);
	SendMessage(light_slider_handle, TBM_SETRANGE, TRUE, MAKELONG(-100, 100));
	SendMessage(light_slider_handle, TBM_SETPOS, TRUE,
		(int)(-brightness * 100.f));

	// Show the light window.

	ShowWindow(light_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the light window.
//------------------------------------------------------------------------------

void
close_light_window(void)
{
	if (light_window_handle) {
		DestroyWindow(light_window_handle);
		light_window_handle = NULL;
	}
}

//==============================================================================
// Options window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to handle option window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_options_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND control_handle;

	switch (message) {
	case WM_INITDIALOG:
		return(TRUE);
	case WM_DESTROY:
		DeleteObject(bold_font_handle);
		DeleteObject(symbol_font_handle);
		return(TRUE);
	case WM_COMMAND:
		control_handle = (HWND)lParam;
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDB_OK:
				(*options_callback_ptr)(OK_BUTTON, 1);
				break;
			case IDB_CANCEL:
				(*options_callback_ptr)(CANCEL_BUTTON, 1);
				break;
			case IDB_CLASSIC_CONTROLS:
				if (SendMessage(control_handle, BM_GETCHECK, 0, 0) == BST_CHECKED)
					(*options_callback_ptr)(CLASSIC_CONTROLS_CHECKBOX, 1);
				else
					(*options_callback_ptr)(CLASSIC_CONTROLS_CHECKBOX, 0);
				break;
			case IDB_BE_SILENT:
				(*options_callback_ptr)(DEBUG_LEVEL_OPTION, BE_SILENT);
				break;
			case IDB_LET_SPOT_DECIDE:
				(*options_callback_ptr)(DEBUG_LEVEL_OPTION, LET_SPOT_DECIDE);
				break;
			case IDB_SHOW_ERRORS_ONLY:
				(*options_callback_ptr)(DEBUG_LEVEL_OPTION, SHOW_ERRORS_ONLY);
				break;
			case IDB_SHOW_ERRORS_AND_WARNINGS:
				(*options_callback_ptr)(DEBUG_LEVEL_OPTION, SHOW_ERRORS_AND_WARNINGS);
				break;
			case IDB_FORCE_SOFTWARE_RENDERING:
				if (SendMessage(control_handle, BM_GETCHECK, 0, 0) == BST_CHECKED)
					(*options_callback_ptr)(FORCE_SOFTWARE_RENDERING_CHECKBOX, 1);
				else
					(*options_callback_ptr)(FORCE_SOFTWARE_RENDERING_CHECKBOX, 0);
			}
			break;
		}
		return(TRUE);
	case WM_HSCROLL:
		if (lParam == (LPARAM)view_radius_slider_handle) {
			(*options_callback_ptr)(VIEW_RADIUS_SLIDER, LOWORD(SendMessage(view_radius_slider_handle, TBM_GETPOS, 0, 0)));
		} else if (lParam == (LPARAM)move_rate_slider_handle) {
			(*options_callback_ptr)(MOVE_RATE_SLIDER, LOWORD(SendMessage(move_rate_slider_handle, TBM_GETPOS, 0, 0)));
		} else if (lParam == (LPARAM)turn_rate_slider_handle) {
			(*options_callback_ptr)(TURN_RATE_SLIDER, LOWORD(SendMessage(turn_rate_slider_handle, TBM_GETPOS, 0, 0)));
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the options window.
//------------------------------------------------------------------------------

void
open_options_window(int viewing_distance_value, bool use_classic_controls_value,
					int move_rate_value, int turn_rate_value, int user_debug_level_value, bool force_software_rendering,
					void (*options_callback)(int option_ID, int option_value))
{
	HWND control_handle;

	// If the options window is already open, do nothing.

	if (options_window_handle != NULL)
		return;

	// Save the options callback pointer.

	options_callback_ptr = options_callback;

	// Create the options window.

	options_window_handle = CreateDialog(app_instance_handle, MAKEINTRESOURCE(IDD_OPTIONS), app_window_handle, handle_options_event);

	// Initialize the "classic controls" check box.

	control_handle = GetDlgItem(options_window_handle, IDB_CLASSIC_CONTROLS);
	SendMessage(control_handle, BM_SETCHECK, use_classic_controls_value ? BST_CHECKED : BST_UNCHECKED, 0);
	
	// Initialise the "viewing distance" edit box and spin control.

	view_radius_slider_handle = GetDlgItem(options_window_handle, IDC_SLIDER_VIEW_RADIUS);
	init_slider_control(view_radius_slider_handle, 10, 100, viewing_distance_value, 10);

	// Initialize the "move rate" slider control.

	move_rate_slider_handle = GetDlgItem(options_window_handle, IDC_SLIDER_MOVE_RATE);
	init_slider_control(move_rate_slider_handle, MIN_MOVE_RATE, MAX_MOVE_RATE, move_rate_value, 1);

	// Initialize the "turn rate" slider control.

	turn_rate_slider_handle = GetDlgItem(options_window_handle, IDC_SLIDER_TURN_RATE);
	init_slider_control(turn_rate_slider_handle, MIN_TURN_RATE, MAX_TURN_RATE, turn_rate_value, 1);

	// Initialise the "debug option" radio box.

	switch (user_debug_level_value) {
	case BE_SILENT:
		control_handle = GetDlgItem(options_window_handle, IDB_BE_SILENT);
		break;
	case LET_SPOT_DECIDE:
		control_handle = GetDlgItem(options_window_handle, IDB_LET_SPOT_DECIDE);
		break;
	case SHOW_ERRORS_ONLY:
		control_handle = GetDlgItem(options_window_handle, IDB_SHOW_ERRORS_ONLY);
		break;
	case SHOW_ERRORS_AND_WARNINGS:
		control_handle = GetDlgItem(options_window_handle, IDB_SHOW_ERRORS_AND_WARNINGS);
	}
	SendMessage(control_handle, BM_SETCHECK, BST_CHECKED, 0);

	// Initialize the "force software rendering" check box.  This is hidden in release builds.

	control_handle = GetDlgItem(options_window_handle, IDB_FORCE_SOFTWARE_RENDERING);
#ifdef _DEBUG
	SendMessage(control_handle, BM_SETCHECK, force_software_rendering ? BST_CHECKED : BST_UNCHECKED, 0);
#else
	ShowWindow(control_handle, FALSE);
#endif

	// Show the options window.

	ShowWindow(options_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the options window.
//------------------------------------------------------------------------------

void
close_options_window(void)
{
	if (options_window_handle) {
		DestroyWindow(options_window_handle);
		options_window_handle = NULL;
	}
}

//==============================================================================
// Help window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to handle help window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_help_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC dc_handle;
	int text_height;
	HWND control_handle;

	switch (message) {

	// Load the required fonts, and set the font for each control that needs
	// it.

	case WM_INITDIALOG:
		dc_handle = GetDC(window_handle);
		text_height = POINTS_TO_PIXELS(8);
		ReleaseDC(window_handle, dc_handle);
		bold_font_handle = CreateFont(-text_height, 0, 0, 0, FW_BOLD,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"MS Sans Serif");
		symbol_font_handle = CreateFont(-text_height, 0, 0, 0, FW_BOLD,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"Symbol");
		control_handle = GetDlgItem(window_handle, IDC_STATIC_BOLD1);
		SendMessage(control_handle, WM_SETFONT, (WPARAM)bold_font_handle, 0);
		control_handle = GetDlgItem(window_handle, IDC_STATIC_BOLD2);
		SendMessage(control_handle, WM_SETFONT, (WPARAM)bold_font_handle, 0);
		control_handle = GetDlgItem(window_handle, IDC_STATIC_BOLD3);
		SendMessage(control_handle, WM_SETFONT, (WPARAM)bold_font_handle, 0);
		control_handle = GetDlgItem(window_handle, IDC_STATIC_SYMBOL);
		SendMessage(control_handle, WM_SETFONT, (WPARAM)(use_classic_controls.get() ? symbol_font_handle : bold_font_handle), 0);
		return(TRUE);
	case WM_DESTROY:
		DeleteObject(bold_font_handle);
		DeleteObject(symbol_font_handle);
		return(TRUE);
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDB_OK:
				close_help_window();
				break;
			}
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the help window.
//------------------------------------------------------------------------------

void
open_help_window(void)
{
	// If the help window is already open, do nothing.

	if (help_window_handle != NULL)
		return;

	// Create the help window.

	help_window_handle = CreateDialog(app_instance_handle,
		MAKEINTRESOURCE(use_classic_controls.get() ? IDD_HELP_CLASSIC_CONTROLS : IDD_HELP_NEW_CONTROLS),
		app_window_handle, handle_help_event);

	// Show the help window.

	ShowWindow(help_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the help window.
//------------------------------------------------------------------------------

void
close_help_window(void)
{
	if (help_window_handle) {
		DestroyWindow(help_window_handle);
		help_window_handle = NULL;
	}
}

//==============================================================================
// About window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to handle help window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_about_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC dc_handle;
	int text_height;
	HWND control_handle;

	switch (message) {

	// Load the required fonts, and set the font for each control that needs
	// it.

	case WM_INITDIALOG:
		dc_handle = GetDC(window_handle);
		text_height = POINTS_TO_PIXELS(8);
		ReleaseDC(window_handle, dc_handle);
		bold_font_handle = CreateFont(-text_height, 0, 0, 0, FW_BOLD,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
			CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"MS Sans Serif");
		control_handle = GetDlgItem(window_handle, IDC_STATIC_BOLD4);
		SendMessage(control_handle, WM_SETFONT, (WPARAM)bold_font_handle, 0);
		return(TRUE);
	case WM_DESTROY:
		DeleteObject(bold_font_handle);
		return(TRUE);
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDB_OK:
				close_about_window();
				break;
			}
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the about window.
//------------------------------------------------------------------------------

void
open_about_window(void)
{
	// If the about window is already open, do nothing.

	if (about_window_handle != NULL)
		return;

	// Create the about window.

	about_window_handle = CreateDialog(app_instance_handle,
		MAKEINTRESOURCE(IDD_ABOUT), app_window_handle,
		handle_about_event);

	// Show the about window.

	ShowWindow(about_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the about window.
//------------------------------------------------------------------------------

void
close_about_window(void)
{
	if (about_window_handle != NULL) {
		DestroyWindow(about_window_handle);
		about_window_handle = NULL;
	}
}

//==============================================================================
// Blockset manager window functions.
//==============================================================================

//------------------------------------------------------------------------------
// Add a column to the blockset list view control.
//------------------------------------------------------------------------------

static void
add_blockset_list_view_column(char *column_title, int column_index, int column_width)
{
	LV_COLUMN list_view_column;

	list_view_column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	list_view_column.fmt = LVCFMT_LEFT;
	list_view_column.cx = column_width;
	list_view_column.pszText = column_title;
	list_view_column.iSubItem = column_index;
	ListView_InsertColumn(blockset_list_view_handle, column_index, 
		&list_view_column);
}

//------------------------------------------------------------------------------
// Set the time remaining for an item in the blockset list view.
//------------------------------------------------------------------------------

static void
set_time_remaining(int item_no, cached_blockset *cached_blockset_ptr)
{
	time_t time_remaining;
	char time_remaining_str[25];

	time_remaining = min_blockset_update_period - 
		(time(NULL) - cached_blockset_ptr->updated);
	if (time_remaining >= SECONDS_PER_DAY)
		bprintf(time_remaining_str, 25, "%d days", time_remaining / 
			SECONDS_PER_DAY);
	else if (time_remaining >= SECONDS_PER_HOUR)
		bprintf(time_remaining_str, 25, "%d hours", time_remaining / 
			SECONDS_PER_HOUR);
	else if (time_remaining >= SECONDS_PER_MINUTE)
		bprintf(time_remaining_str, 25, "%d minutes", time_remaining / 
			SECONDS_PER_MINUTE);
	else if (time_remaining >= 0)
		bprintf(time_remaining_str, 25, "%d seconds", time_remaining);
	else
		bprintf(time_remaining_str, 25, "Next use");
	ListView_SetItemText(blockset_list_view_handle, item_no, 4, 
		time_remaining_str);
}

//------------------------------------------------------------------------------
// Add an item to the blockset list view control, returning the item index.
//------------------------------------------------------------------------------

static void
add_blockset_list_view_item(cached_blockset *cached_blockset_ptr)
{
	LV_ITEM list_view_item;
	int item_no;
	char size_str[16];

	// Insert a new item into the list view.  The URL of the blockset gets set
	// here.

	list_view_item.mask = LVIF_STATE | LVIF_TEXT | LVIF_PARAM;
	list_view_item.iItem = 0;
	list_view_item.iSubItem = 0;
	list_view_item.state = 0;
	list_view_item.stateMask = 0;
	list_view_item.pszText = cached_blockset_ptr->href;
	list_view_item.lParam = (LPARAM)cached_blockset_ptr;
	item_no = ListView_InsertItem(blockset_list_view_handle, &list_view_item);

	// Set the name of the blockset.

	ListView_SetItemText(blockset_list_view_handle, item_no, 1, cached_blockset_ptr->name);

	// Set the version number of the blockset, if available.

	string version = version_number_to_string(cached_blockset_ptr->version);
	if (cached_blockset_ptr->version > 0) {
		ListView_SetItemText(blockset_list_view_handle, item_no, 2, (char *)version);
	} else
		ListView_SetItemText(blockset_list_view_handle, item_no, 2, "N/A");

	// Set the size of the blockset.

	if (cached_blockset_ptr->size >= 1024)
		bprintf(size_str, 16, "%d Kb", cached_blockset_ptr->size / 1024);
	else
		bprintf(size_str, 16, "%d bytes", cached_blockset_ptr->size);
	ListView_SetItemText(blockset_list_view_handle, item_no, 3, size_str);

	// Set the time remaining before next update for the blockset.

	set_time_remaining(item_no, cached_blockset_ptr);
}

//------------------------------------------------------------------------------
// Set the minimum update period, changing the time remaining for all blocksets
// in the list view.
//------------------------------------------------------------------------------

static void
set_min_update_period(int new_min_update_period)
{
	int item_no;
	LV_ITEM list_view_item;
	cached_blockset *cached_blockset_ptr;

	// Store the new minimum update period.

	min_blockset_update_period = new_min_update_period;

	// Reset the update time of all blocksets.

	item_no = -1;
	while ((item_no = ListView_GetNextItem(blockset_list_view_handle, item_no, LVNI_ALL)) >= 0) {

		// Get a pointer to the cached blockset.

		list_view_item.mask = LVIF_PARAM;
		list_view_item.iItem = item_no;
		list_view_item.iSubItem = 0;
		ListView_GetItem(blockset_list_view_handle, &list_view_item);
		cached_blockset_ptr = (cached_blockset *)list_view_item.lParam;

		// Recalculate it's update time.

		set_time_remaining(item_no, cached_blockset_ptr);
	}
}

//------------------------------------------------------------------------------
// Function to handle blockset list view events.
//------------------------------------------------------------------------------

static LRESULT
handle_blockset_list_view_event(HWND window_handle, int control_ID, NMHDR *notify_ptr)
{
	int items_selected;

	switch(notify_ptr->code)
	{
	case LVN_ITEMCHANGED:

		// If the number of selected items has changed, enable or disable the
		// update and delete buttons.

		items_selected = ListView_GetSelectedCount(blockset_list_view_handle);
		if (items_selected > 0) {
			EnableWindow(blockset_update_button_handle, TRUE);
			EnableWindow(blockset_delete_button_handle, TRUE);
		} else {
			EnableWindow(blockset_update_button_handle, FALSE);
			EnableWindow(blockset_delete_button_handle, FALSE);
		}
		break;
	}
	return(FORWARD_WM_NOTIFY(window_handle, control_ID, notify_ptr, DefWindowProc));
}

//------------------------------------------------------------------------------
// Function to handle blockset manager window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_blockset_manager_event(HWND window_handle, UINT message, WPARAM wParam,
							  LPARAM lParam)
{
	int item_index;
	LV_ITEM list_view_item;
	string cached_blockset_URL;
	string cached_blockset_path;
	cached_blockset *cached_blockset_ptr;

	switch (message) {
	case WM_INITDIALOG:
		return(TRUE);
	case WM_DESTROY:
		return(TRUE);
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDB_OK:
				save_config_file();
				close_blockset_manager_window();
				break;
			case IDB_UPDATE:

				// Reset the update time of the selected blocksets.

				item_index = -1;
				while ((item_index = ListView_GetNextItem(blockset_list_view_handle, item_index, LVNI_SELECTED)) >= 0) {

					// Get a pointer to the cached blockset.

					list_view_item.mask = LVIF_PARAM;
					list_view_item.iItem = item_index;
					list_view_item.iSubItem = 0;
					ListView_GetItem(blockset_list_view_handle, &list_view_item);
					cached_blockset_ptr = (cached_blockset *)list_view_item.lParam;

					// Reset it's update time to zero.

					cached_blockset_ptr->updated = 0;
					save_cached_blockset_list();
					ListView_SetItemText(blockset_list_view_handle, item_index, 4, "Next use");
				}
				break;
			case IDB_DELETE:

				// Delete each of the selected blocksets.

				item_index = -1;
				while ((item_index = ListView_GetNextItem(blockset_list_view_handle, item_index, LVNI_SELECTED)) >= 0) {

					// Get a pointer to the cached blockset.

					list_view_item.mask = LVIF_PARAM;
					list_view_item.iItem = item_index;
					list_view_item.iSubItem = 0;
					ListView_GetItem(blockset_list_view_handle, &list_view_item);
					cached_blockset_ptr = (cached_blockset *)list_view_item.lParam;

					// Create the path to the blockset in the cache.

					cached_blockset_URL = create_URL(flatland_dir, (char *)cached_blockset_ptr->href + 7);
					cached_blockset_path = URL_to_file_path(cached_blockset_URL);

					// Delete the cached blockset, updating the cache file.

					delete_cached_blockset(cached_blockset_ptr->href);
					save_cached_blockset_list();
					remove(cached_blockset_path);

					// Delete the blockset from the list view.

					ListView_DeleteItem(blockset_list_view_handle, item_index);
					item_index--;
				}
			}
			break;
		case EN_CHANGE:
			set_min_update_period(SECONDS_PER_DAY * SendMessage(update_period_spin_control_handle, UDM_GETPOS, 0, 0));
		}
		return(TRUE);
	case WM_VSCROLL:
		set_min_update_period(SECONDS_PER_DAY * SendMessage(update_period_spin_control_handle, UDM_GETPOS, 0, 0));
		return(TRUE);
	HANDLE_MSG(window_handle, WM_NOTIFY, handle_blockset_list_view_event);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the blockset manager window.
//------------------------------------------------------------------------------

void
open_blockset_manager_window(void)
{
	cached_blockset *cached_blockset_ptr;

	// If the blockset manager window is already open, do nothing.

	if (blockset_manager_window_handle != NULL)
		return;

	// Create the blockset manager dialog box.

	blockset_manager_window_handle = CreateDialog(app_instance_handle,
		MAKEINTRESOURCE(IDD_BLOCKSET_MANAGER), app_window_handle,
		handle_blockset_manager_event);

	// Initialise the blockset list view columns.

	blockset_list_view_handle = GetDlgItem(blockset_manager_window_handle, IDC_BLOCKSETS);
	add_blockset_list_view_column("Blockset URL", 0, 325);
	add_blockset_list_view_column("Name", 1, 75);
	add_blockset_list_view_column("Version", 2, 50);
	add_blockset_list_view_column("Size", 3, 75);
	add_blockset_list_view_column("Next update", 4, 75);
	cached_blockset_ptr = cached_blockset_list;
	while (cached_blockset_ptr != NULL) {
		add_blockset_list_view_item(cached_blockset_ptr);
		cached_blockset_ptr = cached_blockset_ptr->next_cached_blockset_ptr;
	}

	// Get the handles to the update and delete buttons.

	blockset_update_button_handle = GetDlgItem(blockset_manager_window_handle, IDB_UPDATE);
	blockset_delete_button_handle = GetDlgItem(blockset_manager_window_handle, IDB_DELETE);

	// Initialise the "days between updates" edit box and spin control.

	update_period_spin_control_handle = GetDlgItem(blockset_manager_window_handle, IDC_SPIN_UPDATE_PERIOD);
	init_spin_control(update_period_spin_control_handle,
		GetDlgItem(blockset_manager_window_handle, IDC_EDIT_UPDATE_PERIOD),
		3, 1, 100, min_blockset_update_period / SECONDS_PER_DAY);

	// Show the blockset manager window.

	ShowWindow(blockset_manager_window_handle, SW_NORMAL);
}

//------------------------------------------------------------------------------
// Close the blockset manager window.
//------------------------------------------------------------------------------

void
close_blockset_manager_window(void)
{
	if (blockset_manager_window_handle) {
		DestroyWindow(blockset_manager_window_handle);
		blockset_manager_window_handle = NULL;
	}
}

//==============================================================================
// Builder window functions.
//==============================================================================

#define MAX_COLUMNS				10
#define MAX_ROWS				3
#define CUSTOM_BLOCKSET_NAME	"--- custom ---"

static HWND scrollbar_handle;
static HWND block_icons_handle;
static HWND block_palette_handle;
static HWND selected_block_icon_handle;
static HWND selected_block_name_handle;
static HWND selected_block_symbol_handle;
static HWND blocksets_combobox_handle;
static BLENDFUNCTION opaque_blend = {AC_SRC_OVER, 0, 255, 0};
static BLENDFUNCTION transparent_blend = {AC_SRC_OVER, 0, 127, 0};
static block_def *first_block_def_ptr;
static HFONT block_symbol_font_handle;
static HBRUSH grey_brush_handle;

//------------------------------------------------------------------------------
// Update the builder dialog by counting how many block definitions in the
// currently selected blockset there are, selecting the first block definition
// from the blockset, and updating the scrollbar for the block icons static
// control.
//------------------------------------------------------------------------------

static void
update_builder_dialog()
{
	blockset *blockset_ptr = NULL;

	// If the selected cached blockset has not been loaded, or has not had its builder
	// icons created, then request the load and/or builder icon creation and wait for it
	// to complete.

	if (selected_cached_blockset_ptr) {
		blockset_ptr = blockset_list_ptr->find_blockset(selected_cached_blockset_ptr->href);
	} else {
		blockset_ptr = custom_blockset_ptr;
	}
	if (blockset_ptr == NULL || !blockset_ptr->created_builder_icons) {
		cached_blockset_load_requested.send_event(true);
		if (cached_blockset_load_completed.wait_for_event()) {
			blockset_ptr = loaded_blockset_ptr;
		}
	}

	// Count the number of block definitions in the selected blockset, and select the first block
	// definition if there is at least one.

	int block_defs = 0;
	selected_block_def_ptr.set(NULL);
	selected_blockset_ptr = blockset_ptr;
	if (blockset_ptr) {
		block_def *block_def_ptr = blockset_ptr->block_def_list;
		while (block_def_ptr) {
			if (block_defs == 0) {
				selected_block_def_ptr.set(block_def_ptr);
			}
			block_defs++;
			block_def_ptr = block_def_ptr->next_block_def_ptr;
		}
	}

	// Set up the scrollbar based on the number of rows of icons there are.

	SCROLLINFO scroll_info;
	scroll_info.cbSize = sizeof(SCROLLINFO);
	scroll_info.fMask = SIF_DISABLENOSCROLL | SIF_POS | SIF_RANGE;
	scroll_info.nMin = 0;
	int rows = (block_defs - 1) / MAX_COLUMNS + 1;
	scroll_info.nMax = rows - MAX_ROWS < 0 ? 0 : rows - MAX_ROWS;
	scroll_info.nPos = 0;
	SetScrollInfo(scrollbar_handle, SB_CTL, &scroll_info, FALSE);
}

//------------------------------------------------------------------------------
// Draw a block icon from the source device context into the target device 
// context at the given position and size.  It will be drawn semi-transparent
// if selected, with a grey background behind it.  If no icon exists, the
// block symbol will be drawn centered on the background instead.
//------------------------------------------------------------------------------

static void
draw_block_icon(block_def *block_def_ptr, int x, int y, int width, int height, HDC source_hdc, HDC target_hdc, bool selected)
{
	RECT rect = { x, y, x + width, y + height};

	// Fill the background of the icon with the grey brush if selected.

	if (selected) {
		FillRect(target_hdc, &rect, grey_brush_handle);
	}

	// Now draw either the icon or the symbol of the block definition.  If the block is selected, use a transparent icon
	// so that the grey background blends in.

	bitmap *icon_bitmap_ptr = block_def_ptr->icon_bitmap_ptr;
	if (icon_bitmap_ptr) {
		SelectBitmap(source_hdc, icon_bitmap_ptr->handle);
		AlphaBlend(target_hdc, x, y, width, height, source_hdc, 0, 0, icon_bitmap_ptr->width, icon_bitmap_ptr->height,
			selected ? transparent_blend : opaque_blend);
	} else {
		string symbol = block_def_ptr->get_symbol();
		DrawText(target_hdc, (char *)symbol, strlen(symbol), &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}
}

//------------------------------------------------------------------------------
// Draw the visible block icons in the block icons static control.
//------------------------------------------------------------------------------

static void
draw_block_icons(DRAWITEMSTRUCT *draw_item_ptr)
{
	blockset *blockset_ptr = selected_blockset_ptr;
	if (blockset_ptr) {

		// Skip over block definitions to reach the first visible row, remembering the
		// first block definition that is visible.

		block_def *block_def_ptr = blockset_ptr->block_def_list;
		int curr_scrollbar_pos = GetScrollPos(scrollbar_handle,  SB_CTL);
		int first_block_index = curr_scrollbar_pos * MAX_COLUMNS;
		int block_index = 0;
		while (block_def_ptr && block_index < first_block_index) {
			block_index++;
			block_def_ptr = block_def_ptr->next_block_def_ptr;
		}
		first_block_def_ptr = block_def_ptr;

		// Clear the background of the control and select transparent background drawing.

		FillRect(draw_item_ptr->hDC, &draw_item_ptr->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
		SetBkMode(draw_item_ptr->hDC, TRANSPARENT);

		// Draw up to one more than the maximum number of rows of block icons.  For blocks without an icon, draw their symbol.

		HDC source_hdc = CreateCompatibleDC(draw_item_ptr->hDC);
		int x = 0;
		int y = 0;
		int block_count = (MAX_ROWS + 1) * MAX_COLUMNS;
		block_index = 0;
		while (block_def_ptr && block_index < block_count) {
			draw_block_icon(block_def_ptr, x, y, 60, 60, source_hdc, draw_item_ptr->hDC, block_def_ptr == selected_block_def_ptr.get());
			x += 64;
			if (x == 64 * MAX_COLUMNS) {
				x = 0;
				y += 64;
			}
			block_index++;
			block_def_ptr = block_def_ptr->next_block_def_ptr;
		}
		DeleteDC(source_hdc);
	}
}

//------------------------------------------------------------------------------
// Draw the block palette.
//------------------------------------------------------------------------------

static void
draw_block_palette(DRAWITEMSTRUCT *draw_item_ptr)
{
	// Clear the background of the control and select transparent background drawing.

	FillRect(draw_item_ptr->hDC, &draw_item_ptr->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
	SetBkMode(draw_item_ptr->hDC, TRANSPARENT);

	// Set up the source device context.

	HDC source_hdc = CreateCompatibleDC(draw_item_ptr->hDC);

	// Draw the icon of each block definition currently present in the palette, along with the palette index below the icon.

	for (int block_palette_position = 0; block_palette_position < 10; block_palette_position++) {
		int x = block_palette_position * 64;
		int block_palette_index = (block_palette_position + 1) % 10;
		block_def *block_def_ptr = block_palette_list[block_palette_index].get();
		if (block_def_ptr != NULL) {
			draw_block_icon(block_def_ptr, x, 0, 60, 60, source_hdc, draw_item_ptr->hDC, false);
		}
		RECT rect = {x, 64, x + 60, 84};
		char palette_index_string[2];
		sprintf(palette_index_string, "%d", block_palette_index);
		DrawText(draw_item_ptr->hDC, palette_index_string, 1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	}

	// Delete the source device context.

	DeleteDC(source_hdc);
}

//------------------------------------------------------------------------------
// Draw the selected block icon its the static control.
//------------------------------------------------------------------------------

static void
draw_selected_block_icon(DRAWITEMSTRUCT *draw_item_ptr)
{
	// If a block is selected...

	block_def *block_def_ptr = selected_block_def_ptr.get();
	if (block_def_ptr) {

		// Clear the background of the control and select transparent background drawing.

		FillRect(draw_item_ptr->hDC, &draw_item_ptr->rcItem, (HBRUSH)(COLOR_WINDOW + 1));
		SetBkMode(draw_item_ptr->hDC, TRANSPARENT);

		// Draw the selected block definition's icon.

		HDC source_hdc = CreateCompatibleDC(draw_item_ptr->hDC);
		draw_block_icon(block_def_ptr, 0, 0, 120, 120, source_hdc, draw_item_ptr->hDC, false);
		DeleteDC(source_hdc);

		// Display the name of the selected block, and its symbol, below the icon.

		SendMessage(selected_block_name_handle, WM_SETTEXT, 0, (LPARAM)(char *)block_def_ptr->name);
		SendMessage(selected_block_symbol_handle, WM_SETTEXT, 0, (LPARAM)(char *)block_def_ptr->get_symbol());
	}

	// If a block is not selected, display no name or symbol below the icon.

	else {
		SendMessage(selected_block_name_handle, WM_SETTEXT, 0, (LPARAM)"");
		SendMessage(selected_block_symbol_handle, WM_SETTEXT, 0, (LPARAM)"");
	}
}

//------------------------------------------------------------------------------
// Select a block icon.
//------------------------------------------------------------------------------

static void
select_block_icon()
{
	POINT cursor_pos;

	// Get the cursor position within the block icons static control, and calculate which block index that represents.

	GetCursorPos(&cursor_pos);
	ScreenToClient(block_icons_handle, &cursor_pos);
	int selected_block_index = cursor_pos.y / 64 * MAX_COLUMNS + cursor_pos.x / 64;

	// Locate the selected block definition by starting from the first block definition and counting to the block index.

	block_def *block_def_ptr = first_block_def_ptr;
	int block_index = 0;
	while (block_def_ptr && block_index < selected_block_index) {
		block_index++;
		block_def_ptr = block_def_ptr->next_block_def_ptr;	
	}
	selected_block_def_ptr.set(block_def_ptr);

	// Force the builder dialog to redraw.

	InvalidateRect(builder_window_handle, NULL, TRUE);
}

//------------------------------------------------------------------------------
// Set a block palette entry provided there is a selected block definition.
//------------------------------------------------------------------------------

static void
set_block_palette_entry()
{
	POINT cursor_pos;

	// Get the cursor position within the block palette static control, and calculate which palette index that represents.

	GetCursorPos(&cursor_pos);
	ScreenToClient(block_palette_handle, &cursor_pos);
	int selected_block_palette_index = ((cursor_pos.x / 64) + 1) % 10;

	// If a block definition is selected, assign it to the selected palette index.

	block_def *block_def_ptr = selected_block_def_ptr.get();
	if (block_def_ptr != NULL) {
		block_palette_list[selected_block_palette_index].set(block_def_ptr);
	}

	// Force the builder dialog to redraw.

	InvalidateRect(builder_window_handle, NULL, TRUE);
}

//------------------------------------------------------------------------------
// Function to handle builder window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_builder_event(HWND window_handle, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG:
		{
			// Remember the handles to the various controls.

			scrollbar_handle = GetDlgItem(window_handle, IDC_BLOCK_ICONS_SCROLLBAR);
			block_icons_handle = GetDlgItem(window_handle, IDC_BLOCK_ICONS);
			block_palette_handle = GetDlgItem(window_handle, IDC_BLOCK_PALETTE);
			selected_block_icon_handle = GetDlgItem(window_handle, IDC_SELECTED_BLOCK_ICON);
			selected_block_name_handle = GetDlgItem(window_handle, IDC_SELECTED_BLOCK_NAME);
			selected_block_symbol_handle = GetDlgItem(window_handle, IDC_SELECTED_BLOCK_SYMBOL);
			blocksets_combobox_handle = GetDlgItem(window_handle, IDC_BLOCKSETS_COMBOBOX);
			
			// Increase the width of the dropdown list of the blocksets combo box.

			SendMessage(blocksets_combobox_handle, CB_SETDROPPEDWIDTH, 200, 0);

			// Set up the font used by the block icons and selected block icon static control.

			HDC dc_handle = GetDC(block_icons_handle);
			block_symbol_font_handle = CreateFont(-POINTS_TO_PIXELS(20), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
				CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");
			SendMessage(block_icons_handle, WM_SETFONT, (WPARAM)block_symbol_font_handle, 0);
			SendMessage(block_palette_handle, WM_SETFONT, (WPARAM)block_symbol_font_handle, 0);
			SendMessage(selected_block_icon_handle, WM_SETFONT, (WPARAM)block_symbol_font_handle, 0);
			ReleaseDC(block_icons_handle, dc_handle);

			// Create a grey brush.

			grey_brush_handle = CreateSolidBrush(RGB(127, 127, 127));

			// Initialize the combo box with pointers to all cached blocksets, whether they are loaded or not.
			// We use NULL to represent the custom blockset.
			
			SendMessage(blocksets_combobox_handle, CB_ADDSTRING, 0, NULL);
			cached_blockset *cached_blockset_ptr = cached_blockset_list;
			while (cached_blockset_ptr) {
				SendMessage(blocksets_combobox_handle, CB_ADDSTRING, 0, (LPARAM)cached_blockset_ptr);
				cached_blockset_ptr = cached_blockset_ptr->next_cached_blockset_ptr;
			}

			// Select the first entry (the custom blockset), and remember its name.

			SendMessage(blocksets_combobox_handle, CB_SETMINVISIBLE, 10, 0);
			SendMessage(blocksets_combobox_handle, CB_SETCURSEL, 0, 0);
			selected_cached_blockset_ptr = NULL;

			// Update the dialog box.

			update_builder_dialog();
		}
		return(TRUE);

	case WM_MEASUREITEM:
		if (wParam == IDC_BLOCKSETS_COMBOBOX) {
			LPMEASUREITEMSTRUCT measure_item_ptr = (LPMEASUREITEMSTRUCT)lParam;
			measure_item_ptr->itemWidth = 400;
			measure_item_ptr->itemHeight = 20;
		}
		return(TRUE);

	case WM_COMPAREITEM:
		if (wParam == IDC_BLOCKSETS_COMBOBOX) {
			LPCOMPAREITEMSTRUCT compare_item_ptr = (LPCOMPAREITEMSTRUCT)lParam;
			cached_blockset *cached_blockset1_ptr = (cached_blockset *)compare_item_ptr->itemData1;
			cached_blockset *cached_blockset2_ptr = (cached_blockset *)compare_item_ptr->itemData2;
			string cached_blockset1_name = cached_blockset1_ptr ? cached_blockset1_ptr->display_name : CUSTOM_BLOCKSET_NAME;
			string cached_blockset2_name = cached_blockset2_ptr ? cached_blockset2_ptr->display_name : CUSTOM_BLOCKSET_NAME;
			return _stricmp(cached_blockset1_name, cached_blockset2_name);
		}
		return(TRUE);

	// Draw items in various controls.

	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT *draw_item_ptr = (DRAWITEMSTRUCT *)lParam;
			switch (wParam) {
			case IDC_BLOCK_ICONS:
				draw_block_icons(draw_item_ptr);
				break;

			case IDC_BLOCK_PALETTE:
				draw_block_palette(draw_item_ptr);
				break;

			case IDC_SELECTED_BLOCK_ICON:
				draw_selected_block_icon(draw_item_ptr);
				break;

			case IDC_BLOCKSETS_COMBOBOX:
				{
					// If the item ID is -1, there is no actual item to draw, but there will still be a focus rectangle.

					if (draw_item_ptr->itemID != -1) {

						// The colors depend on whether the item is selected.

						COLORREF prev_foreground_colour = SetTextColor(draw_item_ptr->hDC, 
							GetSysColor(draw_item_ptr->itemState & ODS_SELECTED ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
						COLORREF prev_background_colour = SetBkColor(draw_item_ptr->hDC, 
							GetSysColor(draw_item_ptr->itemState & ODS_SELECTED ? COLOR_HIGHLIGHT : COLOR_WINDOW));

						// Calculate the vertical and horizontal position.

						TEXTMETRIC tm;
						GetTextMetrics(draw_item_ptr->hDC, &tm);
						int y = (draw_item_ptr->rcItem.bottom + draw_item_ptr->rcItem.top - tm.tmHeight) / 2;

						// Get and display the text for the list item.

						cached_blockset *cached_blockset_ptr = (cached_blockset *)SendMessage(draw_item_ptr->hwndItem, CB_GETITEMDATA, draw_item_ptr->itemID, 0);
						string display_name = cached_blockset_ptr ? cached_blockset_ptr->display_name : CUSTOM_BLOCKSET_NAME;
						ExtTextOut(draw_item_ptr->hDC, 5, y,ETO_CLIPPED | ETO_OPAQUE, &draw_item_ptr->rcItem, (char *)display_name, strlen(display_name), NULL);

						// Restore the previous colors.

						SetTextColor(draw_item_ptr->hDC, prev_foreground_colour);
						SetBkColor(draw_item_ptr->hDC, prev_background_colour);
					}

					// If the item has the focus, draw the focus rectangle.

					if (draw_item_ptr->itemState & ODS_FOCUS)
						DrawFocusRect(draw_item_ptr->hDC, &draw_item_ptr->rcItem);
				}
				break;
			}
		}
		return(TRUE);

	case WM_DESTROY:
		DeleteFont(block_symbol_font_handle);
		DeleteBrush(grey_brush_handle);
		return(TRUE);

	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDOK:
				close_builder_window();
				break;
			case IDC_BLOCK_ICONS:
				select_block_icon();
				break;
			case IDC_BLOCK_PALETTE:
				set_block_palette_entry();
			}
			break;
		case CBN_SELCHANGE:
			{
				int selected_index = SendMessage(blocksets_combobox_handle, CB_GETCURSEL, 0, 0);
				selected_cached_blockset_ptr = (cached_blockset *)SendMessage(blocksets_combobox_handle, CB_GETITEMDATA, selected_index, 0);
				update_builder_dialog();
				InvalidateRect(window_handle, NULL, TRUE);
			}
			break;
		}
		return(TRUE);

	case WM_VSCROLL:
		switch (LOWORD(wParam)) {
		case SB_LINEDOWN:
			SetScrollPos(scrollbar_handle, SB_CTL, GetScrollPos(scrollbar_handle, SB_CTL) + 1, TRUE);
			InvalidateRect(block_icons_handle, NULL, TRUE);
			break;
		case SB_LINEUP:
			SetScrollPos(scrollbar_handle, SB_CTL, GetScrollPos(scrollbar_handle, SB_CTL) - 1, TRUE);
			InvalidateRect(block_icons_handle, NULL, TRUE);
			break;
		case SB_PAGEDOWN:
			SetScrollPos(scrollbar_handle, SB_CTL, GetScrollPos(scrollbar_handle, SB_CTL) + MAX_ROWS, TRUE);
			InvalidateRect(block_icons_handle, NULL, TRUE);
			break;
		case SB_PAGEUP:
			SetScrollPos(scrollbar_handle, SB_CTL, GetScrollPos(scrollbar_handle, SB_CTL) - MAX_ROWS, TRUE);
			InvalidateRect(block_icons_handle, NULL, TRUE);
			break;
		case SB_THUMBTRACK:
			SetScrollPos(scrollbar_handle, SB_CTL, HIWORD(wParam), TRUE);
			InvalidateRect(block_icons_handle, NULL, TRUE);
		}
		return(TRUE);

	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Open the builder window.
//------------------------------------------------------------------------------

void
open_builder_window()
{
	// If the about window is already open, do nothing.

	if (builder_window_handle != NULL)
		return;

	// Create the builder window.

	builder_window_handle = CreateDialog(app_instance_handle, MAKEINTRESOURCE(IDD_BUILDER), app_window_handle, handle_builder_event);

	// Show the builder window.

	ShowWindow(builder_window_handle, SW_NORMAL);

	// Enable build mode.

	build_mode.set(true);
}

//------------------------------------------------------------------------------
// Close the builder window.
//------------------------------------------------------------------------------

void
close_builder_window()
{
	// Destroy the builder window.

	if (builder_window_handle) {
		DestroyWindow(builder_window_handle);
		builder_window_handle = NULL;
	}

	// Disable build mode.

	build_mode.set(false);
}

#ifdef STREAMING_MEDIA

//==============================================================================
// Password window functions.
//==============================================================================

static char username[16], password[16];

//------------------------------------------------------------------------------
// Function to handle password window events.
//------------------------------------------------------------------------------

static BOOL CALLBACK
handle_password_event(HWND window_handle, UINT message, WPARAM wParam,
					  LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			switch (LOWORD(wParam)) {
			case IDOK:
				GetDlgItemText(window_handle, IDC_STREAM_USERNAME, username, 16);
				GetDlgItemText(window_handle, IDC_STREAM_PASSWORD, password, 16);
				EndDialog(window_handle, TRUE);
				break;
			case IDCANCEL:
				EndDialog(window_handle, FALSE);
				break;
			}
		}
		return(TRUE);
	default:
		return(FALSE);
	}
}

//------------------------------------------------------------------------------
// Get a username and password.
//------------------------------------------------------------------------------

bool
get_password(string *username_ptr, string *password_ptr)
{
	// Bring up a dialog box that requests a username and password.

	if (DialogBox(app_instance_handle, MAKEINTRESOURCE(IDD_PASSWORD), 
		app_window_handle, handle_password_event)) {
		*username_ptr = username;
		*password_ptr = password;
		return(true);
	}
	return(false);
}

#endif

//==============================================================================
// Frame buffer functions.
//==============================================================================

//------------------------------------------------------------------------------
// Select the main render target.
//------------------------------------------------------------------------------

void
select_main_render_target()
{
	// Set a flag indicating the main render target is selected.

	main_render_target_selected = true;

	// If hardware acceleration is enabled, set the main render target and depth stencil view, and the main viewport.

	if (hardware_acceleration) {
		d3d_device_context_ptr->OMSetRenderTargets(1, &d3d_main_render_target_view_ptr, d3d_main_depth_stencil_view_ptr);
		d3d_device_context_ptr->RSSetViewports(1, &d3d_main_viewport);
	}
}

//------------------------------------------------------------------------------
// Select the builder render target.
//------------------------------------------------------------------------------

void
select_builder_render_target()
{
	// Set a flag indicating the main render target is not selected.

	main_render_target_selected = false;

	// If hardware acceleration is enabled, set the builder render target and depth stencil view, and the builder viewport.

	if (hardware_acceleration) {
		d3d_device_context_ptr->OMSetRenderTargets(1, &d3d_builder_render_target_view_ptr, d3d_builder_depth_stencil_view_ptr);
		d3d_device_context_ptr->RSSetViewports(1, &d3d_builder_viewport);
	}
}

//------------------------------------------------------------------------------
// Create the frame buffer.  Only used by the software renderer.
//------------------------------------------------------------------------------

bool
create_frame_buffer(void)
{
	DDSURFACEDESC ddraw_surface_desc;
	HRESULT result;

	// Create the primary surface.

	memset(&ddraw_surface_desc, 0, sizeof(DDSURFACEDESC));
	ddraw_surface_desc.dwSize = sizeof(DDSURFACEDESC);
	ddraw_surface_desc.dwFlags = DDSD_CAPS;
	ddraw_surface_desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	if (ddraw_object_ptr->CreateSurface(&ddraw_surface_desc,
		&ddraw_primary_surface_ptr, NULL) != DD_OK) {
		failed_to_create("primary surface");
		return(false);
	}

	// Create a seperate frame buffer surface in system memory.

	ddraw_surface_desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	ddraw_surface_desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	ddraw_surface_desc.dwWidth = window_width;
	ddraw_surface_desc.dwHeight = window_height;
	if ((result = ddraw_object_ptr->CreateSurface(&ddraw_surface_desc, &ddraw_frame_buffer_surface_ptr, NULL)) != DD_OK) {
		failed_to_create("frame buffer surface");
		return(false);
	}
	
	// If the display depth is 8 bits, also allocate an intermediate 16-bit frame buffer.  This will be dithered down
	// to 8-bit display pixels.

	if (display_depth == 8 && (intermediate_frame_buffer_ptr = new byte[window_width * window_height * 2]) == NULL) {
		failed_to_create("intermediate frame buffer");
		return(false);
	}

	// Create a clipper object for the main window, and attach it to the primary surface.

	if (ddraw_object_ptr->CreateClipper(0, &ddraw_clipper_ptr, NULL) != DD_OK || 
		ddraw_clipper_ptr->SetHWnd(0, main_window_handle) != DD_OK ||
		ddraw_primary_surface_ptr->SetClipper(ddraw_clipper_ptr) != DD_OK) {
		failed_to_create("clipper");
		return(false);
	}

	// Create a 32-bit frame buffer used to render builder icons.

	if ((builder_frame_buffer_ptr = new byte[BUILDER_ICON_WIDTH * BUILDER_ICON_HEIGHT * 4]) == NULL) {
		failed_to_create("builder frame buffer");
		return(false);
	}

	// Return success status.

	return(true);
}

//------------------------------------------------------------------------------
// Recreate the frame buffer.  Only used by the hardware accelerated renderer.
//------------------------------------------------------------------------------

bool
recreate_frame_buffer(void)
{
	shut_down_hardware_renderer();
	return (start_up_hardware_renderer());
}

//------------------------------------------------------------------------------
// Destroy the frame buffer.  Only used by the software renderer.
//------------------------------------------------------------------------------

void
destroy_frame_buffer(void)
{
	// If the display depth is 8 bits, delete the intermediate 16-bit frame buffer.

	if (display_depth == 8 && intermediate_frame_buffer_ptr != NULL) {
		delete []intermediate_frame_buffer_ptr;
		intermediate_frame_buffer_ptr = NULL;
	}

	// Release the clipper object and the frame buffer and primary surfaces.

	if (ddraw_frame_buffer_surface_ptr != NULL) {
		ddraw_frame_buffer_surface_ptr->Release();
		ddraw_frame_buffer_surface_ptr = NULL;
	}
	if (ddraw_clipper_ptr != NULL) {
		ddraw_primary_surface_ptr->SetClipper(NULL);
		ddraw_clipper_ptr->Release();
		ddraw_clipper_ptr = NULL;
	}
	if (ddraw_primary_surface_ptr != NULL) {
		ddraw_primary_surface_ptr->Release();
		ddraw_primary_surface_ptr = NULL;
	}

	// Delete the builder frame buffer.

	if (builder_frame_buffer_ptr != NULL) {
		delete []builder_frame_buffer_ptr;
		builder_frame_buffer_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Lock the frame buffer  (software renderer only).
//------------------------------------------------------------------------------

bool
lock_frame_buffer()
{
	// Don't do anything if the frame buffer is already locked.

	if (frame_buffer_ptr != NULL) {
		return true;
	}

	// If the builder render target has been selected, use the 32-bit builder frame buffer.

	if (!main_render_target_selected) {
		frame_buffer_ptr = builder_frame_buffer_ptr;
		frame_buffer_row_pitch = BUILDER_ICON_WIDTH * 4;
		frame_buffer_depth = 32;
		frame_buffer_pixel_format_ptr = &builder_pixel_format;
	}

	// If the display depth is 8, use the 16-bit intermediate frame buffer.

	else if (display_depth == 8) {
		frame_buffer_ptr = intermediate_frame_buffer_ptr;
		frame_buffer_row_pitch = window_width * 2;
		frame_buffer_depth = 16;
		frame_buffer_pixel_format_ptr = &display_pixel_format;
	}

	// If the display depth is 16, 24 or 32, lock the DirectDraw frame buffer surface.

	else {
		DDSURFACEDESC ddraw_surface_desc;
		ddraw_surface_desc.dwSize = sizeof(ddraw_surface_desc);
		if (ddraw_frame_buffer_surface_ptr->Lock(NULL, &ddraw_surface_desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL) != DD_OK)
			return(false);
		frame_buffer_ptr = (byte *)ddraw_surface_desc.lpSurface;
		frame_buffer_row_pitch = ddraw_surface_desc.lPitch;
		frame_buffer_depth = display_depth;
		frame_buffer_pixel_format_ptr = &display_pixel_format;
	}

	// Return success status.

	return(true);
}

//------------------------------------------------------------------------------
// Unlock the frame buffer (software renderer only).
//------------------------------------------------------------------------------

void
unlock_frame_buffer(void)
{
	// Don't do anything if the frame buffer is already unlocked.

	if (frame_buffer_ptr == NULL) {
		return;
	}

	// If the display depth is 16, 24 or 32, unlock the DirectDraw frame buffer surface.

	if (display_depth >= 16) {
		ddraw_frame_buffer_surface_ptr->Unlock(frame_buffer_ptr);
	}
	frame_buffer_ptr = NULL;
}

//------------------------------------------------------------------------------
// Display the frame buffer.
//------------------------------------------------------------------------------

bool
display_frame_buffer(void)
{
	// If a spot is loaded, hardware acceleration is not enabled, and the
	// display depth is 8, dither the contents of the 16-bit frame buffer into
	// the frame buffer surface.

	if (spot_loaded.get() && !hardware_acceleration && display_depth == 8) {
		DDSURFACEDESC ddraw_surface_desc;
		byte *fb_ptr;
		int row_pitch;
		word *old_pixel_ptr;
		byte *new_pixel_ptr;
		int row_gap;
		int row, col;

		// Lock the frame buffer surface.

		ddraw_surface_desc.dwSize = sizeof(ddraw_surface_desc);
		if (ddraw_frame_buffer_surface_ptr->Lock(NULL, &ddraw_surface_desc, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL) != DD_OK)
			return(false);
		fb_ptr = (byte *)ddraw_surface_desc.lpSurface;
		row_pitch = ddraw_surface_desc.lPitch;

		// Get pointers to the first pixel in the frame buffer and primary surface, and compute the row gap.

		old_pixel_ptr = (word *)frame_buffer_ptr;
		new_pixel_ptr = fb_ptr;
		row_gap = row_pitch - window_width;

		// Perform the dither.

		for (row = 0; row < window_height - 1; row += 2) {
			for (col = 0; col < window_width - 1; col += 2) {
				*new_pixel_ptr++ = dither00[*old_pixel_ptr++];
				*new_pixel_ptr++ = dither01[*old_pixel_ptr++];
			}
			if (col < window_width)
				*new_pixel_ptr++ = dither00[*old_pixel_ptr++];
			new_pixel_ptr += row_gap;
			for (col = 0; col < window_width - 1; col += 2) {
				*new_pixel_ptr++ = dither10[*old_pixel_ptr++];
				*new_pixel_ptr++ = dither11[*old_pixel_ptr++];
			}
			if (col < window_width)
				*new_pixel_ptr++ = dither10[*old_pixel_ptr++];
			new_pixel_ptr += row_gap;
		}
		if (row < window_height) {
			for (col = 0; col < window_width - 1; col += 2) {
				*new_pixel_ptr++ = dither00[*old_pixel_ptr++];
				*new_pixel_ptr++ = dither01[*old_pixel_ptr++];
			}
			if (col < window_width)
				*new_pixel_ptr++ = dither00[*old_pixel_ptr++];
		}

		// Unlock the frame buffer surface.

		if (ddraw_frame_buffer_surface_ptr->Unlock(fb_ptr) != DD_OK)
			return(false);
	}

	// Draw the label if it's visible.

	if (label_visible)
		draw_label();

	// If hardware acceleration is enabled, present the frame buffer.  Otherwise
	// blit the frame buffer onto the primary surface.

	if (hardware_acceleration) {
		d3d_swap_chain_ptr->Present(0, 0);
	} else
		blit_frame_buffer();
	return(true);
}

//------------------------------------------------------------------------------
// Clear the frame buffer (hardware renderer only).
//------------------------------------------------------------------------------

void
clear_frame_buffer(void)
{
	// Clear the currently selcted render target and depth stencil.

	if (main_render_target_selected) {
		d3d_device_context_ptr->ClearRenderTargetView(d3d_main_render_target_view_ptr, Colors::Black);
		d3d_device_context_ptr->ClearDepthStencilView(d3d_main_depth_stencil_view_ptr, D3D11_CLEAR_DEPTH, 1.0f, 0);
	} else {
		d3d_device_context_ptr->ClearRenderTargetView(d3d_builder_render_target_view_ptr, Colors::Black);
		d3d_device_context_ptr->ClearDepthStencilView(d3d_builder_depth_stencil_view_ptr, D3D11_CLEAR_DEPTH, 1.0f, 0);
	}
}

//------------------------------------------------------------------------------
// Method to clear a rectangle in the builder frame buffer (software renderer only).
//------------------------------------------------------------------------------

void
clear_builder_frame_buffer(void)
{
	byte *fb_ptr = builder_frame_buffer_ptr;
	for (int row = 0; row < BUILDER_ICON_HEIGHT; row++) {
		for (int column = 0; column < BUILDER_ICON_WIDTH * 4; column++) {
			*fb_ptr++ = 0;
		}
	}
}

//==============================================================================
// Software rendering functions.
//==============================================================================

bool
create_lit_image(cache_entry *cache_entry_ptr, int image_dimensions)
{
	if (frame_buffer_depth == 16)
		cache_entry_ptr->lit_image_size = image_dimensions * image_dimensions * 2;
	else
		cache_entry_ptr->lit_image_size = image_dimensions * image_dimensions * 4;
	NEWARRAY(cache_entry_ptr->lit_image_ptr, cachebyte, cache_entry_ptr->lit_image_size);
	return cache_entry_ptr->lit_image_ptr != NULL;
}

//------------------------------------------------------------------------------
// Set a lit image for the given cache entry.
//------------------------------------------------------------------------------

void
set_lit_image(cache_entry *cache_entry_ptr, int image_dimensions)
{
	pixmap *pixmap_ptr;
	byte *image_ptr, *end_image_ptr, *new_image_ptr;
	pixel *palette_ptr;
	int transparent_index;
	word transparency_mask16;
	pixel transparency_mask32;
	int image_width, image_height;
	int lit_image_dimensions;
	int column_index;

	// Get the unlit image pointer and it's dimensions, and set a pointer to
	// the end of the image data.

	pixmap_ptr = cache_entry_ptr->pixmap_ptr;
	image_ptr = pixmap_ptr->image_ptr;
	image_width = pixmap_ptr->width * pixmap_ptr->bytes_per_pixel;
	image_height = pixmap_ptr->height;
	end_image_ptr = image_ptr + image_width * image_height;

	// Put the transparent index in a static variable so that the assembly code
	// can get to it, and get a pointer to the palette for the desired brightness
	// index.

	if (pixmap_ptr->bytes_per_pixel == 1) {
		transparent_index = pixmap_ptr->transparent_index;
		palette_ptr = pixmap_ptr->display_palette_list +
			cache_entry_ptr->brightness_index * pixmap_ptr->colours;
	} else
		palette_ptr = light_table[cache_entry_ptr->brightness_index];

	// Get the start address of the lit image and it's dimensions.

	new_image_ptr = cache_entry_ptr->lit_image_ptr;
	lit_image_dimensions = image_dimensions;

	// If the pixmap is a 16-bit image...

	if (pixmap_ptr->bytes_per_pixel == 2) {
		switch (frame_buffer_depth) {

		// If the frame buffer depth is 16, convert the unlit image to a 16-bit lit image.

		case 16:

			// Get the transparency mask.

			transparency_mask16 = (word)frame_buffer_pixel_format_ptr->alpha_comp_mask;
		
			// Perform the conversion.

			__asm {

				// EBX: holds pointer to current row of old image.
				// EDX: holds pointer to current pixel of new image.
				// EDI: holds the number of rows left to copy.

				mov ebx, image_ptr
				mov edx, new_image_ptr
				mov edi, lit_image_dimensions

			next_row1:

				// ESI: holds number of columns left to copy.

				mov esi, lit_image_dimensions

				// Clear old image offset.

				mov eax, 0
				mov column_index, eax

			next_column1:

				// Get the unlit 16-bit pixel from the old image, use it to 
				// obtain the lit 16-bit pixel, and store it in the new image.
				// The transparency mask must be transfered from the unlit to
				// lit pixel.

				mov eax, 0
				mov ecx, column_index
				mov ax, [ebx + ecx]
				mov ecx, palette_ptr
				test ax, 0x8000
				jnz opaque_pixel1
				mov ax, [ecx + eax * 4]
				jmp store_pixel1
			opaque_pixel1:
				and ax, 0x7fff
				mov ax, [ecx + eax * 4]
				or ax, transparency_mask16
			store_pixel1:
				mov [edx], ax

				// Increment the old image offset, wrapping back to zero if the 
				// end of the row is reached.

				mov eax, column_index
				add eax, 2
				cmp eax, image_width 
				jl next_pixel1
				mov eax, 0
			next_pixel1:
				mov column_index, eax

				// Increment the new image pointer.

				add edx, 2

				// Decrement the column counter, and copy next pixel in row if
				// there are any left.

				dec esi
				jnz next_column1

				// Increment the old image row pointer, and wrap back to the
				// first row if the end of the image has been reached.

				add ebx, image_width
				cmp ebx, end_image_ptr
				jl skip_wrap1
				mov ebx, image_ptr

			skip_wrap1:

				// Decrement the row counter, and copy next row if there are any
				// left.

				dec edi
				jnz next_row1
			}
			break;

		// If the frame buffer depth is 24 or 32, convert the unlit image to a 32-bit lit image.

		case 24:
		case 32:

			// Get the transparency mask.

			transparency_mask32 = frame_buffer_pixel_format_ptr->alpha_comp_mask;

			// Perform the conversion.

			__asm {
			
				// EBX: holds pointer to current row of old image.
				// EDX: holds pointer to current pixel of new image.
				// EDI: holds the number of rows left to copy.

				mov ebx, image_ptr
				mov edx, new_image_ptr
				mov edi, lit_image_dimensions

			next_row2:

				// ESI: holds number of columns left to copy.

				mov esi, lit_image_dimensions

				// Clear old image offset.

				mov eax, 0
				mov column_index, eax

			next_column2:

				// Get the unlit 16-bit index from the old image, use it to
				// obtain the 32-bit pixel, and store it in the new image.
				// The transparency mask must be transfered from the unlit to
				// lit pixel.

				mov eax, 0
				mov ecx, column_index
				mov ax, [ebx + ecx]
				mov ecx, palette_ptr
				test ax, 0x8000
				jnz opaque_pixel2
				mov eax, [ecx + eax * 4]	
				jmp store_pixel2
			opaque_pixel2:
				and ax, 0x7fff
				mov eax, [ecx + eax * 4]
				or eax, transparency_mask32
			store_pixel2:
				mov [edx], eax

				// Increment the old image offset, wrapping back to zero if the 
				// end of the row is reached.

				mov eax, column_index
				add eax, 2
				cmp eax, image_width 
				jl next_pixel2
				mov eax, 0
			next_pixel2:
				mov column_index, eax

				// Increment the new image pointer.

				add edx, 4

				// Decrement the column counter, and copy next pixel in row if
				// there are any left.

				dec esi
				jnz next_column2

				// Increment the old image row pointer, and wrap back to the
				// first row if the end of the image has been reached.

				add ebx, image_width
				cmp ebx, end_image_ptr
				jl skip_wrap2
				mov ebx, image_ptr

			skip_wrap2:

				// Decrement the row counter, and copy next row if there are any
				// left.

				dec edi
				jnz next_row2
			}
		}
	} 
	
	// If the pixmap is an 8-bit image...

	else {
		switch (frame_buffer_depth) {

		// If the frame buffer depth is 16, convert the unlit image to a 16-bit lit image...

		case 16:

			// Get the transparency mask.

			transparency_mask16 = (word)frame_buffer_pixel_format_ptr->alpha_comp_mask;

			// Perform the conversion.

			__asm {
			
				// EBX: holds pointer to current row of old image.
				// EDX: holds pointer to current pixel of new image.
				// EDI: holds the number of rows left to copy.

				mov ebx, image_ptr
				mov edx, new_image_ptr
				mov edi, lit_image_dimensions

			next_row3:

				// ESI: holds number of columns left to copy.

				mov esi, lit_image_dimensions

				// Clear old image offset.

				mov eax, 0
				mov column_index, eax

			next_column3:

				// Get the current 8-bit index from the old image, use it to
				// obtain the 16-bit pixel, and store it in the new image.  If 
				// the 8-bit index is not the transparent index, mark the pixel as
				// opaque by setting the transparency mask.

				mov eax, 0
				mov ecx, column_index
				mov al, [ebx + ecx]
				mov ecx, palette_ptr
				cmp eax, transparent_index
				jne opaque_pixel3
				mov ax, [ecx + eax * 4]	
				jmp store_pixel3
			opaque_pixel3:
				mov ax, [ecx + eax * 4]
				or ax, transparency_mask16
			store_pixel3:
				mov [edx], ax

				// Increment the old image offset, wrapping back to zero if the 
				// end of the row is reached.

				mov eax, column_index
				inc eax
				cmp eax, image_width 
				jl next_pixel3
				mov eax, 0
			next_pixel3:
				mov column_index, eax

				// Increment the new image pointer.

				add edx, 2

				// Decrement the column counter, and copy next pixel in row if
				// there are any left.

				dec esi
				jnz next_column3

				// Increment the old image row pointer, and wrap back to the
				// first row if the end of the image has been reached.

				add ebx, image_width
				cmp ebx, end_image_ptr
				jl skip_wrap3
				mov ebx, image_ptr

			skip_wrap3:

				// Decrement the row counter, and copy next row if there are any
				// left.

				dec edi
				jnz next_row3
			}
			break;

		// If the display depth is 24 or 32, convert the unlit image to a 32-bit
		// lit image...

		case 24:
		case 32:

			// Get the transparency mask.

			transparency_mask32 = frame_buffer_pixel_format_ptr->alpha_comp_mask;

			// Perform the conversion.

			__asm {
			
				// EBX: holds pointer to current row of old image.
				// EDX: holds pointer to current pixel of new image.
				// EDI: holds the number of rows left to copy.

				mov ebx, image_ptr
				mov edx, new_image_ptr
				mov edi, lit_image_dimensions

			next_row4:

				// ESI: holds number of columns left to copy.

				mov esi, lit_image_dimensions

				// Clear old image offset.

				mov eax, 0
				mov column_index, eax

			next_column4:

				// Get the current 8-bit index from the old image, use it to
				// obtain the 24-bit pixel, and store it in the new image.  If 
				// the 8-bit index is not the transparent index, mark the pixel as
				// opaque by setting the tranparency mask.

				mov eax, 0
				mov ecx, column_index
				mov al, [ebx + ecx]
				mov ecx, palette_ptr
				cmp eax, transparent_index
				jne opaque_pixel4
				mov eax, [ecx + eax * 4]	
				jmp store_pixel4
			opaque_pixel4:
				mov eax, [ecx + eax * 4]
				or eax, transparency_mask32
			store_pixel4:
				mov [edx], eax

				// Increment the old image offset, wrapping back to zero if the 
				// end of the row is reached.

				mov eax, column_index
				inc eax
				cmp eax, image_width 
				jl next_pixel4
				mov eax, 0
			next_pixel4:
				mov column_index, eax

				// Increment the new image pointer.

				add edx, 4

				// Decrement the column counter, and copy next pixel in row if
				// there are any left.

				dec esi
				jnz next_column4

				// Increment the old image row pointer, and wrap back to the
				// first row if the end of the image has been reached.

				add ebx, image_width
				cmp ebx, end_image_ptr
				jl skip_wrap4
				mov ebx, image_ptr

			skip_wrap4:

				// Decrement the row counter, and copy next row if there are any
				// left.

				dec edi
				jnz next_row4
			}
		}
	}
}

//------------------------------------------------------------------------------
// Render a colour span to a 16-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_colour_span16(span *span_ptr)
{
	byte *fb_ptr;
	word colour_pixel16;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Calculate the frame buffer pointer and span width.

	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 1);
	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Get the 16-bit colour pixel.

	colour_pixel16 = (word)span_ptr->colour_pixel;

	// Render the span into the 16-bit frame buffer.

	__asm {

		// Put the texture colour into AX, the image pointer into EBX, and
		// the span width into ECX.

		mov ax, colour_pixel16
		mov ebx, fb_ptr
		mov ecx, span_width

	next_pixel16:

		// Store the texture colour in the frame buffer, then advance the
		// frame buffer pointer.

		mov [ebx], ax
		add ebx, 2

		// Decrement the loop counter, and continue if we haven't filled the
		// whole span yet.

		dec ecx
		jnz next_pixel16
	}
}

//------------------------------------------------------------------------------
// Render a colour span to a 24-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_colour_span24(span *span_ptr)
{
	byte *fb_ptr;
	pixel colour_pixel24;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Calculate the frame buffer pointer and span width.

	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + span_ptr->start_sx * 3;
	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Get the 24-bit colour pixel.

	colour_pixel24 = span_ptr->colour_pixel;

	// Render the span into the 24-bit frame buffer.

	__asm {

		// Put the texture colour into EAX, the image pointer into EBX, and
		// the span width into ECX.

		mov eax, colour_pixel24
		mov ebx, fb_ptr
		mov ecx, span_width

	next_pixel24:

		// Store the texture colour in the frame buffer, then advance the
		// frame buffer pointer.

		mov edx, [ebx]
		and edx, 0xff000000
		or edx, eax
		mov [ebx], edx
		add ebx, 3

		// Decrement the loop counter, and continue if we haven't filled the
		// whole span yet.

		dec ecx
		jnz next_pixel24
	}
}

//------------------------------------------------------------------------------
// Render a colour span to a 32-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_colour_span32(span *span_ptr)
{
	byte *fb_ptr;
	pixel colour_pixel32;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Calculate the frame buffer pointer and span width.

	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 2);
	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Get the 32-bit colour pixel.

	colour_pixel32 = span_ptr->colour_pixel;

	// Render the span into the 32-bit frame buffer.

	__asm {

		// Put the texture colour into EAX, the image pointer into EBX, and
		// the span width into ECX.

		mov eax, colour_pixel32
		mov ebx, fb_ptr
		mov ecx, span_width

	next_pixel32:

		// Store the texture colour in the frame buffer, then advance the
		// frame buffer pointer.

		mov [ebx], eax
		add ebx, 4

		// Decrement the loop counter, and continue if we haven't filled the
		// whole span yet.

		dec ecx
		jnz next_pixel32
	}
}

//------------------------------------------------------------------------------
// Render a colour span to the frame buffer.
//------------------------------------------------------------------------------

void
render_colour_span(span *span_ptr)
{
	switch (frame_buffer_depth) {
	case 16:
		render_colour_span16(span_ptr);
		break;
	case 24:
		render_colour_span24(span_ptr);
		break;
	case 32:
		render_colour_span32(span_ptr);
	}
}

//------------------------------------------------------------------------------
// Render a transparent span to a 16-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_transparent_span16(span *span_ptr)
{
	cache_entry *cache_entry_ptr;
	float start_one_on_tz;
	float end_one_on_tz, end_tz;
	int span_start_sx, span_end_sx, end_sx;
	float u_on_tz, v_on_tz;
	fixed u, v;
	fixed end_u, end_v;
	fixed delta_u, delta_v;
	span_data scaled_delta_span;
	byte *image_ptr, *fb_ptr;
	int mask, shift;
	word transparency_mask16;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the lit image data.

	cache_entry_ptr = get_cache_entry(span_ptr->pixmap_ptr, span_ptr->brightness_index);
	image_ptr = cache_entry_ptr->lit_image_ptr;
	mask = cache_entry_ptr->lit_image_mask;
	shift = cache_entry_ptr->lit_image_shift;

	// Pre-scale the deltas for faster calculations when rendering spans that
	// are SPAN_WIDTH in width.

	scaled_delta_span.one_on_tz = span_ptr->delta_span.one_on_tz * SPAN_WIDTH;
	scaled_delta_span.u_on_tz = span_ptr->delta_span.u_on_tz * SPAN_WIDTH;
	scaled_delta_span.v_on_tz = span_ptr->delta_span.v_on_tz * SPAN_WIDTH;

	// Get the starting 1/tz value; if it is zero, make it one (this is used
	// by sky spans to ensure they are furthest from the viewer, rather than
	// using a tiny 1/tz value that introduces errors into the texture
	// coordinates).

	start_one_on_tz = span_ptr->start_span.one_on_tz;
	if (start_one_on_tz == 0.0)
		start_one_on_tz = 1.0;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 1);

	// Get the transparency mask.

	transparency_mask16 = (word)frame_buffer_pixel_format_ptr->alpha_comp_mask;

	// Compute (u,v) for that pixel.  We are now representing (u,v) as true fixed
	// point values for speed.

	u_on_tz = span_ptr->start_span.u_on_tz;
	v_on_tz = span_ptr->start_span.v_on_tz;
	end_tz = 1.0f / start_one_on_tz;
	COMPUTE_UV(u, v, u_on_tz, v_on_tz, end_tz);

	// Compute the start and end data for the first span.

	span_start_sx = span_ptr->start_sx;
	span_end_sx = span_ptr->start_sx + SPAN_WIDTH;
	end_sx = span_ptr->end_sx;
	end_one_on_tz = start_one_on_tz + scaled_delta_span.one_on_tz;
	end_tz = 1.0f / end_one_on_tz;

	// Now render the row one span at a time, until we have less than a span's
	// width of pixels left.

	while (span_end_sx < end_sx) {
			
		// Compute (end_u, end_v) and (delta_u, delta_v) for this span, 
		// storing them as fixed point numbers for speed.  We also compute
		// the ending 1/tz value for the *next* span.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;
		end_one_on_tz += scaled_delta_span.one_on_tz;

		// The rest of the render span code is done in assembler for speed...

		__asm {

			// Start computing 1/one_on_tz for the next span (the floating 
			// point divide will overlap the span render loop on a Pentium).

			fld const_1
			fdiv end_one_on_tz

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask and shift in ECX; the mask occupies the top 
			// word, and the shift occupies CL.

			mov ecx, mask
			or  ecx, shift

			// Move the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render the pixels in the span.

			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_1)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_2)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_3)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_4)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_5)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_6)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_7)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_8)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_9)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_10)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_11)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_12)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_13)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_14)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_15)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_16)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_17)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_18)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_19)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_20)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_21)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_22)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_23)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_24)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_25)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_26)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_27)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_28)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_29)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_30)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_31)
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16_32)

			// Save new value of frame buffer

			mov fb_ptr, esi

			// Store the result of 1/one_on_tz in end_tz (this computation
			// should be well and truly completed by now).

			fstp end_tz
		}

		// Get ready for the next span.

		u = end_u;
		v = end_v;
		span_start_sx = span_end_sx;
		span_end_sx += SPAN_WIDTH;
	}

	// If there are pixels left, render one more shorter span.

	if (span_start_sx < end_sx) {

		// Compute (end_u, end_v) and (delta_u, delta_v) for this span,
		// storing them as fixed point numbers for speed.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;

		// Compute the size of this last span.

		span_width = (end_sx - span_start_sx) << 8;

		// Render the span...

		__asm {

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask, shift and span width in ECX; the mask
			// occupies the top word, the shift occupies CL, and the span
			// width occupies CH.

			mov ecx, mask
			or  ecx, shift
			or	ecx, span_width

			// Move the the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render span_width texture mapped pixels.

		next_pixel16:
			DRAW_TRANSPARENT_PIXEL16(skip_pixel16)
			dec ch
			jnz next_pixel16
			
			// Save new value of the frame buffer pointer.

			mov fb_ptr, esi
		}
	}
}

//------------------------------------------------------------------------------
// Render a transparent span to a 24-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_transparent_span24(span *span_ptr)
{
	cache_entry *cache_entry_ptr;
	float start_one_on_tz;
	float end_one_on_tz, end_tz;
	int span_start_sx, span_end_sx, end_sx;
	float u_on_tz, v_on_tz;
	fixed u, v;
	fixed end_u, end_v;
	fixed delta_u, delta_v;
	span_data scaled_delta_span;
	byte *image_ptr, *fb_ptr;
	int mask, shift;
	pixel transparency_mask24;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the lit image data.

	cache_entry_ptr = get_cache_entry(span_ptr->pixmap_ptr, span_ptr->brightness_index);
	image_ptr = cache_entry_ptr->lit_image_ptr;
	mask = cache_entry_ptr->lit_image_mask;
	shift = cache_entry_ptr->lit_image_shift;

	// Pre-scale the deltas for faster calculations when rendering spans that
	// are SPAN_WIDTH in width.

	scaled_delta_span.one_on_tz = span_ptr->delta_span.one_on_tz * SPAN_WIDTH;
	scaled_delta_span.u_on_tz = span_ptr->delta_span.u_on_tz * SPAN_WIDTH;
	scaled_delta_span.v_on_tz = span_ptr->delta_span.v_on_tz * SPAN_WIDTH;

	// Get the starting 1/tz value; if it is zero, make it one (this is used
	// by sky spans to ensure they are furthest from the viewer, rather than
	// using a tiny 1/tz value that introduces errors into the texture
	// coordinates).

	start_one_on_tz = span_ptr->start_span.one_on_tz;
	if (start_one_on_tz == 0.0)
		start_one_on_tz = 1.0;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx * 3);

	// Get the transparency mask.

	transparency_mask24 = frame_buffer_pixel_format_ptr->alpha_comp_mask;

	// Compute (u,v) for that pixel.  We are now representing (u,v) as true fixed
	// point values for speed.

	u_on_tz = span_ptr->start_span.u_on_tz;
	v_on_tz = span_ptr->start_span.v_on_tz;
	end_tz = 1.0f / start_one_on_tz;
	COMPUTE_UV(u, v, u_on_tz, v_on_tz, end_tz);

	// Compute the start and end data for the first span.

	span_start_sx = span_ptr->start_sx;
	span_end_sx = span_ptr->start_sx + SPAN_WIDTH;
	end_sx = span_ptr->end_sx;
	end_one_on_tz = start_one_on_tz + scaled_delta_span.one_on_tz;
	end_tz = 1.0f / end_one_on_tz;

	// Now render the row one span at a time, until we have less than a span's
	// width of pixels left.

	while (span_end_sx < end_sx) {
			
		// Compute (end_u, end_v) and (delta_u, delta_v) for this span, 
		// storing them as fixed point numbers for speed.  We also compute
		// the ending 1/tz value for the *next* span.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;
		end_one_on_tz += scaled_delta_span.one_on_tz;

		// The rest of the render span code is done in assembler for speed...

		__asm {

			// Start computing 1/one_on_tz for the next span (the floating 
			// point divide will overlap the span render loop on a Pentium).

			fld const_1
			fdiv end_one_on_tz

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask and shift in ECX; the mask occupies the top 
			// word, and the shift occupies CL.

			mov ecx, mask
			or  ecx, shift

			// Move the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render the pixels in the span.

			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_1)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_2)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_3)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_4)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_5)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_6)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_7)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_8)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_9)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_10)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_11)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_12)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_13)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_14)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_15)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_16)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_17)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_18)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_19)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_20)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_21)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_22)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_23)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_24)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_25)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_26)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_27)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_28)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_29)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_30)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_31)
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24_32)

			// Save new value of frame buffer

			mov fb_ptr, esi

			// Store the result of 1/one_on_tz in end_tz (this computation
			// should be well and truly completed by now).

			fstp end_tz
		}

		// Get ready for the next span.

		u = end_u;
		v = end_v;
		span_start_sx = span_end_sx;
		span_end_sx += SPAN_WIDTH;
	}

	// If there are pixels left, render one more shorter span.

	if (span_start_sx < end_sx) {

		// Compute (end_u, end_v) and (delta_u, delta_v) for this span,
		// storing them as fixed point numbers for speed.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;

		// Compute the size of this last span.

		span_width = (end_sx - span_start_sx) << 8;

		// Render the span...

		__asm {

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask, shift and span width in ECX; the mask
			// occupies the top word, the shift occupies CL, and the span
			// width occupies CH.

			mov ecx, mask
			or  ecx, shift
			or	ecx, span_width

			// Move the the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render span_width texture mapped pixels.

		next_pixel24:
			DRAW_TRANSPARENT_PIXEL24(skip_pixel24)
			dec ch
			jnz next_pixel24
			
			// Save new value of the frame buffer pointer.

			mov fb_ptr, esi
		}
	}
}

//------------------------------------------------------------------------------
// Render a transparent span to a 32-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_transparent_span32(span *span_ptr)
{
	cache_entry *cache_entry_ptr;
	float start_one_on_tz;
	float end_one_on_tz, end_tz;
	int span_start_sx, span_end_sx, end_sx;
	float u_on_tz, v_on_tz;
	fixed u, v;
	fixed end_u, end_v;
	fixed delta_u, delta_v;
	span_data scaled_delta_span;
	byte *image_ptr, *fb_ptr;
	int mask, shift;
	pixel transparency_mask32;
	int span_width;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the lit image data.

	cache_entry_ptr = get_cache_entry(span_ptr->pixmap_ptr, 
		span_ptr->brightness_index);
	image_ptr = cache_entry_ptr->lit_image_ptr;
	mask = cache_entry_ptr->lit_image_mask;
	shift = cache_entry_ptr->lit_image_shift;

	// Pre-scale the deltas for faster calculations when rendering spans that
	// are SPAN_WIDTH in width.

	scaled_delta_span.one_on_tz = span_ptr->delta_span.one_on_tz * SPAN_WIDTH;
	scaled_delta_span.u_on_tz = span_ptr->delta_span.u_on_tz * SPAN_WIDTH;
	scaled_delta_span.v_on_tz = span_ptr->delta_span.v_on_tz * SPAN_WIDTH;

	// Get the starting 1/tz value; if it is zero, make it one (this is used
	// by sky spans to ensure they are furthest from the viewer, rather than
	// using a tiny 1/tz value that introduces errors into the texture
	// coordinates).

	start_one_on_tz = span_ptr->start_span.one_on_tz;
	if (start_one_on_tz == 0.0)
		start_one_on_tz = 1.0;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 2);

	// Get the transparency mask.

	transparency_mask32 = frame_buffer_pixel_format_ptr->alpha_comp_mask;

	// Compute (u,v) for that pixel.  We are now representing (u,v) as true fixed
	// point values for speed.

	u_on_tz = span_ptr->start_span.u_on_tz;
	v_on_tz = span_ptr->start_span.v_on_tz;
	end_tz = 1.0f / start_one_on_tz;
	COMPUTE_UV(u, v, u_on_tz, v_on_tz, end_tz);

	// Compute the start and end data for the first span.

	span_start_sx = span_ptr->start_sx;
	span_end_sx = span_ptr->start_sx + SPAN_WIDTH;
	end_sx = span_ptr->end_sx;
	end_one_on_tz = start_one_on_tz + scaled_delta_span.one_on_tz;
	end_tz = 1.0f / end_one_on_tz;

	// Now render the row one span at a time, until we have less than a span's
	// width of pixels left.

	while (span_end_sx < end_sx) {
			
		// Compute (end_u, end_v) and (delta_u, delta_v) for this span, 
		// storing them as fixed point numbers for speed.  We also compute
		// the ending 1/tz value for the *next* span.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;
		end_one_on_tz += scaled_delta_span.one_on_tz;

		// The rest of the render span code is done in assembler for speed...

		__asm {

			// Start computing 1/one_on_tz for the next span (the floating 
			// point divide will overlap the span render loop on a Pentium).

			fld const_1
			fdiv end_one_on_tz

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask and shift in ECX; the mask occupies the top 
			// word, and the shift occupies CL.

			mov ecx, mask
			or  ecx, shift

			// Move the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render the pixels in the span.

			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_1)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_2)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_3)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_4)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_5)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_6)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_7)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_8)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_9)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_10)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_11)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_12)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_13)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_14)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_15)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_16)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_17)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_18)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_19)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_20)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_21)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_22)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_23)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_24)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_25)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_26)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_27)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_28)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_29)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_30)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_31)
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32_32)

			// Save new value of frame buffer

			mov fb_ptr, esi

			// Store the result of 1/one_on_tz in end_tz (this computation
			// should be well and truly completed by now).

			fstp end_tz
		}

		// Get ready for the next span.

		u = end_u;
		v = end_v;
		span_start_sx = span_end_sx;
		span_end_sx += SPAN_WIDTH;
	}

	// If there are pixels left, render one more shorter span.

	if (span_start_sx < end_sx) {

		// Compute (end_u, end_v) and (delta_u, delta_v) for this span,
		// storing them as fixed point numbers for speed.
		
		u_on_tz += scaled_delta_span.u_on_tz;
		v_on_tz += scaled_delta_span.v_on_tz;
		COMPUTE_UV(end_u, end_v, u_on_tz, v_on_tz, end_tz);
		delta_u = (end_u - u) >> SPAN_SHIFT;
		delta_v = (end_v - v) >> SPAN_SHIFT;

		// Compute the size of this last span.

		span_width = (end_sx - span_start_sx) << 8;

		// Render the span...

		__asm {

			// Put u in EBX and v in EDX.

			mov ebx, u
			mov edx, v

			// Combine the mask, shift and span width in ECX; the mask
			// occupies the top word, the shift occupies CL, and the span
			// width occupies CH.

			mov ecx, mask
			or  ecx, shift
			or	ecx, span_width

			// Move the the frame buffer pointer into ESI.

			mov esi, fb_ptr

			// Render span_width texture mapped pixels.

		next_pixel32:
			DRAW_TRANSPARENT_PIXEL32(skip_pixel32)
			dec ch
			jnz next_pixel32
			
			// Save new value of the frame buffer pointer.

			mov fb_ptr, esi
		}
	}
}

//------------------------------------------------------------------------------
// Render a transparent span to the frame buffer.
//------------------------------------------------------------------------------

void
render_transparent_span(span *span_ptr)
{
	switch (frame_buffer_depth) {
	case 16:
		render_transparent_span16(span_ptr);
		break;
	case 24:
		render_transparent_span24(span_ptr);
		break;
	case 32:
		render_transparent_span32(span_ptr);
	}
}

//------------------------------------------------------------------------------
// Render a linear span into a 16-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_linear_span16(bool image_is_16_bit, byte *image_ptr, byte *fb_ptr,
					 pixel *palette_ptr, int transparent_index,
					 pixel transparency_mask32, int image_width, int span_width,
					 fixed u)
{
	// If the image is 16 bit...

	if (image_is_16_bit) {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

			// Put the frame buffer pointer in ESI.

			mov esi, fb_ptr

		next_pixel16a:

			// Load the 16-bit image pixel into EAX.  If it's transparent, skip it.
			
			mov eax, 0
			mov edi, image_ptr
			mov ax, [edi + ebx]
			test eax, transparency_mask32
			jz skip_pixel16a

			// Use the unlit 16-bit pixel as an index into the palette to obtain
			// the lit 16-bit pixel, which we then store in the frame buffer.

			mov edi, palette_ptr
			mov ax, [edi + eax * 4]
			mov [esi], ax

		skip_pixel16a:

			// Advance the frame buffer pointer.

			add esi, 2

			// Advance u, wrapping to zero if it equals image_width.

			add ebx, 2
			cmp ebx, ecx
			jl done_pixel16a
			mov ebx, 0

		done_pixel16a:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel16a
		}
	}

	// If the image is 8 bit...

	else {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

			// Put the frame buffer pointer in ESI.

			mov esi, fb_ptr

		next_pixel16b:

			// Load the 8-bit image pixel into EAX.  If it's the transparent
			// colour index, skip this pixel.
			
			mov eax, 0
			mov edi, image_ptr
			mov al, [edi + ebx]
			cmp eax, transparent_index
			je skip_pixel16b

			// Use the 8-bit pixel as an index into the palette to obtain the 
			// 16-bit pixel, which we then store in the frame buffer.

			mov edi, palette_ptr
			mov ax, [edi + eax * 4]
			mov [esi], ax

		skip_pixel16b:

			// Advance the frame buffer pointer.

			add esi, 2

			// Advance u, wrapping to zero if it equals image_width.

			inc ebx
			cmp ebx, ecx
			jl done_pixel16b
			mov ebx, 0

		done_pixel16b:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel16b
		}
	}
}

//------------------------------------------------------------------------------
// Render a linear span into a 24-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_linear_span24(bool image_is_16_bit, byte *image_ptr, byte *fb_ptr,
					 pixel *palette_ptr, int transparent_index,
					 pixel transparency_mask32, int image_width, int span_width,
					 fixed u)
{
	// If the image is 16 bit...

	if (image_is_16_bit) {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

		next_pixel24a:

			// Load the 16-bit image pixel into EAX.  If it's transparent,
			// skip this pixel.
			
			mov eax, 0
			mov edi, image_ptr
			mov ax, [edi + ebx]
			test eax, transparency_mask32
			jz skip_pixel24a

			// Use the 16-bit pixel as an index into the palette to obtain the 
			// 24-bit pixel, which we then store in the frame buffer.

			mov edi, fb_ptr
			mov esi, [edi]
			and esi, 0xff000000
			mov edi, palette_ptr
			or esi, [edi + eax * 4]
			mov edi, fb_ptr
			mov [edi], esi

		skip_pixel24a:

			// Advance the frame buffer pointer.

			add edi, 3
			mov fb_ptr, edi

			// Advance u, wrapping to zero if it equals image_width.

			add ebx, 2
			cmp ebx, ecx
			jl done_pixel24a
			mov ebx, 0

		done_pixel24a:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel24a
		}
	}

	// If the image is 8 bit...
	
	else {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

		next_pixel24b:

			// Load the 8-bit image pixel into EAX.  If it's the transparent
			// colour index, skip this pixel.
			
			mov eax, 0
			mov edi, image_ptr
			mov al, [edi + ebx]
			cmp eax, transparent_index
			je skip_pixel24b

			// Use the 8-bit pixel as an index into the palette to obtain the 
			// 24-bit pixel, which we then store in the frame buffer.

			mov edi, fb_ptr
			mov esi, [edi]
			and esi, 0xff000000
			mov edi, palette_ptr
			or esi, [edi + eax * 4]
			mov edi, fb_ptr
			mov [edi], esi

		skip_pixel24b:

			// Advance the frame buffer pointer.

			add edi, 3
			mov fb_ptr, edi

			// Advance u, wrapping to zero if it equals image_width.

			inc ebx
			cmp ebx, ecx
			jl done_pixel24b
			mov ebx, 0

		done_pixel24b:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel24b
		}
	}
}

//------------------------------------------------------------------------------
// Render a linear span into a 32-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_linear_span32(bool image_is_16_bit, byte *image_ptr, byte *fb_ptr,
					 pixel *palette_ptr, int transparent_index,
					 pixel transparency_mask32, int image_width, int span_width,
					 fixed u)
{
	// If the image is 16 bit...

	if (image_is_16_bit) {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

			// Put the frame buffer pointer in ESI.

			mov esi, fb_ptr

		next_pixel32a:

			// Load the 16-bit image pixel into EAX.  If it's transparent,
			// skip this pixel.

			mov eax, 0
			mov edi, image_ptr
			mov ax, [edi + ebx]
			test eax, transparency_mask32
			jz skip_pixel32a

			// Use the 16-bit pixel as an index into the palette to obtain the 
			// 32-bit pixel, which we then store in the frame buffer.

			mov edi, palette_ptr
			mov eax, [edi + eax * 4]
			mov [esi], eax

		skip_pixel32a:

			// Advance the frame buffer pointer.

			add esi, 4

			// Advance u, wrapping to zero if it equals image_width.

			add ebx, 2
			cmp ebx, ecx
			jl done_pixel32a
			mov ebx, 0

		done_pixel32a:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel32a
		}
	}

	// If the pixmap is 8 bit...
	
	else {
		__asm {

			// Put u in EBX, image_width in ECX and span_width in EDX.

			mov ebx, u
			mov ecx, image_width
			mov edx, span_width

			// Put the frame buffer pointer in ESI.

			mov esi, fb_ptr

		next_pixel32b:

			// Advance the frame buffer pointer.

			mov eax, 0
			mov edi, image_ptr
			mov al, [edi + ebx]
			cmp eax, transparent_index
			je skip_pixel32b

			// Use the 8-bit pixel as an index into the palette to obtain the 
			// 24-bit pixel, which we then store in the frame buffer.

			mov edi, palette_ptr
			mov eax, [edi + eax * 4]
			mov [esi], eax

		skip_pixel32b:

			// Advance the frame buffer pointer.

			add esi, 4

			// Advance u, wrapping to zero if it equals image_width.

			inc ebx
			cmp ebx, ecx
			jl done_pixel32b
			mov ebx, 0

		done_pixel32b:

			// If there are still pixels to render, repeat loop.

			dec edx
			jnz next_pixel32b
		}
	}
}

//------------------------------------------------------------------------------
// Render a popup span into a 16-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_popup_span16(span *span_ptr)
{
	pixmap *pixmap_ptr;
	byte *image_ptr, *fb_ptr;
	pixel *palette_ptr;
	int transparent_index;
	pixel transparency_mask32;
	int image_width, span_width;
	fixed u, v;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the pointer to the pixmap to render, the display palette for
	// the desired brightness level, the transparent colour index or mask,
	// and the image width (in bytes).
	
	pixmap_ptr = span_ptr->pixmap_ptr;
	if (pixmap_ptr->bytes_per_pixel == 2) {
		palette_ptr = light_table[span_ptr->brightness_index];
		transparency_mask32 = texture_pixel_format.alpha_comp_mask;
		image_width = pixmap_ptr->width * 2;
		u = ((int)span_ptr->start_span.u_on_tz % pixmap_ptr->width) * 2;
	} else {
		palette_ptr = pixmap_ptr->display_palette_list + 
			span_ptr->brightness_index * pixmap_ptr->colours;
		transparent_index = pixmap_ptr->transparent_index;
		image_width = pixmap_ptr->width;
		u = (int)span_ptr->start_span.u_on_tz % pixmap_ptr->width;
	}
	v = (int)span_ptr->start_span.v_on_tz % pixmap_ptr->height;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 1);

	// Compute the image pointer.

	image_ptr = pixmap_ptr->image_ptr + v * image_width;

	// Compute the span width.

	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Render the span

	render_linear_span16(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
		palette_ptr, transparent_index, transparency_mask32, image_width,
		span_width, u);
}

//------------------------------------------------------------------------------
// Render a popup span into a 24-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_popup_span24(span *span_ptr)
{
	pixmap *pixmap_ptr;
	byte *image_ptr, *fb_ptr;
	pixel *palette_ptr;
	int transparent_index;
	pixel transparency_mask32;
	int image_width, span_width;
	fixed u, v;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the pointer to the pixmap to render, the display palette for
	// the desired brightness level, the transparent colour index or mask,
	// and the image width (in bytes).
	
	pixmap_ptr = span_ptr->pixmap_ptr;
	if (pixmap_ptr->bytes_per_pixel == 2) {
		palette_ptr = light_table[span_ptr->brightness_index];
		transparency_mask32 = texture_pixel_format.alpha_comp_mask;
		image_width = pixmap_ptr->width * 2;
		u = ((int)span_ptr->start_span.u_on_tz % pixmap_ptr->width) * 2;
	} else {
		palette_ptr = pixmap_ptr->display_palette_list + 
			span_ptr->brightness_index * pixmap_ptr->colours;
		transparent_index = pixmap_ptr->transparent_index;
		image_width = pixmap_ptr->width;
		u = (int)span_ptr->start_span.u_on_tz % pixmap_ptr->width;
	}
	v = (int)span_ptr->start_span.v_on_tz % pixmap_ptr->height;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + span_ptr->start_sx * 3;

	// Compute the image pointer.

	image_ptr = pixmap_ptr->image_ptr + v * image_width;

	// Compute the span width.

	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Render the span.

	render_linear_span24(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
		palette_ptr, transparent_index, transparency_mask32, image_width,
		span_width, u);
}


//------------------------------------------------------------------------------
// Render a popup span into a 32-bit frame buffer.
//------------------------------------------------------------------------------

static void
render_popup_span32(span *span_ptr)
{
	pixmap *pixmap_ptr;
	byte *image_ptr, *fb_ptr;
	pixel *palette_ptr;
	int transparent_index;
	pixel transparency_mask32 = 0;
	int image_width, span_width;
	fixed u, v;

	// Ignore span if it has zero width.

	if (span_ptr->start_sx == span_ptr->end_sx)
		return;

	// Get the pointer to the pixmap to render, the display palette for
	// the desired brightness level, the transparent colour index or mask,
	// and the image width (in bytes).
	
	pixmap_ptr = span_ptr->pixmap_ptr;
	if (pixmap_ptr->bytes_per_pixel == 2) {
		palette_ptr = light_table[span_ptr->brightness_index];
		transparency_mask32 = texture_pixel_format.alpha_comp_mask;
		image_width = pixmap_ptr->width * 2;
		u = ((int)span_ptr->start_span.u_on_tz % pixmap_ptr->width) * 2;
	} else {
		palette_ptr = pixmap_ptr->display_palette_list + 
			span_ptr->brightness_index * pixmap_ptr->colours;
		transparent_index = pixmap_ptr->transparent_index;
		image_width = pixmap_ptr->width;
		u = (int)span_ptr->start_span.u_on_tz % pixmap_ptr->width;
	}
	v = (int)span_ptr->start_span.v_on_tz % pixmap_ptr->height;

	// Get the pointer to the starting pixel in the frame buffer.
	
	fb_ptr = frame_buffer_ptr + frame_buffer_row_pitch * span_ptr->sy + (span_ptr->start_sx << 2);

	// Compute the image pointer.

	image_ptr = pixmap_ptr->image_ptr + v * image_width;

	// Compute the span width.

	span_width = span_ptr->end_sx - span_ptr->start_sx;

	// Render the span.

	render_linear_span32(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
		palette_ptr, transparent_index, transparency_mask32, image_width,
		span_width, u);
}

//------------------------------------------------------------------------------
// Render a popup span to the frame buffer.
//------------------------------------------------------------------------------

void
render_popup_span(span *span_ptr)
{
	switch (frame_buffer_depth) {
	case 16:
		render_popup_span16(span_ptr);
		break;
	case 24:
		render_popup_span24(span_ptr);
		break;
	case 32:
		render_popup_span32(span_ptr);
	}
}

//------------------------------------------------------------------------------
// Render a pixel to the frame buffer.
//------------------------------------------------------------------------------

static void
render_pixel16(int x, int y, pixel pixel_colour)
{
	*(word *)(frame_buffer_ptr + frame_buffer_row_pitch * y + (x << 1)) = pixel_colour;
}

static void
render_pixel24(int x, int y, pixel pixel_colour)
{
	word *fb_ptr = (word *)(frame_buffer_ptr + frame_buffer_row_pitch * y + (x * 3));
	*fb_ptr = (*fb_ptr & 0xFF000000) | (pixel_colour & 0x00FFFFFF);
}

static void
render_pixel32(int x, int y, pixel pixel_colour)
{
	*(pixel *)(frame_buffer_ptr + frame_buffer_row_pitch * y + (x << 2)) = pixel_colour;
}

//------------------------------------------------------------------------------
// Render a line to the frame buffer.
//------------------------------------------------------------------------------

static void
render_line(spoint *spoint1_ptr, spoint *spoint2_ptr, pixel pixel_colour)
{
	int x0 = (int)spoint1_ptr->sx;
	int y0 = (int)spoint1_ptr->sy;
	int x1 = (int)spoint2_ptr->sx;
	int y1 = (int)spoint2_ptr->sy;
	int dx = abs(x1 - x0);
	int sx = x0 < x1 ? 1 : -1;
	int dy = abs(y1 - y0);
	int sy = y0 < y1 ? 1 : -1; 
	int err = (dx > dy ? dx : -dy) / 2;
	for (;;) {
		if (x0 >= 0 && x0 < window_width && y0 >= 0 && y0 < window_height) {
			switch (frame_buffer_depth) {
			case 16:
				render_pixel16(x0, y0, pixel_colour);
				break;
			case 24:
				render_pixel24(x0, y0, pixel_colour);
				break;
			case 32:
				render_pixel32(x0, y0, pixel_colour);
			}
		}
		if (x0 == x1 && y0 == y1) {
			break;
		}
		int e2 = err;
		if (e2 > -dx) { 
			err -= dy; 
			x0 += sx; 
		}
		if (e2 < dy) { 
			err += dx; 
			y0 += sy;
		}
	}
}

//------------------------------------------------------------------------------
// Render a list of lines to the frame buffer.
//------------------------------------------------------------------------------

void
render_lines(spoint *spoint_list, int spoints, RGBcolour colour)
{
	pixel pixel_colour = RGB_to_display_pixel(colour);
	for (int i = 0; i < spoints; i += 2) {
		spoint *spoint1_ptr = spoint_list++;
		spoint *spoint2_ptr = spoint_list++;
		render_line(spoint1_ptr, spoint2_ptr, pixel_colour);
	}
}

//==============================================================================
// Function to determine intersection of the mouse with a polygon.
//==============================================================================

bool
mouse_intersects_with_polygon(float mouse_x, float mouse_y, vector *camera_direction_ptr, tpolygon *tpolygon_ptr)
{
	XMVECTOR ray_start = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR ray_direction = XMVectorSet(camera_direction_ptr->dx, camera_direction_ptr->dy, camera_direction_ptr->dz, 0.0f);
	tvertex *tvertex_ptr = tpolygon_ptr->tvertex_list;
	XMVECTOR tvertex0 = XMVectorSet(tvertex_ptr->x, tvertex_ptr->y, tvertex_ptr->z, 0.0f);
	tvertex_ptr = tvertex_ptr->next_tvertex_ptr;
	XMVECTOR tvertex1 = XMVectorSet(tvertex_ptr->x, tvertex_ptr->y, tvertex_ptr->z, 0.0f);
	tvertex_ptr = tvertex_ptr->next_tvertex_ptr;
	while (tvertex_ptr) {
		XMVECTOR tvertex2 = XMVectorSet(tvertex_ptr->x, tvertex_ptr->y, tvertex_ptr->z, 0.0f);
		float distance;
		if (TriangleTests::Intersects(ray_start, ray_direction, tvertex0, tvertex1, tvertex2, distance)) {
			return true;
		}
		tvertex0 = tvertex1;
		tvertex1 = tvertex2;
		tvertex_ptr = tvertex_ptr->next_tvertex_ptr;
	}
	return false;
}

//==============================================================================
// Hardware rendering functions.
//==============================================================================

//------------------------------------------------------------------------------
// Set the perspective transform.
//------------------------------------------------------------------------------

void
hardware_set_projection_transform(float viewport_width, float viewport_height, float near_z, float far_z)
{
	XMMATRIX d3d_projection_matrix = XMMatrixPerspectiveLH(viewport_width, viewport_height, near_z, far_z);
	hardware_matrix_constant_buffer constant_buffer;
	constant_buffer.projection = XMMatrixTranspose(d3d_projection_matrix);
	d3d_device_context_ptr->UpdateSubresource(d3d_constant_buffer_list[1], 0, nullptr, &constant_buffer, 0, 0);
}

//------------------------------------------------------------------------------
// Update fog settings.
//------------------------------------------------------------------------------

void
hardware_update_fog_settings(bool enabled, fog *fog_ptr, float max_radius)
{
	hardware_fog_constant_buffer constant_buffer;
	constant_buffer.fog_style = enabled ? fog_ptr->style + 1 : 0;
	constant_buffer.fog_start = fog_ptr->start_radius == 0.0f ? 1.0f : fog_ptr->start_radius;
	constant_buffer.fog_end = fog_ptr->end_radius == 0.0f ? max_radius : fog_ptr->end_radius;
	constant_buffer.fog_density = fog_ptr->density;
	constant_buffer.fog_colour = XMFLOAT4(fog_ptr->colour.red, fog_ptr->colour.green, fog_ptr->colour.blue, 1.0f);
	d3d_device_context_ptr->UpdateSubresource(d3d_constant_buffer_list[0], 0, nullptr, &constant_buffer, 0, 0);
}

//------------------------------------------------------------------------------
// Create a hardware texture and return an opaque pointer to it.
//------------------------------------------------------------------------------

void *
hardware_create_texture(int image_size_index)
{
	hardware_texture *hardware_texture_ptr;

	// Create the hardware texture object.

	if ((hardware_texture_ptr = new hardware_texture) == NULL) {
		return NULL;
	}
	hardware_texture_ptr->image_dimensions = image_dimensions_list[image_size_index];

	// Create the Direct3D texture object.

	hardware_texture_ptr->d3d_texture_ptr = create_d3d_texture(hardware_texture_ptr->image_dimensions, hardware_texture_ptr->image_dimensions,
		DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, D3D11_CPU_ACCESS_WRITE);
	if (hardware_texture_ptr->d3d_texture_ptr == NULL) {
		delete hardware_texture_ptr;
		return NULL;
	}

	// Create the shader resource view object.

	if (FAILED(d3d_device_ptr->CreateShaderResourceView(hardware_texture_ptr->d3d_texture_ptr, NULL, &hardware_texture_ptr->d3d_shader_resource_view_ptr))) {
		delete hardware_texture_ptr;
		return NULL;
	}
	return hardware_texture_ptr;
}

//------------------------------------------------------------------------------
// Destroy an existing hardware texture.
//------------------------------------------------------------------------------

void
hardware_destroy_texture(void *hardware_texture_ptr)
{
	if (hardware_texture_ptr != NULL) {
		delete (hardware_texture *)hardware_texture_ptr;
	}
}

//------------------------------------------------------------------------------
// Set the image of a hardware texture.
//------------------------------------------------------------------------------

void
hardware_set_texture(cache_entry *cache_entry_ptr)
{
	hardware_texture *hardware_texture_ptr;
	ID3D11Texture2D *d3d_texture_ptr;
	D3D11_MAPPED_SUBRESOURCE d3d_mapped_subresource;
	byte *surface_ptr;
	int row_pitch, row_gap;
	int image_dimensions;
	pixmap *pixmap_ptr;
	byte *image_ptr, *end_image_ptr, *new_image_ptr;
	pixel *palette_ptr;
	int transparent_index;
	pixel transparency_mask32;
	int image_width, image_height;
	int column_index;

	// Get the image dimensions and texture object.

	hardware_texture_ptr =  (hardware_texture *)cache_entry_ptr->hardware_texture_ptr;
	image_dimensions = hardware_texture_ptr->image_dimensions;
	d3d_texture_ptr = hardware_texture_ptr->d3d_texture_ptr;

	// Lock the texture surface.

	if (FAILED(d3d_device_context_ptr->Map(d3d_texture_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3d_mapped_subresource))) {
		diagnose("Failed to lock texture");
		return;
	}
	surface_ptr = (byte *)d3d_mapped_subresource.pData;
	row_pitch = d3d_mapped_subresource.RowPitch;
	row_gap = row_pitch - image_dimensions * 4;

	// Get the unlit image pointer and it's dimensions, and set a pointer to
	// the end of the image data.

	pixmap_ptr = cache_entry_ptr->pixmap_ptr;
	image_ptr = pixmap_ptr->image_ptr;
	image_width = pixmap_ptr->width * pixmap_ptr->bytes_per_pixel;
	image_height = pixmap_ptr->height;
	end_image_ptr = image_ptr + image_width * image_height;

	// If the pixmap is an 8-bit image, put the transparent index and palette 
	// pointer in static variables so that the assembly code can get to them.
	// Also put the transparency mask into a static variable.

	if (pixmap_ptr->bytes_per_pixel == 1) {
		transparent_index = pixmap_ptr->transparent_index;
		palette_ptr = pixmap_ptr->texture_palette_list;
	}
	transparency_mask32 = texture_pixel_format.alpha_comp_mask;

	// Get the start address of the lit image.

	new_image_ptr = surface_ptr;

	// If the pixmap is a 32-bit image, simply copy it to the new image buffer.

	if (pixmap_ptr->bytes_per_pixel == 4) {
		__asm {
		
			// EBX: holds pointer to current row of old image.
			// EDX: holds pointer to current pixel of new image.
			// EDI: holds the number of rows left to copy.

			mov ebx, image_ptr
			mov edx, new_image_ptr
			mov edi, image_dimensions

		next_row:

			// ECX: holds the old image offset.
			// ESI: holds number of columns left to copy.

			mov ecx, 0
			mov esi, image_dimensions

		next_column:

			// Get the current 32-bit pixel from the old image, and store it in
			// the new image.

			mov eax, [ebx + ecx]
			mov [edx], eax

			// Increment the old image offset, wrapping back to zero if the 
			// end of the row is reached.

			add ecx, 4
			cmp ecx, image_width 
			jl next_pixel
			mov ecx, 0

		next_pixel:

			// Increment the new image pointer.

			add edx, 4

			// Decrement the column counter, and copy next pixel in row if
			// there are any left.

			dec esi
			jnz next_column

			// Increment the old image row pointer, and wrap back to the
			// first row if the end of the image has been reached.

			add ebx, image_width
			cmp ebx, end_image_ptr
			jl skip_wrap
			mov ebx, image_ptr

		skip_wrap:

			// Skip over the gap in the new image row.

			add edx, row_gap

			// Decrement the row counter, and copy next row if there are any
			// left.

			dec edi
			jnz next_row
		}
	}

	// If the pixmap is an 8-bit image, convert it to a 32-bit lit image.

	else {
		__asm {
		
			// EBX: holds pointer to current row of old image.
			// EDX: holds pointer to current pixel of new image.
			// EDI: holds the number of rows left to copy.

			mov ebx, image_ptr
			mov edx, new_image_ptr
			mov edi, image_dimensions

		next_row2:

			// ESI: holds number of columns left to copy.

			mov esi, image_dimensions

			// Clear old image offset.

			mov eax, 0
			mov column_index, eax

		next_column2:

			// Get the current 8-bit index from the old image, use it to
			// obtain the 32-bit pixel, and store it in the new image.  If 
			// the 8-bit index is not the transparent index, mark the pixel as
			// opaque by setting the transparency mask.

			mov eax, 0
			mov ecx, column_index
			mov al, [ebx + ecx]
			mov ecx, palette_ptr
			cmp eax, transparent_index
			jne opaque_pixel2
			mov eax, [ecx + eax * 4]	
			jmp store_pixel2
		opaque_pixel2:
			mov eax, [ecx + eax * 4]
			or eax, transparency_mask32
		store_pixel2:
			mov [edx], eax

			// Increment the old image offset, wrapping back to zero if the 
			// end of the row is reached.

			mov eax, column_index
			inc eax
			cmp eax, image_width 
			jl next_pixel2
			mov eax, 0
		next_pixel2:
			mov column_index, eax

			// Increment the new image pointer.

			add edx, 4

			// Decrement the column counter, and copy next pixel in row if
			// there are any left.

			dec esi
			jnz next_column2

			// Increment the old image row pointer, and wrap back to the
			// first row if the end of the image has been reached.

			add ebx, image_width
			cmp ebx, end_image_ptr
			jl skip_wrap2
			mov ebx, image_ptr

		skip_wrap2:

			// Skip over the gap in the new image row.

			add edx, row_gap

			// Decrement the row counter, and copy next row if there are any
			// left.

			dec edi
			jnz next_row2
		}
	}

	// Unlock the texture surface.

	d3d_device_context_ptr->Unmap(d3d_texture_ptr, 0);
}

//------------------------------------------------------------------------------
// Add a transformed vertex to the vertex buffer.
//------------------------------------------------------------------------------

static void
add_vertex_to_buffer(hardware_vertex *&vertex_buffer_ptr, float x, float y, float z, float u, float v, RGBcolour *colour_ptr, float alpha)
{
	*vertex_buffer_ptr++ = hardware_vertex(x, y, z, u, v, colour_ptr, alpha);
}

static void
add_tvertex_to_buffer(hardware_vertex *&vertex_buffer_ptr, tvertex *&tvertex_ptr, tpolygon *tpolygon_ptr)
{
	add_vertex_to_buffer(vertex_buffer_ptr, tvertex_ptr->x, tvertex_ptr->y, tvertex_ptr->z, tvertex_ptr->u, tvertex_ptr->v,
		&tvertex_ptr->colour, tpolygon_ptr->alpha);
}

//------------------------------------------------------------------------------
// Render a 2D polygon onto the Direct3D viewport.
//------------------------------------------------------------------------------

void
hardware_render_2D_polygon(pixmap *pixmap_ptr, RGBcolour colour, float brightness,
						   float sx, float sy, float width, float height,
						   float start_u, float start_v, float end_u, float end_v)
{
	D3D11_MAPPED_SUBRESOURCE d3d_mapped_subresource;
	hardware_vertex *vertex_buffer_ptr;

	// If the polygon has a pixmap, get the cache entry and enable the texture,
	// and use a grayscale colour for lighting.
	
	if (pixmap_ptr != NULL) {
		d3d_device_context_ptr->VSSetShader(d3d_texture_vertex_shader_ptr, NULL, 0);
		d3d_device_context_ptr->PSSetShader(d3d_texture_pixel_shader_ptr, NULL, 0);
		cache_entry *cache_entry_ptr = get_cache_entry(pixmap_ptr, 0);
		hardware_texture *hardware_texture_ptr = (hardware_texture *)cache_entry_ptr->hardware_texture_ptr;
		d3d_device_context_ptr->PSSetShaderResources(0, 1, &hardware_texture_ptr->d3d_shader_resource_view_ptr);
		colour.red = brightness;
		colour.green = colour.red;
		colour.blue = colour.red;
	} 
	
	// If the polygon has a colour, disable the texture and use the colour for lighting.
	
	else {
		d3d_device_context_ptr->VSSetShader(d3d_colour_vertex_shader_ptr, NULL, 0);
		d3d_device_context_ptr->PSSetShader(d3d_colour_pixel_shader_ptr, NULL, 0);	
	}

	// Map the vertex buffer.

	if (FAILED(d3d_device_context_ptr->Map(d3d_vertex_buffer_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3d_mapped_subresource))) {
		diagnose("Failed to map vertex buffer");
		return;
	}
	vertex_buffer_ptr = (hardware_vertex *)d3d_mapped_subresource.pData;

	// Construct the Direct3D vertex list for the polygon.  The polygon is placed at z = 1 so that the texture is not scaled.

	float x = (sx - half_window_width) / half_window_width * half_viewport_width;
	float y = (half_window_height - sy) / half_window_height * half_viewport_height;
	float w = width / half_window_width * half_viewport_width;
	float h = height / half_window_width * half_viewport_width;
	add_vertex_to_buffer(vertex_buffer_ptr, x, y, 1.0f, start_u, start_v, &colour, 1.0f);
	add_vertex_to_buffer(vertex_buffer_ptr, x + w, y, 1.0f, end_u, start_v, &colour, 1.0f);
	add_vertex_to_buffer(vertex_buffer_ptr, x, y - h, 1.0f, start_u, end_v, &colour, 1.0f);
	add_vertex_to_buffer(vertex_buffer_ptr, x + w, y - h, 1.0f, end_u, end_v, &colour, 1.0f);

	// Unmap the vertex buffer.

	d3d_device_context_ptr->Unmap(d3d_vertex_buffer_ptr, 0);

	// Set up the context for the draw, then render the polygon.  We turn off the depth buffer during the draw.

	d3d_device_context_ptr->IASetInputLayout(d3d_vertex_layout_ptr);
	d3d_device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3d_device_context_ptr->VSSetConstantBuffers(0, 2, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetConstantBuffers(0, 1, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetSamplers(0, 1, &d3d_sampler_state_ptr);
	d3d_device_context_ptr->OMSetDepthStencilState(d3d_2D_depth_stencil_state_ptr, 1);
	d3d_device_context_ptr->OMSetBlendState(d3d_blend_state_ptr, NULL, 0xFFFFFFFF);
	d3d_device_context_ptr->RSSetState(d3d_rasterizer_state_ptr);
	d3d_device_context_ptr->Draw(4, 0);
}

//------------------------------------------------------------------------------
// Render a polygon onto the Direct3D viewport.
//------------------------------------------------------------------------------

void
hardware_render_polygon(tpolygon *tpolygon_ptr)
{
	pixmap *pixmap_ptr;
	D3D11_MAPPED_SUBRESOURCE d3d_mapped_subresource;
	hardware_vertex *vertex_buffer_ptr;

	// If the polygon has a pixmap, use the texture shaders with the shader resource view for the pixmap's texture,
	// otherwise use the colour shaders.

	pixmap_ptr = tpolygon_ptr->pixmap_ptr;
	if (pixmap_ptr != NULL) {
		d3d_device_context_ptr->VSSetShader(d3d_texture_vertex_shader_ptr, NULL, 0);
		d3d_device_context_ptr->PSSetShader(d3d_texture_pixel_shader_ptr, NULL, 0);
		cache_entry *cache_entry_ptr = get_cache_entry(pixmap_ptr, 0);
		hardware_texture *hardware_texture_ptr = (hardware_texture *)cache_entry_ptr->hardware_texture_ptr;
		d3d_device_context_ptr->PSSetShaderResources(0, 1, &hardware_texture_ptr->d3d_shader_resource_view_ptr);
	} else {
		d3d_device_context_ptr->VSSetShader(d3d_colour_vertex_shader_ptr, NULL, 0);
		d3d_device_context_ptr->PSSetShader(d3d_colour_pixel_shader_ptr, NULL, 0);	
	}

	// Fill the vertex buffer with transformed vertices from the polygon, removing the vertices from the polygon in the process.

	if (FAILED(d3d_device_context_ptr->Map(d3d_vertex_buffer_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3d_mapped_subresource))) {
		diagnose("Failed to map vertex buffer");
		return;
	}
	vertex_buffer_ptr = (hardware_vertex *)d3d_mapped_subresource.pData;
	tvertex *tvertex_ptr = tpolygon_ptr->tvertex_list;
	int vertices = 0;
	while (tvertex_ptr && vertices < MAX_VERTICES) {
		add_tvertex_to_buffer(vertex_buffer_ptr, tvertex_ptr, tpolygon_ptr);
		tvertex_ptr = del_tvertex(tvertex_ptr);
		vertices++;
	}
	tpolygon_ptr->tvertex_list = NULL;
	d3d_device_context_ptr->Unmap(d3d_vertex_buffer_ptr, 0);

	// Set up the context for the draw, then render the polygon.

	d3d_device_context_ptr->IASetInputLayout(d3d_vertex_layout_ptr);
	d3d_device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	d3d_device_context_ptr->VSSetConstantBuffers(0, 2, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetConstantBuffers(0, 1, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetSamplers(0, 1, &d3d_sampler_state_ptr);
	d3d_device_context_ptr->OMSetDepthStencilState(d3d_3D_depth_stencil_state_ptr, 1);
	d3d_device_context_ptr->OMSetBlendState(d3d_blend_state_ptr, NULL, 0xFFFFFFFF);
	d3d_device_context_ptr->RSSetState(d3d_rasterizer_state_ptr);
	d3d_device_context_ptr->Draw(tpolygon_ptr->tvertices, 0);
}

//------------------------------------------------------------------------------
// Render a list of 3D lines onto the Direct3D viewport.
//------------------------------------------------------------------------------

void
hardware_render_lines(vertex *vertex_list, int vertices, RGBcolour colour)
{
	D3D11_MAPPED_SUBRESOURCE d3d_mapped_subresource;
	hardware_vertex *vertex_buffer_ptr;
	vertex *vertex_ptr;

	// Use the colour shaders.

	d3d_device_context_ptr->VSSetShader(d3d_colour_vertex_shader_ptr, NULL, 0);
	d3d_device_context_ptr->PSSetShader(d3d_colour_pixel_shader_ptr, NULL, 0);

	// Fill the vertex buffer with the transformed vertices.

	if (FAILED(d3d_device_context_ptr->Map(d3d_vertex_buffer_ptr, 0, D3D11_MAP_WRITE_DISCARD, 0, &d3d_mapped_subresource))) {
		diagnose("Failed to map vertex buffer");
		return;
	}
	vertex_buffer_ptr = (hardware_vertex *)d3d_mapped_subresource.pData;
	vertex_ptr = vertex_list;
	for (int vertex_index = 0; vertex_index < vertices; vertex_index++) {
		add_vertex_to_buffer(vertex_buffer_ptr, vertex_ptr->x, vertex_ptr->y, vertex_ptr->z, 0.0f, 0.0f, &colour, 1.0f);
		vertex_ptr++;
	}
	d3d_device_context_ptr->Unmap(d3d_vertex_buffer_ptr, 0);

	// Set up the context for the draw, then render the lines.  We turn off the depth buffer during the draw.

	d3d_device_context_ptr->IASetInputLayout(d3d_vertex_layout_ptr);
	d3d_device_context_ptr->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	d3d_device_context_ptr->VSSetConstantBuffers(0, 2, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetConstantBuffers(0, 1, d3d_constant_buffer_list);
	d3d_device_context_ptr->PSSetSamplers(0, 1, &d3d_sampler_state_ptr);
	d3d_device_context_ptr->OMSetDepthStencilState(d3d_2D_depth_stencil_state_ptr, 1);
	d3d_device_context_ptr->OMSetBlendState(d3d_blend_state_ptr, NULL, 0xFFFFFFFF);
	d3d_device_context_ptr->RSSetState(d3d_rasterizer_state_ptr);
	d3d_device_context_ptr->Draw(vertices, 0);
}

//==============================================================================
// 2D drawing functions.
//==============================================================================

//------------------------------------------------------------------------------
// Function to draw a pixmap at the given brightness index onto the frame buffer
// surface at the given (x,y) coordinates.
//------------------------------------------------------------------------------

void
draw_pixmap(pixmap *pixmap_ptr, int brightness_index, int x, int y, int width, int height)
{
	int clipped_x, clipped_y;
	int clipped_width, clipped_height;
	byte *surface_ptr;
	byte *image_ptr, *fb_ptr;
	int fb_bytes_per_row, bytes_per_pixel;
	int fb_row_gap, image_row_gap;
	int row, col;
	pixel *palette_ptr;
	byte *palette_index_table;
	int transparent_index = 0;
	pixel transparency_mask32 = 0;
	int image_width, span_width;
	fixed u, v;

	// If the pixmap is completely off screen then return without having drawn
	// anything.

	if (x >= window_width || y >= window_height ||
		x + width <= 0 || y + height <= 0) 
		return;

	// If the frame buffer x or y coordinates are negative, then we clamp them
	// at zero and adjust the image coordinates and size to match.

	if (x < 0) {
		clipped_x = -x;
		clipped_width = width - clipped_x;
		x = 0;
	} else {
		clipped_x = 0;
		clipped_width = width;
	}	
	if (y < 0) {
		clipped_y = -y;
		clipped_height = height - clipped_y;
		y = 0;
	} else {
		clipped_y = 0;
		clipped_height = height;
	}

	// If the pixmap crosses the right or bottom edge of the display, we must
	// adjust the size of the area we are going to draw even further.

	if (x + clipped_width > window_width)
		clipped_width = window_width - x;
	if (y + clipped_height > window_height)
		clipped_height = window_height - y;

	// Lock the frame buffer surface.

	DDSURFACEDESC ddraw_surface_desc;

	ddraw_surface_desc.dwSize = sizeof(ddraw_surface_desc);
	if (ddraw_frame_buffer_surface_ptr->Lock(NULL, &ddraw_surface_desc,
		DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL) != DD_OK)
		return;
	surface_ptr = (byte *)ddraw_surface_desc.lpSurface;
	fb_bytes_per_row = ddraw_surface_desc.lPitch;

	// Calculate the bytes per pixel.

	bytes_per_pixel = display_depth / 8;

	// Determine the transprency mask or index, and the palette pointer.  For
	// 16-bit pixmaps, the image width must be doubled to give the width in
	// bytes.

	if (pixmap_ptr->bytes_per_pixel == 2) {
		transparency_mask32 = texture_pixel_format.alpha_comp_mask;
		palette_ptr = light_table[brightness_index];
		image_width = pixmap_ptr->width * 2;
		u = (clipped_x % pixmap_ptr->width) * 2;
	} else {
		palette_ptr = pixmap_ptr->display_palette_list + 
			brightness_index * pixmap_ptr->colours;
		transparent_index = pixmap_ptr->transparent_index;
		image_width = pixmap_ptr->width;
		u = clipped_x % pixmap_ptr->width;
	}
	v = clipped_y % pixmap_ptr->height;

	// Compute the starting frame buffer and image pointers.

	fb_ptr = surface_ptr + (y * fb_bytes_per_row) + (x * bytes_per_pixel);
	image_ptr = pixmap_ptr->image_ptr + v * image_width;
	
	// The span width is the same as the clipped width.

	span_width = clipped_width;

	// Now render the pixmap.  Note that rendering to an 8-bit frame buffer is
	// a special case that is only defined for 8-bit pixmaps.

	switch (bytes_per_pixel) {
	case 1:
		if (pixmap_ptr->bytes_per_pixel == 1) {
			fb_row_gap = fb_bytes_per_row - clipped_width * bytes_per_pixel;	
			image_row_gap = image_width - clipped_width;
			palette_index_table = pixmap_ptr->palette_index_table;
			image_ptr += u;
			for (row = 0; row < clipped_height; row++) {
				for (col = 0; col < clipped_width; col++) {
					if (*image_ptr != transparent_index)
						*fb_ptr = palette_index_table[*image_ptr];
					fb_ptr++;
					image_ptr++;
				}
				fb_ptr += fb_row_gap;
				image_ptr += image_row_gap;
			}
		}
		break;
	case 2:
		for (row = 0; row < clipped_height; row++) {
			render_linear_span16(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
				palette_ptr, transparent_index, transparency_mask32, image_width,
				span_width, u);
			fb_ptr += fb_bytes_per_row;
			image_ptr += image_width;
		}
		break;
	case 3:
		for (row = 0; row < clipped_height; row++) {
			render_linear_span24(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
				palette_ptr, transparent_index, transparency_mask32, image_width,
				span_width, u);
			fb_ptr += fb_bytes_per_row;
			image_ptr += image_width;
		}
		break;
	case 4:
		for (row = 0; row < clipped_height; row++) {
			render_linear_span32(pixmap_ptr->bytes_per_pixel == 2, image_ptr, fb_ptr,
				palette_ptr, transparent_index, transparency_mask32, image_width,
				span_width, u);
			fb_ptr += fb_bytes_per_row;
			image_ptr += image_width;
		}
		break;
	}

	// Unlock the frame buffer surface.

	ddraw_frame_buffer_surface_ptr->Unlock(surface_ptr);
}

//------------------------------------------------------------------------------
// Convert an RGB colour to a display pixel.
//------------------------------------------------------------------------------

pixel
RGB_to_display_pixel(RGBcolour colour)
{
	pixel red, green, blue;

	// Compute the pixel for this RGB colour.

	red = (pixel)colour.red & display_pixel_format.red_mask;
	red >>= display_pixel_format.red_right_shift;
	red <<= display_pixel_format.red_left_shift;
	green = (pixel)colour.green & display_pixel_format.green_mask;
	green >>= display_pixel_format.green_right_shift;
	green <<= display_pixel_format.green_left_shift;
	blue = (pixel)colour.blue & display_pixel_format.blue_mask;
	blue >>= display_pixel_format.blue_right_shift;
	blue <<= display_pixel_format.blue_left_shift;
	return(red | green | blue);
}

//------------------------------------------------------------------------------
// Convert an RGB colour to a texture pixel.
//------------------------------------------------------------------------------

pixel
RGB_to_texture_pixel(RGBcolour colour)
{
	pixel red, green, blue, alpha;

	// Compute the pixel for this RGB colour.

	red = (pixel)colour.red & texture_pixel_format.red_mask;
	red >>= texture_pixel_format.red_right_shift;
	red <<= texture_pixel_format.red_left_shift;
	green = (pixel)colour.green & texture_pixel_format.green_mask;
	green >>= texture_pixel_format.green_right_shift;
	green <<= texture_pixel_format.green_left_shift;
	blue = (pixel)colour.blue & texture_pixel_format.blue_mask;
	blue >>= texture_pixel_format.blue_right_shift;
	blue <<= texture_pixel_format.blue_left_shift;
	alpha = colour.alpha ? texture_pixel_format.alpha_comp_mask : 0;
	return(red | green | blue | alpha);
}

//------------------------------------------------------------------------------
// Return an index to the nearest colour in the standard palette.
//------------------------------------------------------------------------------

byte
get_standard_palette_index(RGBcolour colour)
{
	return(GetNearestPaletteIndex(standard_palette_handle,
		RGB((byte)colour.red, (byte)colour.green, (byte)colour.blue)));
}

//------------------------------------------------------------------------------
// Get the title.
//------------------------------------------------------------------------------

const char *
get_title(void)
{
	return(title_text);
}

//------------------------------------------------------------------------------
// Set the title.  If the format is NULL, reset the existing title.
//------------------------------------------------------------------------------

void
set_title(char *format, ...)
{
	va_list arg_ptr;
	char title[BUFSIZ];

	// Construct the title text if a format is given.

	if (format != NULL) {
		va_start(arg_ptr, format);
		vbprintf(title, BUFSIZ, format, arg_ptr);
		va_end(arg_ptr);
		title_text = title;
	}

	// Draw the title on the status bar.

	SendMessage(status_bar_handle, SB_SETTEXT, 0, (LPARAM)(char *)title_text);
}

//------------------------------------------------------------------------------
// Set the status text.
//------------------------------------------------------------------------------

void
set_status_text(int status_box_index, char *format, ...)
{
	va_list arg_ptr;
	char status_text[BUFSIZ];

	// Construct the status text.

	va_start(arg_ptr, format);
	vbprintf(status_text, BUFSIZ, format, arg_ptr);
	va_end(arg_ptr);

	// Draw the title on the status bar.

	SendMessage(status_bar_handle, SB_SETTEXT, status_box_index, (LPARAM)(char *)status_text);
}

//------------------------------------------------------------------------------
// Set the spot URL in the edit box.
//------------------------------------------------------------------------------

void
set_spot_URL(string spot_URL)
{
	SendMessage(spot_URL_edit_box_handle, WM_SETTEXT, 0, (LPARAM)(char *)spot_URL);
}

//------------------------------------------------------------------------------
// Display a label near the current cursor position.  If the label is NULL,
// reset the existing label, if there is one.
//------------------------------------------------------------------------------

void
show_label(const char *label)
{
	// Set the label visible flag and text, if a label is given.

	if (label != NULL) {
		label_visible = true;
		label_text = label;
	}

	// If the label is visible, draw the label text into the label texture,
	// and remember the width of the text.

	if (label_visible)
		label_width = draw_text_on_pixmap(label_texture_ptr, label_text, 
			LEFT, false);
}

//------------------------------------------------------------------------------
// Hide the label.
//------------------------------------------------------------------------------

void
hide_label(void)
{
	label_visible = false;
}

//------------------------------------------------------------------------------
// Initialise a popup.
//------------------------------------------------------------------------------

void
init_popup(popup *popup_ptr)
{
	texture *bg_texture_ptr;
	int popup_width, popup_height;

	// If this popup has a background texture, create it's 16-bit display
	// palette list, and set the size of the popup to be the size of the
	// background texture.  Otherwise use the popup's default size.

	if ((bg_texture_ptr = popup_ptr->bg_texture_ptr) != NULL) {
		if (bg_texture_ptr->bytes_per_pixel == 1)
			bg_texture_ptr->create_display_palette_list();
		popup_width = bg_texture_ptr->width;
		popup_height = bg_texture_ptr->height;
	} else {
		popup_width = popup_ptr->width;
		popup_height = popup_ptr->height;
	}

	// If this popup does not require a foreground texture, then we are done.

	if (!popup_ptr->create_foreground)
		return;
		
	// Create the pixmap for the foreground texture.

	create_pixmap_for_text(popup_ptr->fg_texture_ptr, popup_width, 
		popup_height, popup_ptr->text_colour,
		popup_ptr->transparent_background ? NULL : &popup_ptr->colour);
	draw_text_on_pixmap(popup_ptr->fg_texture_ptr, popup_ptr->text, 
		popup_ptr->text_alignment, true);
}

//------------------------------------------------------------------------------
// Get the relative or absolute position of the mouse.
//------------------------------------------------------------------------------

void
get_mouse_position(int *x, int *y, bool relative)
{
	POINT cursor_pos;

	// Get the absolute cursor position.

	GetCursorPos(&cursor_pos);

	// If a relative position is requested, adjust the cursor position so that
	// it's relative to the main window position.

	if (relative) {
		POINT main_window_pos;

		main_window_pos.x = 0;
		main_window_pos.y = 0;
		ClientToScreen(main_window_handle, &main_window_pos);
		cursor_pos.x -= main_window_pos.x;
		cursor_pos.y -= main_window_pos.y;
	}

	// Pass the cursor position back via the parameters.

	*x = cursor_pos.x;
	*y = cursor_pos.y;
}

//------------------------------------------------------------------------------
// Set the arrow cursor.
//------------------------------------------------------------------------------

void
set_arrow_cursor(void)
{	
	set_active_cursor(arrow_cursor_ptr);
}

//------------------------------------------------------------------------------
// Set a movement cursor.
//------------------------------------------------------------------------------

void
set_movement_cursor(arrow movement_arrow)
{	
	set_active_cursor(movement_cursor_ptr_list[movement_arrow]);
}

//------------------------------------------------------------------------------
// Set the hand cursor.
//------------------------------------------------------------------------------

void
set_hand_cursor(void)
{	
	set_active_cursor(hand_cursor_ptr);
}

//------------------------------------------------------------------------------
// Set the crosshair cursor.
//------------------------------------------------------------------------------

void
set_crosshair_cursor(void)
{	
	set_active_cursor(crosshair_cursor_ptr);
}

//------------------------------------------------------------------------------
// Enable mouse look mode.
//------------------------------------------------------------------------------

void
enable_mouse_look_mode(void)
{
	if (!mouse_look_mode.get()) {
		SetCapture(app_window_handle);
		mouse_look_mode.set(true);
	}
}

//------------------------------------------------------------------------------
// Disable mouse look mode.
//------------------------------------------------------------------------------

void
disable_mouse_look_mode(void)
{
	if (mouse_look_mode.get()) {
		ReleaseCapture();
		mouse_look_mode.set(false);
	}
}

//------------------------------------------------------------------------------
// Get the time since Windows last started, in milliseconds.
//------------------------------------------------------------------------------

int
get_time_ms(void)
{
	return(GetTickCount());
}

//------------------------------------------------------------------------------
// Load wave data into a wave object.
//------------------------------------------------------------------------------

bool
load_wave_data(wave *wave_ptr, char *wave_file_buffer, int wave_file_size)
{
	MMIOINFO info;
	MMCKINFO parent, child;
	HMMIO handle;
	WAVEFORMATEX *wave_format_ptr;
	char *wave_data_ptr;
	int wave_data_size;

	// Initialise the parent and child MMCKINFO structures.

	memset(&parent, 0, sizeof(MMCKINFO));
	memset(&child, 0, sizeof(MMCKINFO));

	// Open the specified wave file; the file has already been loaded into a
	// memory buffer.

	memset(&info, 0, sizeof(MMIOINFO));
	info.fccIOProc = FOURCC_MEM;
	info.pchBuffer = wave_file_buffer;
	info.cchBuffer = wave_file_size;
	if ((handle = mmioOpen(NULL, &info, MMIO_READ | MMIO_ALLOCBUF)) == NULL)
		return(false);

	// Verify we've open a wave file by descending into the WAVE chunk.

	parent.fccType = mmioFOURCC('W', 'A', 'V', 'E');
	if (mmioDescend(handle, &parent, NULL, MMIO_FINDRIFF)) {
		mmioClose(handle, 0);
		return(false);
	}

	// Descend into the fmt chunk.

	child.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(handle, &child, &parent, 0)) {
		mmioClose(handle, 0);
		return(false);
	}

	// Allocate the wave format structure.

	NEW(wave_format_ptr, WAVEFORMATEX);
	if (wave_format_ptr == NULL) {
		mmioClose(handle, 0);
		return(false);
	}

	// Read the wave format.

	if (mmioRead(handle, (char *)wave_format_ptr, sizeof(WAVEFORMATEX)) !=
		sizeof(WAVEFORMATEX)) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		mmioClose(handle, 0);
		return(false);
	}

	// Verify that the wave is in PCM format.

	if (wave_format_ptr->wFormatTag != WAVE_FORMAT_PCM) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		mmioClose(handle, 0);
		return(false);
	}

	// Ascend out of the fmt chunk.

	if (mmioAscend(handle, &child, 0)) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		mmioClose(handle, 0);
		return(false);
	}

	// Descend into the data chunk.

	child.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(handle, &child, &parent, MMIO_FINDCHUNK)) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		mmioClose(handle, 0);
		return(false);
	}

	// Allocate the wave data buffer.

	wave_data_size = child.cksize;
	if ((wave_data_ptr = new char[wave_data_size]) == NULL) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		mmioClose(handle, 0);
		return(false);
	}

	// Read the wave data into the buffer.

	if (mmioRead(handle, wave_data_ptr, wave_data_size) != wave_data_size) {
		DEL(wave_format_ptr, WAVEFORMATEX);
		delete []wave_data_ptr;
		mmioClose(handle, 0);
		return(false);
	}

	// Set up the wave structure, close the file and return with success.

	wave_ptr->format_ptr = wave_format_ptr;
	wave_ptr->data_ptr = wave_data_ptr;
	wave_ptr->data_size = wave_data_size;
	mmioClose(handle, 0);
	return(true);
}

//------------------------------------------------------------------------------
// Destroy the wave data in the given wave object.
//------------------------------------------------------------------------------

void
destroy_wave_data(wave *wave_ptr)
{
	if (wave_ptr->format_ptr != NULL)
		DEL(wave_ptr->format_ptr, WAVEFORMATEX);
	if (wave_ptr->data_ptr != NULL)
		delete []wave_ptr->data_ptr;
}

//------------------------------------------------------------------------------
// Write the specified wave data to the given sound buffer, starting from the
// given write position.
//------------------------------------------------------------------------------

void
update_sound_buffer(void *sound_buffer_ptr, char *data_ptr, int data_size,
				   int data_start)
{
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr;
	HRESULT result;
	LPVOID buffer1_ptr, buffer2_ptr;
	DWORD buflen1, buflen2;

	// Lock the buffer.

	dsound_buffer_ptr = (LPDIRECTSOUNDBUFFER)sound_buffer_ptr;
	result = dsound_buffer_ptr->Lock(data_start, data_size, 
		&buffer1_ptr, &buflen1, &buffer2_ptr, &buflen2, 0);
	if (result == DSERR_BUFFERLOST) {
		dsound_buffer_ptr->Restore();
		result = dsound_buffer_ptr->Lock(data_start, data_size, 
			&buffer1_ptr, &buflen1, &buffer2_ptr, &buflen2, 0);
	}
	if (result != DS_OK) {
		failed_to("lock DirectSound buffer");
		return;
	}

	// Copy the wave data into the DirectSoundBuffer object.

	CopyMemory(buffer1_ptr, data_ptr, buflen1);
	if (buffer2_ptr != NULL)
		CopyMemory(buffer2_ptr, data_ptr + buflen1, buflen2);

	// Unlock the buffer.

	if (dsound_buffer_ptr->Unlock(buffer1_ptr, buflen1, buffer2_ptr,
		buflen2) != DS_OK)
		failed_to("unlock DirectSound buffer");
}

//------------------------------------------------------------------------------
// Create a sound buffer for the given sound.
//------------------------------------------------------------------------------

bool
create_sound_buffer(sound *sound_ptr)
{
	DSBUFFERDESC dsbdesc;
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr;
	wave *wave_ptr;
	WAVEFORMATEX *wave_format_ptr;

	// If there is no wave defined, the sound buffer cannot be created.

	wave_ptr = sound_ptr->wave_ptr;
	if (wave_ptr == NULL || wave_ptr->data_size == 0)
		return(false);

	// Get a pointer to the wave format.

	wave_format_ptr = (WAVEFORMATEX *)wave_ptr->format_ptr;

	// Initialise the DirectSound buffer description structure.

	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN | DSBCAPS_STATIC;
	dsbdesc.dwBufferBytes = wave_ptr->data_size;
	dsbdesc.lpwfxFormat = wave_format_ptr;

	// Create the DirectSoundBuffer object.

	if (dsound_object_ptr->CreateSoundBuffer(&dsbdesc, &dsound_buffer_ptr,
		NULL) != DS_OK)
		return(false);

	// If there is wave data present, initialise the wave data buffer with
	// it.

	if (wave_ptr->data_ptr != NULL)
		update_sound_buffer(dsound_buffer_ptr, wave_ptr->data_ptr, 
			wave_ptr->data_size, 0);

	// Store a void pointer to the DirectSoundBuffer object in the sound
	// object.

	sound_ptr->sound_buffer_ptr = (void *)dsound_buffer_ptr;
	return(true);
}

//------------------------------------------------------------------------------
// Destroy a sound buffer.
//------------------------------------------------------------------------------

void
destroy_sound_buffer(sound *sound_ptr)
{
	void *sound_buffer_ptr = sound_ptr->sound_buffer_ptr;
	if (sound_buffer_ptr != NULL) {
		((LPDIRECTSOUNDBUFFER)sound_buffer_ptr)->Release();
		sound_ptr->sound_buffer_ptr = NULL;
	}
}

//------------------------------------------------------------------------------
// Set the delay for a sound buffer.
//------------------------------------------------------------------------------

static void
set_sound_delay(sound *sound_ptr) 
{
	sound_ptr->start_time_ms = curr_time_ms;
	sound_ptr->delay_ms = sound_ptr->delay_range.min_delay_ms;
	if (sound_ptr->delay_range.delay_range_ms > 0)
		sound_ptr->delay_ms += (int)((float)rand() / RAND_MAX * 
			sound_ptr->delay_range.delay_range_ms);
}

//------------------------------------------------------------------------------
// Set the volume of a sound buffer.
//------------------------------------------------------------------------------

void
set_sound_volume(sound *sound_ptr, float volume)
{
	int volume_level;
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr = 
		(LPDIRECTSOUNDBUFFER)sound_ptr->sound_buffer_ptr;
	if (dsound_buffer_ptr != NULL) {

		// Convert the volume level from a fractional value between 0.0 and
		// 1.0, to an integer value between -10,000 (-100 dB) and 0 (0 dB).

		if (FLT(volume, 0.005f))
			volume_level = -10000;
		else if (FLT(volume, 0.05f))
			volume_level = (int)(47835.76f * sqrt(0.4f * volume) - 
				12756.19f);
		else if (FLT(volume, 0.5f))
			volume_level = (int)(-500.0f / volume);
		else
			volume_level = (int)(2000.0f * (volume - 1.0f));	
		dsound_buffer_ptr->SetVolume(volume_level);
	}
}

//------------------------------------------------------------------------------
// Set the playback position for a sound.
//------------------------------------------------------------------------------

static void
set_sound_position(sound *sound_ptr, int position)
{
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr = 
		(LPDIRECTSOUNDBUFFER)sound_ptr->sound_buffer_ptr;
	if (dsound_buffer_ptr != NULL)
		dsound_buffer_ptr->SetCurrentPosition(position);
}

//------------------------------------------------------------------------------
// Play a sound.
//------------------------------------------------------------------------------

void
play_sound(sound *sound_ptr, bool looped)
{
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr = 
		(LPDIRECTSOUNDBUFFER)sound_ptr->sound_buffer_ptr;
	if (dsound_buffer_ptr != NULL)
		dsound_buffer_ptr->Play(0, 0, looped ? DSBPLAY_LOOPING : 0);
}

//------------------------------------------------------------------------------
// Stop a sound from playing.
//------------------------------------------------------------------------------

void
stop_sound(sound *sound_ptr)
{
	LPDIRECTSOUNDBUFFER dsound_buffer_ptr = 
		(LPDIRECTSOUNDBUFFER)sound_ptr->sound_buffer_ptr;
	if (dsound_buffer_ptr != NULL)
		dsound_buffer_ptr->Stop();
}

//------------------------------------------------------------------------------
// Update a sound.
//------------------------------------------------------------------------------

void
update_sound(sound *sound_ptr, vertex *translation_ptr)
{
	if (sound_ptr->sound_buffer_ptr != NULL) {
		LPDIRECTSOUNDBUFFER dsound_buffer_ptr;
		float volume;
		int pan_position;
		vertex sound_position;
		vertex relative_sound_position;
		vertex transformed_sound_position;
		vector transformed_sound_vector;
		float distance;
		bool prev_in_range, in_range;

		// Get the position of the sound source, adding the translation vertex
		// if present; ambient sound sources are located one unit above the 
		// player's position.

		if (sound_ptr->ambient) {
			sound_position = player_viewpoint.position;
			sound_position.y += 1.0f;
		} else if (translation_ptr != NULL)
			sound_position = sound_ptr->position + *translation_ptr;
		else {
			vertex translation;
			translation.set_map_translation(sound_ptr->map_coords.column, sound_ptr->map_coords.row, sound_ptr->map_coords.level);
			sound_position = sound_ptr->position + translation;
		}

		// Determine the position of the sound source relative to the player 
		// position, then compute the distance between the player position and
		// the sound source.

		translate_vertex(&sound_position, &relative_sound_position);
		distance = (float)sqrt(relative_sound_position.x * 
			relative_sound_position.x + relative_sound_position.y * 
			relative_sound_position.y + relative_sound_position.z * 
			relative_sound_position.z);

		// Determine if the player is in or out of range of the sound,
		// remembering the previous state.  The sound radius cannot exceed the
		// audio radius; sounds that don't specify a radius (which includes
		// non-flood sounds using "looped" or "random" playback mode) are always
		// in range.

		prev_in_range = sound_ptr->in_range;
		if (sound_ptr->radius == 0.0f)
			in_range = true;
		else
			in_range = FLE(distance, sound_ptr->radius);
		sound_ptr->in_range = in_range;

		// If this sound is using "looped" playback mode...

		switch (sound_ptr->playback_mode) {
		case LOOPED_PLAY:

			// Play sound looped if player has come into range.

			if (!prev_in_range && in_range)
				play_sound(sound_ptr, true);

			// Stop sound if player has gone out of range and this is a
			// flood sound.

			if (sound_ptr->flood && prev_in_range && !in_range)
				stop_sound(sound_ptr);
			break;

		// If this sound is using "random" playback mode...

		case RANDOM_PLAY:

			// Reset the delay time if the player has come into range.

			if (!prev_in_range && in_range)
				set_sound_delay(sound_ptr);

			// If the delay time has elapsed while the player is in range,
			// play the sound once through and calculate the next delay time.

			if (in_range && (curr_time_ms - sound_ptr->start_time_ms >= 
				sound_ptr->delay_ms)) {
				play_sound(sound_ptr, false);
				set_sound_delay(sound_ptr);
			}
			break;

		// If this sound is using "single" playback mode, play it once through
		// if the player has come into range.

		case SINGLE_PLAY:
			if (!prev_in_range && in_range)
				play_sound(sound_ptr, false);
			break;

		// If this sound is using "once" playback mode, play it once through if
		// the player has come into range and the sound has not being played
		// before.

		case ONE_PLAY:
			if (!prev_in_range && in_range && !sound_ptr->played_once) {
				play_sound(sound_ptr, false);
				sound_ptr->played_once = true;
			}
		}

		// If the sound is currently in range, update it's presence in the
		// sound field...

		if (in_range) {

			// Get a pointer to the DirectSound buffer.

			dsound_buffer_ptr = 
				(LPDIRECTSOUNDBUFFER)sound_ptr->sound_buffer_ptr;

			// If the flood flag is set for this sound, play it at full
			// volume regardless of the distance from the source.

			if (sound_ptr->flood)
				set_sound_volume(sound_ptr, sound_ptr->volume);

			// Otherwise compute the volume of the sound as a function of
			// the scaled distance from the source.

			else {
				volume = sound_ptr->volume * 
					(1.0f / (distance * sound_ptr->rolloff * 
					world_ptr->audio_scale + 1.0f));
				set_sound_volume(sound_ptr, volume);
			}

			// Compute the fully transformed position of the sound, and
			// store it as a vector.

			rotate_vertex(&relative_sound_position,
				&transformed_sound_position);

			// Treat the transformed position as a vector, and normalise
			// it.

			transformed_sound_vector.dx = transformed_sound_position.x;
			transformed_sound_vector.dy = transformed_sound_position.y;
			transformed_sound_vector.dz = transformed_sound_position.z;
			transformed_sound_vector.normalise();
			
			// Compute the pan position of the sound based upon the x
			// component of the normalised sound vector.

			pan_position = (int)(transformed_sound_vector.dx * 10000.0f);
			dsound_buffer_ptr->SetPan(pan_position);
		}
	}
}

#ifdef STREAMING_MEDIA

//------------------------------------------------------------------------------
// Initialise the unscaled video texture. 
//------------------------------------------------------------------------------

static void
init_unscaled_video_texture(void)
{
	pixmap *pixmap_ptr;

	// If the unscaled video texture already has a pixmap list, assume it has
	// been initialised already.

	if (unscaled_video_texture_ptr->pixmap_list != NULL)
		return;

	// Create the video pixmap object and initialise it.

	NEWARRAY(pixmap_ptr, pixmap, 1);
	if (pixmap_ptr == NULL) {
		diagnose("Unable to create video pixmap");
		return;
	}
	pixmap_ptr->image_is_16_bit = true;
	pixmap_ptr->width = unscaled_video_width;
	pixmap_ptr->height = unscaled_video_height;
	pixmap_ptr->image_size = unscaled_video_width * unscaled_video_height * 2;
	NEWARRAY(pixmap_ptr->image_ptr, imagebyte, pixmap_ptr->image_size);
	if (pixmap_ptr->image_ptr == NULL) {
		diagnose("Unable to create video pixmap image");
		DELARRAY(pixmap_ptr, pixmap, 1);
		return;
	}
	memset(pixmap_ptr->image_ptr, 0, pixmap_ptr->image_size);
	pixmap_ptr->colours = 0;
	pixmap_ptr->transparent_index = -1;
	pixmap_ptr->delay_ms = 0;

	// Initialise the texture object.

	unscaled_video_texture_ptr->transparent = false;
	unscaled_video_texture_ptr->loops = false;
	unscaled_video_texture_ptr->is_16_bit = true;
	unscaled_video_texture_ptr->width = unscaled_video_width;
	unscaled_video_texture_ptr->height = unscaled_video_height;
	unscaled_video_texture_ptr->pixmaps = 1;
	unscaled_video_texture_ptr->pixmap_list = pixmap_ptr;
	set_size_indices(unscaled_video_texture_ptr);
}

//------------------------------------------------------------------------------
// Initialise a scaled video texture. 
//------------------------------------------------------------------------------

static void
init_scaled_video_texture(video_texture *scaled_video_texture_ptr)
{
	video_rect *rect_ptr;
	float unscaled_width, unscaled_height;
	float scaled_width, scaled_height;
	float aspect_ratio;
	pixmap *pixmap_ptr;
	texture *texture_ptr;

	// If this scaled video texture has a pixmap list, assume it has already
	// been initialised.

	texture_ptr = scaled_video_texture_ptr->texture_ptr;
	if (texture_ptr->pixmap_list != NULL)
		return;

	// Get a pointer to the source rectangle.  If ratios are being used for
	// any of the coordinates, convert them to pixel units.

	rect_ptr = &scaled_video_texture_ptr->source_rect;
	if (rect_ptr->x1_is_ratio)
		rect_ptr->x1 *= unscaled_video_width;
	if (rect_ptr->y1_is_ratio)
		rect_ptr->y1 *= unscaled_video_height;
	if (rect_ptr->x2_is_ratio)
		rect_ptr->x2 *= unscaled_video_width;
	if (rect_ptr->y2_is_ratio)
		rect_ptr->y2 *= unscaled_video_height;

	// Swap x1 and x2 if x2 < x1.

	if (rect_ptr->x2 < rect_ptr->x1) {
		float temp_x = rect_ptr->x1;
		rect_ptr->x1 = rect_ptr->x2;
		rect_ptr->x2 = temp_x;
	}

	// Swap y1 and y2 if y2 < y1.

	if (rect_ptr->y2 < rect_ptr->y1) {
		float temp_y = rect_ptr->y1;
		rect_ptr->y1 = rect_ptr->y2;
		rect_ptr->y2 = temp_y;
	}

	// Clamp the source rectangle to the boundaries of the unscaled video
	// frame.

	rect_ptr->x1 = MAX(rect_ptr->x1, 0);
	rect_ptr->y1 = MAX(rect_ptr->y1, 0);
	rect_ptr->x2 = MIN(rect_ptr->x2, unscaled_video_width);
	rect_ptr->y2 = MIN(rect_ptr->y2, unscaled_video_height);

	// If the clamped rectangle doesn't intersect the unscaled video frame
	// or has zero width or height, then set the source rectangle to be the
	// whole frame.

	if (rect_ptr->x1 >= unscaled_video_width || rect_ptr->x2 <= 0 || 
		rect_ptr->y1 >= unscaled_video_height || rect_ptr->y2 <= 0 ||
		rect_ptr->x1 == rect_ptr->x2 || rect_ptr->y1 == rect_ptr->y2) {
		rect_ptr->x1 = 0.0f;
		rect_ptr->y1 = 0.0f;
		rect_ptr->x2 = (float)unscaled_video_width;
		rect_ptr->y2 = (float)unscaled_video_height;
	}

	// Get the unscaled width and height of the texture from the source
	// rectangle, then calculate the scaled width and height such that the
	// dimensions are no greater than 256 pixels.

	unscaled_width = rect_ptr->x2 - rect_ptr->x1;
	unscaled_height = rect_ptr->y2 - rect_ptr->y1;
	if (unscaled_width <= 256.0f && unscaled_height <= 256.0f) {
		scaled_width = unscaled_width;
		scaled_height = unscaled_height;
	} else {
		aspect_ratio = unscaled_width / unscaled_height;
		if (aspect_ratio > 1.0f) {
			scaled_width = 256.0f;
			scaled_height = unscaled_height / aspect_ratio; 
		} else {
			scaled_width = unscaled_width * aspect_ratio;
			scaled_height = 256.0f;
		}
	}

	// Calculate delta (u,v) for the video texture.

	scaled_video_texture_ptr->delta_u = unscaled_width / scaled_width;
	scaled_video_texture_ptr->delta_v = unscaled_height / scaled_height;

	// Create the video pixmap object and initialise it.

	NEWARRAY(pixmap_ptr, pixmap, 1);
	if (pixmap_ptr == NULL) {
		diagnose("Unable to create video pixmap");
		return;
	}
	pixmap_ptr->image_is_16_bit = true;
	pixmap_ptr->width = (int)scaled_width;
	pixmap_ptr->height = (int)scaled_height;
	pixmap_ptr->image_size = pixmap_ptr->width * pixmap_ptr->height * 2;
	NEWARRAY(pixmap_ptr->image_ptr, imagebyte, pixmap_ptr->image_size);
	if (pixmap_ptr->image_ptr == NULL) {
		diagnose("Unable to create video pixmap image");
		DELARRAY(pixmap_ptr, pixmap, 1);
		return;
	}
	memset(pixmap_ptr->image_ptr, 0, pixmap_ptr->image_size);
	pixmap_ptr->colours = 0;
	pixmap_ptr->transparent_index = -1;
	pixmap_ptr->delay_ms = 0;

	// Initialise the texture object.

	texture_ptr->transparent = false;
	texture_ptr->loops = false;
	texture_ptr->is_16_bit = true;
	texture_ptr->width = pixmap_ptr->width;
	texture_ptr->height = pixmap_ptr->height;
	texture_ptr->pixmaps = 1;
	texture_ptr->pixmap_list = pixmap_ptr;
	set_size_indices(texture_ptr);
}

//------------------------------------------------------------------------------
// Initialise all video textures for the given unscaled video dimensions.
//------------------------------------------------------------------------------

void
init_video_textures(int video_width, int video_height, int pixel_format)
{
	// Set the unscaled video width and height, and the pixel format.

	unscaled_video_width = video_width;
	unscaled_video_height = video_height;
	video_pixel_format = pixel_format;

	// Initialise the unscaled video texture, if it exists.

	if (unscaled_video_texture_ptr != NULL)
		init_unscaled_video_texture();

	// Step through the list of scaled video textures, and initialise each one.

	video_texture *video_texture_ptr = scaled_video_texture_list;
	while (video_texture_ptr != NULL) {
		init_scaled_video_texture(video_texture_ptr);
		video_texture_ptr = video_texture_ptr->next_video_texture_ptr;
	}

	// Send an event to the player thread indicating that the stream has
	// started.

	stream_opened.send_event(true);
}

//------------------------------------------------------------------------------
// Initialise the video stream (Windows Media Player only).
//------------------------------------------------------------------------------

static bool
init_video_stream(void)
{
	DDSURFACEDESC ddraw_surface_desc;
	RECT rect;
	int video_width, video_height;

	// Get the primary video stream, if there is one.

	if (global_stream_ptr->GetMediaStream(MSPID_PrimaryVideo,
		&primary_video_stream_ptr) != S_OK)
		return(false);

	// Obtain the DirectDraw stream object from the primary video stream.

	if (primary_video_stream_ptr->QueryInterface(IID_IDirectDrawMediaStream,
		(void **)&ddraw_stream_ptr) != S_OK)
		return(false);

	// Determine the unscaled size of the video frame.

	ddraw_surface_desc.dwSize = sizeof(DDSURFACEDESC);
	if (ddraw_stream_ptr->GetFormat(&ddraw_surface_desc, NULL, NULL, NULL)
		!= S_OK)
		return(false);
	video_width = ddraw_surface_desc.dwWidth;
	video_height = ddraw_surface_desc.dwHeight;

	// Create a DirectDraw video surface using the texture pixel format, but 
	// without an alpha channel (otherwise CreateSample will spit the dummy).

	memset(&ddraw_surface_desc, 0, sizeof(DDSURFACEDESC));
	ddraw_surface_desc.dwSize = sizeof(DDSURFACEDESC);
	ddraw_surface_desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT |
		DDSD_PIXELFORMAT;
	ddraw_surface_desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_SYSTEMMEMORY;
	ddraw_surface_desc.dwWidth = video_width;
	ddraw_surface_desc.dwHeight = video_height;
	ddraw_surface_desc.ddpfPixelFormat = ddraw_video_pixel_format;
	if (ddraw_object_ptr->CreateSurface(&ddraw_surface_desc, &video_surface_ptr,
		NULL) != DD_OK)
		return(false);

	// Set the rectangle that is to be rendered to on the video surface.

	rect.left = 0;
 	rect.right = video_width;
 	rect.top = 0;
 	rect.bottom = video_height;

	// Create the video sample for the video surface.

	if (ddraw_stream_ptr->CreateSample(video_surface_ptr, &rect, 0, 
		&video_sample_ptr) != S_OK)
		return(false);

	// Create the event that will be used to signal that a video frame is
	// available.

	video_frame_available.create_event();

	// Initialise the video textures now, since we already know the
	// dimensions of the video frame.

	init_video_textures(video_width, video_height, RGB16);
	return(true);
}

//------------------------------------------------------------------------------
// Create a stream for the given streaming media file.
//------------------------------------------------------------------------------

static int
create_stream(const char *file_path)
{
	IAMMultiMediaStream *local_stream_ptr;
	WCHAR wPath[MAX_PATH];

	// Initialise the COM library.

	CoInitialize(NULL);

	// Initialise the global variables.

	global_stream_ptr = NULL;
	primary_video_stream_ptr = NULL;
	ddraw_stream_ptr = NULL;
	video_sample_ptr = NULL;
	video_surface_ptr = NULL;

	// Create the local multi-media stream object.

	if (CoCreateInstance(CLSID_AMMultiMediaStream, NULL, 
		CLSCTX_INPROC_SERVER, IID_IAMMultiMediaStream, 
		(void **)&local_stream_ptr) != S_OK)
		return(PLAYER_UNAVAILABLE);

	// Initialise the local stream object.

	if (local_stream_ptr->Initialize(STREAMTYPE_READ, AMMSF_NOGRAPHTHREAD,
		NULL) != S_OK) {
		local_stream_ptr->Release();
		return(PLAYER_UNAVAILABLE);
	}

	// Add a primary video stream to the local stream object.

	if (local_stream_ptr->AddMediaStream(ddraw_object_ptr, 
		&MSPID_PrimaryVideo, 0, NULL) != S_OK) {
		local_stream_ptr->Release();
		return(PLAYER_UNAVAILABLE);
	}

	// Add a primary audio stream to the local stream object, using the 
	// default audio renderer for playback.

	if (local_stream_ptr->AddMediaStream(NULL, &MSPID_PrimaryAudio, 
		AMMSF_ADDDEFAULTRENDERER, NULL) != S_OK) {
		local_stream_ptr->Release();
		return(PLAYER_UNAVAILABLE);
	}

	// Open the streaming media file.

	MultiByteToWideChar(CP_ACP, 0, file_path, -1, wPath, MAX_PATH);    
	if (local_stream_ptr->OpenFile(wPath, 0) != S_OK) {
		local_stream_ptr->Release();
		diagnose("Windows Media Player was unable to open stream URL %s", 
			file_path);
		return(STREAM_UNAVAILABLE);
	}

	// Convert the local stream object into a global stream object.

	global_stream_ptr = local_stream_ptr;

	// Initialise the primary video stream, if it exists.

	streaming_video_available = init_video_stream();

	// Get the end of stream event handle.

	global_stream_ptr->GetEndOfStreamEventHandle(&end_of_stream_handle);

	// Return a success status.

	return(STREAM_STARTED);
}

//------------------------------------------------------------------------------
// Convert a 24-bit YUV pixel to a 555 RGB pixel.
//------------------------------------------------------------------------------

float yuvmatrix[9] = {
	1.164f, 1.164f, 1.164f,
	0.000f,-0.391f, 2.018f,
	1.596f,-0.813f, 0.000f
};

word
YUV_to_pixel(float *yuv)
{
	float rgb[3];
	word pix;

	// Convert YUV to RGB.

	rgb[0] = 1.164f * yuv[0] + 1.596f * yuv[2];
	rgb[1] = 1.164f * yuv[0] - 0.391f * yuv[1] - 0.813f * yuv[2];
	rgb[2] = 1.164f * yuv[0] + 2.018f * yuv[1];

	// Set red component in 16-bit pixel.

	if (rgb[0] <= 0.0f)
		pix = 0x0000;
	else if (rgb[0] > 255.0f)
		pix = 0x7c00;
	else
		pix = ((word)rgb[0] & 0x00f8) << 7;
		
	// Add green component to 16-bit pixel.

	if (rgb[1] > 255.0f)
		pix |= 0x003e0;
	else if (rgb[1] > 0.0f)
		pix |= ((word)rgb[1] & 0x00f8) << 2;

	// Add blue component to 16-bit pixel.

	if (rgb[2] > 255.0f)
		pix |= 0x001f;
	else if (rgb[2] > 0.0f)
		pix |= (word)rgb[2] >> 3;
	
	// Return this pixel value.

	return(pix);
}

//------------------------------------------------------------------------------
// Play the stream.
//------------------------------------------------------------------------------

static void
play_stream(void)
{
	global_stream_ptr->SetState(STREAMSTATE_RUN);
	if (streaming_video_available)
		video_sample_ptr->Update(0, video_frame_available.event_handle, NULL, NULL);
}

//------------------------------------------------------------------------------
// Stop the stream.
//------------------------------------------------------------------------------

static void
stop_stream(void)
{
	global_stream_ptr->SetState(STREAMSTATE_STOP);
	global_stream_ptr->Seek(0);
}

//------------------------------------------------------------------------------
// Pause the stream.
//------------------------------------------------------------------------------

static void
pause_stream(void)
{
	global_stream_ptr->SetState(STREAMSTATE_STOP);
}

//------------------------------------------------------------------------------
// Update stream (Windows Media Player only).
//------------------------------------------------------------------------------

static void
update_stream(void)
{
	// If there is a video stream, and the next video frame is available...

	if (streaming_video_available && video_frame_available.event_sent()) {
		DDSURFACEDESC ddraw_surface_desc;
		byte *fb_ptr, *fb_row_ptr, *pixmap_row_ptr;
		int row_pitch;
		int row, column;
		float u, v, start_u, start_v, delta_u, delta_v;
		video_texture *scaled_video_texture_ptr;
		texture *texture_ptr;
		pixmap *pixmap_ptr;
		int index;

		// Lock the video surface.

		ddraw_surface_desc.dwSize = sizeof(ddraw_surface_desc);
		if (video_surface_ptr->Lock(NULL, &ddraw_surface_desc,
			DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL) != DD_OK)
			return;
		fb_ptr = (byte *)ddraw_surface_desc.lpSurface;
		row_pitch = ddraw_surface_desc.lPitch;

		// If there is an unscaled texture, copy the video surface to it.

		if (unscaled_video_texture_ptr != NULL) {
			pixmap_ptr = &unscaled_video_texture_ptr->pixmap_list[0];
			memcpy(pixmap_ptr->image_ptr, fb_ptr, pixmap_ptr->image_size);

			// Indicate the pixmap has been updated.

			raise_semaphore(image_updated_semaphore);
			for (index = 0; index < BRIGHTNESS_LEVELS; index++)
				pixmap_ptr->image_updated[index] = true;
			lower_semaphore(image_updated_semaphore);
		}

		// If there are scaled video textures, copy the video surface to each
		// one.

		scaled_video_texture_ptr = scaled_video_texture_list;
		while (scaled_video_texture_ptr != NULL) {
			texture_ptr = scaled_video_texture_ptr->texture_ptr;
			pixmap_ptr = texture_ptr->pixmap_list;
			start_u = scaled_video_texture_ptr->source_rect.x1;
			start_v = scaled_video_texture_ptr->source_rect.y1;
			delta_u = scaled_video_texture_ptr->delta_u;
			delta_v = scaled_video_texture_ptr->delta_v;
			v = start_v;
			for (row = 0; row < pixmap_ptr->height; row++) {
				fb_row_ptr = fb_ptr + (int)v * row_pitch;
				pixmap_row_ptr = pixmap_ptr->image_ptr + 
					row * pixmap_ptr->width * 2;
				u = start_u;
				for (column = 0; column < pixmap_ptr->width; column++) {
					*(word *)pixmap_row_ptr = *((word *)fb_row_ptr + (int)u);
					pixmap_row_ptr += 2;
					u += delta_u;
				}
				v += delta_v;
			}

			// Indicate the pixmap has been updated.

			raise_semaphore(image_updated_semaphore);
			for (index = 0; index < BRIGHTNESS_LEVELS; index++)
				pixmap_ptr->image_updated[index] = true;
			lower_semaphore(image_updated_semaphore);

			// Move onto the next video texture.

			scaled_video_texture_ptr = 
				scaled_video_texture_ptr->next_video_texture_ptr;
		}

		// Unlock the video surface.

		video_surface_ptr->Unlock(fb_ptr);

		// Request the next video frame.
		
		video_sample_ptr->Update(0, video_frame_available.event_handle,
			NULL, NULL);
	}

	// If the end of the stream has been reached, restart the stream.

	if (end_of_stream_handle && 
		WaitForSingleObject(end_of_stream_handle, 0) != WAIT_TIMEOUT) {
		stop_stream();
		play_stream();
	}
}

//------------------------------------------------------------------------------
// Destroy the stream.
//------------------------------------------------------------------------------

static void
destroy_stream(void)
{
	// Destroy the "video frame available" event.

	if (video_frame_available.event_handle)
		video_frame_available.destroy_event();

	// Release the video sample object.

	if (video_sample_ptr != NULL)
		video_sample_ptr->Release();

	// Release the video surface object.

	if (video_surface_ptr != NULL)
		video_surface_ptr->Release();

	// Release the DirectDraw stream object.

	if (ddraw_stream_ptr != NULL)
		ddraw_stream_ptr->Release();

	// Release the primary video stream object.

	if (primary_video_stream_ptr != NULL)
		primary_video_stream_ptr->Release();

	// Release the global stream object.

	if (global_stream_ptr != NULL)
		global_stream_ptr->Release();

	// Shut down the COM library.

	CoUninitialize();
}

//------------------------------------------------------------------------------
// Ask user whether they want to download Windows Media Player.
//------------------------------------------------------------------------------

static void
download_wmp(void)
{
	if (query("Windows Media Player required", true,
		"Flatland requires the latest version of Windows Media Player\n"
		"to play back the streaming media in this spot.\n\n"
		"Do you wish to download Windows Media Player now?"))
		wmp_download_requested.send_event(true);
}

//------------------------------------------------------------------------------
// Display a message telling the user that Windows Media Player does not
// support the video format.
//------------------------------------------------------------------------------

static void
unsupported_wmp_format(void)
{
	information("Unable to play streaming media",
		"Windows Media Player is unable to play the streaming media in this\n"
		"spot; either the streaming media URL is invalid, or the streaming\n"
		"media format is not supported by Windows Media Player.");
}

//------------------------------------------------------------------------------
// Streaming thread.
//------------------------------------------------------------------------------

static void
streaming_thread(void *arg_list)
{
	int wmp_result;

	// Decrease the priority level on this thread, to ensure that the browser
	// and the rest of the system remains responsive.

	decrease_thread_priority();

	// If the stream URL for Windows Media Player was specified, create that
	// stream.

	if (strlen(wmp_stream_URL) > 0) {
		wmp_result = create_stream(wmp_stream_URL);
		if (wmp_result != STREAM_STARTED)
			destroy_stream();
	}

	// If there was a stream URL for Windows Media Player, and it failed
	// to start, then either ask the user if they want to download Windows
	// Media Player, or display an error message.

	if (strlen(wmp_stream_URL) > 0 && wmp_result != STREAM_STARTED) { 
		if (wmp_result == PLAYER_UNAVAILABLE)
			download_wmp();
		else
			unsupported_wmp_format();
		return;
	}

	// Start playing the stream.

	play_stream();

	// Loop until a request to terminate the thread has been recieved.

	while (!terminate_streaming_thread.event_sent()) {
		update_stream();
	}

	// Stop then destroy the stream.

	stop_stream();
	destroy_stream();
}

//------------------------------------------------------------------------------
// Determine if the stream is ready.
//------------------------------------------------------------------------------

bool
stream_ready(void)
{
	return(stream_opened.event_sent());
}

//------------------------------------------------------------------------------
// Determine if download of Windows Media Player was requested.
//------------------------------------------------------------------------------

bool
download_of_wmp_requested(void)
{
	return(wmp_download_requested.event_sent());
}

//------------------------------------------------------------------------------
// Start the streaming thread.
//------------------------------------------------------------------------------

void
start_streaming_thread(void)
{
	// Create the events needed to communicate with the streaming thread.

	stream_opened.create_event();
	terminate_streaming_thread.create_event();
	wmp_download_requested.create_event();

	// Start the stream thread.

	streaming_thread_handle = _beginthread(streaming_thread, 0, NULL);
}

//------------------------------------------------------------------------------
// Stop the streaming thread.
//------------------------------------------------------------------------------

void
stop_streaming_thread(void)
{
	// Send the streaming thread a request to terminate, then wait for it to
	// do so.

	if (streaming_thread_handle >= 0) {
		terminate_streaming_thread.send_event(true);
		WaitForSingleObject((HANDLE)streaming_thread_handle, INFINITE);
	}

	// Destroy the events used to communicate with the stream thread.

	stream_opened.destroy_event();
	terminate_streaming_thread.destroy_event();
	wmp_download_requested.destroy_event();
}

#endif