
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "SDL_gfxPrimitives.h"
#include "SDL_rotozoom.h"

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_apps.h"

#include "mmenu.h"
#include "mmcat.h"
#include "mmcache.h"
#include "mmui.h"
#include "mmwrapcmd.h"

/* SDL
 */
SDL_Surface *sdl_realscreen = NULL;
unsigned int sdl_ticks = 0;

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

static unsigned int ui_timer ( unsigned int interval ) {
  sdl_ticks++;
  return ( interval );
}

unsigned char ui_setup ( void ) {

  /* set up SDL
   */

  SDL_Init ( SDL_INIT_EVERYTHING | SDL_INIT_NOPARACHUTE );

  SDL_SetTimer ( 30, ui_timer ); // 30fps

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
  sprintf ( fullpath, "%s/%s", pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ), pnd_conf_get_as_char ( g_conf, "minimenu.font" ) );
  g_big_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "minimenu.font_ptsize", 24 ) );
  if ( ! g_big_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "minimenu.font" ), pnd_conf_get_as_int_d ( g_conf, "minimenu.font_ptsize", 24 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // grid font
  sprintf ( fullpath, "%s/%s", pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ), pnd_conf_get_as_char ( g_conf, MMENU_GRID_FONT ) );
  g_grid_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, MMENU_GRID_FONTSIZE, 10 ) );
  if ( ! g_grid_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, MMENU_GRID_FONT ), pnd_conf_get_as_int_d ( g_conf, MMENU_GRID_FONTSIZE, 10 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // detailtext font
  sprintf ( fullpath, "%s/%s", pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ), pnd_conf_get_as_char ( g_conf, "detailtext.font" ) );
  g_detailtext_font = TTF_OpenFont ( fullpath, pnd_conf_get_as_int_d ( g_conf, "detailtext.font_ptsize", 10 ) );
  if ( ! g_detailtext_font ) {
    pnd_log ( pndn_error, "ERROR: Couldn't load font '%s' for size %u\n",
	      pnd_conf_get_as_char ( g_conf, "detailtext.font" ), pnd_conf_get_as_int_d ( g_conf, "detailtext.font_ptsize", 10 ) );
    return ( 0 ); // couldn't set up SDL TTF
  }

  // tab font
  sprintf ( fullpath, "%s/%s", pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ), pnd_conf_get_as_char ( g_conf, "tabs.font" ) );
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
  { IMG_ICON_MISSING,         "graphics.IMG_ICON_MISSING" },
  { IMG_SELECTED_HILITE,      "graphics.IMG_SELECTED_HILITE" },
  { IMG_PREVIEW_MISSING,      "graphics.IMG_PREVIEW_MISSING" },
  { IMG_ARROW_UP,             "graphics.IMG_ARROW_UP", },
  { IMG_ARROW_DOWN,           "graphics.IMG_ARROW_DOWN", },
  { IMG_ARROW_SCROLLBAR,      "graphics.IMG_ARROW_SCROLLBAR", },
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

    sprintf ( fullpath, "%s/%s", basepath, filename );

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
  SDL_SetAlpha ( g_imagecache [ IMG_DETAIL_BG ].i, SDL_SRCALPHA, pnd_conf_get_as_int_d ( g_conf, "display.detail_bg_alpha", 50 ) );

  return ( 1 );
} // ui_imagecache

void ui_render ( unsigned int render_mask ) {

  // 800x480:
  // divide width: 550 / 250
  // divide left side: 5 columns == 110px width
  //   20px buffer either side == 70px wide icon + 20 + 20?

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

  // ensure selection is visible
  if ( ui_selected ) {

    int index = ui_selected_index();
    int topleft = col_max * ui_rows_scrolled_down;
    int botright = ( col_max * ( ui_rows_scrolled_down + row_max ) - 1 );

    pnd_log ( PND_LOG_DEFAULT, "index %u tl %u br %u\n", index, topleft, botright );

    if ( index < topleft ) {
      ui_rows_scrolled_down -= pnd_conf_get_as_int_d ( g_conf, "grid.scroll_increment", 1 );
    } else if ( index > botright ) {
      ui_rows_scrolled_down += pnd_conf_get_as_int_d ( g_conf, "grid.scroll_increment", 1 );
    }

    if ( ui_rows_scrolled_down < 0 ) {
      ui_rows_scrolled_down = 0;
    } else if ( ui_rows_scrolled_down > icon_rows ) {
      ui_rows_scrolled_down = icon_rows;
    }

  } // ensire visible

  // render background
  if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
    dest -> x = 0;
    dest -> y = 0;
    dest -> w = sdl_realscreen -> w;
    dest -> h = sdl_realscreen -> h;
    SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, NULL /* whole image */, sdl_realscreen, dest /* 0,0 */ );
    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
    dest++;
  }

  // tabmask
  if ( g_imagecache [ IMG_BACKGROUND_TABMASK ].i ) {
    dest -> x = 0;
    dest -> y = 0;
    dest -> w = sdl_realscreen -> w;
    dest -> h = sdl_realscreen -> h;
    SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_TABMASK ].i, NULL /* whole image */, sdl_realscreen, dest /* 0,0 */ );
    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
    dest++;
  }

  // tabs
  if ( g_imagecache [ IMG_TAB_SEL ].i && g_imagecache [ IMG_TAB_UNSEL ].i ) {
    unsigned int tab_width = pnd_conf_get_as_int ( g_conf, "tabs.tab_width" );
    unsigned int tab_height = pnd_conf_get_as_int ( g_conf, "tabs.tab_height" );
    unsigned int tab_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_x" );
    unsigned int tab_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.tab_offset_y" );
    unsigned int text_offset_x = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_x" );
    unsigned int text_offset_y = pnd_conf_get_as_int ( g_conf, "tabs.text_offset_y" );
    unsigned int text_width = pnd_conf_get_as_int ( g_conf, "tabs.text_width" );

    for ( col = ui_catshift;
	  col < ( 
		   ( screen_width / tab_width ) < g_categorycount ? ( screen_width / tab_width ) + ui_catshift : g_categorycount + ui_catshift
		);
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
      src.h = tab_height;
      dest -> x = tab_offset_x + ( col * tab_width );
      dest -> y = tab_offset_y;
      //pnd_log ( pndn_debug, "tab %u at %ux%u\n", col, dest.x, dest.y );
      SDL_BlitSurface ( s, &src, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;

      // draw text
      SDL_Surface *rtext;
      SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
      rtext = TTF_RenderText_Blended ( g_tab_font, g_categories [ col ].catname, tmpfontcolor );
      src.x = 0;
      src.y = 0;
      src.w = rtext -> w < text_width ? rtext -> w : text_width;
      src.h = rtext -> h;
      dest -> x = tab_offset_x + ( col * tab_width ) + text_offset_x;
      dest -> y = tab_offset_y + text_offset_y;
      SDL_BlitSurface ( rtext, &src, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;

    } // for

  } // tabs

  // scroll bars and arrows
  {
    unsigned char show_bar = 0;

    // up?
    if ( ui_rows_scrolled_down && g_imagecache [ IMG_ARROW_UP ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_x", 450 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_up_y", 80 );
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_UP ].i, NULL /* whole image */, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;

      show_bar = 1;
    }

    // down?
    if ( ui_rows_scrolled_down + row_max < icon_rows && g_imagecache [ IMG_ARROW_DOWN ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_x", 450 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "grid.arrow_down_y", 80 );
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_DOWN ].i, NULL /* whole image */, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    } // bar

  } // scroll bars

  // render detail pane bg
  if ( pnd_conf_get_as_int_d ( g_conf, "detailpane.show", 1 ) ) {

    if ( g_imagecache [ IMG_DETAIL_BG ].i ) {
      src.x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      src.y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      src.w = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> w;
      src.h = ((SDL_Surface*)(g_imagecache [ IMG_DETAIL_PANEL ].i)) -> h;
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_BG ].i, &src, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    }

    // render detail pane
    if ( g_imagecache [ IMG_DETAIL_PANEL ].i ) {
      dest -> x = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_x", 460 );
      dest -> y = pnd_conf_get_as_int_d ( g_conf, "detailpane.pane_offset_y", 60 );
      SDL_BlitSurface ( g_imagecache [ IMG_DETAIL_PANEL ].i, NULL /* whole image */, sdl_realscreen, dest );
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    }

  } // detailpane frame/bg

  // anything to render?
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
	    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
	    dest++;
	    // text
	    dest -> x = grid_offset_x + ( col * cell_width ) + text_clip_x;
	    dest -> y = grid_offset_y + ( displayrow * cell_height ) + pnd_conf_get_as_int ( g_conf, "grid.text_hilite_offset_y" );
	    SDL_BlitSurface ( g_imagecache [ IMG_SELECTED_HILITE ].i, NULL /* all */, sdl_realscreen, dest );
	    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
	    dest++;
	  } // selected?

	  // show icon
	  mm_cache_t *ic = cache_query_icon ( appiter -> ref -> unique_id );
	  SDL_Surface *iconsurface;
	  if ( ic ) {
	    iconsurface = ic -> i;
	  } else {
	    pnd_log ( pndn_warning, "WARNING: TBD: Need Missin-icon icon for '%s'\n", IFNULL(appiter -> ref -> title_en,"No Name") );
	    iconsurface = g_imagecache [ IMG_ICON_MISSING ].i;
	  }
	  if ( iconsurface ) {
	    //pnd_log ( pndn_debug, "Got an icon for '%s'\n", IFNULL(appiter -> ref -> title_en,"No Name") );

	    src.x = 0;
	    src.y = 0;
	    src.w = 60;
	    src.h = 60;
	    dest -> x = grid_offset_x + ( col * cell_width ) + icon_offset_x;
	    dest -> y = grid_offset_y + ( displayrow * cell_height ) + icon_offset_y;

	    SDL_BlitSurface ( iconsurface, &src, sdl_realscreen, dest );
	    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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
	    //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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

  // detail panel
  if ( ui_selected ) {

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
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
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
      //SDL_UpdateRects ( sdl_realscreen, 1, &dest );
      dest++;
    }

  } // selected?

  // update all the rects and send it all to sdl
  SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );

} // ui_render

void ui_process_input ( unsigned char block_p ) {
  SDL_Event event;

  unsigned char ui_event = 0; // if we get a ui event, flip to 1 and break
  static ui_sdl_button_e ui_mask = uisb_none; // current buttons down

  while ( ! ui_event &&
	  block_p ? SDL_WaitEvent ( &event ) : SDL_PollEvent ( &event ) )
  {

    switch ( event.type ) {

#if 0 // joystick motion
    case SDL_JOYAXISMOTION:

      pnd_log ( PND_LOG_DEFAULT, "joystick axis\n" );

	if ( event.jaxis.axis == 0 ) {
	  // horiz
	  if ( event.jaxis.value < 0 ) {
	    ui_push_left();
	    pnd_log ( PND_LOG_DEFAULT, "joystick axis - LEFT\n" );
	  } else if ( event.jaxis.value > 0 ) {
	    ui_push_right();
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

      pnd_log ( pndn_debug, "key up %u\n", event.key.keysym.sym );

      // directional
      if ( event.key.keysym.sym == SDLK_RIGHT ) {
	ui_push_right();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_LEFT ) {
	ui_push_left();
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
      } else if ( event.key.keysym.sym == SDLK_z ) {
	ui_push_ltrigger();
	ui_event++;
      } else if ( event.key.keysym.sym == SDLK_x ) {
	ui_push_rtrigger();
	ui_event++;
      }

      // extras
      if ( event.key.keysym.sym == SDLK_q ) {
	emit_and_quit ( MM_QUIT );
      }

      break;
#endif

#if 0 // mouse / touchscreen
    case SDL_MOUSEBUTTONDOWN:
      if ( event.button.button == SDL_BUTTON_LEFT ) {
	cb_pointer_press ( gc, event.button.x / g_scale, event.button.y / g_scale );
	ui_event++;
      }
      break;

    case SDL_MOUSEBUTTONUP:
      if ( event.button.button == SDL_BUTTON_LEFT ) {
	cb_pointer_release ( gc, event.button.x / g_scale, event.button.y / g_scale );
	retval |= STAT_pen;
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

void ui_push_left ( void ) {

  if ( ! ui_selected ) {
    ui_push_right();
    return;
  }

  // are we alreadt at first item?
  if ( g_categories [ ui_category ].refs == ui_selected ) {
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

  return;
}

void ui_push_right ( void ) {

  if ( ui_selected ) {

    if ( ui_selected -> next ) {
      ui_selected = ui_selected -> next;
    }

  } else {
    ui_selected = g_categories [ ui_category ].refs;
  }

  return;
}

void ui_push_up ( void ) {
  unsigned char col_max = pnd_conf_get_as_int ( g_conf, MMENU_DISP_COLMAX );

  while ( col_max ) {
    ui_push_left();
    col_max--;
  }

  return;
}

void ui_push_down ( void ) {
  unsigned char col_max = pnd_conf_get_as_int ( g_conf, MMENU_DISP_COLMAX );

  if ( ui_selected ) {
    while ( col_max ) {
      ui_push_right();
      col_max--;
    }
  } else {
    ui_push_right();
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
		    atoi ( ui_selected -> ref -> clockspeed ),
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
  }

  return;
}

void ui_push_rtrigger ( void ) {
  unsigned char oldcat = ui_category;

  if ( ui_category < ( g_categorycount - 1 ) ) {
    ui_category++;
  } else {
    if ( pnd_conf_get_as_int_d ( g_conf, "tabs.wraparound", 0 ) > 0 ) {
      ui_category = 0;
    }
  }

  if ( oldcat != ui_category ) {
    ui_selected = NULL;
  }

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

  pnd_log ( pndn_debug, "  Upscaling; scale factor %f\n", scale );
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

  // render icon
  if ( g_imagecache [ IMG_ICON_MISSING ].i ) {
    dest.x = rtext -> w + 30;
    dest.y = 20;
    SDL_BlitSurface ( g_imagecache [ IMG_ICON_MISSING ].i, NULL, sdl_realscreen, &dest );
    SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  }

  return;
}

void ui_cachescreen ( unsigned char clearscreen ) {

  SDL_Rect dest;

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
  rtext = TTF_RenderText_Blended ( g_big_font, "Caching applications artwork...", tmpfontcolor );
  if ( clearscreen ) {
    dest.x = 20;
    dest.y = 20;
  } else {
    dest.x = 20;
    dest.y = 40;
  }
  SDL_BlitSurface ( rtext, NULL /* full src */, sdl_realscreen, &dest );
  SDL_UpdateRects ( sdl_realscreen, 1, &dest );

  // render icon
  if ( g_imagecache [ IMG_ICON_MISSING ].i ) {
    dest.x = rtext -> w + 30 + stepx;
    dest.y = 20;
    SDL_BlitSurface ( g_imagecache [ IMG_ICON_MISSING ].i, NULL, sdl_realscreen, &dest );
    SDL_UpdateRects ( sdl_realscreen, 1, &dest );
  }

  // move across
  stepx += 20;

  if ( stepx > 350 ) {
    stepx = 0;
  }

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
