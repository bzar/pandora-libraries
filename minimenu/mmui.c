
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */
#include <limits.h> /* for PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>
#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */
#include <time.h>
#include <ftw.h>
#include <ctype.h>

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"
#include "SDL_thread.h"
#include <dirent.h>

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_apps.h"
#include "pnd_device.h"
#include "../lib/pnd_pathiter.h"
#include "pnd_utility.h"
#include "pnd_pndfiles.h"
#include "pnd_notify.h"
#include "pnd_dbusnotify.h"

#include "mmenu.h"
#include "mmcat.h"
#include "mmcache.h"
#include "mmui.h"
#include "mmwrapcmd.h"
#include "mmconf.h"
#include "mmui_context.h"
#include "freedesktop_cats.h"

#define CHANGED_NOTHING     (0)
#define CHANGED_CATEGORY    (1<<0)  /* changed to different category */
#define CHANGED_SELECTION   (1<<1)  /* changed app selection */
#define CHANGED_DATAUPDATE  (1<<2)  /* deferred preview pic or icon came in */
#define CHANGED_APPRELOAD   (1<<3)  /* new set of applications entirely */
#define CHANGED_EVERYTHING  (1<<4)  /* redraw it all! */
unsigned int render_mask = CHANGED_EVERYTHING;

#define MIMETYPE_EXE "/usr/bin/file"     /* check for file type prior to invocation */

/* SDL
 */
SDL_Surface *sdl_realscreen = NULL;
unsigned int sdl_ticks = 0;
SDL_Thread *g_preview_thread = NULL;
SDL_Thread *g_timer_thread = NULL;

enum { sdl_user_ticker = 0,
       sdl_user_finishedpreview = 1,
       sdl_user_finishedicon = 2,
       sdl_user_checksd = 3,
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
ui_context_t ui_display_context;        // display paramaters: see mmui_context.h
unsigned char ui_detail_hidden = 0;     // if >0, detail panel is hidden
// FUTURE: If multiple panels can be shown/hidden, convert ui_detail_hidden to a bitmask

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

  // key repeat
  SDL_EnableKeyRepeat ( 500, 150 );

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
  sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, "grid.font" ) );
  g_grid_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "grid.font_ptsize", 10 ) );
  if ( ! g_grid_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "grid.font" ), pnd_conf_get_as_int_d ( g_conf, "grid.font_ptsize", 10 ) );
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

  // determine display context
  if ( pnd_conf_get_as_int_d ( g_conf, "display.show_detail_pane", 1 ) > 0 ) {
    ui_detail_hidden = 0;
  } else {
    ui_detail_hidden = 1;
  }
  ui_recache_context ( &ui_display_context );

  return ( 1 );
}

mm_imgcache_t g_imagecache [ IMG_TRUEMAX ] = {
  { IMG_BACKGROUND_800480,    "graphics.IMG_BACKGROUND_800480" },
  { IMG_BACKGROUND_TABMASK,   "graphics.IMG_BACKGROUND_TABMASK" },
  { IMG_DETAIL_PANEL,         "graphics.IMG_DETAIL_PANEL" },
  { IMG_DETAIL_BG,            "graphics.IMG_DETAIL_BG" },
  { IMG_SELECTED_ALPHAMASK,   "graphics.IMG_SELECTED_ALPHAMASK" },
  { IMG_TAB_SEL,              "graphics.IMG_TAB_SEL" },
  { IMG_TAB_SEL_L,            "graphics.IMG_TAB_SEL_L" },
  { IMG_TAB_SEL_R,            "graphics.IMG_TAB_SEL_R" },
  { IMG_TAB_UNSEL,            "graphics.IMG_TAB_UNSEL" },
  { IMG_TAB_UNSEL_L,          "graphics.IMG_TAB_UNSEL_L" },
  { IMG_TAB_UNSEL_R,          "graphics.IMG_TAB_UNSEL_R" },
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
  { IMG_FOLDER,               "graphics.IMG_FOLDER", },
  { IMG_EXECBIN,              "graphics.IMG_EXECBIN", },
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

  ui_context_t *c = &ui_display_context; // for convenience and shorthand

  // how many total rows do we need?
  if ( g_categorycount ) {
    icon_rows = g_categories [ ui_category ] -> refcount / c -> col_max;
    if ( g_categories [ ui_category ] -> refcount % c -> col_max > 0 ) {
      icon_rows++;
    }
  } else {
    icon_rows = 0;
  }

#if 1
  // if no selected app yet, select the first one
  if ( ! ui_selected && pnd_conf_get_as_int_d ( g_conf, "minimenu.start_selected", 0 ) ) {
    ui_selected = g_categories [ ui_category ] -> refs;
  }
#endif

  // reset touchscreen regions
  if ( render_jobs_b ) {
    ui_register_reset();
  }

  // ensure selection is visible
  if ( ui_selected ) {

    int index = ui_selected_index();
    int topleft = c -> col_max * ui_rows_scrolled_down;
    int botright = ( c -> col_max * ( ui_rows_scrolled_down + c -> row_max ) - 1 );

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
    static unsigned int tab_width;
    static unsigned int tab_height;
    static unsigned int tab_offset_x;
    static unsigned int tab_offset_y;
    static unsigned int text_offset_x;
    static unsigned int text_offset_y;
    static unsigned int text_width;

    static unsigned char tab_first_run = 1;
    if ( tab_first_run ) {
      tab_first_run = 0;
      tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );
      tab_height = pnd_conf_get_as_int ( g_conf, "tabs.tab_height" );
      tab_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_x" );
      tab_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_y" );
      text_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_x" );
      text_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_y" );
      text_width = pnd_conf_get_as_int ( g_conf, "tabs.text_width" );
    }

    unsigned int maxtab = ( c -> screen_width / tab_width ) < g_categorycount ? ( c -> screen_width / tab_width ) + ui_catshift : g_categorycount + ui_catshift;
    unsigned int maxtabspot = ( c -> screen_width / tab_width );

    if ( g_categorycount > 0 ) {

      // draw tabs with categories
      for ( col = ui_catshift;
	    col < maxtab;
	    col++ )
      {

	SDL_Surface *s;

	// if this is the first (leftmost) tab, we use different artwork
	// than if the other tabs (so skinner can link lines up nicely.)
	if ( col == ui_catshift ) {
	  // leftmost tab, special case

	  if ( col == ui_category ) {
	    s = g_imagecache [ IMG_TAB_SEL_L ].i;
	  } else {
	    s = g_imagecache [ IMG_TAB_UNSEL_L ].i;
	  }

	} else if ( col == maxtab - 1 ) {
	  // rightmost tab, special case

	  if ( col == ui_category ) {
	    s = g_imagecache [ IMG_TAB_SEL_R ].i;
	  } else {
	    s = g_imagecache [ IMG_TAB_UNSEL_R ].i;
	  }

	} else {
	  // normal (not leftmost) tab

	  if ( col == ui_category ) {
	    s = g_imagecache [ IMG_TAB_SEL ].i;
	  } else {
	    s = g_imagecache [ IMG_TAB_UNSEL ].i;
	  }

	} // first col, or not first col?

	// draw tab
	src.x = 0;
	src.y = 0;
#if 0
	src.w = tab_width;
	if ( col == ui_category ) {
	  src.h = tab_selheight;
	} else {
	  src.h = tab_height;
	}
#else
	src.w = s -> w;
	src.h = s -> h;
#endif
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
	  rtext = TTF_RenderText_Blended ( g_tab_font, g_categories [ col ] -> catname, c -> fontcolor );
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

    } // if we got categories

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
      dest -> x = c -> arrow_up_x;
      dest -> y = c -> arrow_up_y;
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_UP ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;

      show_bar = 1;
    }

    // down?
    if ( ui_rows_scrolled_down + c -> row_max < icon_rows && g_imagecache [ IMG_ARROW_DOWN ].i ) {
      dest -> x = c -> arrow_down_x;
      dest -> y = c -> arrow_down_y;
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_DOWN ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;

      show_bar = 1;
    }

    if ( show_bar ) {
      // show scrollbar as well
      src.x = 0;
      src.y = 0;
      src.w = c -> arrow_bar_clip_w;
      src.h = c -> arrow_bar_clip_h;
      dest -> x = c -> arrow_bar_x;
      dest -> y = c -> arrow_bar_y;
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_SCROLLBAR ].i, &src /* whole image */, sdl_realscreen, dest );
      dest++;
    } // bar

  } // r_bg, scroll bars

  // render detail pane bg
  if ( render_jobs_b & R_DETAIL && ui_detail_hidden == 0 ) {

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
  if ( render_jobs_b & R_GRID && g_categorycount ) {

    // if just rendering grid, and nothing else, better clear it first
    if ( ! ( render_jobs_b & R_BG ) ) {
      if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
	src.x = c -> grid_offset_x;
	src.y = c -> grid_offset_y + c -> sel_icon_offset_y;
	src.w = c -> col_max * c -> cell_width;
	src.h = c -> row_max * c -> cell_height;

	dest -> x = c -> grid_offset_x;
	dest -> y = c -> grid_offset_y + c -> sel_icon_offset_y;

	SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, &src, sdl_realscreen, dest );
	dest++;

      }
    }

    if ( g_categories [ ui_category ] -> refs ) {

      appiter = g_categories [ ui_category ] -> refs;
      row = 0;
      displayrow = 0;

      // until we run out of apps, or run out of space
      while ( appiter != NULL ) {

	for ( col = 0; col < c -> col_max && appiter != NULL; col++ ) {

	  // do we even need to render it? or are we suppressing it due to rows scrolled off the top?
	  if ( row >= ui_rows_scrolled_down ) {

	    // selected? show hilights
	    if ( appiter == ui_selected ) {
	      SDL_Surface *s = g_imagecache [ IMG_SELECTED_ALPHAMASK ].i;
	      // icon
	      //dest -> x = grid_offset_x + ( col * cell_width ) + icon_offset_x + ( ( icon_max_width - s -> w ) / 2 );
	      dest -> x = c -> grid_offset_x + ( col * c -> cell_width ) + c -> icon_offset_x + c -> sel_icon_offset_x;
	      //dest -> y = grid_offset_y + ( displayrow * cell_height ) + icon_offset_y + ( ( icon_max_height - s -> h ) / 2 );
	      dest -> y = c -> grid_offset_y + ( displayrow * c -> cell_height ) + c -> icon_offset_y + c -> sel_icon_offset_y;
	      SDL_BlitSurface ( s, NULL /* all */, sdl_realscreen, dest );
	      dest++;
	      // text
	      dest -> x = c -> grid_offset_x + ( col * c -> cell_width ) + c -> text_clip_x;
	      dest -> y = c -> grid_offset_y + ( displayrow * c -> cell_height ) + c -> text_hilite_offset_y;
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

	      // no icon override; was this a pnd-file (show the unknown icon then), or was this generated from
	      // filesystem (file or directory icon)
	      if ( appiter -> ref -> object_flags & PND_DISCO_GENERATED ) {
		if ( appiter -> ref -> object_type == pnd_object_type_directory ) {
		  iconsurface = g_imagecache [ IMG_FOLDER ].i;
		} else {
		  iconsurface = g_imagecache [ IMG_EXECBIN ].i;
		}
	      } else {
		iconsurface = g_imagecache [ IMG_ICON_MISSING ].i;
	      }

	    }

	    // got an icon I hope?
	    if ( iconsurface ) {
	      //pnd_log ( pndn_debug, "Got an icon for '%s'\n", IFNULL(appiter -> ref -> title_en,"No Name") );

	      src.x = 0;
	      src.y = 0;
	      src.w = 60;
	      src.h = 60;
	      dest -> x = c -> grid_offset_x + ( col * c -> cell_width ) + c -> icon_offset_x + (( c -> icon_max_width - iconsurface -> w ) / 2);
	      dest -> y = c -> grid_offset_y + ( displayrow * c -> cell_height ) + c -> icon_offset_y + (( c -> icon_max_height - iconsurface -> h ) / 2);

	      SDL_BlitSurface ( iconsurface, &src, sdl_realscreen, dest );

	      // store touch info
	      ui_register_app ( appiter, dest -> x, dest -> y, src.w, src.h );

	      dest++;

	    }

	    // show text
	    if ( appiter -> ref -> title_en ) {
	      SDL_Surface *rtext;
	      rtext = TTF_RenderText_Blended ( g_grid_font, appiter -> ref -> title_en, c -> fontcolor );
	      src.x = 0;
	      src.y = 0;
	      src.w = c -> text_width < rtext -> w ? c -> text_width : rtext -> w;
	      src.h = rtext -> h;
	      if ( rtext -> w > c -> text_width ) {
		dest -> x = c -> grid_offset_x + ( col * c -> cell_width ) + c -> text_clip_x;
	      } else {
		dest -> x = c -> grid_offset_x + ( col * c -> cell_width ) + c -> text_offset_x - ( rtext -> w / 2 );
	      }
	      dest -> y = c -> grid_offset_y + ( displayrow * c -> cell_height ) + c -> text_offset_y;
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
	if ( displayrow >= c -> row_max ) {
	  break;
	}

      } // while

    } else {
      // no apps to render?
      pnd_log ( pndn_rem, "No applications to render?\n" );
    } // apps to renser?

  } // r_grid

  // detail panel - show app details or blank-message
  if ( render_jobs_b & R_DETAIL && ui_detail_hidden == 0 ) {

    unsigned int cell_offset_x = pnd_conf_get_as_int ( g_conf, "detailtext.cell_offset_x" );
    unsigned int cell_offset_y = pnd_conf_get_as_int ( g_conf, "detailtext.cell_offset_y" );
    unsigned int cell_width = pnd_conf_get_as_int ( g_conf, "detailtext.cell_width" );

    unsigned int desty = cell_offset_y;

    if ( ui_selected ) {

      char buffer [ 256 ];

      // full name
      if ( ui_selected -> ref -> title_en ) {
	SDL_Surface *rtext;
	rtext = TTF_RenderText_Blended ( g_detailtext_font, ui_selected -> ref -> title_en, c -> fontcolor );
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
#if 0
      if ( ui_selected -> ref -> main_category ) {

	sprintf ( buffer, "Category: %s", ui_selected -> ref -> main_category );

	SDL_Surface *rtext;
	rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, c -> fontcolor );
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
#endif

      // clock
      if ( ui_selected -> ref -> clockspeed ) {

	sprintf ( buffer, "CPU Clock: %s", ui_selected -> ref -> clockspeed );

	SDL_Surface *rtext;
	rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, c -> fontcolor );
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
	rtext = TTF_RenderText_Blended ( g_grid_font, buffer, c -> fontcolor );
	dest -> x = cell_offset_x + cell_width - rtext -> w;
	dest -> y = desty - src.h;
	SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
	SDL_FreeSurface ( rtext );
	dest++;
      }

      // info hint
#if 0 // merged into hint-line
      if ( ui_selected -> ref -> info_filename ) {

	sprintf ( buffer, "Documentation - hit Y" );

	SDL_Surface *rtext;
	rtext = TTF_RenderText_Blended ( g_detailtext_font, buffer, c -> fontcolor );
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
#endif

      // notes
      if ( ui_selected -> ovrh ) {
	char *n;
	unsigned char i;
	char buffer [ 50 ];

	desty += 5; // a touch of spacing can't hurt

	for ( i = 1; i < 4; i++ ) {
	  sprintf ( buffer, "Application-%u.note-%u", ui_selected -> ref -> subapp_number, i );
	  n = pnd_conf_get_as_char ( ui_selected -> ovrh, buffer );

	  if ( n ) {
	    SDL_Surface *rtext;
	    rtext = TTF_RenderText_Blended ( g_detailtext_font, n, c -> fontcolor );
	    src.x = 0;
	    src.y = 0;
	    src.w = rtext -> w < cell_width ? rtext -> w : cell_width;
	    src.h = rtext -> h;
	    dest -> x = cell_offset_x;
	    dest -> y = desty;
	    SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
	    SDL_FreeSurface ( rtext );
	    dest++;
	    desty += rtext -> h;
	  }
	} // for

      } // r_detail -> notes

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

    } else {

      char *empty_message = "Press SELECT for menu";

      SDL_Surface *rtext;

      rtext = TTF_RenderText_Blended ( g_detailtext_font, empty_message, c -> fontcolor );

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

      rtext = TTF_RenderText_Blended ( g_detailtext_font, "START or B to run app", c -> fontcolor );
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;

      rtext = TTF_RenderText_Blended ( g_detailtext_font, "SPACE for app menu", c -> fontcolor );
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;

      rtext = TTF_RenderText_Blended ( g_detailtext_font, "A to toggle details", c -> fontcolor );
      dest -> x = cell_offset_x;
      dest -> y = desty;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
      desty += src.h;

    } // r_detail && selected?

  } // r_detail

  // extras
  //

  // battery
  if ( render_jobs_b & R_BG ) {
    static int last_battlevel = 0;
    static unsigned char batterylevel = 0;
    char buffer [ 100 ];

    if ( time ( NULL ) - last_battlevel > 60 ) {
      batterylevel = pnd_device_get_battery_gauge_perc();
      last_battlevel = time ( NULL );
    }

    sprintf ( buffer, "Battery: %u%%", batterylevel );

    SDL_Surface *rtext;
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, c -> fontcolor );
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "display.battery_x", 20 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "display.battery_y", 450 );
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // hints
  if ( pnd_conf_get_as_char ( g_conf, "display.hintline" ) ) {
    char *buffer;
    unsigned int hintx, hinty;
    hintx = pnd_conf_get_as_int_d ( g_conf, "display.hint_x", 40 );
    hinty = pnd_conf_get_as_int_d ( g_conf, "display.hint_y", 450 );
    static unsigned int lastwidth = 3000;

    if ( ui_selected && ui_selected -> ref -> info_filename ) {
      buffer = "Documentation - hit Y";
    } else {
      buffer = pnd_conf_get_as_char ( g_conf, "display.hintline" );
    }

    SDL_Surface *rtext;
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, c -> fontcolor );

    // clear bg
    if ( ! ( render_jobs_b & R_BG ) ) {
      src.x = hintx;
      src.y = hinty;
      src.w = lastwidth;
      src.h = rtext -> h;
      dest -> x = hintx;
      dest -> y = hinty;
      SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_TABMASK ].i, &src, sdl_realscreen, dest );
      dest++;
      lastwidth = rtext -> w;
    }

    // now render text
    dest -> x = hintx;
    dest -> y = hinty;
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // clock time
  if ( render_jobs_b & R_BG &&
       pnd_conf_get_as_int_d ( g_conf, "display.clock_x", -1 ) != -1 )
  {
    char buffer [ 50 ];

    time_t t = time ( NULL );
    struct tm *tm = localtime ( &t );
    strftime ( buffer, 50, "%a %H:%M %F", tm );

    SDL_Surface *rtext;
    rtext = TTF_RenderText_Blended ( g_grid_font, buffer, c -> fontcolor );
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "display.clock_x", 700 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "display.clock_y", 450 );
    SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;
  }

  // update all the rects and send it all to sdl
  // - at this point, we could probably just do 1 rect, of the
  //   whole screen, and be faster :/
  SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );

} // ui_render

void ui_process_input ( pnd_dbusnotify_handle dbh, pnd_notify_handle nh ) {
  SDL_Event event;

  unsigned char ui_event = 0; // if we get a ui event, flip to 1 and break
  //static ui_sdl_button_e ui_mask = uisb_none; // current buttons down

  while ( ( ! ui_event ) &&
	  /*block_p ?*/ SDL_WaitEvent ( &event ) /*: SDL_PollEvent ( &event )*/ )
  {

    switch ( event.type ) {

    case SDL_USEREVENT:
      // update something

      // the user-defined SDL events are all for threaded/delayed previews (and icons, which
      // generally are not used); if we're in wide mode, we can skip previews
      // to avoid slowing things down when they're not shown.

      if ( event.user.code == sdl_user_ticker ) {

	if ( ui_detail_hidden ) {
	  break; // skip building previews when not showing them
	}

	// timer went off, time to load something
	if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.load_previews_later", 0 ) ) {

	  if ( ! ui_selected ) {
	    break;
	  }

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

      } else if ( event.user.code == sdl_user_checksd ) {
	// check if any inotify-type events occured, forcing us to rescan/re-disco the SDs

	unsigned char watch_dbus = 0;
	unsigned char watch_inotify = 0;

	if ( dbh ) {
	  watch_dbus = pnd_dbusnotify_rediscover_p ( dbh );
	}

	if ( nh ) {
	  watch_inotify = pnd_notify_rediscover_p ( nh );
	}

	if ( watch_dbus || watch_inotify ) {
	  pnd_log ( pndn_debug, "dbusnotify detected SD event\n" );
	  applications_free();
	  applications_scan();

	  ui_event++;
	}

      } // SDL user event

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
    //case SDL_KEYUP:
    case SDL_KEYDOWN:

      //pnd_log ( pndn_debug, "key up %u\n", event.key.keysym.sym );

      // SDLK_LALT -> Start
      // page up/down for y/x
      // home/end for a and b

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
      } else if ( event.key.keysym.sym == SDLK_END ) { // B
	ui_push_exec();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_SPACE ) { // space
	if ( ui_selected ) {
	  ui_menu_context ( ui_selected );
	} else {
	  // TBD: post an error?
	}
	render_mask |= CHANGED_EVERYTHING;
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_TAB || event.key.keysym.sym == SDLK_HOME ) { // tab or A
	// if detail panel is togglable, then toggle it
	// if not, make sure its ruddy well shown!
	if ( ui_is_detail_hideable() ) {
	  ui_toggle_detail_pane();
	} else {
	  ui_detail_hidden = 0;
	}
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_RSHIFT || event.key.keysym.sym == SDLK_COMMA ) { // left trigger or comma
	ui_push_ltrigger();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_RCTRL || event.key.keysym.sym == SDLK_PERIOD ) { // right trigger or period
	ui_push_rtrigger();
	ui_event++;

      } else if ( event.key.keysym.sym == SDLK_PAGEUP ) { // Y
	// info
	if ( ui_selected ) {
	  ui_show_info ( pnd_run_script, ui_selected -> ref );
	  ui_event++;
	}
      } else if ( event.key.keysym.sym == SDLK_PAGEDOWN ) { // X
	ui_push_backup();

	// forget the selection, nolonger applies
	ui_selected = NULL;
	ui_set_selected ( ui_selected );
	// rescan the dir
	if ( g_categories [ ui_category ] -> fspath ) {
	  category_fs_restock ( g_categories [ ui_category ] );
	}
	// redraw the grid
	render_mask |= CHANGED_EVERYTHING;
	ui_event++;

      } else if ( event.key.keysym.sym == SDLK_LALT ) { // start button
	ui_push_exec();
	ui_event++;

      } else if ( event.key.keysym.sym == SDLK_LCTRL /*LALT*/ ) { // select button
	char *opts [ 20 ] = {
	  "Reveal hidden category",
	  "Shutdown Pandora",
	  "Configure Minimenu",
	  "Rescan for applications",
	  "Cache previews to SD now",
	  "Run a terminal/console",
	  "Run another GUI (xfce, etc)",
	  "Quit (<- beware)",
	  "Select a Minimenu skin",
	  "About Minimenu"
	};
	int sel = ui_modal_single_menu ( opts, 10, "Minimenu", "Enter to select; other to return." );

	char buffer [ 100 ];
	if ( sel == 0 ) {
	  // do nothing
	  ui_revealscreen();
	} else if ( sel == 1 ) {
	  // shutdown
	  sprintf ( buffer, "sudo poweroff" );
	  system ( buffer );
	} else if ( sel == 2 ) {
	  // configure mm
	  unsigned char restart = conf_run_menu ( NULL );
	  conf_write ( g_conf, conf_determine_location ( g_conf ) );
	  if ( restart ) {
	    emit_and_quit ( MM_RESTART );
	  }
	} else if ( sel == 3 ) {
	  // rescan apps
	  pnd_log ( pndn_debug, "Freeing up applications\n" );
	  applications_free();
	  pnd_log ( pndn_debug, "Rescanning applications\n" );
	  applications_scan();
	} else if ( sel == 4 ) {
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

	} else if ( sel == 5 ) {
	  // run terminal
	  char *argv[5];
	  argv [ 0 ] = pnd_conf_get_as_char ( g_conf, "utility.terminal" );
	  argv [ 1 ] = NULL;

	  if ( argv [ 0 ] ) {
	    ui_forkexec ( argv );
	  }

	} else if ( sel == 6 ) {
	  char buffer [ PATH_MAX ];
	  sprintf ( buffer, "%s %s\n", MM_RUN, "/usr/pandora/scripts/op_switchgui.sh" );
	  emit_and_quit ( buffer );
	} else if ( sel == 7 ) {
	  emit_and_quit ( MM_QUIT );
	} else if ( sel == 8 ) {
	  // select skin
	  if ( ui_pick_skin() ) {
	    emit_and_quit ( MM_RESTART );
	  }
	} else if ( sel == 9 ) {
	  // about
	  char buffer [ PATH_MAX ];
	  sprintf ( buffer, "%s/about.txt", g_skinpath );
	  ui_aboutscreen ( buffer );
	}

	ui_event++;
	render_mask |= CHANGED_EVERYTHING;

      } else {
	// unknown SDLK_ keycode?

	// many SDLK_keycodes map to ASCII ("a" is ascii(a)), so try to jump to a filename of that name, in this category?
	// and if already there, try to jump to next, maybe?
	// future: look for sequence typing? ie: user types 'm' then 'a', look for 'ma*' instead of 'm' then 'a' matching
	if ( isalpha ( event.key.keysym.sym ) && g_categories [ ui_category ] -> refcount > 0 ) {
	  mm_appref_t *app = g_categories [ ui_category ] -> refs;

	  //fprintf ( stderr, "sel %s next %s\n", ui_selected -> ref -> title_en, ui_selected -> next -> ref -> title_en );

	  // are we already matching the same char? and next item is also same char?
	  if ( app && ui_selected &&
	       ui_selected -> ref -> title_en && ui_selected -> next -> ref -> title_en &&
	       toupper ( ui_selected -> ref -> title_en [ 0 ] ) == toupper ( ui_selected -> next -> ref -> title_en [ 0 ] ) &&
	       toupper ( ui_selected -> ref -> title_en [ 0 ] ) == toupper ( event.key.keysym.sym )
	     )
	  {
	    // just skip down one
	    app = ui_selected -> next;
	  } else {

	    // walk the category, looking for a first-char match
	    while ( app ) {
	      if ( app -> ref -> title_en && toupper ( app -> ref -> title_en [ 0 ] ) == toupper ( event.key.keysym.sym ) ) {
		break;
	      }
	      app = app -> next;
	    }

	  } // same start letter, or new run?

	  // found something, or no?
	  if ( app ) {
	    // looks like we found a potential match; try switching it to visible selection
	    ui_selected = app;
	    ui_set_selected ( ui_selected );
	    ui_event++;
	  }


	} // SDLK_alphanumeric?

      } // SDLK_....

      // extras
#if 1
      if ( event.key.keysym.sym == SDLK_ESCAPE ) {
	emit_and_quit ( MM_QUIT );
      }
#endif

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

  // are we already at first item?
  if ( forcecoil == 0 &&
       pnd_conf_get_as_int_d ( g_conf, "grid.wrap_horiz_samerow", 0 ) &&
       col == 0 )
  {
    unsigned int i = ui_display_context.col_max - 1;
    while ( i && ui_selected -> next ) {
      ui_push_right ( 0 );
      i--;
    }

  } else if ( g_categories [ ui_category ] -> refs == ui_selected ) {
    // can't go any more left, we're at the head

  } else {
    // figure out the previous item; yay for singly linked list :/
    mm_appref_t *i = g_categories [ ui_category ] -> refs;
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
	 // and selected is far-right, or last icon in category (might not be far right)
	 ( ( col == ui_display_context.col_max - 1 ) ||
	   ( ui_selected -> next == NULL ) )
       )
    {
      // same wrap
      //unsigned int i = pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 ) - 1;
      while ( col /*i*/ ) {
	ui_push_left ( 0 );
	col--; //i--;
      }

    } else {
      // just go to the next

      if ( ui_selected -> next ) {
	ui_selected = ui_selected -> next;
      }

    }

  } else {
    ui_selected = g_categories [ ui_category ] -> refs;
  }

  ui_set_selected ( ui_selected );

  return;
}

void ui_push_up ( void ) {
  unsigned char col_max = ui_display_context.col_max;

  if ( ! ui_selected ) {
    return;
  }

  // what row we in?
  unsigned int row = ui_determine_row ( ui_selected );

  if ( row == 0 &&
       pnd_conf_get_as_int_d ( g_conf, "grid.wrap_vert_stop", 0 ) == 0 )
  {
    // wrap around instead

    unsigned int col = ui_determine_screen_col ( ui_selected );

    // go to end
    ui_selected = g_categories [ ui_category ] -> refs;
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
    if ( r - ui_display_context.row_max > 0 ) {
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
  unsigned char col_max = ui_display_context.col_max;

  if ( ui_selected ) {

    // what row we in?
    unsigned int row = ui_determine_row ( ui_selected );

    // max rows?
    unsigned int icon_rows = g_categories [ ui_category ] -> refcount / col_max;
    if ( g_categories [ ui_category ] -> refcount % col_max > 0 ) {
      icon_rows++;
    }

    // we at the end?
    if ( row == ( icon_rows - 1 ) &&
	 pnd_conf_get_as_int_d ( g_conf, "grid.wrap_vert_stop", 0 ) == 0 )
    {

      unsigned char col = ui_determine_screen_col ( ui_selected );

      ui_selected = g_categories [ ui_category ] -> refs;

      while ( col ) {
	ui_selected = ui_selected -> next;
	col--;
      }

      ui_rows_scrolled_down = 0;

      render_mask |= CHANGED_EVERYTHING;

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

// 'backup' is currently 'X', for going back up in a folder/subcat without having to hit exec on the '..' entry
void ui_push_backup ( void ) {

  // a subcat-as-dir, or a dir browser?
  if ( g_categories [ ui_category] -> fspath ) {
    // dir browser, just climb our way back up

    // go up
    char *c;

    // lop off last word; if the thing ends with /, lop that one, then the next word.
    while ( ( c = strrchr ( g_categories [ ui_category] -> fspath, '/' ) ) ) {
      *c = '\0'; // lop off the last hunk
      if ( *(c+1) != '\0' ) {
	break;
      }
    } // while

    // nothing left?
    if ( g_categories [ ui_category] -> fspath [ 0 ] == '\0' ) {
      free ( g_categories [ ui_category] -> fspath );
      g_categories [ ui_category] -> fspath = strdup ( "/" );
    }

  } else {
    // a pnd subcat .. are we in one, or at the 'top'?
    char *pcatname = g_categories [ ui_category ] -> parent_catname;

    if ( ! pcatname ) {
      return; // we're at the 'top' already
    }

    // set to first cat!
    ui_category = 0;
    // republish cats .. shoudl just be the one
    category_publish ( CFNORMAL, NULL );

    if ( pcatname ) {
      ui_category = category_index ( pcatname );
    }

  } // dir or subcat?

  return;
}

void ui_push_exec ( void ) {

  if ( ! ui_selected ) {
    return;
  }

  // was this icon generated from filesystem, or from pnd-file?
  if ( ui_selected -> ref -> object_flags & PND_DISCO_GENERATED ) {

    if ( ! ui_selected -> ref -> title_en ) {
      return; // no filename
    }

    if ( ui_selected -> ref -> object_type == pnd_object_type_directory ) {

      // check if this guy is a dir-browser tab, or is a directory on a pnd tab
      if ( ! g_categories [ ui_category] -> fspath ) {
	// pnd subcat as dir

	// are we already in a subcat? if so, go back to parent; there is no grandparenting or deeper
	if ( g_categories [ ui_category ] -> parent_catname ) {
	  // go back up
	  ui_push_backup();

	} else {
	  // delve into subcat

	  // set to first cat!
	  ui_category = 0;
	  // republish cats .. shoudl just be the one
	  category_publish ( CFBYNAME, ui_selected -> ref -> object_path );

	}

	// forget the selection, nolonger applies
	ui_selected = NULL;
	ui_set_selected ( ui_selected );
	// redraw the grid
	render_mask |= CHANGED_EVERYTHING;

      } else {

	// delve up/down the dir tree
	if ( strcmp ( ui_selected -> ref -> title_en, ".." ) == 0 ) {
	  ui_push_backup();

	} else {
	  // go down
	  char *temp = malloc ( strlen ( g_categories [ ui_category] -> fspath ) + strlen ( ui_selected -> ref -> title_en ) + 1 + 1 );
	  sprintf ( temp, "%s/%s", g_categories [ ui_category] -> fspath, ui_selected -> ref -> title_en );
	  free ( g_categories [ ui_category] -> fspath );
	  g_categories [ ui_category] -> fspath = temp;
	}

	pnd_log ( pndn_debug, "Cat %s is now in path %s\n", g_categories [ ui_category ] -> catname, g_categories [ ui_category ]-> fspath );

	// forget the selection, nolonger applies
	ui_selected = NULL;
	ui_set_selected ( ui_selected );
	// rescan the dir
	category_fs_restock ( g_categories [ ui_category ] );
	// redraw the grid
	render_mask |= CHANGED_SELECTION;

      } // directory browser or pnd subcat?

    } else {
      // just run it arbitrarily?

      // if this a pnd-file, or just some executable?
      if ( strcasestr ( ui_selected -> ref -> object_filename, PND_PACKAGE_FILEEXT ) ) {
	// looks like a pnd, now what do we do..
	pnd_box_handle h = pnd_disco_file ( ui_selected -> ref -> object_path, ui_selected -> ref -> object_filename );

	if ( h ) {
	  pnd_disco_t *d = pnd_box_get_head ( h );
	  pnd_apps_exec_disco ( pnd_run_script, d, PND_EXEC_OPTION_NORUN, NULL );
	  char buffer [ PATH_MAX ];
	  sprintf ( buffer, "%s %s\n", MM_RUN, pnd_apps_exec_runline() );
	  if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.live_on_run", 0 ) == 0 ) {
	    emit_and_quit ( buffer );
	  } else {
	    emit_and_run ( buffer );
	  }
	}

      } else {
	// random bin file

	// is it even executable? if we don't have handlers for non-executables yet (Jan 2011 we don't),
	// then don't even try to run things not-flagged as executable.. but wait most people are on
	// FAT filesystems, what a drag, we can't tell at the fs level.
	// ... but we can still invoke 'file' and grep out the good bits, at least.
	//
	// open a stream reading 'file /path/to/file' and check output for 'executable'
	// -- not checking for "ARM" so it can pick up x86 (or whatever native) executables in build environment
	unsigned char is_executable = 0;

	// popen test
	{
	  char popenbuf [ FILENAME_MAX ];
	  snprintf ( popenbuf, FILENAME_MAX, "%s %s/%s", MIMETYPE_EXE, g_categories [ ui_category ] -> fspath, ui_selected -> ref -> title_en );

	  FILE *marceau;
	  if ( ! ( marceau = popen ( popenbuf, "r" ) ) ) {
	    return; // error, we need some useful error handling and dialog boxes here
	  }

	  if ( fgets ( popenbuf, FILENAME_MAX, marceau ) ) {
	    //printf ( "File test returns: %s\n", popenbuf );
	    if ( strstr ( popenbuf, "executable" ) != NULL ) {
	      is_executable = 1;
	    }
	  }

	  pclose ( marceau );

	} // popen test

	if ( ! is_executable ) {
	  fprintf ( stderr, "ERROR: File to invoke is not executable, skipping. (%s)\n", ui_selected -> ref -> title_en );
	  return; // need some error handling here
	}

#if 0 // eat up any pending SDL events and toss 'em?
	{
	  SDL_PumpEvents();
	  SDL_Event e;
	  while ( SDL_PeepEvents ( &e, 1, SDL_GETEVENT, SDL_ALLEVENTS ) > 0 ) {
	    // spin
	  }
	}
#endif

#if 1
	// just exec it
	//

	// get CWD so we can restore it on return
	char cwd [ PATH_MAX ];
	getcwd ( cwd, PATH_MAX );

	// full path to executable so we don't rely on implicit "./"
	char execbuf [ FILENAME_MAX ];
	snprintf ( execbuf, FILENAME_MAX, "%s/%s", g_categories [ ui_category ] -> fspath, ui_selected -> ref -> title_en );

	// do it!
	chdir ( g_categories [ ui_category ] -> fspath );
	exec_raw_binary ( execbuf /*ui_selected -> ref -> title_en*/ );
	chdir ( cwd );
#else
	// DEPRECATED / NOT TESTED
	// get mmwrapper to run it
	char buffer [ PATH_MAX ];
	sprintf ( buffer, "%s %s/%s\n", MM_RUN, g_categories [ ui_category ] -> fspath, ui_selected -> ref -> title_en );
	if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.live_on_run", 0 ) == 0 ) {
	  emit_and_quit ( buffer );
	} else {
	  emit_and_run ( buffer );
	}
#endif
      } // pnd or bin?

    } // dir or file?

  } else {

    // set app-run speed
    int use_run_speed = pnd_conf_get_as_int_d ( g_conf, "minimenu.use_run_speed", 0 );
    if ( use_run_speed > 0 ) {
      int mm_speed = pnd_conf_get_as_int_d ( g_conf, "minimenu.run_speed", -1 );
      if ( mm_speed > -1 ) {
	char buffer [ 512 ];
	snprintf ( buffer, 500, "sudo /usr/pandora/scripts/op_cpuspeed.sh %d", mm_speed );
	system ( buffer );
      }
    } // do speed change?

    // request app to run and quit mmenu
    pnd_apps_exec_disco ( pnd_run_script, ui_selected -> ref, PND_EXEC_OPTION_NORUN, NULL );
    char buffer [ PATH_MAX ];
    sprintf ( buffer, "%s %s\n", MM_RUN, pnd_apps_exec_runline() );
    if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.live_on_run", 0 ) == 0 ) {
      emit_and_quit ( buffer );
    } else {
      emit_and_run ( buffer );
    }
  }

  return;
}

void ui_push_ltrigger ( void ) {
  unsigned char oldcat = ui_category;
  unsigned int screen_width = ui_display_context.screen_width;
  unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );

  if ( g_categorycount == 0 ) {
    return;
  }

  if ( ui_category > 0 ) {
    ui_category--;
    category_fs_restock ( g_categories [ ui_category ] );
  } else {
    if ( pnd_conf_get_as_int_d ( g_conf, "tabs.wraparound", 0 ) > 0 ) {
      ui_category = g_categorycount - 1;
      ui_catshift = 0;
      if ( ui_category >= ( screen_width / tab_width ) ) {
	ui_catshift = ui_category - ( screen_width / tab_width ) + 1;
      }
      category_fs_restock ( g_categories [ ui_category ] );
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

  if ( g_categorycount == 0 ) {
    return;
  }

  unsigned int screen_width = ui_display_context.screen_width;
  unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );

  if ( ui_category < ( g_categorycount - 1 ) ) {
    ui_category++;
    category_fs_restock ( g_categories [ ui_category ] );
  } else {
    if ( pnd_conf_get_as_int_d ( g_conf, "tabs.wraparound", 0 ) > 0 ) {
      ui_category = 0;
      ui_catshift = 0;
      category_fs_restock ( g_categories [ ui_category ] );
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

  // clear the screen
  SDL_FillRect( SDL_GetVideoSurface(), NULL, 0 );

  // render text
  SDL_Surface *rtext;
  rtext = TTF_RenderText_Blended ( g_big_font, "Setting up menu...", ui_display_context.fontcolor );
  dest.x = 20;
  dest.y = 20;
  SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, &dest );
  SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  SDL_FreeSurface ( rtext );

  return;
}

void ui_discoverscreen ( unsigned char clearscreen ) {

  SDL_Rect dest;

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
  rtext = TTF_RenderText_Blended ( g_big_font, "Looking for applications...", ui_display_context.fontcolor );
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

  mm_appref_t *r = g_categories [ ui_category ] -> refs;
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
  unsigned char max_visible = pnd_conf_get_as_int_d ( g_conf, "detailtext.max_visible" , 11 );
  unsigned char first_visible = 0;

  bzero ( rects, sizeof(SDL_Rect) * 40 );

  unsigned int sel = 0;

  SDL_Color selfontcolor = { 0/*font_rgba_r*/, ui_display_context.font_rgba_g, ui_display_context.font_rgba_b, ui_display_context.font_rgba_a };

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
      src.x = 0;
      src.y = 0;
      src.w = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_BG ].i)) -> w;
      src.h = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_BG ].i)) -> h;
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      // repeat for darken?
      //SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      //SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
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
      rtext = TTF_RenderText_Blended ( g_tab_font, title, ui_display_context.fontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 20;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show footer
    if ( footer ) {
      rtext = TTF_RenderText_Blended ( g_tab_font, footer, ui_display_context.fontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) +
	((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> h
	- 60;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show options
    for ( i = first_visible; i < first_visible + max_visible && i < argc; i++ ) {

      // show options
      if ( sel == i ) {
	rtext = TTF_RenderText_Blended ( g_tab_font, argv [ i ], selfontcolor );
      } else {
	rtext = TTF_RenderText_Blended ( g_tab_font, argv [ i ], ui_display_context.fontcolor );
      }
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 40 + ( 20 * ( i + 1 - first_visible ) );
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

      //case SDL_KEYUP:
      case SDL_KEYDOWN:

	if ( event.key.keysym.sym == SDLK_UP ) {
	  if ( sel ) {
	    sel--;

	    if ( sel < first_visible ) {
	      first_visible--;
	    }

	  }
	} else if ( event.key.keysym.sym == SDLK_DOWN ) {

	  if ( sel < argc - 1 ) {
	    sel++;

	    // ensure visibility
	    if ( sel >= first_visible + max_visible ) {
	      first_visible++;
	    }

	  }

	} else if ( event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_END ) { // return, or "B"
	  return ( sel );

#if 0
	} else if ( event.key.keysym.sym == SDLK_q ) {
	  exit ( 0 );
#endif

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

  mm_appref_t *i = g_categories [ ui_category ] -> refs;
  while ( i != a ) {
    i = i -> next;
    row++;
  } // while
  row /= ui_display_context.col_max;

  return ( row );
}

unsigned char ui_determine_screen_row ( mm_appref_t *a ) {
  return ( ui_determine_row ( a ) % ui_display_context.row_max );
}

unsigned char ui_determine_screen_col ( mm_appref_t *a ) {
  unsigned int col = 0;

  mm_appref_t *i = g_categories [ ui_category ] -> refs;
  while ( i != a ) {
    i = i -> next;
    col++;
  } // while
  col %= ui_display_context.col_max;

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
	// rescan the dir
	category_fs_restock ( g_categories [ ui_category ] );
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

unsigned char ui_threaded_timer_create ( void ) {

  g_timer_thread = SDL_CreateThread ( (void*)ui_threaded_timer, NULL );

  if ( ! g_timer_thread ) {
    pnd_log ( pndn_error, "ERROR: Couldn't create timer thread\n" );
    return ( 0 );
  }

  return ( 1 );
}

int ui_threaded_timer ( pnd_disco_t *p ) {

  // this timer's job is to ..
  // - do nothing for quite awhile
  // - on wake, post event to SDL event queue, so that main thread will check if SD insert/eject occurred
  // - goto 10

  unsigned int delay_s = 2; // seconds

  while ( 1 ) {

    // pause...
    sleep ( delay_s );

    // .. trigger SD check
    SDL_Event e;
    bzero ( &e, sizeof(SDL_Event) );
    e.type = SDL_USEREVENT;
    e.user.code = sdl_user_checksd;
    SDL_PushEvent ( &e );

  } // while

  return ( 0 );
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

  // reset view
  ui_selected = NULL;
  ui_rows_scrolled_down = 0;
  // set back to first tab, to be safe
  ui_category = 0;
  ui_catshift = 0;

  // do we have a preferred category to jump to?
  char *dc = pnd_conf_get_as_char ( g_conf, "categories.default_cat" );
  if ( dc ) {

    // attempt to find default cat; if we do find it, select it; otherwise
    // default behaviour will pick first cat (ie: usually All)
    unsigned int i;
    for ( i = 0; i < g_categorycount; i++ ) {
      if ( strcasecmp ( g_categories [ i ] -> catname, dc ) == 0 ) {
	ui_category = i;
	// ensure visibility
	unsigned int screen_width = ui_display_context.screen_width;
	unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );
	if ( ui_category > ui_catshift + ( screen_width / tab_width ) - 1 ) {
	  ui_catshift = ui_category - ( screen_width / tab_width ) + 1;
	}
	break;
      }
    }

    if ( i == g_categorycount ) {
      pnd_log ( pndn_warning, "  User defined default category '%s' but not found, so using default behaviour\n", dc );
    }

  } // default cat

  // if we're sent right to a dirbrowser tab, restock it now (normally we restock on entry)
  if ( g_categories [ ui_category ] -> fspath ) {
    printf ( "Restock on start: '%s'\n", g_categories [ ui_category ] -> fspath );
    category_fs_restock ( g_categories [ ui_category ] );
  }

  // redraw all
  render_mask |= CHANGED_EVERYTHING;

  return;
}

unsigned char ui_threaded_defered_icon ( void *p ) {
  extern pnd_box_handle g_active_apps;
  pnd_box_handle h = g_active_apps;

  unsigned char maxwidth, maxheight;
  maxwidth = pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 );
  maxheight = pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_height", 50 );

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

unsigned char ui_pick_skin ( void ) {
#define MAXSKINS 10
  char *skins [ MAXSKINS ];
  unsigned char iter;

  char *searchpath = pnd_conf_get_as_char ( g_conf, "minimenu.skin_searchpath" );
  char tempname [ 100 ];

  iter = 0;

  skins [ iter++ ] = "No skin change";

  SEARCHPATH_PRE
  {
    DIR *d = opendir ( buffer );

    if ( d ) {
      struct dirent *dd;

      while ( ( dd = readdir ( d ) ) ) {

	if ( dd -> d_name [ 0 ] == '.' ) {
	  // ignore
	} else if ( ( dd -> d_type == DT_DIR || dd -> d_type == DT_UNKNOWN ) &&
		    iter < MAXSKINS )
	{
	  snprintf ( tempname, 100, "Skin: %s", dd -> d_name );
	  skins [ iter++ ] = strdup ( tempname );
	}

      }

      closedir ( d );
    }

  }
  SEARCHPATH_POST

  int sel = ui_modal_single_menu ( skins, iter, "Skins", "Enter to select; other to return." );

  // did they pick one?
  if ( sel > 0 ) {
    FILE *f;

    char *s = strdup ( pnd_conf_get_as_char ( g_conf, "minimenu.skin_selected" ) );
    s = pnd_expand_tilde ( s );

    f = fopen ( s, "w" );

    free ( s );

    if ( f ) {
      fprintf ( f, "%s\n", skins [ sel ] + 6 );
      fclose ( f );
    }

    return ( 1 );
  }

  return ( 0 );
}

void ui_aboutscreen ( char *textpath ) {
#define PIXELW 7
#define PIXELH 7
#define MARGINW 3
#define MARGINH 3
#define SCRW 800
#define SCRH 480
#define ROWS SCRH / ( PIXELH + MARGINH )
#define COLS SCRW / ( PIXELW + MARGINW )

  unsigned char pixelboard [ ROWS * COLS ]; // pixel heat
  bzero ( pixelboard, ROWS * COLS );

  SDL_Surface *rtext;
  SDL_Rect r;

  SDL_Color rtextc = { 200, 200, 200, 100 };

  // pixel scroller
  char *textloop [ 500 ];
  unsigned int textmax = 0;
  bzero ( textloop, 500 * sizeof(char*) );

  // cursor scroller
  char cbuffer [ 50000 ];
  bzero ( cbuffer, 50000 );
  unsigned int crevealed = 0;

  FILE *f = fopen ( textpath, "r" );

  if ( ! f ) {
    pnd_log ( pndn_error, "ERROR: Couldn't open about text: %s\n", textpath );
    return;
  }

  char textbuf [ 100 ];
  while ( fgets ( textbuf, 100, f ) ) {

    // add to full buffer
    strncat ( cbuffer, textbuf, 50000 );

    // chomp
    if ( strchr ( textbuf, '\n' ) ) {
      * strchr ( textbuf, '\n' ) = '\0';
    }

    // add to pixel loop
    if ( 1||textbuf [ 0 ] ) {
      textloop [ textmax ] = strdup ( textbuf );
      textmax++;
    }

  } // while fgets

  fclose ( f );

  unsigned int textiter = 0;
  while ( textiter < textmax ) {
    char *text = textloop [ textiter ];

    rtext = NULL;
    if ( text [ 0 ] ) {
      // render to surface
      rtext = TTF_RenderText_Blended ( g_grid_font, text, rtextc );

      // render font to pixelboard
      unsigned int px, py;
      unsigned char *ph;
      unsigned int *pixels = rtext -> pixels;
      unsigned char cr, cg, cb, ca;
      for ( py = 0; py < rtext -> h; py ++ ) {
	for ( px = 0; px < ( rtext -> w > COLS ? COLS : rtext -> w ); px++ ) {

	  SDL_GetRGBA ( pixels [ ( py * rtext -> pitch / 4 ) + px ],
			rtext -> format, &cr, &cg, &cb, &ca );

	  if ( ca != 0 ) {

	    ph = pixelboard + ( /*y offset */ 30 * COLS ) + ( py * COLS ) + px /* / 2 */;

	    if ( *ph < 100 ) {
	      *ph = 100;
	    }

	    ca /= 5;
	    if ( *ph + ca < 250 ) {
	      *ph += ca;
	    }

	  } // got a pixel?

	} // x
      } // y

    } // got text?

    unsigned int runcount = 10;
    while ( runcount-- ) {

      // clear display
      SDL_FillRect( sdl_realscreen, NULL /* whole */, 0 );

      // render pixelboard
      unsigned int x, y;
      unsigned int c;
      for ( y = 0; y < ROWS; y++ ) {
	for ( x = 0; x < COLS; x++ ) {

	  if ( 1||pixelboard [ ( y * COLS ) + x ] ) {

	    // position
	    r.x = x * ( PIXELW + MARGINW );
	    r.y = y * ( PIXELH + MARGINH );
	    r.w = PIXELW;
	    r.h = PIXELH;
	    // heat -> colour
	    c = SDL_MapRGB ( sdl_realscreen -> format, 100 /* r */, 0 /* g */, pixelboard [ ( y * COLS ) + x ] );
	    // render
	    SDL_FillRect( sdl_realscreen, &r /* whole */, c );

	  }

	} // x
      } // y

      // cool pixels
      unsigned char *pc = pixelboard;
      for ( y = 0; y < ROWS; y++ ) {
	for ( x = 0; x < COLS; x++ ) {

	  if ( *pc > 10 ) {
	    (*pc) -= 3;
	  }

	  pc++;
	} // x
      } // y

      // slide pixels upwards
      memmove ( pixelboard, pixelboard + COLS, ( COLS * ROWS ) - COLS );

      // render actual readable text
      {

	// display up to cursor
	SDL_Rect dest;
	unsigned int cdraw = 0;
	SDL_Surface *cs;
	char ctb [ 2 ];

	if ( crevealed > 200 ) {
	  cdraw = crevealed - 200;
	}

	dest.x = 400;
	dest.y = 20;

	for ( ; cdraw < crevealed; cdraw++ ) {
	  ctb [ 0 ] = cbuffer [ cdraw ];
	  ctb [ 1 ] = '\0';
	  // move over or down
	  if ( cbuffer [ cdraw ] == '\n' ) {
	    // EOL
	    dest.x = 400;
	    dest.y += 14;

	    if ( dest.y > 450 ) {
	      dest.y = 450;
	    }

	  } else {
	    // draw the char
	    cs = TTF_RenderText_Blended ( g_tab_font, ctb, rtextc );
	    if ( cs ) {
	      SDL_BlitSurface ( cs, NULL /* all */, sdl_realscreen, &dest );
	      SDL_FreeSurface ( cs );
	      // over
	      dest.x += cs -> w;
	    }
	  }

	}

	dest.w = 10;
	dest.h = 20;
	SDL_FillRect ( sdl_realscreen, &dest /* whole */, 220 );

	// increment cursor to next character
	if ( cbuffer [ crevealed ] != '\0' ) {
	  crevealed++;
	}

      } // draw cursor text

      // reveal
      //
      SDL_UpdateRect ( sdl_realscreen, 0, 0, 0, 0 ); // whole screen

      usleep ( 50000 );

      // any button? if so, about
      {
	SDL_PumpEvents();

	SDL_Event e;

	if ( SDL_PeepEvents ( &e, 1, SDL_GETEVENT, SDL_EVENTMASK(/*SDL_KEYUP|*/SDL_KEYDOWN) ) > 0 ) {
	  return;
	}

      }

    } // while cooling

    if ( rtext ) {
      SDL_FreeSurface ( rtext );
    }

    textiter++;
  } // while more text

  // free up
  unsigned int i;
  for ( i = 0; i < textmax; i++ ) {
    if ( textloop [ i ] ) {
      free ( textloop [ i ] );
      textloop [ i ] = 0;
    }
  }

  return;
}

void ui_revealscreen ( void ) {
  char *labels [ 500 ];
  unsigned int labelmax = 0;
  unsigned int i;
  char fulllabel [ 200 ];

  if ( ! category_count ( CFHIDDEN ) ) {
    return; // nothing to do
  }

  // switch to hidden categories
  category_publish ( CFHIDDEN, NULL );

  // build up labels to show in menu
  for ( i = 0; i < g_categorycount; i++ ) {

    if ( g_categories [ i ] -> parent_catname ) {
      sprintf ( fulllabel, "%s [%s]", g_categories [ i ] -> catname, g_categories [ i ] -> parent_catname );
    } else {
      sprintf ( fulllabel, "%s", g_categories [ i ] -> catname );
    }

    labels [ labelmax++ ] = strdup ( fulllabel );
  }

  // show menu
  int sel = ui_modal_single_menu ( labels, labelmax, "Temporary Category Reveal",
				   "Enter to select; other to return." );

  // if selected, try to set this guy to visible
  if ( sel >= 0 ) {

    // fix up category name, if its been hacked
    if ( strchr ( g_categories [ sel ] -> catname, '.' ) ) {
      char *t = g_categories [ sel ] -> catname;
      g_categories [ sel ] -> catname = strdup ( strchr ( g_categories [ sel ] -> catname, '.' ) + 1 );
      free ( t );
    }

    // reflag this guy to be visible
    g_categories [ sel ] -> catflags = CFNORMAL;

    // switch to the new category.. cache name.
    char *switch_to_name = g_categories [ sel ] -> catname;

    // republish categories
    category_publish ( CFNORMAL, NULL );

    // switch to the new category.. with the cached name!
    ui_category = category_index ( switch_to_name );

    // ensure visibility
    unsigned int screen_width = ui_display_context.screen_width;
    unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );
    if ( ui_category > ui_catshift + ( screen_width / tab_width ) - 1 ) {
      ui_catshift = ui_category - ( screen_width / tab_width ) + 1;
    }

    // redraw tabs
    render_mask |= CHANGED_CATEGORY;
  }

  for ( i = 0; i < g_categorycount; i++ ) {
    free ( labels [ i ] );
  }

  return;
}

void ui_recache_context ( ui_context_t *c ) {

  c -> screen_width = pnd_conf_get_as_int_d ( g_conf, "display.screen_width", 800 );

  c -> font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  c -> font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  c -> font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  c -> font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  c -> grid_offset_x = pnd_conf_get_as_int ( g_conf, "grid.grid_offset_x" );
  c -> grid_offset_y = pnd_conf_get_as_int ( g_conf, "grid.grid_offset_y" );

  c -> icon_offset_x = pnd_conf_get_as_int ( g_conf, "grid.icon_offset_x" );
  c -> icon_offset_y = pnd_conf_get_as_int ( g_conf, "grid.icon_offset_y" );
  c -> icon_max_width = pnd_conf_get_as_int ( g_conf, "grid.icon_max_width" );
  c -> icon_max_height = pnd_conf_get_as_int ( g_conf, "grid.icon_max_height" );
  c -> sel_icon_offset_x = pnd_conf_get_as_int_d ( g_conf, "grid.sel_offoffset_x", 0 );
  c -> sel_icon_offset_y = pnd_conf_get_as_int_d ( g_conf, "grid.sel_offoffset_y", 0 );

  c -> text_width = pnd_conf_get_as_int ( g_conf, "grid.text_width" );
  c -> text_clip_x = pnd_conf_get_as_int ( g_conf, "grid.text_clip_x" );
  c -> text_offset_x = pnd_conf_get_as_int ( g_conf, "grid.text_offset_x" );
  c -> text_offset_y = pnd_conf_get_as_int ( g_conf, "grid.text_offset_y" );
  c -> text_hilite_offset_y = pnd_conf_get_as_int ( g_conf, "grid.text_hilite_offset_y" );

  c -> row_max = pnd_conf_get_as_int_d ( g_conf, "grid.row_max", 4 );
  c -> col_max = pnd_conf_get_as_int_d ( g_conf, "grid.col_max", 5 );

  c -> cell_width = pnd_conf_get_as_int ( g_conf, "grid.cell_width" );
  c -> cell_height = pnd_conf_get_as_int ( g_conf, "grid.cell_height" );

  c -> arrow_bar_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_x", 450 );
  c -> arrow_bar_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_y", 100 );
  c -> arrow_bar_clip_w = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_clip_w", 10 );
  c -> arrow_bar_clip_h = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_clip_h", 100 );
  c -> arrow_up_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_x", 450 );
  c -> arrow_up_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_y", 80 );
  c -> arrow_down_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_x", 450 );
  c -> arrow_down_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_y", 80 );

  // font colour
  SDL_Color tmp = { c -> font_rgba_r, c -> font_rgba_g, c -> font_rgba_b, c -> font_rgba_a };
  c -> fontcolor = tmp;

  // now that we've got 'normal' (detail pane shown) param's, lets check if detail pane
  // is hidden; if so, override some values with those alternate skin values where possible.
  if ( ui_detail_hidden ) {
    // if detail panel is hidden, and theme cannot support it, unhide the bloody thing. (This may help
    // when someone is amid theme hacking or changing.)
    if ( ! ui_is_detail_hideable() ) {
      ui_detail_hidden = 0;
    }

    // still hidden?
    if ( ui_detail_hidden ) {

      c -> row_max = pnd_conf_get_as_int_d ( g_conf, "grid.row_max_w", c -> row_max );
      c -> col_max = pnd_conf_get_as_int_d ( g_conf, "grid.col_max_w", c -> col_max );

      c -> cell_width = pnd_conf_get_as_int_d ( g_conf, "grid.cell_width_w", c -> cell_width );
      c -> cell_height = pnd_conf_get_as_int_d ( g_conf, "grid.cell_height_w", c -> cell_height );

      c -> arrow_bar_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_x_w", 450 );
      c -> arrow_bar_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_bar_y_w", 100 );
      c -> arrow_up_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_x_w", 450 );
      c -> arrow_up_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_y_w", 80 );
      c -> arrow_down_x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_x_w", 450 );
      c -> arrow_down_y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_y_w", 80 );

    } // if detail hidden.. still.

  } // if detail hidden

  return;
}

unsigned char ui_is_detail_hideable ( void ) {

  // if skin has a bit of info for wide-mode, we assume wide-mode is available
  if ( pnd_conf_get_as_char ( g_conf, "grid.row_max_w" ) != NULL ) {
    return ( 1 );
  }

  // else not!
  return ( 0 );
}

void ui_toggle_detail_pane ( void ) {

  // no bitmask trickery here; I like it to be stand-out obvious at 3am.

  if ( ui_detail_hidden ) {
    ui_detail_hidden = 0;
  } else {
    ui_detail_hidden = 1;
  }

  // repull skin config
  ui_recache_context ( &ui_display_context );

  // redraw
  render_mask |= CHANGED_EVERYTHING;

  return;
}

void ui_menu_context ( mm_appref_t *a ) {

  unsigned char rescan_apps = 0;
  unsigned char context_alive = 1;

  enum {
    context_done = 0,
    context_file_info,
    context_file_delete,
    context_app_info,
    context_app_hide,
    context_app_recategorize,
    context_app_recategorize_sub,
    context_app_rename,
    context_app_cpuspeed,
    context_app_run,
    context_menu_max
  };

  char *verbiage[] = {
    "Done (return to grid)",      // context_done
    "Get info about file/dir",    // context_file_info
    "Delete file/dir",            // context_file_delete
    "Get info",                   // context_app_info
    "Hide application",           //             hide
    "Recategorize",               //             recategorize
    "Recategorize subcategory",   //             recategorize
    "Change displayed title",     //             rename
    "Set CPU speed for launch",   //             cpuspeed
    "Run application"             //             run
  };

  unsigned short int menu [ context_menu_max ];
  char *menustring [ context_menu_max ];
  unsigned char menumax = 0;

  // ++ done
  menu [ menumax ] = context_done; menustring [ menumax++ ] = verbiage [ context_done ];

  // hook up appropriate menu options based on tab-type and object-type
  if ( g_categories [ ui_category ] -> fspath ) {
    return; // TBD
    menu [ menumax ] = context_file_info; menustring [ menumax++ ] = verbiage [ context_file_info ];
    menu [ menumax ] = context_file_delete; menustring [ menumax++ ] = verbiage [ context_file_delete ];
  } else {
    //menu [ menumax ] = context_app_info; menustring [ menumax++ ] = verbiage [ context_app_info ];
    menu [ menumax ] = context_app_hide; menustring [ menumax++ ] = verbiage [ context_app_hide ];
    menu [ menumax ] = context_app_recategorize; menustring [ menumax++ ] = verbiage [ context_app_recategorize ];
    menu [ menumax ] = context_app_recategorize_sub; menustring [ menumax++ ] = verbiage [ context_app_recategorize_sub ];
    menu [ menumax ] = context_app_rename; menustring [ menumax++ ] = verbiage [ context_app_rename ];
    menu [ menumax ] = context_app_cpuspeed; menustring [ menumax++ ] = verbiage [ context_app_cpuspeed ];
    menu [ menumax ] = context_app_run; menustring [ menumax++ ] = verbiage [ context_app_run ];
  }

  // operate the menu
  while ( context_alive ) {

    int sel = ui_modal_single_menu ( menustring, menumax, a -> ref -> title_en ? a -> ref -> title_en : "Quickpick Menu" /* title */, "B or Enter; other to cancel." /* footer */ );

    if ( sel < 0 ) {
      context_alive = 0;

    } else {

      switch ( menu [ sel ] ) {

      case context_done:
	context_alive = 0;
	break;

      case context_file_info:
	break;

      case context_file_delete:
	//ui_menu_twoby ( "Delete - Are you sure?", "B/enter; other to cancel", "Delete", "Do not delete" );
	break;

      case context_app_info:
	break;

      case context_app_hide:
	{
	  // determine key
	  char confkey [ 1000 ];
	  snprintf ( confkey, 999, "%s.%s", "appshow", a -> ref -> unique_id );

	  // turn app 'off'
	  pnd_conf_set_char ( g_conf, confkey, "0" );

	  // write conf, so it will take next time
	  conf_write ( g_conf, conf_determine_location ( g_conf ) );

	  // request rescan and wrap up
	  rescan_apps++;
	  context_alive = 0; // nolonger visible, so lets just get out

	}
    
	break;

      case context_app_recategorize:
	{
	  char *opts [ 250 ];
	  unsigned char optmax = 0;
	  unsigned char i;

	  i = 0;
	  while ( 1 ) {

	    if ( ! freedesktop_complete [ i ].cat ) {
	      break;
	    }

	    if ( ! freedesktop_complete [ i ].parent_cat ) {
	      opts [ optmax++ ] = freedesktop_complete [ i ].cat;
	    }

	    i++;
	  } // while

	  char prompt [ 101 ];
	  snprintf ( prompt, 100, "Pick category [%s]", a -> ref -> main_category ? a -> ref -> main_category : "NoParentCategory" );

	  int sel = ui_modal_single_menu ( opts, optmax, prompt /*"Select parent category"*/, "Enter to select; other to skip." );

	  if ( sel >= 0 ) {
	    char confirm [ 1001 ];
	    snprintf ( confirm, 1000, "Confirm: %s", opts [ sel ] );

	    if ( ui_menu_twoby ( confirm, "B/enter; other to cancel", "Confirm categorization", "Do not set category" ) == 1 ) {
	      ovr_replace_or_add ( a, "maincategory", opts [ sel ] );
	      rescan_apps++;
	    }

	  }

	}
	break;

      case context_app_recategorize_sub:
	{
	  char *opts [ 250 ];
	  unsigned char optmax = 0;
	  unsigned char i;

	  i = 0;
	  while ( 1 ) {

	    if ( ! freedesktop_complete [ i ].cat ) {
	      break;
	    }

	    if ( freedesktop_complete [ i ].parent_cat ) {
	      opts [ optmax++ ] = freedesktop_complete [ i ].cat;
	    }

	    i++;
	  } // while

	  char prompt [ 101 ];
	  snprintf ( prompt, 100, "Pick subcategory [%s]", a -> ref -> main_category1 ? a -> ref -> main_category1 : "NoSubcategory" );

	  int sel = ui_modal_single_menu ( opts, optmax, prompt /*"Select subcategory"*/, "Enter to select; other to skip." );

	  if ( sel >= 0 ) {
	    char confirm [ 1001 ];
	    snprintf ( confirm, 1000, "Confirm: %s", opts [ sel ] );

	    if ( ui_menu_twoby ( confirm, "B/enter; other to cancel", "Confirm sub-categorization", "Do not set sub-category" ) == 1 ) {
	      ovr_replace_or_add ( a, "maincategorysub1", opts [ sel ] );
	      rescan_apps++;
	    }

	  }

	}
	break;

      case context_app_rename:
	{
	  char namebuf [ 101 ];
	  unsigned char changed;

	  changed = ui_menu_get_text_line ( "Rename application", "Use keyboard; Enter when done.",
					    a -> ref -> title_en ? a -> ref -> title_en : "blank", namebuf, 30, 0 /* alphanumeric */ );

	  if ( changed ) {
	    char confirm [ 1001 ];
	    snprintf ( confirm, 1000, "Confirm: %s", namebuf );

	    if ( ui_menu_twoby ( confirm, "B/enter; other to cancel", "Confirm rename", "Do not rename" ) == 1 ) {
	      ovr_replace_or_add ( a, "title", namebuf );
	      rescan_apps++;
	    }

	  }

	}

	break;

      case context_app_cpuspeed:
	{
	  char namebuf [ 101 ];
	  unsigned char changed;

	  changed = ui_menu_get_text_line ( "Specify runspeed", "Use keyboard; Enter when done.",
					    a -> ref -> clockspeed ? a -> ref -> clockspeed : "500", namebuf, 6, 1 /* numeric */ );

	  if ( changed ) {
	    char confirm [ 1001 ];
	    snprintf ( confirm, 1000, "Confirm: %s", namebuf );

	    if ( ui_menu_twoby ( confirm, "B/enter; other to cancel", "Confirm clockspeed", "Do not set" ) == 1 ) {
	      ovr_replace_or_add ( a, "clockspeed", namebuf );
	      rescan_apps++;
	    }

	  }

	}

	break;

      case context_app_run:
	ui_push_exec();
	break;

      default:
	return;

      } // switch

    } // if useful return

  } // menu is alive?

  // rescan apps?
  if ( rescan_apps ) {
    applications_free();
    applications_scan();
  }

  return;
}

unsigned char ui_menu_twoby ( char *title, char *footer, char *one, char *two ) {
  char *opts [ 3 ];
  opts [ 0 ] = one;
  opts [ 1 ] = two;
  int sel = ui_modal_single_menu ( opts, 2, title, footer );
  if ( sel < 0 ) {
    return ( 0 );
  }
  return ( sel + 1 );
}

unsigned char ui_menu_get_text_line ( char *title, char *footer, char *initialvalue,
				      char *r_buffer, unsigned char maxlen, unsigned char numbersonlyp )
{
  SDL_Rect rects [ 40 ];
  SDL_Rect *dest = rects;
  SDL_Rect src;
  SDL_Surface *rtext;

  char hacktext [ 1024 ];
  unsigned char shifted = 0;

  bzero ( rects, sizeof(SDL_Rect) * 40 );

  if ( initialvalue ) {
    strncpy ( r_buffer, initialvalue, maxlen );
  } else {
    bzero ( r_buffer, maxlen );
  }

  while ( 1 ) {

    // clear
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
    dest -> w = ((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> w;
    dest -> h = ((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> h;
    SDL_FillRect( sdl_realscreen, dest, 0 );

    // show dialog background
    if ( g_imagecache [ IMG_DETAIL_BG ].i ) {
      src.x = 0;
      src.y = 0;
      src.w = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_BG ].i)) -> w;
      src.h = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_BG ].i)) -> h;
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      dest++;
    }

    // show dialog frame
    if ( g_imagecache [ IMG_DETAIL_PANEL ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_PANEL ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;
    }

    // show header
    if ( title ) {
      rtext = TTF_RenderText_Blended ( g_tab_font, title, ui_display_context.fontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 20;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show footer
    if ( footer ) {
      rtext = TTF_RenderText_Blended ( g_tab_font, footer, ui_display_context.fontcolor );
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) +
	((SDL_Surface*) g_imagecache [ IMG_DETAIL_PANEL ].i) -> h
	- 60;
      SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
      SDL_FreeSurface ( rtext );
      dest++;
    }

    // show text line - and embed cursor
    dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 ) + 20;
    dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 ) + 40 + ( 20 * ( 0/*i*/ + 1 - 0/*first_visible*/ ) );

    strncpy ( hacktext, r_buffer, 1000 );
    strncat ( hacktext, "\n", 1000 ); // add [] in most fonts

    rtext = TTF_RenderText_Blended ( g_tab_font, hacktext, ui_display_context.fontcolor );
    SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, dest );
    SDL_FreeSurface ( rtext );
    dest++;

    // update all the rects and send it all to sdl
    SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );
    dest = rects;

    // check for input
    SDL_Event event;
    while ( SDL_WaitEvent ( &event ) ) {

      switch ( event.type ) {

      case SDL_KEYUP:
	if ( event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT ) {
	  shifted = 0;
	}
	break;

      case SDL_KEYDOWN:

	if ( event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_BACKSPACE ) {
	  char *eol = strchr ( r_buffer, '\0' );
	  *( eol - 1 ) = '\0';
	} else if ( event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_END ) { // return, or "B"
	  return ( 1 );

	} else if ( event.key.keysym.sym == SDLK_LSHIFT || event.key.keysym.sym == SDLK_RSHIFT ) {
	  shifted = 1;

	} else if ( event.key.keysym.sym == SDLK_ESCAPE ||
		    event.key.keysym.sym == SDLK_PAGEUP ||
		    event.key.keysym.sym == SDLK_PAGEDOWN ||
		    event.key.keysym.sym == SDLK_HOME
		  )
	{
	  return ( 0 );

	} else {

	  if ( isprint(event.key.keysym.sym) ) {

	    unsigned char good = 1;

	    if ( numbersonlyp && ( ! isdigit(event.key.keysym.sym) ) ) {
	      good = 0;
	    }

	    if ( maxlen && strlen(r_buffer) >= maxlen ) {
	      good = 0;
	    }

	    if ( good ) {
	      char b [ 2 ] = { '\0', '\0' };
	      if ( shifted ) {
		b [ 0 ] = toupper ( event.key.keysym.sym );
	      } else {
		b [ 0 ] = event.key.keysym.sym;
	      }
	      strncat ( r_buffer, b, maxlen );
	    } // good?

	  } // printable?

	}

	break;

      } // switch

      break;

    } // while waiting for input

  } // while
  
  return ( 0 );
}

unsigned char ovr_replace_or_add ( mm_appref_t *a, char *keybase, char *newvalue ) {
  printf ( "setting %s:%u - '%s' to '%s' - %s/%s\n", a -> ref -> title_en, a -> ref -> subapp_number, keybase, newvalue, a -> ref -> object_path, a -> ref -> object_filename );

  char fullpath [ PATH_MAX ];

  sprintf ( fullpath, "%s/%s", a -> ref -> object_path, a -> ref -> object_filename );
  char *dot = strrchr ( fullpath, '.' );
  if ( dot ) {
    sprintf ( dot, PXML_SAMEPATH_OVERRIDE_FILEEXT );
  } else {
    fprintf ( stderr, "ERROR: Bad pnd-path in disco_t! %s\n", fullpath );
    return ( 0 );
  }

  struct stat statbuf;

  if ( stat ( fullpath, &statbuf ) == 0 ) {
    // file exists
    pnd_conf_handle h;

    h = pnd_conf_fetch_by_path ( fullpath );

    if ( ! h ) {
      return ( 0 ); // fail!
    }

    char key [ 101 ];
    snprintf ( key, 100, "Application-%u.%s", a -> ref -> subapp_number, keybase );

    pnd_conf_set_char ( h, key, newvalue );

    return ( pnd_conf_write ( h, fullpath ) );

  } else {
    // file needs to be created - easy!

    FILE *f = fopen ( fullpath, "w" );

    if ( f ) {
      fprintf ( f, "Application-%u.%s\t%s\n", a -> ref -> subapp_number, keybase, newvalue );
      fclose ( f );

    } else {
      return ( 0 ); // fail!
    }

  } // new or used?

  return ( 1 );
}
