




#include "bmpman/bmpman.h"
#include "cfile/cfile.h"
#include "cmdline/cmdline.h"
#include "ddsutils/ddsutils.h"
#include "debugconsole/console.h"
#include "globalincs/systemvars.h"
#include "graphics/2d.h"
#include "graphics/gropengl.h"
#include "graphics/gropenglbmpman.h"
#include "graphics/gropengldraw.h"
#include "graphics/gropenglextension.h"
#include "graphics/gropengllight.h"
#include "graphics/gropenglpostprocessing.h"
#include "graphics/gropenglshader.h"
#include "graphics/gropenglstate.h"
#include "graphics/gropengltexture.h"
#include "graphics/gropengltnl.h"
#include "graphics/line.h"
#include "io/mouse.h"
#include "io/timer.h"
#include "math/floating.h"
#include "model/model.h"
#include "nebula/neb.h"
#include "osapi/osapi.h"
#include "osapi/osregistry.h"
#include "palman/palman.h"
#include "render/3d.h"


#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>
#include <direct.h>
#elif defined(__APPLE__)
#include "OpenGL.h"
#else
typedef int ( * PFNGLXSWAPINTERVALSGIPROC) (int interval);
#endif


#if defined(_WIN32) && !defined(__GNUC__)
#pragma comment (lib, "opengl32")
#pragma comment (lib, "glu32")
#endif

// minimum GL version we can reliably support is 1.2
static const int MIN_REQUIRED_GL_VERSION = 12;

// minimum GLSL version we can reliably support is 110
static const int MIN_REQUIRED_GLSL_VERSION = 110;

int GL_version = 0;
int GLSL_version = 0;

bool GL_initted = 0;

//0==no fog
//1==linear
//2==fog coord EXT
//3==NV Radial
int OGL_fogmode = 0;

static ushort *GL_original_gamma_ramp = NULL;

int Use_VBOs = 0;
int Use_PBOs = 0;

static ubyte *GL_saved_screen = NULL;
static int GL_saved_screen_id = -1;
static GLuint GL_screen_pbo = 0;

extern const char *Osreg_title;

extern GLfloat GL_anisotropy;

extern float FreeSpace_gamma;
void gr_opengl_set_gamma(float gamma);

extern float FreeSpace_gamma;
void gr_opengl_set_gamma(float gamma);

static int GL_fullscreen = 0;
static int GL_windowed = 0;
static int GL_minimized = 0;

static GLenum GL_read_format = GL_BGRA;

SDL_GLContext GL_context = NULL;

void opengl_go_fullscreen()
{
	if (Cmdline_fullscreen_window || Cmdline_window || GL_fullscreen || Fred_running)
		return;

	if ( (os_config_read_uint(NULL, NOX("Fullscreen"), 1) == 1) && (!os_get_window() || !(SDL_GetWindowFlags(os_get_window()) & SDL_WINDOW_FULLSCREEN)) ) {
		os_suspend();
		if(os_get_window())
		{
			SDL_SetWindowFullscreen(os_get_window(), SDL_WINDOW_FULLSCREEN);
		}
		else
		{
			uint display = os_config_read_uint("Video", "Display", 0);
			SDL_Window* window = SDL_CreateWindow(Osreg_title, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
				gr_screen.max_w, gr_screen.max_h, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
			if (window == NULL) {
				mprintf(("Couldn't go fullscreen!\n"));
				if ((window = SDL_CreateWindow(Osreg_title, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
					gr_screen.max_w, gr_screen.max_h, SDL_WINDOW_OPENGL)) == NULL) {
					mprintf(("Couldn't drop back to windowed mode either!\n"));
					exit(1);
				}
			}

			os_set_window(window);
		}
		os_resume();
	}

	gr_opengl_set_gamma(FreeSpace_gamma);

	GL_fullscreen = 1;
	GL_minimized = 0;
	GL_windowed = 0;
}

void opengl_go_windowed()
{
	if ( ( !Cmdline_fullscreen_window && !Cmdline_window ) /*|| GL_windowed*/ || Fred_running )
		return;

	if (!os_get_window() || SDL_GetWindowFlags(os_get_window()) & SDL_WINDOW_FULLSCREEN) {
		os_suspend();
		if(os_get_window())
		{
			SDL_SetWindowFullscreen(os_get_window(), Cmdline_fullscreen_window ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0 );
		}
		else
		{
			uint display = os_config_read_uint("Video", "Display", 0);
			SDL_Window* new_window = SDL_CreateWindow(Osreg_title, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
				gr_screen.max_w, gr_screen.max_h, SDL_WINDOW_OPENGL);
			if (new_window == NULL) {
				Warning( LOCATION, "Unable to enter windowed mode: %s!", SDL_GetError() );
			}
			os_set_window(new_window);
		}

		os_resume();
	}

	GL_windowed = 1;
	GL_minimized = 0;
	GL_fullscreen = 0;
}

void opengl_minimize()
{
	// don't attempt to minimize if we are already in a window, or already minimized, or when playing a movie
	if (GL_minimized /*|| GL_windowed || Cmdline_window*/ || Fred_running)
		return;

	// lets not minimize if we are in windowed mode
	if ( !(SDL_GetWindowFlags(os_get_window()) & SDL_WINDOW_FULLSCREEN) )
		return;

	os_suspend();

	if (GL_original_gamma_ramp != NULL) {
		SDL_SetWindowGammaRamp( os_get_window(), GL_original_gamma_ramp, (GL_original_gamma_ramp+256), (GL_original_gamma_ramp+512) );
	}

	SDL_MinimizeWindow(os_get_window());
	os_resume();

	GL_minimized = 1;
	GL_windowed = 0;
	GL_fullscreen = 0;
}

void gr_opengl_activate(int active)
{
	if (active) {
		if (Cmdline_fullscreen_window||Cmdline_window)
			opengl_go_windowed();
		else
			opengl_go_fullscreen();
	} else {
		opengl_minimize();
	}
}

void gr_opengl_clear()
{
	float red = gr_screen.current_clear_color.red / 255.0f;
	float green = gr_screen.current_clear_color.green / 255.0f;
	float blue = gr_screen.current_clear_color.blue / 255.0f;
	float alpha = gr_screen.current_clear_color.alpha / 255.0f;

	if ( High_dynamic_range ) {
		const float SRGB_GAMMA = 2.2f;

		red = pow(red, SRGB_GAMMA);
		green = pow(green, SRGB_GAMMA);
		blue = pow(blue, SRGB_GAMMA);
	}

	glClearColor(red, green, blue, alpha);

	glClear ( GL_COLOR_BUFFER_BIT );
}

void gr_opengl_flip()
{
	if ( !GL_initted )
		return;

	gr_reset_clip();

	mouse_reset_deltas();

	if (Cmdline_gl_finish)
		glFinish();

	SDL_GL_SwapWindow(os_get_window());

	opengl_tcache_frame();

#ifndef NDEBUG
	int ic = opengl_check_for_errors();

	if (ic) {
		mprintf(("!!DEBUG!! OpenGL Errors this frame: %i\n", ic));
	}
#endif
}

void gr_opengl_set_clip(int x, int y, int w, int h, int resize_mode)
{
	// check for sanity of parameters
	if (x < 0) {
		x = 0;
	}

	if (y < 0) {
		y = 0;
	}

	int to_resize = (resize_mode != GR_RESIZE_NONE && (gr_screen.custom_size || (gr_screen.rendering_to_texture != -1)));

	int max_w = ((to_resize) ? gr_screen.max_w_unscaled : gr_screen.max_w);
	int max_h = ((to_resize) ? gr_screen.max_h_unscaled : gr_screen.max_h);

	if ((gr_screen.rendering_to_texture != -1) && to_resize) {
		gr_unsize_screen_pos(&max_w, &max_h);
	}

	if (x >= max_w) {
		x = max_w - 1;
	}

	if (y >= max_h) {
		y = max_h - 1;
	}

	if (x + w > max_w) {
		w = max_w - x;
	}

	if (y + h > max_h) {
		h = max_h - y;
	}

	if (w > max_w) {
		w = max_w;
	}

	if (h > max_h) {
		h = max_h;
	}

	gr_screen.offset_x_unscaled = x;
	gr_screen.offset_y_unscaled = y;
	gr_screen.clip_left_unscaled = 0;
	gr_screen.clip_right_unscaled = w-1;
	gr_screen.clip_top_unscaled = 0;
	gr_screen.clip_bottom_unscaled = h-1;
	gr_screen.clip_width_unscaled = w;
	gr_screen.clip_height_unscaled = h;

	if (to_resize) {
		gr_resize_screen_pos(&x, &y, &w, &h, resize_mode);
	} else {
		gr_unsize_screen_pos( &gr_screen.offset_x_unscaled, &gr_screen.offset_y_unscaled );
		gr_unsize_screen_pos( &gr_screen.clip_right_unscaled, &gr_screen.clip_bottom_unscaled );
		gr_unsize_screen_pos( &gr_screen.clip_width_unscaled, &gr_screen.clip_height_unscaled );
	}

	gr_screen.offset_x = x;
	gr_screen.offset_y = y;
	gr_screen.clip_left = 0;
	gr_screen.clip_right = w-1;
	gr_screen.clip_top = 0;
	gr_screen.clip_bottom = h-1;
	gr_screen.clip_width = w;
	gr_screen.clip_height = h;

	gr_screen.clip_aspect = i2fl(w) / i2fl(h);
	gr_screen.clip_center_x = (gr_screen.clip_left + gr_screen.clip_right) * 0.5f;
	gr_screen.clip_center_y = (gr_screen.clip_top + gr_screen.clip_bottom) * 0.5f;

	// just return early if we aren't actually going to need the scissor test
	if ( (x == 0) && (y == 0) && (w == max_w) && (h == max_h) ) {
		GL_state.ScissorTest(GL_FALSE);
		return;
	}

	GL_state.ScissorTest(GL_TRUE);
	if(GL_rendering_to_texture) {
		glScissor(x, y, w, h);
	} else {
		glScissor(x, gr_screen.max_h-y-h, w, h);
	}
}

void gr_opengl_reset_clip()
{
	gr_screen.offset_x = gr_screen.offset_x_unscaled = 0;
	gr_screen.offset_y = gr_screen.offset_y_unscaled = 0;
	gr_screen.clip_left = gr_screen.clip_left_unscaled = 0;
	gr_screen.clip_top = gr_screen.clip_top_unscaled = 0;
	gr_screen.clip_right = gr_screen.clip_right_unscaled = gr_screen.max_w - 1;
	gr_screen.clip_bottom = gr_screen.clip_bottom_unscaled = gr_screen.max_h - 1;
	gr_screen.clip_width = gr_screen.clip_width_unscaled = gr_screen.max_w;
	gr_screen.clip_height = gr_screen.clip_height_unscaled = gr_screen.max_h;

	if (gr_screen.custom_size) {
		gr_unsize_screen_pos( &gr_screen.clip_right_unscaled, &gr_screen.clip_bottom_unscaled );
		gr_unsize_screen_pos( &gr_screen.clip_width_unscaled, &gr_screen.clip_height_unscaled );
	}

	gr_screen.clip_aspect = i2fl(gr_screen.clip_width) / i2fl(gr_screen.clip_height);
	gr_screen.clip_center_x = (gr_screen.clip_left + gr_screen.clip_right) * 0.5f;
	gr_screen.clip_center_y = (gr_screen.clip_top + gr_screen.clip_bottom) * 0.5f;

	GL_state.ScissorTest(GL_FALSE);
}

void gr_opengl_print_screen(const char *filename)
{
	char tmp[MAX_PATH_LEN];
	ubyte tga_hdr[18];
	int i;
	ushort width, height;
	GLubyte *pixels = NULL;
	GLuint pbo = 0;

	// save to a "screenshots" directory and tack on the filename
	snprintf(tmp, MAX_PATH_LEN-1, "screenshots/%s.tga", filename);
    
    _mkdir(os_get_config_path("screenshots").c_str());

	FILE *fout = fopen(os_get_config_path(tmp).c_str(), "wb");

	if (fout == NULL) {
		return;
	}

//	glReadBuffer(GL_FRONT);

	// now for the data
	if (Use_PBOs) {
		Assert( !pbo );
		vglGenBuffersARB(1, &pbo);

		if ( !pbo ) {
			if (fout != NULL)
				fclose(fout);

			return;
		}

		vglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pbo);
		vglBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, (gr_screen.max_w * gr_screen.max_h * 4), NULL, GL_STATIC_READ);

		glReadBuffer(GL_FRONT);
		glReadPixels(0, 0, gr_screen.max_w, gr_screen.max_h, GL_read_format, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

		// map the image data so that we can save it to file
		pixels = (GLubyte*) vglMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
	} else {
		pixels = (GLubyte*) vm_malloc_q(gr_screen.max_w * gr_screen.max_h * 4);

		if (pixels == NULL) {
			if (fout != NULL) {
				fclose(fout);
			}

			return;
		}

		glReadPixels(0, 0, gr_screen.max_w, gr_screen.max_h, GL_read_format, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
		glFlush();
	}

	// Write the TGA header
	width = INTEL_SHORT((ushort)gr_screen.max_w);
	height = INTEL_SHORT((ushort)gr_screen.max_h);

	memset( tga_hdr, 0, sizeof(tga_hdr) );

	tga_hdr[2] = 2;		// ImageType    2 = 24bpp, uncompressed
	memcpy( tga_hdr + 12, &width, sizeof(ushort) );		// Width
	memcpy( tga_hdr + 14, &height, sizeof(ushort) );	// Height
	tga_hdr[16] = 24;	// PixelDepth

	fwrite( tga_hdr, sizeof(tga_hdr), 1, fout );

	// now for the data, we convert it from 32-bit to 24-bit
	for (i = 0; i < (gr_screen.max_w * gr_screen.max_h * 4); i += 4) {
#if BYTE_ORDER == BIG_ENDIAN
		int pix, *pix_tmp;

		pix_tmp = (int*)(pixels + i);
		pix = INTEL_INT(*pix_tmp);

		fwrite( &pix, 1, 3, fout );
#else
		fwrite( pixels + i, 1, 3, fout );
#endif
	}

	if (pbo) {
		vglUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		pixels = NULL;
		vglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		vglDeleteBuffersARB(1, &pbo);
	}

	// done!
	fclose(fout);

	if (pixels != NULL) {
		vm_free(pixels);
	}
}

void gr_opengl_cleanup(bool closing, int minimize)
{
	if ( !GL_initted ) {
		return;
	}

	if ( !closing && !Fred_running ) {
		gr_reset_clip();
		gr_clear();
		gr_flip();
		gr_clear();
		gr_flip();
		gr_clear();
	}

	GL_initted = false;

	opengl_tcache_flush();

	opengl_minimize();

	if (minimize) {

	}
}

void gr_opengl_fog_set(int fog_mode, int r, int g, int b, float fog_near, float fog_far)
{
//	mprintf(("gr_opengl_fog_set(%d,%d,%d,%d,%f,%f)\n",fog_mode,r,g,b,fog_near,fog_far));

	Assert((r >= 0) && (r < 256));
	Assert((g >= 0) && (g < 256));
	Assert((b >= 0) && (b < 256));

	if (fog_mode == GR_FOGMODE_NONE) {
		if ( GL_state.Fog() ) {
			GL_state.Fog(GL_FALSE);
		}

		gr_screen.current_fog_mode = fog_mode;

		return;
	}

  	if (OGL_fogmode == 3) {
		glFogf(GL_FOG_DISTANCE_MODE_NV, GL_EYE_RADIAL_NV);
		glFogf(GL_FOG_COORDINATE_SOURCE, GL_FRAGMENT_DEPTH);
	} else {
		glFogf(GL_FOG_COORDINATE_SOURCE, GL_FRAGMENT_DEPTH);
	}

	GL_state.Fog(GL_TRUE);
	glFogf(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, fog_near);
	glFogf(GL_FOG_END, fog_far);

	gr_screen.current_fog_mode = fog_mode;

	if ( (gr_screen.current_fog_color.red != r) ||
			(gr_screen.current_fog_color.green != g) ||
			(gr_screen.current_fog_color.blue != b) )
	{
		GLfloat fc[4];

		gr_init_color( &gr_screen.current_fog_color, r, g, b );

		fc[0] = (float)r/255.0f;
		fc[1] = (float)g/255.0f;
		fc[2] = (float)b/255.0f;
		fc[3] = 1.0f;

		glFogfv(GL_FOG_COLOR, fc);
	}

}

int gr_opengl_set_cull(int cull)
{
	GLboolean enabled = GL_FALSE;

	if (cull) {
		enabled = GL_state.CullFace(GL_TRUE);
		GL_state.FrontFaceValue(GL_CCW);
		GL_state.CullFaceValue(GL_BACK);
	} else {
		enabled = GL_state.CullFace(GL_FALSE);
	}

	return (enabled) ? 1 : 0;
}

void gr_opengl_set_clear_color(int r, int g, int b)
{
	gr_init_color(&gr_screen.current_clear_color, r, g, b);
}

int gr_opengl_set_color_buffer(int mode)
{
	GLboolean enabled = GL_FALSE;

	if ( mode ) {
		enabled = GL_state.ColorMask(GL_TRUE);
	} else {
		enabled = GL_state.ColorMask(GL_FALSE);
	}

	GL_state.SetAlphaBlendMode(ALPHA_BLEND_ALPHA_BLEND_ALPHA);

	return (enabled) ? 1 : 0;
}

int gr_opengl_zbuffer_get()
{
	if ( !gr_global_zbuffering ) {
		return GR_ZBUFF_NONE;
	}

	return gr_zbuffering_mode;
}

int gr_opengl_zbuffer_set(int mode)
{
	int tmp = gr_zbuffering_mode;

	gr_zbuffering_mode = mode;

	if (gr_zbuffering_mode == GR_ZBUFF_NONE) {
		gr_zbuffering = 0;
		GL_state.SetZbufferType(ZBUFFER_TYPE_NONE);
	} else if ( gr_zbuffering_mode == GR_ZBUFF_READ ) {
		gr_zbuffering = 1;
		GL_state.SetZbufferType(ZBUFFER_TYPE_READ);
	} else {
		gr_zbuffering = 1;
		GL_state.SetZbufferType(ZBUFFER_TYPE_FULL);
	}

	return tmp;
}

void gr_opengl_zbuffer_clear(int mode)
{
	if (mode) {
		gr_zbuffering = 1;
		gr_zbuffering_mode = GR_ZBUFF_FULL;
		gr_global_zbuffering = 1;

		GL_state.SetTextureSource(TEXTURE_SOURCE_NONE);
		GL_state.SetAlphaBlendMode(ALPHA_BLEND_NONE);
		GL_state.SetZbufferType(ZBUFFER_TYPE_FULL);

		glClear(GL_DEPTH_BUFFER_BIT);
	} else {
		gr_zbuffering = 0;
		gr_zbuffering_mode = GR_ZBUFF_NONE;
		gr_global_zbuffering = 0;

		GL_state.DepthTest(GL_FALSE);
	}
}

int gr_opengl_stencil_set(int mode)
{
	int tmp = gr_stencil_mode;

	gr_stencil_mode = mode;

	if ( mode == GR_STENCIL_READ ) {
		GL_state.StencilTest(1);
		GL_state.SetStencilType(STENCIL_TYPE_READ);
	} else if ( mode == GR_STENCIL_WRITE ) {
		GL_state.StencilTest(1);
		GL_state.SetStencilType(STENCIL_TYPE_WRITE);
	} else {
		GL_state.StencilTest(0);
		GL_state.SetStencilType(STENCIL_TYPE_NONE);
	}

	return tmp;
}

void gr_opengl_stencil_clear()
{
	glClear(GL_STENCIL_BUFFER_BIT);
}

int gr_opengl_alpha_mask_set(int mode, float alpha)
{
	int tmp = gr_alpha_test;

	gr_alpha_test = mode;

	if ( mode ) {
		GL_state.AlphaTest(GL_TRUE);
		GL_state.AlphaFunc(GL_GREATER, alpha);
	} else {
		GL_state.AlphaTest(GL_FALSE);
		GL_state.AlphaFunc(GL_ALWAYS, 1.0f);
	}

	return tmp;
}

// I feel dirty...
static void opengl_make_gamma_ramp(float gamma, ushort *ramp)
{
	ushort x, y;
	ushort base_ramp[256];

	Assert( ramp != NULL );

	// generate the base ramp values first off

	// if no gamma set then just do this quickly
	if (gamma <= 0.0f) {
		memset( ramp, 0, 3 * 256 * sizeof(ushort) );
		return;
	}
	// identity gamma, avoid all of the math
	else if ( (gamma == 1.0f) || (GL_original_gamma_ramp == NULL) ) {
		if (GL_original_gamma_ramp != NULL) {
			memcpy( ramp, GL_original_gamma_ramp, 3 * 256 * sizeof(ushort) );
		}
		// set identity if no original ramp
		else {
			for (x = 0; x < 256; x++) {
				ramp[x]	= (x << 8) | x;
				ramp[x + 256] = (x << 8) | x;
				ramp[x + 512] = (x << 8) | x;
			}
		}

		return;
	}
	// for everything else we need to actually figure it up
	else {
		double g = 1.0 / (double)gamma;
		double val;

		Assert( GL_original_gamma_ramp != NULL );

		for (x = 0; x < 256; x++) {
			val = (pow(x/255.0, g) * 65535.0 + 0.5);
			CLAMP( val, 0, 65535 );

			base_ramp[x] = (ushort)val;
		}

		for (y = 0; y < 3; y++) {
			for (x = 0; x < 256; x++) {
				val = (base_ramp[x] * 2) - GL_original_gamma_ramp[x + y * 256];
				CLAMP( val, 0, 65535 );

				ramp[x + y * 256] = (ushort)val;
			}
		}
	}
}

void gr_opengl_set_gamma(float gamma)
{
	ushort *gamma_ramp = NULL;

	Gr_gamma = gamma;
	Gr_gamma_int = int (Gr_gamma*10);

	// new way - but not while running FRED
	if (!Fred_running && !Cmdline_no_set_gamma) {
		gamma_ramp = (ushort*) vm_malloc_q( 3 * 256 * sizeof(ushort) );

		if (gamma_ramp == NULL) {
			Int3();
			return;
		}

		memset( gamma_ramp, 0, 3 * 256 * sizeof(ushort) );

		// Create the Gamma lookup table
		opengl_make_gamma_ramp(gamma, gamma_ramp);

		SDL_SetWindowGammaRamp( os_get_window(), gamma_ramp, (gamma_ramp+256), (gamma_ramp+512) );

		vm_free(gamma_ramp);
	}
}

void gr_opengl_get_region(int front, int w, int h, ubyte *data)
{

//	if (front) {
//		glReadBuffer(GL_FRONT);
//	} else {
		glReadBuffer(GL_BACK);
//	}

	GL_state.SetTextureSource(TEXTURE_SOURCE_NO_FILTERING);
	GL_state.SetAlphaBlendMode(ALPHA_BLEND_NONE);
	GL_state.SetZbufferType(ZBUFFER_TYPE_NONE);

	if (gr_screen.bits_per_pixel == 16) {
		glReadPixels(0, gr_screen.max_h-h, w, h, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, data);
	} else if (gr_screen.bits_per_pixel == 32) {
		glReadPixels(0, gr_screen.max_h-h, w, h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
	}


}

int gr_opengl_save_screen()
{
	int i;
	ubyte *sptr = NULL, *dptr = NULL;
	ubyte *opengl_screen_tmp = NULL;
	int width_times_pixel;

	gr_opengl_reset_clip();

	if (GL_saved_screen || GL_screen_pbo) {
		// already have a screen saved so just bail...
		return -1;
	}

	GL_saved_screen = (ubyte*)vm_malloc_q( gr_screen.max_w * gr_screen.max_h * 4 );

	if (!GL_saved_screen) {
		mprintf(( "Couldn't get memory for saved screen!\n" ));
 		return -1;
	}

	GLboolean save_state = GL_state.DepthTest(GL_FALSE);
	glReadBuffer(GL_FRONT_LEFT);

	if ( Use_PBOs ) {
		GLubyte *pixels = NULL;

		vglGenBuffersARB(1, &GL_screen_pbo);

		if (!GL_screen_pbo) {
			if (GL_saved_screen) {
				vm_free(GL_saved_screen);
				GL_saved_screen = NULL;
			}

			return -1;
		}

		vglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_screen_pbo);
		vglBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, gr_screen.max_w * gr_screen.max_h * 4, NULL, GL_STATIC_READ);

		glReadPixels(0, 0, gr_screen.max_w, gr_screen.max_h, GL_read_format, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);

		pixels = (GLubyte*)vglMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);

		width_times_pixel = (gr_screen.max_w * 4);

		sptr = (ubyte *)pixels;
		dptr = (ubyte *)&GL_saved_screen[gr_screen.max_w * gr_screen.max_h * 4];

		for (i = 0; i < gr_screen.max_h; i++) {
			dptr -= width_times_pixel;
			memcpy(dptr, sptr, width_times_pixel);
			sptr += width_times_pixel;
		}

		vglUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		vglBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);

		vglDeleteBuffersARB(1, &GL_screen_pbo);
		GL_screen_pbo = 0;

		GL_saved_screen_id = bm_create(32, gr_screen.max_w, gr_screen.max_h, GL_saved_screen, 0);
	} else {
		opengl_screen_tmp = (ubyte*)vm_malloc_q( gr_screen.max_w * gr_screen.max_h * 4 );

		if (!opengl_screen_tmp) {
			if (GL_saved_screen) {
				vm_free(GL_saved_screen);
				GL_saved_screen = NULL;
			}

			mprintf(( "Couldn't get memory for temporary saved screen!\n" ));
			GL_state.DepthTest(save_state);
	 		return -1;
	 	}

		glReadPixels(0, 0, gr_screen.max_w, gr_screen.max_h, GL_read_format, GL_UNSIGNED_INT_8_8_8_8_REV, opengl_screen_tmp);

		sptr = (ubyte *)&opengl_screen_tmp[gr_screen.max_w * gr_screen.max_h * 4];
		dptr = (ubyte *)GL_saved_screen;

		width_times_pixel = (gr_screen.max_w * 4);

		for (i = 0; i < gr_screen.max_h; i++) {
			sptr -= width_times_pixel;
			memcpy(dptr, sptr, width_times_pixel);
			dptr += width_times_pixel;
		}

		vm_free(opengl_screen_tmp);

		GL_saved_screen_id = bm_create(32, gr_screen.max_w, gr_screen.max_h, GL_saved_screen, 0);
	}

	GL_state.DepthTest(save_state);

	return GL_saved_screen_id;
}

void gr_opengl_restore_screen(int bmp_id)
{
	gr_reset_clip();

	if ( !GL_saved_screen ) {
		gr_clear();
		return;
	}

	Assert( (bmp_id < 0) || (bmp_id == GL_saved_screen_id) );

	if (GL_saved_screen_id < 0)
		return;

	gr_set_bitmap(GL_saved_screen_id);
	gr_bitmap(0, 0, GR_RESIZE_NONE);	// don't scale here since we already have real screen size
}

void gr_opengl_free_screen(int bmp_id)
{
	if (!GL_saved_screen)
		return;

	vm_free(GL_saved_screen);
	GL_saved_screen = NULL;

	Assert( (bmp_id < 0) || (bmp_id == GL_saved_screen_id) );

	if (GL_saved_screen_id < 0)
		return;

	bm_release(GL_saved_screen_id);
	GL_saved_screen_id = -1;
}

//fill mode, solid/wire frame
void gr_opengl_set_fill_mode(int mode)
{
	if (mode == GR_FILL_MODE_SOLID) {
		GL_state.SetPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		return;
	}

	if (mode == GR_FILL_MODE_WIRE) {
		GL_state.SetPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		return;
	}

	// default setting
	GL_state.SetPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void gr_opengl_zbias(int bias)
{
	if (bias) {
		GL_state.PolygonOffsetFill(GL_TRUE);
		if(bias < 0) {
			GL_state.SetPolygonOffset(1.0, -i2fl(bias));
		}
		else {
			GL_state.SetPolygonOffset(0.0, -i2fl(bias));
		}
	} else {
		GL_state.PolygonOffsetFill(GL_FALSE);
	}
}

void gr_opengl_push_texture_matrix(int unit)
{
	GLint current_matrix;

	if (unit > GL_supported_texture_units)
		return;

	glGetIntegerv(GL_MATRIX_MODE, &current_matrix);
	vglActiveTextureARB(GL_TEXTURE0_ARB+unit);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();

	glMatrixMode(current_matrix);
}

void gr_opengl_pop_texture_matrix(int unit)
{
	GLint current_matrix;

	if (unit > GL_supported_texture_units)
		return;

	glGetIntegerv(GL_MATRIX_MODE, &current_matrix);
	vglActiveTextureARB(GL_TEXTURE0_ARB+unit);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glMatrixMode(current_matrix);
}

void gr_opengl_translate_texture_matrix(int unit, const vec3d *shift)
{
	GLint current_matrix;

	if (unit > GL_supported_texture_units) {
		/*tex_shift=*shift;*/
		return;
	}

	glGetIntegerv(GL_MATRIX_MODE, &current_matrix);
	vglActiveTextureARB(GL_TEXTURE0_ARB+unit);

	glMatrixMode(GL_TEXTURE);
	glTranslated(shift->xyz.x, shift->xyz.y, shift->xyz.z);

	glMatrixMode(current_matrix);

//	tex_shift=vmd_zero_vector;
}

void gr_opengl_set_line_width(float width)
{
	glLineWidth(width);
}

// Returns the human readable error string if there is an error or NULL if not
const char *opengl_error_string()
{
	GLenum error = GL_NO_ERROR;

	error = glGetError();

	if ( error != GL_NO_ERROR ) {
		return (const char *)gluErrorString(error);
	}

	return NULL;
}

int opengl_check_for_errors(char *err_at)
{
#ifdef NDEBUG
	return 0;
#endif
	const char *error_str = NULL;
	int num_errors = 0;

	error_str = opengl_error_string();

	if (error_str) {
		if (err_at != NULL) {
			nprintf(("OpenGL", "OpenGL Error from %s: %s\n", err_at, error_str));
		} else {
			nprintf(("OpenGL", "OpenGL Error: %s\n", error_str));
		}

		num_errors++;
	}

	return num_errors;
}

void opengl_set_vsync(int status)
{
	if ( (status < 0) || (status > 1) ) {
		Int3();
		return;
	}

	SDL_GL_SetSwapInterval(status);

	GL_CHECK_FOR_ERRORS("end of set_vsync()");
}

void opengl_setup_viewport()
{
	glViewport(0, 0, gr_screen.max_w, gr_screen.max_h);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// the top and bottom positions are reversed on purpose, but RTT needs them the other way
	if (GL_rendering_to_texture) {
		glOrtho(0, gr_screen.max_w, 0, gr_screen.max_h, -1.0, 1.0);
	} else {
		glOrtho(0, gr_screen.max_w, gr_screen.max_h, 0, -1.0, 1.0);
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

// NOTE: This should only ever be called through os_cleanup(), or when switching video APIs
void gr_opengl_shutdown()
{
	opengl_tcache_shutdown();
	opengl_light_shutdown();
	opengl_tnl_shutdown();
	opengl_scene_texture_shutdown();
	opengl_post_process_shutdown();
	opengl_shader_shutdown();

	GL_initted = false;

	if (GL_original_gamma_ramp != NULL) {
		SDL_SetWindowGammaRamp( os_get_window(), GL_original_gamma_ramp, (GL_original_gamma_ramp+256), (GL_original_gamma_ramp+512) );
	}

	if (GL_original_gamma_ramp != NULL) {
		vm_free(GL_original_gamma_ramp);
		GL_original_gamma_ramp = NULL;
	}

	SDL_GL_DeleteContext(GL_context);
	GL_context = NULL;
}

// NOTE: This should only ever be called through atexit()!!!
void opengl_close()
{
//	if ( !GL_initted )
//		return;
}

int opengl_init_display_device()
{
	int bpp = gr_screen.bits_per_pixel;

	if ((bpp != 16) && (bpp != 32)) {
		Int3();
		return 1;
	}


	// screen format
	switch (bpp) {
		case 16: {
			Gr_red.bits = 5;
			Gr_red.shift = 11;
			Gr_red.scale = 8;
			Gr_red.mask = 0xF800;

			Gr_green.bits = 6;
			Gr_green.shift = 5;
			Gr_green.scale = 4;
			Gr_green.mask = 0x7E0;

			Gr_blue.bits = 5;
			Gr_blue.shift = 0;
			Gr_blue.scale = 8;
			Gr_blue.mask = 0x1F;

			break;
		}

		case 32: {
			Gr_red.bits = 8;
			Gr_red.shift = 16;
			Gr_red.scale = 1;
			Gr_red.mask = 0xff0000;

			Gr_green.bits = 8;
			Gr_green.shift = 8;
			Gr_green.scale = 1;
			Gr_green.mask = 0x00ff00;

			Gr_blue.bits = 8;
			Gr_blue.shift = 0;
			Gr_blue.scale = 1;
			Gr_blue.mask = 0x0000ff;

			Gr_alpha.bits = 8;
			Gr_alpha.shift = 24;
			Gr_alpha.mask = 0xff000000;
			Gr_alpha.scale = 1;

			break;
		}
	}

	// texture format
	Gr_t_red.bits = 5;
	Gr_t_red.mask = 0x7c00;
	Gr_t_red.shift = 10;
	Gr_t_red.scale = 8;

	Gr_t_green.bits = 5;
	Gr_t_green.mask = 0x03e0;
	Gr_t_green.shift = 5;
	Gr_t_green.scale = 8;

	Gr_t_blue.bits = 5;
	Gr_t_blue.mask = 0x001f;
	Gr_t_blue.shift = 0;
	Gr_t_blue.scale = 8;

	Gr_t_alpha.bits = 1;
	Gr_t_alpha.mask = 0x8000;
	Gr_t_alpha.scale = 255;
	Gr_t_alpha.shift = 15;

	// alpha-texture format
	Gr_ta_red.bits = 4;
	Gr_ta_red.mask = 0x0f00;
	Gr_ta_red.shift = 8;
	Gr_ta_red.scale = 17;

	Gr_ta_green.bits = 4;
	Gr_ta_green.mask = 0x00f0;
	Gr_ta_green.shift = 4;
	Gr_ta_green.scale = 17;

	Gr_ta_blue.bits = 4;
	Gr_ta_blue.mask = 0x000f;
	Gr_ta_blue.shift = 0;
	Gr_ta_blue.scale = 17;

	Gr_ta_alpha.bits = 4;
	Gr_ta_alpha.mask = 0xf000;
	Gr_ta_alpha.shift = 12;
	Gr_ta_alpha.scale = 17;

	// allocate storage for original gamma settings
	if ( !Cmdline_no_set_gamma && (GL_original_gamma_ramp == NULL) ) {
		GL_original_gamma_ramp = (ushort*) vm_malloc_q( 3 * 256 * sizeof(ushort) );

		if (GL_original_gamma_ramp == NULL) {
			mprintf(("  Unable to allocate memory for gamma ramp!  Disabling...\n"));
			Cmdline_no_set_gamma = 1;
		} else {
			// assume identity ramp by default, to be overwritten by true ramp later
			for (ushort x = 0; x < 256; x++) {
				GL_original_gamma_ramp[x] = GL_original_gamma_ramp[x + 256] = GL_original_gamma_ramp[x + 512] = (x << 8) | x;
			}
		}
	}

	int r = 0, g = 0, b = 0, depth = 0, stencil = 1, db = 1;

	mprintf(("  Initializing SDL video...\n"));

#ifdef SCP_UNIX
	// Slight hack to make Mesa advertise S3TC support without libtxc_dxtn
	setenv("force_s3tc_enable", "true", 1);
#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Couldn't init SDL video: %s", SDL_GetError());
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, Gr_red.bits);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, Gr_green.bits);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, Gr_blue.bits);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, (bpp == 32) ? 24 : 16);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, (bpp == 32) ? 8 : 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, db);

	int fsaa_samples = os_config_read_uint(NULL, "OGL_AntiAliasSamples", 0);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, (fsaa_samples == 0) ? 0 : 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, fsaa_samples);

	mprintf(("  Requested SDL Video values = R: %d, G: %d, B: %d, depth: %d, stencil: %d, double-buffer: %d, FSAA: %d\n", Gr_red.bits, Gr_green.bits, Gr_blue.bits, (bpp == 32) ? 24 : 16, (bpp == 32) ? 8 : 1, db, fsaa_samples));

	int windowflags = SDL_WINDOW_OPENGL;
	if (Cmdline_fullscreen_window)
		windowflags |= SDL_WINDOW_BORDERLESS;

	// This code also gets called in the FRED initialization, that means we possibly already have a window!

	if (os_get_window() == NULL)
	{
		uint display = os_config_read_uint("Video", "Display", 0);
		SDL_Window* window = SDL_CreateWindow(Osreg_title, SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
			gr_screen.max_w, gr_screen.max_h, windowflags);
		if (window == NULL)
		{
			mprintf(("Could not create window: %s\n", SDL_GetError()));
			return 1;
		}

		os_set_window(window);
	}

	GL_context = SDL_GL_CreateContext(os_get_window());

	if (GL_context == nullptr) {
		mprintf(("Error creating GL_context: %s\n", SDL_GetError()));
		return 1;
	}

	SDL_GL_MakeCurrent(os_get_window(), GL_context);
	//TODO: set up bpp settings

	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth);
	SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &db);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil);
	SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &fsaa_samples);

	mprintf(("  Actual SDL Video values    = R: %d, G: %d, B: %d, depth: %d, stencil: %d, double-buffer: %d, FSAA: %d\n", r, g, b, depth, stencil, db, fsaa_samples));

	if (GL_original_gamma_ramp != NULL) {
		SDL_GetWindowGammaRamp( os_get_window(), GL_original_gamma_ramp, (GL_original_gamma_ramp+256), (GL_original_gamma_ramp+512) );
	}

	return 0;
}


void opengl_setup_function_pointers()
{
	gr_screen.gf_flip				= gr_opengl_flip;
	gr_screen.gf_set_clip			= gr_opengl_set_clip;
	gr_screen.gf_reset_clip			= gr_opengl_reset_clip;

	gr_screen.gf_clear				= gr_opengl_clear;
//	gr_screen.gf_bitmap				= gr_opengl_bitmap;
	gr_screen.gf_bitmap_ex			= gr_opengl_bitmap_ex;
	gr_screen.gf_aabitmap			= gr_opengl_aabitmap;
	gr_screen.gf_aabitmap_ex		= gr_opengl_aabitmap_ex;

//	gr_screen.gf_rect				= gr_opengl_rect;
//	gr_screen.gf_shade				= gr_opengl_shade;
	gr_screen.gf_string				= gr_opengl_string;
	gr_screen.gf_circle				= gr_opengl_circle;
	gr_screen.gf_unfilled_circle	= gr_opengl_unfilled_circle;
	gr_screen.gf_arc				= gr_opengl_arc;
	gr_screen.gf_curve				= gr_opengl_curve;

	gr_screen.gf_line				= gr_opengl_line;
	gr_screen.gf_aaline				= gr_opengl_aaline;
	gr_screen.gf_pixel				= gr_opengl_pixel;
	gr_screen.gf_scaler				= gr_opengl_scaler;
	gr_screen.gf_tmapper			= gr_opengl_tmapper;
	gr_screen.gf_render				= gr_opengl_render;
	gr_screen.gf_render_effect		= gr_opengl_render_effect;

	gr_screen.gf_gradient			= gr_opengl_gradient;

	gr_screen.gf_print_screen		= gr_opengl_print_screen;

	gr_screen.gf_flash				= gr_opengl_flash;
	gr_screen.gf_flash_alpha		= gr_opengl_flash_alpha;

	gr_screen.gf_zbuffer_get		= gr_opengl_zbuffer_get;
	gr_screen.gf_zbuffer_set		= gr_opengl_zbuffer_set;
	gr_screen.gf_zbuffer_clear		= gr_opengl_zbuffer_clear;

	gr_screen.gf_stencil_set		= gr_opengl_stencil_set;
	gr_screen.gf_stencil_clear		= gr_opengl_stencil_clear;

	gr_screen.gf_alpha_mask_set		= gr_opengl_alpha_mask_set;

	gr_screen.gf_save_screen		= gr_opengl_save_screen;
	gr_screen.gf_restore_screen		= gr_opengl_restore_screen;
	gr_screen.gf_free_screen		= gr_opengl_free_screen;

	gr_screen.gf_set_gamma			= gr_opengl_set_gamma;

	gr_screen.gf_fog_set			= gr_opengl_fog_set;

	// UnknownPlayer : Don't recognize this - MAY NEED DEBUGGING
	gr_screen.gf_get_region			= gr_opengl_get_region;

	// now for the bitmap functions
	gr_screen.gf_bm_free_data			= gr_opengl_bm_free_data;
	gr_screen.gf_bm_create				= gr_opengl_bm_create;
	gr_screen.gf_bm_init				= gr_opengl_bm_init;
	gr_screen.gf_bm_page_in_start		= gr_opengl_bm_page_in_start;
	gr_screen.gf_bm_data				= gr_opengl_bm_data;
	gr_screen.gf_bm_make_render_target	= gr_opengl_bm_make_render_target;
	gr_screen.gf_bm_set_render_target	= gr_opengl_bm_set_render_target;

	gr_screen.gf_set_cull			= gr_opengl_set_cull;
	gr_screen.gf_set_color_buffer	= gr_opengl_set_color_buffer;

	gr_screen.gf_cross_fade			= gr_opengl_cross_fade;

	gr_screen.gf_tcache_set			= gr_opengl_tcache_set;

	gr_screen.gf_set_clear_color	= gr_opengl_set_clear_color;

	gr_screen.gf_preload			= gr_opengl_preload;

	gr_screen.gf_push_texture_matrix		= gr_opengl_push_texture_matrix;
	gr_screen.gf_pop_texture_matrix			= gr_opengl_pop_texture_matrix;
	gr_screen.gf_translate_texture_matrix	= gr_opengl_translate_texture_matrix;

	gr_screen.gf_set_texture_addressing	= gr_opengl_set_texture_addressing;
	gr_screen.gf_zbias					= gr_opengl_zbias;
	gr_screen.gf_set_fill_mode			= gr_opengl_set_fill_mode;
	gr_screen.gf_set_texture_panning	= gr_opengl_set_texture_panning;

	gr_screen.gf_create_buffer		= gr_opengl_create_buffer;
	gr_screen.gf_config_buffer		= gr_opengl_config_buffer;
	gr_screen.gf_pack_buffer		= gr_opengl_pack_buffer;
	gr_screen.gf_destroy_buffer		= gr_opengl_destroy_buffer;
	gr_screen.gf_render_buffer		= gr_opengl_render_buffer;
	gr_screen.gf_set_buffer			= gr_opengl_set_buffer;
	gr_screen.gf_update_buffer_object		= gr_opengl_update_buffer_object;

	gr_screen.gf_update_transform_buffer	= gr_opengl_update_transform_buffer;
	gr_screen.gf_set_transform_buffer_offset	= gr_opengl_set_transform_buffer_offset;

	gr_screen.gf_create_stream_buffer		= gr_opengl_create_stream_buffer_object;
	gr_screen.gf_render_stream_buffer		= gr_opengl_render_stream_buffer;

	gr_screen.gf_start_instance_matrix			= gr_opengl_start_instance_matrix;
	gr_screen.gf_end_instance_matrix			= gr_opengl_end_instance_matrix;
	gr_screen.gf_start_angles_instance_matrix	= gr_opengl_start_instance_angles;

	gr_screen.gf_make_light			= gr_opengl_make_light;
	gr_screen.gf_modify_light		= gr_opengl_modify_light;
	gr_screen.gf_destroy_light		= gr_opengl_destroy_light;
	gr_screen.gf_set_light			= gr_opengl_set_light;
	gr_screen.gf_reset_lighting		= gr_opengl_reset_lighting;
	gr_screen.gf_set_ambient_light	= gr_opengl_set_ambient_light;

	gr_screen.gf_post_process_set_effect	= gr_opengl_post_process_set_effect;
	gr_screen.gf_post_process_set_defaults	= gr_opengl_post_process_set_defaults;

	gr_screen.gf_post_process_begin		= gr_opengl_post_process_begin;
	gr_screen.gf_post_process_end		= gr_opengl_post_process_end;
	gr_screen.gf_post_process_save_zbuffer	= gr_opengl_post_process_save_zbuffer;

	gr_screen.gf_scene_texture_begin = gr_opengl_scene_texture_begin;
	gr_screen.gf_scene_texture_end = gr_opengl_scene_texture_end;
	gr_screen.gf_copy_effect_texture = gr_opengl_copy_effect_texture;

	gr_screen.gf_deferred_lighting_begin = gr_opengl_deferred_lighting_begin;
	gr_screen.gf_deferred_lighting_end = gr_opengl_deferred_lighting_end;
	gr_screen.gf_deferred_lighting_finish = gr_opengl_deferred_lighting_finish;

	gr_screen.gf_start_clip_plane	= gr_opengl_start_clip_plane;
	gr_screen.gf_end_clip_plane		= gr_opengl_end_clip_plane;

	gr_screen.gf_lighting			= gr_opengl_set_lighting;
	gr_screen.gf_set_light_factor	= gr_opengl_set_light_factor;

	gr_screen.gf_set_proj_matrix	= gr_opengl_set_projection_matrix;
	gr_screen.gf_end_proj_matrix	= gr_opengl_end_projection_matrix;

	gr_screen.gf_set_view_matrix	= gr_opengl_set_view_matrix;
	gr_screen.gf_end_view_matrix	= gr_opengl_end_view_matrix;

	gr_screen.gf_push_scale_matrix	= gr_opengl_push_scale_matrix;
	gr_screen.gf_pop_scale_matrix	= gr_opengl_pop_scale_matrix;
	gr_screen.gf_center_alpha		= gr_opengl_center_alpha;
	gr_screen.gf_set_thrust_scale	= gr_opengl_set_thrust_scale;

	gr_screen.gf_draw_line_list		= gr_opengl_draw_line_list;

	gr_screen.gf_set_line_width		= gr_opengl_set_line_width;

	gr_screen.gf_line_htl			= gr_opengl_line_htl;
	gr_screen.gf_sphere_htl			= gr_opengl_sphere_htl;

	gr_screen.gf_set_animated_effect = gr_opengl_shader_set_animated_effect;

	gr_screen.gf_maybe_create_shader = gr_opengl_maybe_create_shader;
	gr_screen.gf_shadow_map_start	= gr_opengl_shadow_map_start;
	gr_screen.gf_shadow_map_end		= gr_opengl_shadow_map_end;

	gr_screen.gf_update_texture = gr_opengl_update_texture;
	gr_screen.gf_get_bitmap_from_texture = gr_opengl_get_bitmap_from_texture;

	gr_screen.gf_clear_states	= gr_opengl_clear_states;

	gr_screen.gf_set_team_color		= gr_opengl_set_team_color;
}


bool gr_opengl_init()
{
	const char *ver;
	int major = 0, minor = 0;

	if ( !GL_initted )
		atexit(opengl_close);

	if (GL_initted) {
		gr_opengl_cleanup(false);
		GL_initted = false;
	}

	mprintf(( "Initializing OpenGL graphics device at %ix%i with %i-bit color...\n",
		  gr_screen.max_w,
		  gr_screen.max_h,
		  gr_screen.bits_per_pixel ));

	if ( opengl_init_display_device() ) {
		Error(LOCATION, "Unable to initialize display device!\n");
	}

	// version check
	opengl_check_for_errors("before glGetString(GL_VERSION) call");
	ver = (const char *)glGetString(GL_VERSION);
	opengl_check_for_errors("after glGetString(GL_VERSION) call");
	Assertion(ver, "Failed to get glGetString(GL_VERSION)\n");
	sscanf(ver, "%d.%d", &major, &minor);

	GL_version = (major * 10) + minor;

	if (GL_version < MIN_REQUIRED_GL_VERSION) {
		Error(LOCATION, "Current GL Version of %d.%d is less than the "
				"required version of %d.%d.\n"
				"Switch video modes or update your drivers.",
				major,
				minor,
				(MIN_REQUIRED_GL_VERSION / 10),
				(MIN_REQUIRED_GL_VERSION % 10));
	}

	GL_initted = true;

	// this MUST be done before any other gr_opengl_* or
	// opengl_* function calls!!
	opengl_setup_function_pointers();

	mprintf(( "  OpenGL Vendor    : %s\n", glGetString(GL_VENDOR) ));
	mprintf(( "  OpenGL Renderer  : %s\n", glGetString(GL_RENDERER) ));
	mprintf(( "  OpenGL Version   : %s\n", ver ));
	mprintf(( "\n" ));

	if (Cmdline_fullscreen_window || Cmdline_window) {
		opengl_go_windowed();
	} else {
		opengl_go_fullscreen();
	}

	// initialize the extensions and make sure we aren't missing something
	// that we need
	opengl_extensions_init();

	// setup the lighting stuff that will get used later
	opengl_light_init();

	// init state system (must come AFTER light is set up)
	GL_state.init();

	GLint max_texture_units = GL_supported_texture_units;
	GLint max_texture_coords = GL_supported_texture_units;

	if (is_minimum_GLSL_version()) {
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
	}

	glGetIntegerv(GL_MAX_TEXTURE_COORDS, &max_texture_coords);

	GL_state.Texture.init(max_texture_units);
	GL_state.Array.init(max_texture_coords);

	opengl_set_texture_target();
	GL_state.Texture.SetActiveUnit(0);
	GL_state.Texture.SetTarget(GL_TEXTURE_2D);
	GL_state.Texture.Enable();

	// ready the texture system
	opengl_tcache_init();

	opengl_tnl_init();

	// setup default shaders, and shader related items
	opengl_shader_init();

	// post processing effects, after shaders are initialized
	opengl_setup_scene_textures();
	opengl_post_process_init();

	// must be called after extensions are setup
	opengl_set_vsync( !Cmdline_no_vsync );

	opengl_setup_viewport();

	glClear(GL_DEPTH_BUFFER_BIT);
	glClear(GL_STENCIL_BUFFER_BIT);

	glShadeModel(GL_SMOOTH);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_FOG_HINT, GL_NICEST);

	if ( Is_Extension_Enabled(OGL_ARB_SEAMLESS_CUBEMAP) ) {
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	glDepthRange(0.0, 1.0);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glFlush();

	Gr_current_red = &Gr_red;
	Gr_current_blue = &Gr_blue;
	Gr_current_green = &Gr_green;
	Gr_current_alpha = &Gr_alpha;

	gr_opengl_reset_clip();
	gr_opengl_clear();
	gr_opengl_flip();
	gr_opengl_clear();
	gr_opengl_flip();
	gr_opengl_clear();

	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &GL_max_elements_vertices);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &GL_max_elements_indices);

	mprintf(( "  Max texture units: %i (%i)\n", GL_supported_texture_units, max_texture_units ));
	mprintf(( "  Max client texture states: %i (%i)\n", GL_supported_texture_units, max_texture_coords ));
	mprintf(( "  Max elements vertices: %i\n", GL_max_elements_vertices ));
	mprintf(( "  Max elements indices: %i\n", GL_max_elements_indices ));
	mprintf(( "  Max texture size: %ix%i\n", GL_max_texture_width, GL_max_texture_height ));

	if ( Is_Extension_Enabled(OGL_EXT_FRAMEBUFFER_OBJECT) ) {
		mprintf(( "  Max render buffer size: %ix%i\n",
			  GL_max_renderbuffer_size,
			  GL_max_renderbuffer_size ));
	}

	mprintf(( "  Can use compressed textures: %s\n", Use_compressed_textures ? NOX("YES") : NOX("NO") ));
	mprintf(( "  Texture compression available: %s\n", Texture_compression_available ? NOX("YES") : NOX("NO") ));
	mprintf(( "  Post-processing enabled: %s\n", (Cmdline_postprocess) ? "YES" : "NO"));
	mprintf(( "  Using %s texture filter.\n", (GL_mipmap_filter) ? NOX("trilinear") : NOX("bilinear") ));

	if (is_minimum_GLSL_version()) {
		mprintf(( "  OpenGL Shader Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION) ));
	}

	// This stops fred crashing if no textures are set
	gr_screen.current_bitmap = -1;

	mprintf(("... OpenGL init is complete!\n"));

	if (Cmdline_ati_color_swap)
		GL_read_format = GL_RGBA;

	return true;
}

DCF(ogl_minimize, "Minimizes opengl")
{
	bool minimize_ogl = false;

	if ( gr_screen.mode != GR_OPENGL ) {
		dc_printf("Command only available in OpenGL mode.\n");
		return;
	}

	if (dc_optional_string_either("help", "--help")) {
		dc_printf("[bool] If true is passed, then the OpenGL window will minimize.\n");
		return;
	}
	dc_stuff_boolean(&minimize_ogl);

	if (minimize_ogl) {
		opengl_minimize();
	}
}

DCF(ogl_anisotropy, "toggles anisotropic filtering")
{
	bool process = true;
	int value;

	if ( gr_screen.mode != GR_OPENGL ) {
		dc_printf("Can only set anisotropic filter in OpenGL mode.\n");
		return;
	}

	if (dc_optional_string_either("help", "--help")) {
		dc_printf("Sets OpenGL anisotropic filtering level.\n");
		dc_printf("GL_anisotropy [int]  Valid values are 0 to %i. 0 turns off anisotropic filtering.\n", (int)opengl_get_max_anisotropy());
		process = false;
	}

	if (dc_optional_string_either("status", "--status") || dc_optional_string_either("?", "--?")) {
		dc_printf("Current anisotropic filter value is %i\n", (int)GL_anisotropy);
		process = false;
	}

	if (!process) {
		return;
	}

	if ( !Is_Extension_Enabled(OGL_EXT_TEXTURE_FILTER_ANISOTROPIC) ) {
		dc_printf("Error: Anisotropic filter is not settable!\n");
		return;
	}

	if (!dc_maybe_stuff_int(&value)) {
		// No arg passed, set to default
			GL_anisotropy = 1.0f;
		//	opengl_set_anisotropy();
			dc_printf("Anisotropic filter value reset to default level.\n");
	} else {
		GL_anisotropy = (GLfloat)value;
		//	opengl_set_anisotropy( (float)Dc_arg_float );
	}
}

/**
 * Helper function to enquire whether minimum GLSL version present.
 *
 * Compares global variable set by glGetString(GL_SHADING_LANGUAGE_VERSION)
 * against compile time MIN_REQUIRED_GLSL_VERSION.
 *
 * @return true if GLSL support present is above the minimum version.
 */
bool is_minimum_GLSL_version() {
	return GLSL_version >= MIN_REQUIRED_GLSL_VERSION ? true : false;
}
