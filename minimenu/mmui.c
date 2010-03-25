
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"
#include "SDL_thread.h"

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_apps.h"
#include "pnd_device.h"

#include "mmenu.h"
#include "mmcat.h"
#include "mmcache.h"
#include "mmui.h"
#include "mmwrapcmd.h"

#define CHANGED_NOTHING     (0)
#define CHANGED_CATEGORY    (1<<0)  /* changed to different category */
#define CHANGED_SELECTION   (1<<1)  /* changed app selection */
#define CHANGED_DATAUPDATE  (1<<2)  /* deferred preview pic or icon came in */
#define CHANGED_APPRELOAD   (1<<3)  /* new set of applications entirely */
#define CHANGED_EVERYTHING  (1<<4)  /* redraw it all! */
unsigned int render_mask = CHANGED_EVERYTHING;

/* SDL
 */
SDL_Surface *sdl_realscreen = NULL;
unsigned int sdl_ticks = 0;
SDL_Thread *g_preview_thread = NULL;

enum { sdl_user_ticker = 0,
       sdl_user_finishedpreview = 1,
       sdl_user_finishedicon = 2,
};

/* app state
 */
unsigned short int g_scale = 1; // 1 == noscale

SDL_Surface *g_imgcache [ IMG_MAX ];

TTF_Font *g_big_font = NULL;
TTF_Font *g_grid_font = NULL;
TTF_Font *g_detailtext_font = NULL;
TTF_Font *g_tab_font = NULL;

extern pnd_conf_handle g_conf;

/* current display state
 */
int ui_rows_scrolled_down = 0;          // number of rows that should be missing from top of the display
mm_appref_t *ui_selected = NULL;
unsigned char ui_category = 0;          // current category
unsigned char ui_catshift = 0;          // how many cats are offscreen to the left

extern mm_category_t g_categories [ MAX_CATS ];
extern unsigned char g_categorycount;

static SDL_Surface *ui_scale_image ( SDL_Surface *s, unsigned int maxwidth, int maxheight ); // height -1 means ignore
static int ui_selected_index ( void );

unsigned char ui_setup ( void ) {

  /* set up SDL
   */

  SDL_Init ( SDL_INIT_EVERYTHING | SDL_INIT_NOPARACHUTE );

  SDL_JoystickOpen ( 0 ); // turn on joy-0

  SDL_WM_SetCaption ( "mmenu", "mmenu" );

  // hide the mouse cursor if we can
  if ( SDL_ShowCursor ( -1 ) == 1 ) {
    SDL_ShowCursor ( 0 );
  }

  atexit ( SDL_Quit );

  // open up a surface
  unsigned int svm = SDL_SWSURFACE /*| SDL_FULLSCREEN*/ /* 0*/;
  if ( pnd_conf_get_as_int_d ( g_conf, "display.fullscreen", 0 ) > 0 ) {
    svm |= SDL_FULLSCREEN;
  }

  sdl_realscreen =
    SDL_SetVideoMode ( 800 * g_scale, 480 * g_scale, 16 /*bpp*/, svm );

  if ( ! sdl_realscreen ) {
    pnd_log ( pndn_error, "ERROR: Couldn't open SDL real screen; dieing." );
    return ( 0 );
  }

  pnd_log ( pndn_debug, "Pixel format:" );
  pnd_log ( pndn_debug, "bpp b: %u\n", sdl_realscreen -> format -> BitsPerPixel );
  pnd_log ( pndn_debug, "bpp B: %u\n", sdl_realscreen -> format -> BytesPerPixel );
  pnd_log ( pndn_debug, "R mask: %08x\n", sdl_realscreen -> format -> Rmask );
  pnd_log ( pndn_debug, "G mask: %08x\n", sdl_realscreen -> format -> Gmask );
  pnd_log ( pndn_debug, "B mask: %08x\n", sdl_realscreen -> format -> Bmask );

#if 0 // audio
  {
    SDL_AudioSpec fmt;

    /* Set 16-bit stereo audio at 22Khz */
    fmt.freq = 44100; //22050;
    fmt.format = AUDIO_S16; //AUDIO_S16;
    fmt.channels = 1;
    fmt.samples = 2048;        /* A good value for games */
    fmt.callback = mixaudio;
    fmt.userdata = NULL;

    /* Open the audio device and start playing sound! */
    if ( SDL_OpenAudio ( &fmt, NULL ) < 0 ) {
      zotlog ( "Unable to open audio: %s\n", SDL_GetError() );
      exit ( 1 );
    }

    SDL_PauseAudio ( 0 );
  }
#endif

  // images
  //IMG_Init ( IMG_INIT_JPG | IMG_INIT_PNG );

  /* fonts
   */

  // init
  if ( TTF_Init() == -1 ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up SDL TTF lib\n" );
    return ( 0 ); // couldn't set up SDL TTF
  }

  char fullpath [ PATH_MAX ];
  // big font
  sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, "minimenu.font" ) );
  g_big_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "minimenu.font_ptsize", 24 ) );
  if ( ! g_big_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "minimenu.font" ), pnd_conf_get_as_int_d ( g_conf, "minimenu.font_ptsize", 24 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // grid font
  sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, MMENU_GRID_FONT ) );
  g_grid_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, MMENU_GRID_FONTSIZE, 10 ) );
  if ( ! g_grid_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, MMENU_GRID_FONT ), pnd_conf_get_as_int_d ( g_conf, MMENU_GRID_FONTSIZE, 10 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // detailtext font
  sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, "detailtext.font" ) );
  g_detailtext_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "detailtext.font_ptsize", 10 ) );
  if ( ! g_detailtext_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "detailtext.font" ), pnd_conf_get_as_int_d ( g_conf, "detailtext.font_ptsize", 10 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // tab font
  sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, "tabs.font" ) );
  g_tab_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "tabs.font_ptsize", 10 ) );
  if ( ! g_tab_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "tabs.font" ), pnd_conf_get_as_int_d ( g_conf, "tabs.font_ptsize", 10 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  return ( 1 );
}

mm_imgcache_t g_imagecache [ IMG_TRUEMAX ] = {
  { IMG_BACKGROUND_800480,    "graphics.IMG_BACKGROUND_800480" },
  { IMG_BACKGROUND_TABMASK,   "graphics.IMG_BACKGROUND_TABMASK" },
  { IMG_DETAIL_PANEL,         "graphics.IMG_DETAIL_PANEL" },
  { IMG_DETAIL_BG,            "graphics.IMG_DETAIL_BG" },
  { IMG_SELECTED_ALPHAMASK,   "graphics.IMG_SELECTED_ALPHAMASK" },
  { IMG_TAB_SEL,              "graphics.IMG_TAB_SEL" },
  { IMG_TAB_UNSEL,            "graphics.IMG_TAB_UNSEL" },
  { IMG_TAB_LINE,             "graphics.IMG_TAB_LINE" },
  { IMG_TAB_LINEL,            "graphics.IMG_TAB_LINEL" },
  { IMG_TAB_LINER,            "graphics.IMG_TAB_LINER" },
  { IMG_ICON_MISSING,         "graphics.IMG_ICON_MISSING" },
  { IMG_SELECTED_HILITE,      "graphics.IMG_SELECTED_HILITE" },
  { IMG_PREVIEW_MISSING,      "graphics.IMG_PREVIEW_MISSING" },
  { IMG_ARROW_UP,             "graphics.IMG_ARROW_UP", },
  { IMG_ARROW_DOWN,           "graphics.IMG_ARROW_DOWN", },
  { IMG_ARROW_SCROLLBAR,      "graphics.IMG_ARROW_SCROLLBAR", },
  { IMG_HOURGLASS,            "graphics.IMG_HOURGLASS", },
  { IMG_MAX,                  NULL },
};

unsigned char ui_imagecache ( char *basepath ) {
  unsigned int i;
  char fullpath [ PATH_MAX ];

  // loaded

  for ( i = 0; i < IMG_MAX; i++ ) {

    if ( g_imagecache [ i ].id != i ) {
      pnd_log ( pndn_error, "ERROR: Internal table mismatch during caching [%u]\n", i );
      exit ( -1 );
    }

    char *filename = pnd_conf_get_as_char ( g_conf, g_imagecache [ i ].confname );

    if ( ! filename ) {
      pnd_log ( pndn_error, "ERROR: Missing filename in conf for key: %s\n", g_imagecache [ i ].confname );
      return ( 0 );
    }

    if ( filename [ 0 ] == '/' ) {
      strncpy ( fullpath, filename, PATH_MAX );
    } else {
      sprintf ( fullpath, "%s/%s", basepath, filename );
    }

    if ( ! ( g_imagecache [ i ].i = IMG_Load ( fullpath ) ) ) {
      pnd_log ( pndn_error, "ERROR: Couldn't load static cache image: %s\n", fullpath );
      return ( 0 );
    }

  } // for

  // generated
  //g_imagecache [ IMG_SELECTED_ALPHAMASK ].i = SDL_CreateRGBSurface ( SDL_SWSURFACE, 60, 60, 32, 0xFF0000, 0x00FF00, 0xFF, 0xFF000000 );
  //boxRGBA ( g_imagecache [ IMG_SELECTED_ALPHAMASK ].i, 0, 0, 60, 60, 100, 100, 100, 250 );

  // post processing
  //

  // scale icons
  g_imagecache [ IMG_SELECTED_ALPHAMASK ].i = ui_scale_image ( g_imagecache [ IMG_SELECTED_ALPHAMASK ].i, pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 ), -1 );
  g_imagecache [ IMG_ICON_MISSING ].i = ui_scale_image ( g_imagecache [ IMG_ICON_MISSING ].i, pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 ), -1 );
  // scale text hilight
  g_imagecache [ IMG_SELECTED_HILITE ].i = ui_scale_image ( g_imagecache [ IMG_SELECTED_HILITE ].i, pnd_conf_get_as_int_d ( g_conf, "grid.text_width", 50 ), -1 );
  // scale preview no-pic
  g_imagecache [ IMG_PREVIEW_MISSING ].i = ui_scale_image ( g_imagecache [ IMG_PREVIEW_MISSING ].i,
							    pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 50 ),
							    pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_height", 50 ) );

  // set alpha on detail panel
  //SDL_SetAlpha ( g_imagecache [ IMG_DETAIL_BG ].i, SDL_SRCALPHA, pnd_conf_get_as_int_d ( g_conf, "display.detail_bg_alpha", 50 ) );

  return ( 1 );
} // ui_imagecache

void ui_render ( void ) {

  // 800x480:
  // divide width: 550 / 250
  // divide left side: 5 columns == 110px width
  //   20px buffer either side == 70px wide icon + 20 + 20?

  // what jobs to do during render?
  //

#define R_BG     (1<<0)
#define R_TABS   (1<<1)
#define R_DETAIL (1<<2)
#define R_GRID   (1<<3)

#define R_ALL (R_BG|R_TABS|R_DETAIL|R_GRID)

  unsigned int render_jobs_b = 0;

  if ( render_mask & CHANGED_EVERYTHING ) {
    render_jobs_b |= R_ALL;
  }

  if ( render_mask & CHANGED_SELECTION ) {
    render_jobs_b |= R_GRID;
    render_jobs_b |= R_DETAIL;
  }

  if ( render_mask & CHANGED_CATEGORY ) {
    render_jobs_b |= R_ALL;
  }

  render_mask = CHANGED_NOTHING;

  // render everything
  //
  unsigned int icon_rows;

#define MAXRECTS 200
  SDL_Rect rects [ MAXRECTS ], src;
  SDL_Rect *dest = rects;
  bzero ( dest, sizeof(SDL_Rect)*MAXRECTS );

  unsigned int row, displayrow, col;
  mm_appref_t *appiter;

  unsigned int screen_width = pnd_conf_get_as_int_d ( g_conf, "display.screen_width", 800 );

  unsigned char row_max = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_ROWMAX, 4 );
  unsigned char col_max = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_COLMAX, 5 );

  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  unsigned int grid_offset_x = pnd_conf_get_as_int ( g_conf, "grid.grid_offset_x" );
  unsigned int grid_offset_y = pnd_conf_get_as_int ( g_conf, "grid.grid_offset_y" );

  unsigned int icon_offset_x = pnd_conf_get_as_int ( g_conf, "grid.icon_offset_x" );
  unsigned int icon_offset_y = pnd_conf_get_as_int ( g_conf, "grid.icon_offset_y" );
  unsigned int icon_max_width = pnd_conf_get_as_int ( g_conf, "grid.icon_max_width" );
  unsigned int icon_max_height = pnd_conf_get_as_int ( g_conf, "grid.icon_max_height" );

  unsigned int text_width = pnd_conf_get_as_int ( g_conf, "grid.text_width" );
  unsigned int text_clip_x = pnd_conf_get_as_int ( g_conf, "grid.text_clip_x" );
  unsigned int text_offset_x = pnd_conf_get_as_int ( g_conf, "grid.text_offset_x" );
  unsigned int text_offset_y = pnd_conf_get_as_int ( g_conf, "grid.text_offset_y" );

  unsigned int cell_width = pnd_conf_get_as_int ( g_conf, "grid.cell_width" );
  unsigned int cell_height = pnd_conf_get_as_int ( g_conf, "grid.cell_height" );

  // how many total rows do we need?
  icon_rows = g_categories [ ui_category ].refcount / col_max;
  if ( g_categories [ ui_category ].refcount % col_max > 0 ) {
    icon_rows++;
  }

  // if no selected app yet, select the first one
#if 0
  if ( ! ui_selected ) {
    ui_selected = g_categories [ ui_category ].refs;
  }
#endif

  // reset touchscreen regions
  ui_register_reset();

  // ensure selection is visible
  if ( ui_selected ) {

    int index = ui_selected_index();
    int topleft = col_max * ui_rows_scrolled_down;
    int botright = ( col_max * ( ui_rows_scrolled_down + row_max ) - 1 );

    //pnd_log ( PND_LOG_DEFAULT, "index %u tl %u br %u\n", index, topleft, botright );

    if ( index < topleft ) {
      ui_rows_scrolled_down -= pnd_conf_get_as_int_d ( g_conf, "grid.scroll_increment", 1 );
      render_jobs_b |= R_ALL;
    } else if ( index > botright ) {
      ui_rows_scrolled_down += pnd_conf_get_as_int_d ( g_conf, "grid.scroll_increment", 1 );
      render_jobs_b |= R_ALL;
    }

    if ( ui_rows_scrolled_down < 0 ) {
      ui_rows_scrolled_down = 0;
    } else if ( ui_rows_scrolled_down > icon_rows ) {
      ui_rows_scrolled_down = icon_rows;
    }

  } // ensire visible

  // render background
  if ( render_jobs_b & R_BG ) {

    if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
      dest -> x = 0;
      dest -> y = 0;
      dest -> w = sdl_realscreen -> w;
      dest -> h = sdl_realscreen -> h;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, NULL /* whole image */, sdl_realscreen, dest /* 0,0 */ );
      dest++;
    }

    // tabmask
    if ( g_imagecache [ IMG_BACKGROUND_TABMASK ].i ) {
      dest -> x = 0;
      dest -> y = 0;
      dest -> w = sdl_realscreen -> w;
      dest -> h = sdl_realscreen -> h;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_TABMASK ].i, NULL /* whole image */, sdl_realscreen, dest /* 0,0 */ );
      dest++;
    }

  } // r_bg

  // tabs
  if ( g_imagecache [ IMG_TAB_SEL ].i && g_imagecache [ IMG_TAB_UNSEL ].i ) {
    unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );
    unsigned int tab_height = pnd_conf_get_as_int ( g_conf, "tabs.tab_height" );
    unsigned int tab_selheight = pnd_conf_get_as_int ( g_conf, "tabs.tab_selheight" );
    unsigned int tab_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_x" );
    unsigned int tab_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_y" );
    unsigned int text_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_x" );
    unsigned int text_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_y" );
    unsigned int text_width = pnd_conf_get_as_int ( g_conf, "tabs.text_width" );
    unsigned int maxtab = ( screen_width / tab_width ) < g_categorycount ? ( screen_width / tab_width ) + ui_catshift : g_categorycount + ui_catshift;
    unsigned int maxtabspot = ( screen_width / tab_width );

    // draw tabs with categories
    for ( col = ui_catshift;
	  col < maxtab;
	  col++ )
    {

      SDL_Surface *s;
      if ( col == ui_category ) {
	s = g_imagecache [ IMG_TAB_SEL ].i;
      } else {
	s = g_imagecache [ IMG_TAB_UNSEL ].i;
      }

      // draw tab
      src.x = 0;
      src.y = 0;
      src.w = tab_width;
      if ( col == ui_category ) {
	src.h = tab_selheight;
      } else {
	src.h = tab_height;
      }
      dest -> x = tab_offset_x + ( (col-ui_catshift) * tab_width );
      dest -> y = tab_offset_y;

      // store touch info
      ui_register_tab ( col, dest -> x, dest -> y, tab_width, tab_height );

      if ( render_jobs_b & R_TABS ) {
	//pnd_log ( pndn_debug, "tab %u at %ux%u\n", col, dest.x, dest.y );
	SDL_BlitSurface ( s, &src, sdl_realscreen, dest );
	dest++;

	// draw tab line
	if ( col == ui_category ) {
	  // no line for selected tab
	} else {
	  if ( col - ui_catshift == 0 ) {
	    s = g_imagecache [ IMG_TAB_LINEL ].i;
	  } else if ( col - ui_catshift == maxtabspot - 1 ) {
	    s = g_imagecache [ IMG_TAB_LINER ].i;
	  } else {
	    s = g_imagecache [ IMG_TAB_LINE ].i;
	  }
	  dest -> x = tab_offset_x + ( (col-ui_catshift) * tab_width );
	  dest -> y = tab_offset_y + tab_height;
	  SDL_BlitSurface ( s, NULL /* whole image */, sdl_realscreen, dest );
	  dest++;
	}

	// draw text
	SDL_Surface *rtext;
	SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
	rtext = TTF_RenderText_Blended ( g_tab_font, g_categories [ col ].catname, tmpfontcolor );
	src.x = 0;
	src.y = 0;
	src.w = rtext -> w < text_width ? rtext -> w : text_width;
	src.h = rtext -> h;
	dest -> x = tab_offset_x + ( (col-ui_catshift) * tab_width ) + text_offset_x;
	dest -> y = tab_offset_y + text_offset_y;
	SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
	SDL_FreeSurface ( rtext );
	dest++;

      } // r_tabs

    } // for

    if ( render_jobs_b & R_TABS ) {

      // draw tab lines under where tabs would be if we had categories
      for ( /* foo */; col < maxtabspot; col++ ) {
	SDL_Surface *s;

	if ( col - ui_catshift == 0 ) {
	  s = g_imagecache [ IMG_TAB_LINEL ].i;
	} else if ( col - ui_catshift == maxtabspot - 1 ) {
	  s = g_imagecache [ IMG_TAB_LINER ].i;
	} else {
	  s = g_imagecache [ IMG_TAB_LINE ].i;
	}
	dest -> x = tab_offset_x + ( (col-ui_catshift) * tab_width );
	dest -> y = tab_offset_y + tab_height;
	SDL_BlitSurface ( s, NULL /* whole image */, sdl_realscreen, dest );
	dest++;

      } // for

    } // r_tabs

  } // tabs

  // scroll bars and arrows
  if ( render_jobs_b & R_BG ) {
    unsigned char show_bar = 0;

    // up?
    if ( ui_rows_scrolled_down && g_imagecache [ IMG_ARROW_UP ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_x", 450 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_y", 80 );
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_UP ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;

      show_bar = 1;
    }

    // down?
    if ( ui_rows_scrolled_down + row_max < icon_rows && g_imagecache [ IMG_ARROW_DOWN ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_x", 450 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_y", 80 );
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_DOWN ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;

      show_bar = 1;
    }

    if ( show_bar ) {
      // show scrollbar as well
      src.x = 0;
      src.y = 0;
      src.w = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_clip_w", 10 );
      src.h = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_clip_h", 100 );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_x", 450 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_y", 100 );
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_SCROLLBAR ].i, &src /* whole image */, sdl_realscreen, dest );
      dest++;
    } // bar

  } // r_bg, scroll bars

  // render detail pane bg
  if ( render_jobs_b & R_DETAIL ) {

    if ( pnd_conf_get_as_int_d ( g_conf, "detailpane.show", 1 ) ) {

      // render detail bg
      if ( g_imagecache [ IMG_DETAIL_BG ].i ) {
	src.x = 0; // pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
	src.y = 0; // pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
	src.w = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> w;
	src.h = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> h;
	dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
	dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
	SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
	dest++;
      }

      // render detail pane
      if ( g_imagecache [ IMG_DETAIL_PANEL ].i ) {
	dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
	dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
	SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_PANEL ].i, NULL /* whole image */, sdl_realscreen, dest );
	dest++;
      }

    } // detailpane frame/bg

  } // r_details

  // anything to render?
  if ( render_jobs_b & R_GRID ) {

    // if just rendering grid, and nothing else, better clear it first
    if ( ! ( render_jobs_b & R_BG ) ) {
      if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
	src.x = grid_offset_x;
	src.y = grid_offset_y;
	src.w = col_max * cell_width;
	src.h = row_max * cell_height;

	dest -> x = grid_offset_x;
	dest -> y = grid_offset_y;

	SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, &src, sdl_realscreen, dest );
	dest++;

      }
    }

    if ( g_categories [ ui_category ].refs ) {

      appiter = g_categories [ ui_category ].refs;
      row = 0;
      displayrow = 0;

      // until we run out of apps, or run out of space
      while ( appiter != NULL ) {

	for ( col = 0; col < col_max && appiter != NULL; col++ ) {

	  // do we even need to render it? or are we suppressing it due to rows scrolled off the top?
	  if ( row >= ui_rows_scrolled_down ) {

	    // selected? show hilights
	    if ( appiter == ui_selected ) {
	      // icon
	      dest -> x = grid_offset_x + ( col * cell_width ) + icon_offset_x;
	      dest -> y = grid_offset_y + ( displayrow * cell_height ) + icon_offset_y;
	      SDL_BlitSurface ( g_imagecache [ IMG_SELECTED_ALPHAMASK ].i, NULL /* all */, sdl_realscreen, dest );
	      dest++;
	      // text
	      dest -> x = grid_offset_x + ( col * cell_width ) + text_clip_x;
	      dest -> y = grid_offset_y + ( displayrow * cell_height ) + pnd_conf_get_as_int ( g_conf, "grid.text_hilite_offset_y" );
	      SDL_BlitSurface ( g_imagecache [ IMG_SELECTED_HILITE ].i, NULL /* all */, sdl_realscreen, dest );
	      dest++;
	    } // selected?

	    // show icon
	    mm_cache_t *ic = cache_query_icon ( appiter -> ref -> unique_id );
	    SDL_Surface *iconsurface;
	    if ( ic ) {
	      iconsurface = ic -> i;
	    } else {
	      //pnd_log ( pndn_warning, "WARNING: TBD: Need Missin-icon icon for '%s'\n", IFNULL(appiter -> ref -> title_en,"No Name") );
	      iconsurface = g_imagecache [ IMG_ICON_MISSING ].i;
	    }
	    if ( iconsurface ) {
	      //pnd_log ( pndn_debug, "Got an icon for '%s'\n", IFNULL(appiter -> ref -> title_en,"No Name") );

	      src.x = 0;
	      src.y = 0;
	      src.w = 60;
	      src.h = 60;
	      dest -> x = grid_offset_x + ( col * cell_width ) + icon_offset_x + (( icon_max_width - iconsurface -> w ) / 2);
	      dest -> y = grid_offset_y + ( displayrow * cell_height ) + icon_offset_y + (( icon_max_height - iconsurface -> h ) / 2);

	      SDL_BlitSurface ( iconsurface, &src, sdl_realscreen, dest );

	      // store touch info
	      ui_register_app ( appiter, dest -> x, dest -> y, src.w, src.h );

	      dest++;

	    }

	    // show text
	    if ( appiter -> ref -> title_en ) {
	      SDL_Surface *rtext;
	      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
	      rtext = TTF_RenderText_Blended ( g_grid_font, appiter -> ref -> title_en, tmpfontcolor );
	      src.x = 0;
	      src.y = 0;
	      src.w = text_width < rtext -> w ? text_width : rtext -> w;
	      src.h = rtext -> h;
	      if ( rtext -> w > text_width ) {
		dest -> x = grid_offset_x + ( col * cell_width ) + text_clip_x;
	      } else {
		dest -> x = grid_offset_x + ( col * cell_width ) + text_offset_x - ( rtext -> w / 2 );
	      }
	      dest -> y = grid_offset_y + ( displayrow * cell_height ) + text_offset_y;
	      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
	      SDL_FreeSurface ( rtext );
	      dest++;
	    }

	  } // display now? or scrolled away..

	  // next
	  appiter = appiter -> next;

	} // for column 1...X

	if ( row >= ui_rows_scrolled_down ) {
	  displayrow++;
	}

	row ++;

	// are we done displaying rows?
	if ( displayrow >= row_max ) {
	  break;
	}

      } // while

    } else {
      // no apps to render?
      pnd_log ( pndn_rem, "No applications to render?\n" );
    } // apps to renser?

  } // r_grid

  // detail panel
  if ( ui_selected && render_jobs_b & R_DETAIL ) {

    unsigned int cell_offset_x = pnd_conf_get_as_int ( g_conf, "detailtext.cell_offset_x" );
    unsigned int cell_offset_y = pnd_conf_get_as_int ( g_conf, "detailtext.cell_offset_y" );
    unsigned int cell_width = pnd_conf_get_as_int ( g_conf, "detailtext.cell_width" );

    unsigned int desty = cell_offset_y;

    char buffer [ 256 ];

    // full name
    if ( ui_selected -> ref -> title_en ) {
      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_detailtext_font, ui_selected -> ref -> title_en, tmpfontcolor );
      src.x = 0;
      src.y = 0;
      src.w = rtext -> w < cell_width ? rtext -> w : cell_width;
      src.h = rtext -> h;
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;
    }

    // category
    if ( ui_selected -> ref -> main_category ) {

      sprintf ( buffer, "Category: %s", ui_selected -> ref -> main_category );

      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, tmpfontcolor );
      src.x = 0;
      src.y = 0;
      src.w = rtext -> w < cell_width ? rtext -> w : cell_width;
      src.h = rtext -> h;
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;
    }

    // clock
    if ( ui_selected -> ref -> clockspeed ) {

      sprintf ( buffer, "CPU Clock: %s", ui_selected -> ref -> clockspeed );

      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, tmpfontcolor );
      src.x = 0;
      src.y = 0;
      src.w = rtext -> w < cell_width ? rtext -> w : cell_width;
      src.h = rtext -> h;
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;
    }

    // show sub-app# on right side of cpu clock?
    //if ( ui_selected -> ref -> subapp_number )
    {
      sprintf ( buffer, "(app#%u)", ui_selected -> ref -> subapp_number );

      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_grid_font, buffer, tmpfontcolor );
      dest -> x = cell_offset_x + cell_width - rtext -> w;
      dest -> y = desty - src.h;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // info hint
    if ( ui_selected -> ref -> clockspeed && ui_selected -> ref -> info_filename ) {

      sprintf ( buffer, "Documentation - hit Y" );

      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, tmpfontcolor );
      src.x = 0;
      src.y = 0;
      src.w = rtext -> w < cell_width ? rtext -> w : cell_width;
      src.h = rtext -> h;
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;
    }

    // preview pic
    mm_cache_t *ic = cache_query_preview ( ui_selected -> ref -> unique_id );
    SDL_Surface *previewpic;

    if ( ic ) {
      previewpic = ic -> i;
    } else {
      previewpic = g_imagecache [ IMG_PREVIEW_MISSING ].i;
    }

    if ( previewpic ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_offset_x", 50 ) +
	( ( pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 50 ) - previewpic -> w ) / 2 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_offset_y", 50 );
      SDL_BlitSurface ( previewpic, NULL /* whole image */, sdl_realscreen, dest );
      dest++;
    }

  } // selected?

  // extras
  //

  // battery
  if ( 1 ) {
    static int last_battlevel = 0;
    static unsigned char batterylevel = 0;
    char buffer [ 100 ];

    if ( time ( NULL ) - last_battlevel > 60 ) {
      batterylevel = pnd_device_get_battery_gauge_perc();
      last_battlevel = time ( NULL );
    }

    sprintf ( buffer, "Battery: %u%%", batterylevel );

    SDL_Surface *rtext;
    SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, tmpfontcolor );
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "display.battery_x", 20 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "display.battery_y", 450 );
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // hints
  if ( pnd_conf_get_as_char ( g_conf, "display.hintline" ) ) {
    char *buffer = pnd_conf_get_as_char ( g_conf, "display.hintline" );
    SDL_Surface *rtext;
    SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, tmpfontcolor );
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "display.hint_x", 40 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "display.hint_y", 450 );
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // hints
  if ( pnd_conf_get_as_int_d ( g_conf, "display.clock_x", -1 ) != -1 ) {
    char buffer [ 50 ];

    time_t t = time ( NULL );
    struct tm *tm = localtime ( &t );
    strftime ( buffer, 50, "%a %H:%M %F", tm );

    SDL_Surface *rtext;
    SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, tmpfontcolor );
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "display.clock_x", 700 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "display.clock_y", 450 );
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // update all the rects and send it all to sdl
  SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );

} // ui_render

void ui_process_input ( unsigned char block_p ) {
  SDL_Event event;

  unsigned char ui_event = 0; // if we get a ui event, flip to 1 and break
  //static ui_sdl_button_e ui_mask = uisb_none; // current buttons down

  while ( ! ui_event &&
	  block_p ? SDL_WaitEvent ( &event ) : SDL_PollEvent ( &event ) )
  {

    switch ( event.type ) {

    case SDL_USEREVENT:
      // update something

      if ( event.user.code == sdl_user_ticker ) {

	// timer went off, time to load something
	if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.load_previews_later", 0 ) ) {

	  // load the preview pics now!
	  pnd_disco_t *iter = ui_selected -> ref;

	  if ( iter -> preview_pic1 ) {

	    if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.threaded_preview", 0 ) ) {
	      // load in bg thread, make user experience chuggy

	      g_preview_thread = SDL_CreateThread ( (void*)ui_threaded_defered_preview, iter );

	      if ( ! g_preview_thread ) {
		pnd_log ( pndn_error, "ERROR: Couldn't create preview thread\n" );
	      }

	    } else {
	      // load it now, make user wait

	      if ( ! cache_preview ( iter, pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 200 ),
				     pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_height", 180 ) )
		 )
	      {
		pnd_log ( pndn_debug, "  Couldn't load preview pic: '%s' -> '%s'\n",
			  IFNULL(iter->title_en,"No Name"), iter -> preview_pic1 );
	      }

	    } // threaded?

	  } // got a preview at all?

	  ui_event++;
	}

      } else if ( event.user.code == sdl_user_finishedpreview ) {

	// if we just finished the one we happen to be looking at, better redraw now; otherwise, if
	// we finished another, no big woop
	if ( ui_selected && event.user.data1 == ui_selected -> ref ) {
	  ui_event++;
	}

      } else if ( event.user.code == sdl_user_finishedicon ) {
	// redraw, so we can show the newly loaded icon
	ui_event++;

      }

      render_mask |= CHANGED_EVERYTHING;

      break;

#if 0 // joystick motion
    case SDL_JOYAXISMOTION:

      pnd_log ( PND_LOG_DEFAULT, "joystick axis\n" );

	if ( event.jaxis.axis == 0 ) {
	  // horiz
	  if ( event.jaxis.value < 0 ) {
	    ui_push_left ( 0 );
	    pnd_log ( PND_LOG_DEFAULT, "joystick axis - LEFT\n" );
	  } else if ( event.jaxis.value > 0 ) {
	    ui_push_right ( 0 );
	    pnd_log ( PND_LOG_DEFAULT, "joystick axis - RIGHT\n" );
	  }
	} else if ( event.jaxis.axis == 1 ) {
	  // vert
	  if ( event.jaxis.value < 0 ) {
	    ui_push_up();
	  } else if ( event.jaxis.value > 0 ) {
	    ui_push_down();
	  }
	}

	ui_event++;

	break;
#endif

#if 0 // joystick buttons
    case SDL_JOYBUTTONDOWN:

      pnd_log ( PND_LOG_DEFAULT, "joystick button down %u\n", event.jbutton.button );

      if ( event.jbutton.button == 0 ) { // B
	ui_mask |= uisb_b;
      } else if ( event.jbutton.button == 1 ) { // Y
	ui_mask |= uisb_y;
      } else if ( event.jbutton.button == 2 ) { // X
	ui_mask |= uisb_x;
      } else if ( event.jbutton.button == 3 ) { // A
	ui_mask |= uisb_a;

      } else if ( event.jbutton.button == 4 ) { // Select
	ui_mask |= uisb_select;
      } else if ( event.jbutton.button == 5 ) { // Start
	ui_mask |= uisb_start;

      } else if ( event.jbutton.button == 7 ) { // L
	ui_mask |= uisb_l;
      } else if ( event.jbutton.button == 8 ) { // R
	ui_mask |= uisb_r;

      }

      ui_event++;

      break;

    case SDL_JOYBUTTONUP:

      pnd_log ( PND_LOG_DEFAULT, "joystick button up %u\n", event.jbutton.button );

      if ( event.jbutton.button == 0 ) { // B
	ui_mask &= ~uisb_b;
	ui_push_exec();
      } else if ( event.jbutton.button == 1 ) { // Y
	ui_mask &= ~uisb_y;
      } else if ( event.jbutton.button == 2 ) { // X
	ui_mask &= ~uisb_x;
      } else if ( event.jbutton.button == 3 ) { // A
	ui_mask &= ~uisb_a;

      } else if ( event.jbutton.button == 4 ) { // Select
	ui_mask &= ~uisb_select;
      } else if ( event.jbutton.button == 5 ) { // Start
	ui_mask &= ~uisb_start;

      } else if ( event.jbutton.button == 7 ) { // L
	ui_mask &= ~uisb_l;
	ui_push_ltrigger();
      } else if ( event.jbutton.button == 8 ) { // R
	ui_mask &= ~uisb_r;
	ui_push_rtrigger();

      }

      ui_event++;

      break;
#endif

#if 1 // keyboard events
    case SDL_KEYUP:

      //pnd_log ( pndn_debug, "key up %u\n", event.key.keysym.sym );

      // SDLK_LALT -> Start

      // directional
      if ( event.key.keysym.sym == SDLK_RIGHT ) {
	ui_push_right ( 0 );
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_LEFT ) {
	ui_push_left ( 0 );
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_UP ) {
	ui_push_up();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_DOWN ) {
	ui_push_down();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_END ) {
	ui_push_exec();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_z || event.key.keysym.sym == SDLK_RSHIFT ) {
	ui_push_ltrigger();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_x || event.key.keysym.sym == SDLK_RCTRL ) {
	ui_push_rtrigger();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_y || event.key.keysym.sym == SDLK_PAGEUP ) {
	// info
	if ( ui_selected ) {
	  ui_show_info ( pnd_run_script, ui_selected -> ref );
	  ui_event++;
	}

      } else if ( event.key.keysym.sym == SDLK_LALT ) { // start button
	ui_push_exec();
	ui_event++;

      } else if ( event.key.keysym.sym == SDLK_LCTRL /*LALT*/ ) { // select button
	char *opts [ 20 ] = {
	  "Return to Minimenu",
	  "Shutdown Pandora",
	  "Rescan for Applications",
	  "Cache previews to SD now",
	  "Run xfce4 from Minimenu",
	  "Run a terminal/console",
	  "Exit and run xfce4",
	  "Exit and run pmenu",
	  "Quit (<- beware)",
	  "About Minimenu"
	};
	int sel = ui_modal_single_menu ( opts, 9, "Minimenu", "Enter to select; other to return." );

	char buffer [ 100 ];
	if ( sel == 0 ) {
	  // do nothing
	} else if ( sel == 1 ) {
	  // shutdown
	  sprintf ( buffer, "sudo poweroff" );
	  system ( buffer );
	} else if ( sel == 2 ) {
	  // rescan apps
	  pnd_log ( pndn_debug, "Freeing up applications\n" );
	  applications_free();
	  pnd_log ( pndn_debug, "Rescanning applications\n" );
	  applications_scan();
	  // reset view
	  ui_selected = NULL;
	  ui_rows_scrolled_down = 0;
	} else if ( sel == 3 ) {
	  // cache preview to SD now
	  extern pnd_box_handle g_active_apps;
	  pnd_box_handle h = g_active_apps;

	  unsigned int maxwidth, maxheight;
	  maxwidth = pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 200 );
	  maxheight = pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_height", 180 );

	  pnd_disco_t *iter = pnd_box_get_head ( h );

	  while ( iter ) {

	    // cache it
	    if ( ! cache_preview ( iter, maxwidth, maxheight ) ) {
	      pnd_log ( pndn_debug, "Force cache: Couldn't load preview pic: '%s' -> '%s'\n",
			IFNULL(iter->title_en,"No Name"), iter -> preview_pic1 );
	    }

	    // next
	    iter = pnd_box_get_next ( iter );
	  } // while

	} else if ( sel == 4 ) {
	  // run xfce
	  char buffer [ PATH_MAX ];
	  sprintf ( buffer, "%s %s\n", MM_RUN, "/usr/bin/startxfce4" );
	  emit_and_quit ( buffer );
	} else if ( sel == 5 ) {
	  // run terminal
	  char *argv[5];
	  argv [ 0 ] = pnd_conf_get_as_char ( g_conf, "utility.terminal" );
	  argv [ 1 ] = NULL;

	  if ( argv [ 0 ] ) {
	    ui_forkexec ( argv );
	  }

	} else if ( sel == 6 ) {
	  // set env to xfce
	  sprintf ( buffer, "echo startxfce4 > /tmp/gui.load" );
	  system ( buffer );
	  //sprintf ( buffer, "sudo poweroff" );
	  //system ( buffer );
	  exit ( 0 );
	} else if ( sel == 7 ) {
	  // set env to pmenu
	  sprintf ( buffer, "echo pmenu > /tmp/gui.load" );
	  system ( buffer );
	  //sprintf ( buffer, "sudo poweroff" );
	  //system ( buffer );
	  exit ( 0 );
	} else if ( sel == 8 ) {
	  emit_and_quit ( MM_QUIT );
	} else if ( sel == 9 ) {
	  // about
	}

	ui_event++;
	render_mask |= CHANGED_EVERYTHING;
      }

      // extras
      if ( event.key.keysym.sym == SDLK_q ) {
	emit_and_quit ( MM_QUIT );
      }

      break;
#endif

#if 1 // mouse / touchscreen
#if 0
    case SDL_MOUSEBUTTONDOWN:
      if ( event.button.button == SDL_BUTTON_LEFT ) {
	cb_pointer_press ( gc, event.button.x / g_scale, event.button.y / g_scale );
	ui_event++;
      }
      break;
#endif

    case SDL_MOUSEBUTTONUP:
      if ( event.button.button == SDL_BUTTON_LEFT ) {
	ui_touch_act ( event.button.x, event.button.y );
	ui_event++;
      }
      break;
#endif

    case SDL_QUIT:
      exit ( 0 );
      break;

    default:
      break;

    } // switch event type

  } // while poll

  return;
}

void ui_push_left ( unsigned char forcecoil ) {

  if ( ! ui_selected ) {
    ui_push_right ( 0 );
    return;
  }

  // what column we in?
  unsigned int col = ui_determine_screen_col ( ui_selected );

  // are we alreadt at first item?
  if ( forcecoil == 0 &&
       pnd_conf_get_as_int_d ( g_conf, "grid.wrap_horiz_samerow", 0 ) &&
       col == 0 )
  {
    unsigned int i = pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 ) - 1;
    while ( i ) {
      ui_push_right ( 0 );
      i--;
    }

  } else if ( g_categories [ ui_category ].refs == ui_selected ) {
    // can't go any more left, we're at the head

  } else {
    // figure out the previous item; yay for singly linked list :/
    mm_appref_t *i = g_categories [ ui_category ].refs;
    while ( i ) {
      if ( i -> next == ui_selected ) {
	ui_selected = i;
	break;
      }
      i = i -> next;
    }
  }

  ui_set_selected ( ui_selected );

  return;
}

void ui_push_right ( unsigned char forcecoil ) {

  if ( ui_selected ) {

    // what column we in?
    unsigned int col = ui_determine_screen_col ( ui_selected );

    // wrap same or no?
    if ( forcecoil == 0 &&
	 pnd_conf_get_as_int_d ( g_conf, "grid.wrap_horiz_samerow", 0 ) &&
	 col == pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 ) - 1 )
    {
      // same wrap
      unsigned int i = pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 ) - 1;
      while ( i ) {
	ui_push_left ( 0 );
	i--;
      }

    } else {
      // just go to the next

      if ( ui_selected -> next ) {
	ui_selected = ui_selected -> next;
      }

    }

  } else {
    ui_selected = g_categories [ ui_category ].refs;
  }

  ui_set_selected ( ui_selected );

  return;
}

void ui_push_up ( void ) {
  unsigned char col_max = pnd_conf_get_as_int ( g_conf, MMENU_DISP_COLMAX );

  if ( ! ui_selected ) {
    return;
  }

  // what row we in?
  unsigned int row = ui_determine_row ( ui_selected );

  if ( row == 0 &&
       pnd_conf_get_as_int_d ( g_conf, "grid.wrap_vert_stop", 1 ) == 0 )
  {
    // wrap around instead

    unsigned int col = ui_determine_screen_col ( ui_selected );

    // go to end
    ui_selected = g_categories [ ui_category ].refs;
    while ( ui_selected -> next ) {
      ui_selected = ui_selected -> next;
    }

    // try to move to same column
    unsigned int newcol = ui_determine_screen_col ( ui_selected );
    if ( newcol > col ) {
      col = newcol - col;
      while ( col ) {
	ui_push_left ( 0 );
	col--;
      }
    }

    // scroll down to show it
    int r = ui_determine_row ( ui_selected ) - 1;
    if ( r - pnd_conf_get_as_int ( g_conf, MMENU_DISP_ROWMAX ) > 0 ) {
      ui_rows_scrolled_down = (unsigned int) r;
    }

  } else {
    // stop at top/bottom

    while ( col_max ) {
      ui_push_left ( 1 );
      col_max--;
    }

  }

  return;
}

void ui_push_down ( void ) {
  unsigned char col_max = pnd_conf_get_as_int ( g_conf, MMENU_DISP_COLMAX );

  if ( ui_selected ) {

    // what row we in?
    unsigned int row = ui_determine_row ( ui_selected );

    // max rows?
    unsigned int icon_rows = g_categories [ ui_category ].refcount / col_max;
    if ( g_categories [ ui_category ].refcount % col_max > 0 ) {
      icon_rows++;
    }

    // we at the end?
    if ( row == ( icon_rows - 1 ) &&
	 pnd_conf_get_as_int_d ( g_conf, "grid.wrap_vert_stop", 1 ) == 0 )
    {

      unsigned char col = ui_determine_screen_col ( ui_selected );

      ui_selected = g_categories [ ui_category ].refs;

      while ( col ) {
	ui_selected = ui_selected -> next;
	col--;
      }

      ui_rows_scrolled_down = 0;

    } else {

      while ( col_max ) {
	ui_push_right ( 1 );
	col_max--;
      }

    }

  } else {
    ui_push_right ( 0 );
  }

  return;
}

void ui_push_exec ( void ) {

  if ( ui_selected ) {
    char buffer [ PATH_MAX ];
    sprintf ( buffer, "%s/%s", ui_selected -> ref -> object_path, ui_selected -> ref -> object_filename );
    pnd_apps_exec ( pnd_run_script,
		    buffer,
		    ui_selected -> ref -> unique_id,
		    ui_selected -> ref -> exec,
		    ui_selected -> ref -> startdir,
		    ui_selected -> ref -> execargs,
		    ui_selected -> ref -> clockspeed ? atoi ( ui_selected -> ref -> clockspeed ) : 0,
		    PND_EXEC_OPTION_NORUN );
    sprintf ( buffer, "%s %s\n", MM_RUN, pnd_apps_exec_runline() );
    emit_and_quit ( buffer );
  }

  return;
}

void ui_push_ltrigger ( void ) {
  unsigned char oldcat = ui_category;

  if ( ui_category > 0 ) {
    ui_category--;
  } else {
    if ( pnd_conf_get_as_int_d ( g_conf, "tabs.wraparound", 0 ) > 0 ) {
      ui_category = g_categorycount - 1;
    }
  }

  if ( oldcat != ui_category ) {
    ui_selected = NULL;
    ui_set_selected ( ui_selected );
  }

  // make tab visible?
  if ( ui_catshift > 0 && ui_category == ui_catshift - 1 ) {
    ui_catshift--;
  }

  // unscroll
  ui_rows_scrolled_down = 0;

  render_mask |= CHANGED_CATEGORY;

  return;
}

void ui_push_rtrigger ( void ) {
  unsigned char oldcat = ui_category;

  unsigned int screen_width = pnd_conf_get_as_int_d ( g_conf, "display.screen_width", 800 );
  unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );

  if ( ui_category < ( g_categorycount - 1 ) ) {
    ui_category++;
  } else {
    if ( pnd_conf_get_as_int_d ( g_conf, "tabs.wraparound", 0 ) > 0 ) {
      ui_category = 0;
    }
  }

  if ( oldcat != ui_category ) {
    ui_selected = NULL;
    ui_set_selected ( ui_selected );
  }

  // make tab visible?
  if ( ui_category > ui_catshift + ( screen_width / tab_width ) - 1 ) {
    ui_catshift++;
  }

  // unscroll
  ui_rows_scrolled_down = 0;

  render_mask |= CHANGED_CATEGORY;

  return;
}

SDL_Surface *ui_scale_image ( SDL_Surface *s, unsigned int maxwidth, int maxheight ) {
  double scale = 1000000.0;
  double scalex = 1000000.0;
  double scaley = 1000000.0;
  SDL_Surface *scaled;

  scalex = (double)maxwidth / (double)s -> w;

  if ( maxheight == -1 ) {
    scale = scalex;
  } else {
    scaley = (double)maxheight / (double)s -> h;

    if ( scaley < scalex ) {
      scale = scaley;
    } else {
      scale = scalex;
    }

  }

  scaled = rotozoomSurface ( s, 0 /* angle*/, scale /* scale */, 1 /* smooth==1*/ );
  SDL_FreeSurface ( s );
  s = scaled;

  return ( s );
}

void ui_loadscreen ( void ) {

  SDL_Rect dest;

  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  // clear the screen
  SDL_FillRect( SDL_GetVideoSurface(), NULL, 0 );

  // render text
  SDL_Surface *rtext;
  SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
  rtext = TTF_RenderText_Blended ( g_big_font, "Setting up menu...", tmpfontcolor );
  dest.x = 20;
  dest.y = 20;
  SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, &dest );
  SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  SDL_FreeSurface ( rtext );

  return;
}

void ui_discoverscreen ( unsigned char clearscreen ) {

  SDL_Rect dest;

  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  // clear the screen
  if ( clearscreen ) {
    SDL_FillRect( SDL_GetVideoSurface(), NULL, 0 );

    // render background
    if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
      dest.x = 0;
      dest.y = 0;
      dest.w = sdl_realscreen -> w;
      dest.h = sdl_realscreen -> h;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, NULL /* whole image */, sdl_realscreen, NULL /* 0,0 */ );
      SDL_UpdateRects ( sdl_realscreen, 1, &dest );
    }

  }

  // render text
  SDL_Surface *rtext;
  SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
  rtext = TTF_RenderText_Blended ( g_big_font, "Looking for applications...", tmpfontcolor );
  if ( clearscreen ) {
    dest.x = 20;
    dest.y = 20;
  } else {
    dest.x = 20;
    dest.y = 40;
  }
  SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, &dest );
  SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  SDL_FreeSurface ( rtext );

  // render icon
  if ( g_imagecache [ IMG_ICON_MISSING ].i ) {
    dest.x = rtext -> w + 30;
    dest.y = 20;
    SDL_BlitSurface ( g_imagecache [ IMG_ICON_MISSING ].i, NULL, sdl_realscreen, &dest );
    SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  }

  return;
}

void ui_cachescreen ( unsigned char clearscreen, char *filename ) {

  SDL_Rect rects [ 4 ];
  SDL_Rect *dest = rects;
  SDL_Rect src;
  bzero ( dest, sizeof(SDL_Rect)* 4 );

  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  static unsigned int stepx = 0;

  // clear the screen
  if ( clearscreen ) {
    SDL_FillRect( SDL_GetVideoSurface(), NULL, 0 );

    // render background
    if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
      dest -> x = 0;
      dest -> y = 0;
      dest -> w = sdl_realscreen -> w;
      dest -> h = sdl_realscreen -> h;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, NULL /* whole image */, sdl_realscreen, NULL /* 0,0 */ );
      dest++;
    }

  } else {

    // render background
    if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
      src.x = 0;
      src.y = 0;
      src.w = sdl_realscreen -> w;
      src.h = 100;
      dest -> x = 0;
      dest -> y = 0;
      dest -> w = sdl_realscreen -> w;
      dest -> h = sdl_realscreen -> h;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, &src, sdl_realscreen, dest );
      dest++;
    }

  } // clear it

  // render text
  SDL_Surface *rtext;
  SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
  rtext = TTF_RenderText_Blended ( g_big_font, "Caching applications artwork...", tmpfontcolor );
  dest -> x = 20;
  dest -> y = 20;
  SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
  SDL_FreeSurface ( rtext );
  dest++;

  // render icon
  if ( g_imagecache [ IMG_ICON_MISSING ].i ) {
    dest -> x = rtext -> w + 30 + stepx;
    dest -> y = 20;
    SDL_BlitSurface ( g_imagecache [ IMG_ICON_MISSING ].i, NULL, sdl_realscreen, dest );
    dest++;
  }

  // filename
  if ( filename ) {
    rtext = TTF_RenderText_Blended ( g_tab_font, filename, tmpfontcolor );
    dest -> x = 20;
    dest -> y = 50;
    SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // move across
  stepx += 20;

  if ( stepx > 350 ) {
    stepx = 0;
  }

  SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );

  return;
}

int ui_selected_index ( void ) {

  if ( ! ui_selected ) {
    return ( -1 ); // no index
  }

  mm_appref_t *r = g_categories [ ui_category ].refs;
  int counter = 0;
  while ( r ) {
    if ( r == ui_selected ) {
      return ( counter );
    }
    r = r -> next;
    counter++;
  }

  return ( -1 );
}

static mm_appref_t *timer_ref = NULL;
void ui_set_selected ( mm_appref_t *r ) {

  render_mask |= CHANGED_SELECTION;

  if ( ! pnd_conf_get_as_int_d ( g_conf, "minimenu.load_previews_later", 0 ) ) {
    return; // no desire to defer anything
  }

  if ( ! r ) {
    // cancel timer
    SDL_SetTimer ( 0, NULL );
    timer_ref = NULL;
    return;
  }

  SDL_SetTimer ( pnd_conf_get_as_int_d ( g_conf, "previewpic.defer_timer_ms", 1000 ), ui_callback_f );
  timer_ref = r;

  return;
}

unsigned int ui_callback_f ( unsigned int t ) {

  if ( ui_selected != timer_ref ) {
    return ( 0 ); // user has moved it, who cares
  }

  SDL_Event e;
  bzero ( &e, sizeof(SDL_Event) );
  e.type = SDL_USEREVENT;
  e.user.code = sdl_user_ticker;
  SDL_PushEvent ( &e );

  return ( 0 );
}

int ui_modal_single_menu ( char *argv[], unsigned int argc, char *title, char *footer ) {
  SDL_Rect rects [ 40 ];
  SDL_Rect *dest = rects;
  SDL_Rect src;
  SDL_Surface *rtext;

  bzero ( rects, sizeof(SDL_Rect) * 40 );

  unsigned int sel = 0;

  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };

  SDL_Color selfontcolor = { 0/*font_rgba_r*/, font_rgba_g, font_rgba_b, font_rgba_a };

  unsigned int i;
  SDL_Event event;

  while ( 1 ) {

    // clear
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
    dest -> w = ((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> w;
    dest -> h = ((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> h;
    SDL_FillRect( sdl_realscreen, dest, 0 );

    // show dialog background
    if ( g_imagecache [ IMG_DETAIL_BG ].i ) {
      src.x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      src.y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      src.w = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> w;
      src.h = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> h;
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      // repeat for darken?
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    }

    // show dialog frame
    if ( g_imagecache [ IMG_DETAIL_PANEL ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_PANEL ].i, NULL /* whole image */, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    }

    // show header
    if ( title ) {
      rtext = TTF_RenderText_Blended ( g_tab_font, title, tmpfontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 20;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show footer
    if ( footer ) {
      rtext = TTF_RenderText_Blended ( g_tab_font, footer, tmpfontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) +
	((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> h
	- 60;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show options
    for ( i = 0; i < argc; i++ ) {

      // show options
      if ( sel == i ) {
	rtext = TTF_RenderText_Blended ( g_tab_font, argv [ i ], selfontcolor );
      } else {
	rtext = TTF_RenderText_Blended ( g_tab_font, argv [ i ], tmpfontcolor );
      }
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 40 + ( 20 * ( i + 1 ) );
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;

    } // for

    // update all the rects and send it all to sdl
    SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );
    dest = rects;

    // check for input
    while ( SDL_WaitEvent ( &event ) ) {

      switch ( event.type ) {

      case SDL_KEYUP:

	if ( event.key.keysym.sym == SDLK_UP ) {
	  if ( sel ) {
	    sel--;
	  }
	} else if ( event.key.keysym.sym == SDLK_DOWN ) {
	  if ( sel < argc - 1 ) {
	    sel++;
	  }

	} else if ( event.key.keysym.sym == SDLK_RETURN ) {
	  return ( sel );

	} else if ( event.key.keysym.sym == SDLK_q ) {
	  exit ( 0 );

	} else {
	  return ( -1 ); // nada
	}

	break;

      } // switch

      break;
    } // while

  } // while

  return ( -1 );
}

unsigned char ui_determine_row ( mm_appref_t *a ) {
  unsigned int row = 0;

  mm_appref_t *i = g_categories [ ui_category ].refs;
  while ( i != a ) {
    i = i -> next;
    row++;
  } // while
  row /= pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 );

  return ( row );
}

unsigned char ui_determine_screen_row ( mm_appref_t *a ) {
  return ( ui_determine_row ( a ) % pnd_conf_get_as_int_d ( g_conf, "grid.row_max", 5 ) );
}

unsigned char ui_determine_screen_col ( mm_appref_t *a ) {
  unsigned int col = 0;

  mm_appref_t *i = g_categories [ ui_category ].refs;
  while ( i != a ) {
    i = i -> next;
    col++;
  } // while
  col %= pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 );

  return ( col );
}

unsigned char ui_show_info ( char *pndrun, pnd_disco_t *p ) {
  char *viewer, *searchpath;
  pnd_conf_handle desktoph;

  // viewer
  searchpath = pnd_conf_query_searchpath();

  desktoph = pnd_conf_fetch_by_id ( pnd_conf_desktop, searchpath );

  if ( ! desktoph ) {
    return ( 0 );
  }

  viewer = pnd_conf_get_as_char ( desktoph, "info.viewer" );

  if ( ! viewer ) {
    return ( 0 ); // no way to view the file
  }

  // etc
  if ( ! p -> unique_id ) {
    return ( 0 );
  }

  if ( ! p -> info_filename ) {
    return ( 0 );
  }

  if ( ! p -> info_name ) {
    return ( 0 );
  }

  if ( ! pndrun ) {
    return ( 0 );
  }

  // exec line
  char args [ 1001 ];
  char *pargs = args;
  if ( pnd_conf_get_as_char ( desktoph, "info.viewer_args" ) ) {
    snprintf ( pargs, 1001, "%s %s",
	       pnd_conf_get_as_char ( desktoph, "info.viewer_args" ), p -> info_filename );
  } else {
    pargs = NULL;
  }

  char pndfile [ 1024 ];
  if ( p -> object_type == pnd_object_type_directory ) {
    // for PXML-app-dir, pnd_run.sh doesn't want the PXML.xml.. it just wants the dir-name
    strncpy ( pndfile, p -> object_path, 1000 );
  } else if ( p -> object_type == pnd_object_type_pnd ) {
    // pnd_run.sh wants the full path and filename for the .pnd file
    snprintf ( pndfile, 1020, "%s/%s", p -> object_path, p -> object_filename );
  }

  if ( ! pnd_apps_exec ( pndrun, pndfile, p -> unique_id, viewer, p -> startdir, pargs,
			 p -> clockspeed ? atoi ( p -> clockspeed ) : 0, PND_EXEC_OPTION_NORUN ) )
  {
    return ( 0 );
  }

  pnd_log ( pndn_debug, "Info Exec=%s\n", pnd_apps_exec_runline() );

  // try running it
  int x;
  if ( ( x = fork() ) < 0 ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
    return ( 0 );
  }

  if ( x == 0 ) {
    execl ( "/bin/sh", "/bin/sh", "-c", pnd_apps_exec_runline(), (char*)NULL );
    pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", pnd_apps_exec_runline() );
    return ( 0 );
  }

  return ( 1 );
}

typedef struct {
  SDL_Rect r;
  int catnum;
  mm_appref_t *ref;
} ui_touch_t;
#define MAXTOUCH 100
ui_touch_t ui_touchrects [ MAXTOUCH ];
unsigned char ui_touchrect_count = 0;

void ui_register_reset ( void ) {
  bzero ( ui_touchrects, sizeof(ui_touch_t)*MAXTOUCH );
  ui_touchrect_count = 0;
  return;
}

void ui_register_tab ( unsigned char catnum, unsigned int x, unsigned int y, unsigned int w, unsigned int h ) {

  if ( ui_touchrect_count == MAXTOUCH ) {
    return;
  }

  ui_touchrects [ ui_touchrect_count ].r.x = x;
  ui_touchrects [ ui_touchrect_count ].r.y = y;
  ui_touchrects [ ui_touchrect_count ].r.w = w;
  ui_touchrects [ ui_touchrect_count ].r.h = h;
  ui_touchrects [ ui_touchrect_count ].catnum = catnum;
  ui_touchrect_count++;

  return;
}

void ui_register_app ( mm_appref_t *app, unsigned int x, unsigned int y, unsigned int w, unsigned int h ) {

  if ( ui_touchrect_count == MAXTOUCH ) {
    return;
  }

  ui_touchrects [ ui_touchrect_count ].r.x = x;
  ui_touchrects [ ui_touchrect_count ].r.y = y;
  ui_touchrects [ ui_touchrect_count ].r.w = w;
  ui_touchrects [ ui_touchrect_count ].r.h = h;
  ui_touchrects [ ui_touchrect_count ].ref = app;
  ui_touchrect_count++;

  return;
}

void ui_touch_act ( unsigned int x, unsigned int y ) {

  unsigned char i;
  ui_touch_t *t;

  for ( i = 0; i < ui_touchrect_count; i++ ) {
    t = &(ui_touchrects [ i ]);

    if ( x >= t -> r.x &&
	 x <= t -> r.x + t -> r.w &&
	 y >= t -> r.y &&
	 y <= t -> r.y + t -> r.h
       )
    {

      if ( t -> ref ) {
	ui_selected = t -> ref;
	ui_push_exec();
      } else {
	if ( ui_category != t -> catnum ) {
	  ui_selected = NULL;
	}
	ui_category = t -> catnum;
	render_mask |= CHANGED_CATEGORY;
      }

      break;
    }

  } // for

  return;
}

unsigned char ui_forkexec ( char *argv[] ) {
  char *fooby = argv[0];
  int x;

  if ( ( x = fork() ) < 0 ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fork() for '%s'\n", fooby );
    return ( 0 );
  }

  if ( x == 0 ) { // child
    execv ( fooby, argv );
    pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", fooby );
    return ( 0 );
  }

  // parent, success
  return ( 1 );
}

unsigned char ui_threaded_defered_preview ( pnd_disco_t *p ) {

  if ( ! cache_preview ( p, pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 200 ),
			 pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_height", 180 ) )
     )
  {
    pnd_log ( pndn_debug, "THREAD: Couldn't load preview pic: '%s' -> '%s'\n",
	      IFNULL(p->title_en,"No Name"), p -> preview_pic1 );
  }

  // trigger that we completed
  SDL_Event e;
  bzero ( &e, sizeof(SDL_Event) );
  e.type = SDL_USEREVENT;
  e.user.code = sdl_user_finishedpreview;
  e.user.data1 = p;
  SDL_PushEvent ( &e );

  return ( 0 );
}

SDL_Thread *g_icon_thread = NULL;
void ui_post_scan ( void ) {

  // if deferred icon load, kick off the thread now
  if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.load_icons_later", 0 ) == 1 ) {

    g_icon_thread = SDL_CreateThread ( (void*)ui_threaded_defered_icon, NULL );

    if ( ! g_icon_thread ) {
      pnd_log ( pndn_error, "ERROR: Couldn't create icon thread\n" );
    }

  } // deferred icon load

  return;
}

unsigned char ui_threaded_defered_icon ( void *p ) {
  extern pnd_box_handle g_active_apps;
  pnd_box_handle h = g_active_apps;

  unsigned char maxwidth, maxheight;
  maxwidth = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_ICON_MAX_WIDTH, 50 );
  maxheight = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_ICON_MAX_HEIGHT, 50 );

  pnd_disco_t *iter = pnd_box_get_head ( h );

  while ( iter ) {

    // cache it
    if ( iter -> pnd_icon_pos &&
	 ! cache_icon ( iter, maxwidth, maxheight ) )
    {
      pnd_log ( pndn_warning, "  Couldn't load icon: '%s'\n", IFNULL(iter->title_en,"No Name") );
    } else {

      // trigger that we completed
      SDL_Event e;
      bzero ( &e, sizeof(SDL_Event) );
      e.type = SDL_USEREVENT;
      e.user.code = sdl_user_finishedicon;
      SDL_PushEvent ( &e );

      //pnd_log ( pndn_warning, "  Finished deferred load icon: '%s'\n", IFNULL(iter->title_en,"No Name") );
      usleep ( pnd_conf_get_as_int_d ( g_conf, "minimenu.defer_icon_us", 50000 ) );

    }

    // next
    iter = pnd_box_get_next ( iter );
  } // while

  return ( 0 );
}

void ui_show_hourglass ( unsigned char updaterect ) {

  SDL_Rect dest;
  SDL_Surface *s = g_imagecache [ IMG_HOURGLASS ].i;

  dest.x = ( 800 - s -> w ) / 2;
  dest.y = ( 480 - s -> h ) / 2;

  SDL_BlitSurface ( s, NULL /* whole image */, sdl_realscreen, &dest );

  if ( updaterect ) {
    SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  }

  return;
}
