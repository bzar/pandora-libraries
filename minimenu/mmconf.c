
#include <stdio.h>
#include <limits.h> /* for PATH_MAX */
#define __USE_GNU /* for strndup */
#include <string.h> /* for strdup */

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"

#include "pnd_container.h"
#include "pnd_conf.h"
#include "pnd_discovery.h"

#include "mmenu.h"
#include "mmconf.h"
#include "mmcat.h"
#include "mmui.h"
#include "mmwrapcmd.h"

static unsigned char conf_render_text ( TTF_Font *f, char *buffer, SDL_Rect *dest, unsigned int x, unsigned int y, unsigned char selected );
static unsigned char conf_render_line ( SDL_Rect *dest, unsigned int y );
static char *conf_format_int ( int v, change_type_e c );

confitem_t page_general[] = {
  { "Default tab to show",           "On startup, Minimenu will try to switch to this tab",     NULL /* default */, "categories.default_cat",  ct_visible_tab_list },
#if 0 // and also in mmenu.c -- something crashes during image caching
  { "Set CPU speed within Minimenu", "Whether the next setting is applied or not",              "0",                "minimenu.use_mm_speed",   ct_boolean },
  { "CPU speed within Minimenu",     "Set low; speed to run Minimenu at",                       "400",              "minimenu.mm_speed",       ct_cpu_speed },
#endif
  { "Show 'All' tab",                "Whethor an All tab is shown or not",                      "1",                "categories.do_all_cat",   ct_boolean },
  { "Show directory browser tabs",   "Show a tab for each SD card?",                            "0",                "filesystem.do_browser",   ct_boolean },
  { "Detail panel on start?",        "Show or hide the detail panel when menu starts",          "1",                "display.show_detail_pane", ct_boolean },
  { "Sub-categories as folders?",    "If no, uses tabs instead of folders within tabs.",        "1",                "tabs.subcat_as_folders",  ct_boolean },
  { "Start with app selected",       "Whethor selection is placed by default or not",           "0",                "minimenu.start_selected", ct_boolean },
  { "Auto discover pnd apps?",       "If no, turn on diectory browser to manually find apps",   "1",                "filesystem.do_pnd_disco", ct_boolean },
  { "Set CPU speed when leaving",    "Whether the next setting is applied or not",              "0",                "minimenu.use_run_speed",  ct_boolean },
  { "CPU speed when leaving",        "Before running app, set this speed; app may override.",   "500",              "minimenu.run_speed",      ct_cpu_speed },
  { "Wrap tab change",               "Changing tab left or right, does it wrap around?",        "0",                "tabs.wraparound",         ct_boolean },
  { "Grid stop vertical",            "Changing selection up or down, does it stop or wrap?",    "0",                "grid.wrap_vert_stop",     ct_boolean },
  { "Live (not exit) on app run?",   "Normally menu exits (to save ram) on app run",            "0",                "minimenu.live_on_run",    ct_boolean },
  { "Force wallpaper with..",        "You can force an override over themes background",        "/pandora/appdata/mmenu/wallpaper.png", "minimenu.force_wallpaper",  ct_filename },
  { "",                              "",                                                        NULL,               NULL,                      ct_nil },
  { "^- Back up to main",            "Go back to top level of configuration",                   NULL,               NULL,                      ct_switch_page, NULL },
  { NULL }
};

confitem_t page_appshowhide [ CONF_MAX_LISTLENGTH ] = {
  { "^- Back up to main",        "Go back to top level of configuration",                   NULL,               NULL,                      ct_switch_page, NULL },
  { CONF_APPLIST_TAG,            "Show or hide this application",                           "1",                "appshow",                 ct_nil },
  { NULL }
};

confitem_t page_tabshowhide [ CONF_MAX_LISTLENGTH ] = {
  { "^- Back up to main",        "Go back to top level of configuration",                   NULL,               NULL,                      ct_switch_page, NULL },
  { CONF_TABLIST_TAG,            "Show or hide or reorder this tab",                        "1",                "tabshow",                 ct_nil },
  { NULL }
};

confitem_t pages[] = {
  { "General Options",           "Miscellaneous handy options",                             NULL /* default */, NULL,                      ct_switch_page, page_general },
  { "Show/Hide Applications",    "Each application can be hidden/revealed",                 NULL /* default */, NULL,                      ct_switch_page, page_appshowhide },
  { "Show/Hide/Order Tabs",      "Each tab can be hidden/revealed or re-ordered",           NULL /* default */, NULL,                      ct_switch_page, page_tabshowhide },
  { "",                          "",                                                        NULL,               NULL,                      ct_nil },
  { "Exit configuration",        "Quit and save configuration",                             NULL /* default */, NULL,                      ct_exit },
  { "",                          "",                                                        NULL,               NULL,                      ct_nil },
  { "Reset to defaults",         "Remove any custom options set in this UI",                NULL /* default */, NULL,                      ct_reset },
  { NULL }
};

extern pnd_conf_handle g_conf;
extern SDL_Surface *sdl_realscreen;
extern mm_imgcache_t g_imagecache [ IMG_TRUEMAX ];
extern pnd_box_handle g_active_apps;

unsigned char conf_run_menu ( confitem_t *toplevel ) {
  confitem_t *page = toplevel;
  unsigned int sel = 0;
  unsigned int first_visible = 0;
  unsigned char max_visible = 12;
  SDL_Event event;
  confitem_t *lastpage = NULL;

  while ( 1 ) {

    if ( ! page ) {
      page = pages;
      sel = 0;
      first_visible = 0;
    }

    if ( lastpage != page ) {
      conf_prepare_page ( page );
      lastpage = page;
    }

    conf_display_page ( page, sel, first_visible, max_visible );

    // check for input
    while ( SDL_WaitEvent ( &event ) ) {

      switch ( event.type ) {

      //case SDL_KEYUP:
      case SDL_KEYDOWN:

	if ( event.key.keysym.sym == SDLK_UP ) {

	  do {

	    if ( sel ) {
	      sel--;

	      if ( sel < first_visible ) {
		first_visible--;
	      }

	    }

	  } while ( page [ sel ].type == ct_nil );

	} else if ( event.key.keysym.sym == SDLK_DOWN ) {

	  do {

	    if ( page [ sel + 1 ].text ) {
	      sel++;

	      // ensure visibility
	      if ( sel >= first_visible + max_visible ) {
		first_visible++;
	      }

	    }

	  } while ( page [ sel ].type == ct_nil );

	} else if ( event.key.keysym.sym == SDLK_PAGEUP ) {
	  page = NULL;

	} else if ( event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_RIGHT ) {

	  unsigned char left = 0;
	  if ( event.key.keysym.sym == SDLK_LEFT ) {
	    left = 1;
	  }

	  switch ( page [ sel ].type ) {

	  case ct_cpu_speed:
	    {
	      int v = pnd_conf_get_as_int ( g_conf, page [ sel ].key );
	      if ( v == PND_CONF_BADNUM ) {
		v = 500;
	      }

	      if ( left ) {
		if ( v > 30 ) {
		  v -= 10;
		}
	      } else {
		v += 10;
	      }

	      char buffer [ 20 ];
	      sprintf ( buffer, "%u", v );
	      pnd_conf_set_char ( g_conf, page [ sel ].key, buffer );

	    }
	    break;

	  case ct_visible_tab_list:
	    {
	      if ( g_categorycount ) {
		char *v = pnd_conf_get_as_char ( g_conf, page [ sel ].key );
		if ( v ) {
		  unsigned char n = 0;
		  for ( n = 0; n < g_categorycount; n++ ) {
		    if ( strcmp ( v, g_categories [ n ] -> catname ) == 0 ) {
		      break;
		    }
		  }
		  if ( n < g_categorycount ) {
		    if ( left && n ) {
		      pnd_conf_set_char ( g_conf, page [ sel ].key, g_categories [ n - 1 ] -> catname );
		    } else if ( ! left && n + 1 < g_categorycount ) {
		      pnd_conf_set_char ( g_conf, page [ sel ].key, g_categories [ n + 1 ] -> catname );
		    }
		  } else {
		    pnd_conf_set_char ( g_conf, page [ sel ].key, g_categories [ 0 ] -> catname );
		  }
		} else {
		  pnd_conf_set_char ( g_conf, page [ sel ].key, g_categories [ 0 ] -> catname );
		}
	      } // if category count
	    }
	    break;

	  case ct_boolean:
	    {
	      int v = pnd_conf_get_as_int ( g_conf, page [ sel ].key );
	      if ( v == PND_CONF_BADNUM ) {
		v = 0;
	      }
	      if ( v ) {
		v = 0;
	      } else {
		v = 1;
	      }

	      char buffer [ 20 ];
	      sprintf ( buffer, "%u", v );

	      pnd_conf_set_char ( g_conf, page [ sel ].key, buffer );
	    }
	    break;

	  case ct_filename:
	    break;

	  case ct_nil:
	  case ct_switch_page:
	  case ct_reset:
	  case ct_exit:
	    break;

	  } // switch

	} else if ( event.key.keysym.sym == SDLK_ESCAPE ) {
	  emit_and_quit ( MM_QUIT );

	} else if ( event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_END ) { // return, or "B"

	  switch ( page [ sel ].type ) {
	  case ct_exit:
	    {
	      return ( 1 /* always cause restart for now */ );
	    }
	    break;
	  case ct_reset:
	    {
	      conf_reset_to_default ( g_conf );
	      page = NULL;
	      sel = 0;
	    }
	    break;
	  case ct_switch_page:
	    page = page [ sel ].newhead;
	    sel = 0; // should use a stack..
	    break;
	  case ct_filename:
	    break;
	  case ct_nil:
	  case ct_visible_tab_list:
	  case ct_cpu_speed:
	  case ct_boolean:
	    break;
	  } // switch

	} else {
	  // nada
	}

	break; // case sdl_key_up

      } // switch what SDL event

      break; // get out of sdl-wait-event loop
    } // while events

  } // while forever

  return ( 1 /* always cause restart for now */ );
}

void conf_display_page ( confitem_t *page, unsigned int selitem, unsigned int first_visible, unsigned int max_visible ) {
  extern TTF_Font *g_big_font;
  extern TTF_Font *g_tab_font;

#define MAXRECTS 200
  SDL_Rect rects [ MAXRECTS ];
  SDL_Rect *dest = rects;
  bzero ( dest, sizeof(SDL_Rect)*MAXRECTS );

  unsigned short int tx, ty;

  // background
  if ( g_imagecache [ IMG_BACKGROUND_800480 ].i ) {
    dest -> x = 0;
    dest -> y = 0;
    dest -> w = sdl_realscreen -> w;
    dest -> h = sdl_realscreen -> h;
    SDL_BlitSurface ( g_imagecache [ IMG_BACKGROUND_800480 ].i, NULL /* whole image */, sdl_realscreen, dest /* 0,0 */ );
    dest++;
  }

  // title
  //
  // <items>
  // description
  // default
  //
  // controls help

  // title
  dest += conf_render_text ( g_big_font, "Minimenu Configuration", dest, 10, 10, CONF_UNSELECTED );
  dest += conf_render_line ( dest, 45 );

  // scrollable hints
  {

    // up
    if ( first_visible > 0 ) {
      dest -> x = 10;
      dest -> y = 65;
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_UP ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;
    } // scroll arrow up

    // down
    if ( first_visible + max_visible < conf_determine_pagelength ( page ) ) {
      dest -> x = 10;
      dest -> y = 345;
      SDL_BlitSurface ( g_imagecache [ IMG_ARROW_DOWN ].i, NULL /* whole image */, sdl_realscreen, dest );
      dest++;
    } // scroll arrow up

  } // scrollbar

  // items
  tx = 50; ty = 70;
  unsigned char counter = first_visible;
  while ( page [ counter ].text ) {

    // item
    conf_render_text ( g_tab_font, page [ counter ].text, dest, tx, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );

    // value
    switch ( page [ counter ].type ) {
    case ct_switch_page:
      break;
    case ct_reset:
      break;
    case ct_visible_tab_list:
      {
	char *v = pnd_conf_get_as_char ( g_conf, page [ counter ].key );
	if ( v ) {
	  conf_render_text ( g_tab_font, v, dest, tx + 400, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );
	} else {
	  conf_render_text ( g_tab_font, "Not specified", dest, tx + 400, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );
	}
      }
      break;
    case ct_cpu_speed:
      {
	int v = pnd_conf_get_as_int ( g_conf, page [ counter ].key );
	conf_render_text ( g_tab_font, conf_format_int ( v, page [ counter ].type ), dest, tx + 400, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );
      }
      break;
    case ct_boolean:
      {
	int v = pnd_conf_get_as_int ( g_conf, page [ counter ].key );
	conf_render_text ( g_tab_font, conf_format_int ( v, page [ counter ].type ), dest, tx + 400, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );
      }
      break;
    case ct_filename:
      conf_render_text ( g_tab_font, page [ counter ].def, dest, tx + 400, ty, counter == selitem ? CONF_SELECTED : CONF_UNSELECTED );
      break;
    case ct_exit:
      break;
    case ct_nil:
      break;
    } // switch

    // far enough?
    if ( counter - first_visible >= max_visible - 1 ) {
      break;
    }

    // next line
    ty += 25;
    counter++;
  } // while

  // description and default
  if ( page [ selitem ].desc ) {
    dest += conf_render_text ( g_tab_font, page [ selitem ].desc, dest, 380, 400, CONF_UNSELECTED );
  }
  if ( page [ selitem ].def ) {
    char buffer [ 100 ] = "Default: ";

    switch ( page [ selitem ].type ) {
    case ct_boolean:
      sprintf ( buffer + strlen ( buffer ), "%s", conf_format_int ( atoi ( page [ selitem ].def ), ct_boolean ) );
      break;
    case ct_cpu_speed:
      sprintf ( buffer + strlen ( buffer ), "%s", conf_format_int ( atoi ( page [ selitem ].def ), ct_cpu_speed ) );
      break;
    case ct_filename:
      sprintf ( buffer + strlen ( buffer ), "%s", page [ selitem ].def );
      break;
    case ct_nil:
    case ct_switch_page:
    case ct_reset:
    case ct_exit:
    case ct_visible_tab_list:
      break;
    } // switch

    dest += conf_render_text ( g_tab_font, buffer, dest, 380, 420, CONF_UNSELECTED );
  } else {
    dest += conf_render_text ( g_tab_font, "No default value", dest, 380, 420, CONF_UNSELECTED );
  }

  // cursor's conf item count number - not for top level, just the sublevels
  if ( page != pages ) {
    char buffer [ 40 ];
    sprintf ( buffer, "Config item %d of %d", selitem + 1, conf_determine_pagelength ( page ) );
    /*dest += */conf_render_text ( g_tab_font, buffer, dest, 380, 440, CONF_UNSELECTED );
  }

  // help
  dest += conf_render_line ( dest, 380 );
  dest += conf_render_text ( g_tab_font, "D-pad Up/down; Y return to index", dest, 10, 400, CONF_UNSELECTED );
  dest += conf_render_text ( g_tab_font, "Left and right to alter selected item", dest, 10, 420, CONF_UNSELECTED );
  dest += conf_render_text ( g_tab_font, "B or Enter to activate an option", dest, 10, 440, CONF_UNSELECTED );

  // update all the rects and send it all to sdl
  // - at this point, we could probably just do 1 rect, of the
  //   whole screen, and be faster :/
  SDL_UpdateRects ( sdl_realscreen, dest - rects, rects );

  return;
}

unsigned char conf_render_text ( TTF_Font *f, char *buffer, SDL_Rect *dest, unsigned int x, unsigned int y, unsigned char selected ) {
  unsigned int font_rgba_r = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_r", 200 );
  unsigned int font_rgba_g = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_g", 200 );
  unsigned int font_rgba_b = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_b", 200 );
  unsigned int font_rgba_a = pnd_conf_get_as_int_d ( g_conf, "display.font_rgba_a", 100 );

  SDL_Color tmpfontcolor = { font_rgba_r, font_rgba_g, font_rgba_b, font_rgba_a };
  SDL_Color selfontcolor = { 0/*font_rgba_r*/, font_rgba_g, font_rgba_b, font_rgba_a };

  SDL_Surface *rtext = TTF_RenderText_Blended ( f, buffer, selected ? selfontcolor : tmpfontcolor );
  dest -> x = x;
  dest -> y = y;
  SDL_BlitSurface ( rtext, NULL /* all */, sdl_realscreen, dest );
  SDL_FreeSurface ( rtext );

  return ( 1 );
}

unsigned char conf_render_line ( SDL_Rect *dest, unsigned int y ) {

  dest -> x = 0;
  dest -> y = y;

  SDL_Surface *i = g_imagecache [ IMG_TAB_LINE ].i;

  while ( dest -> x + i -> w < 800 ) {
    SDL_BlitSurface ( i, NULL, sdl_realscreen, dest );
    dest -> x += i -> w;
  }

  dest -> x = 0;
  dest -> w = 480 - 10;
  dest -> h = i -> h;

  return ( 1 );
}

char *conf_format_int ( int v, change_type_e c ) {
  static char buffer [ 50 ];
  buffer [ 0 ] = '\0';

  switch ( c ) {

    case ct_cpu_speed:
      {
	if ( v == PND_CONF_BADNUM ) {
	  strcpy ( buffer, "Leave Alone" );
	} else {
	  sprintf ( buffer, "%u", v );
	}
      }
      break;

    case ct_boolean:
      {
	if ( v == PND_CONF_BADNUM ) {
	  strcpy ( buffer, "FUBAR" );
	} else if ( v == 0 ) {
	  strcpy ( buffer, "no" );
	} else if ( v == 1 ) {
	  strcpy ( buffer, "yes" );
	} else {
	  strcpy ( buffer, "FUBAR 2" );
	}
      }
      break;

  case ct_filename:
    break;

  case ct_exit:
  case ct_reset:
  case ct_switch_page:
  case ct_visible_tab_list:
  case ct_nil:
    break;

  } // switch

  return ( buffer );
}

unsigned char conf_prepare_page ( confitem_t *page ) {
  char buffer [ 100 ];

  confitem_t *p = page;
  confitem_t *template = NULL;
  while ( p -> text != NULL ) {

    if ( strcmp ( p -> text, CONF_APPLIST_TAG ) == 0 ) {
      // rewrite this and subsequent items to be the listing
      template = p;
      p++;

      template -> text = ""; // so it won't get repopulated again later

      // for each app..
      pnd_disco_t *iter = pnd_box_get_head ( g_active_apps );

      while ( p - page < CONF_MAX_LISTLENGTH && iter ) {

	p -> text = strndup ( iter -> title_en ? iter -> title_en : "Unnamed", 40 );
	p -> desc = strdup ( iter -> unique_id );
	p -> def = NULL;

	sprintf ( buffer, "%s.%s", template -> key, iter -> unique_id );
	p -> key = strdup ( buffer );
	p -> type = ct_boolean;
	p -> newhead = NULL;

	// create to positive if not existant
	if ( ! pnd_conf_get_as_char ( g_conf, buffer ) ) {
	  pnd_conf_set_char ( g_conf, buffer, "1" );
	}

	iter = pnd_box_get_next ( iter );
	p++;
      } // while not run off end of buffer

      break;

    } else if ( strcmp ( p -> text, CONF_TABLIST_TAG ) == 0 ) {
      // rewrite this and subsequent items to be the listing
      template = p;
      p++;

      template -> text = ""; // so it won't get repopulated again later

      // switch categories being published
      category_publish ( CFALL, NULL );

      // for each tab
      unsigned int i;
      char catname [ 512 ];
      char *actual_catname;

      for ( i = 0;  i < g_categorycount; i++ ) {

	// if this is an invisi-guy, it has parent-cat prepended; we want the real cat name.
	strncpy ( catname, g_categories [ i ] -> catname, 500 );

	if ( ( actual_catname = strchr ( catname, '.' ) ) ) {
	  actual_catname++; // skip the period
	} else {
	  actual_catname = catname;
	}
	//fprintf ( stderr, "conf ui; got '%s' but showing '%s'\n", cc [ i ].catname, actual_catname );

	if ( strncmp ( g_categories [ i ] -> catname, "All ", 4 ) == 0 ) {
	  // skip All tab, since it is generated, and managed by another config item
	  continue;
	}

	p -> text = strndup ( actual_catname, 40 );
	p -> desc = NULL;
	p -> def = NULL;

	sprintf ( buffer, "%s.%s", template -> key, actual_catname );
	p -> key = strdup ( buffer );
	p -> type = ct_boolean;
	p -> newhead = NULL;

	// create to positive if not existant
	if ( ! pnd_conf_get_as_char ( g_conf, buffer ) ) {
	  pnd_conf_set_char ( g_conf, buffer, "1" );
	}

	//fprintf ( stderr, "Created tabshow entry '%s'\n", cc [ i ].catname );

	p++;
      } // for

      // switch categories being published
      category_publish ( CFNORMAL, NULL );

      break;
    }

    p++;
  }

  return ( 1 );
}

unsigned char conf_write ( pnd_conf_handle h, char *fullpath ) {

  // cherry pick the named keys from the conf-ui-array
  // spill out the apps ands tabs 'broups' of conf keys

  FILE *f = fopen ( fullpath, "w" );

  if ( ! f ) {
    return ( 0 );
  }

  fprintf ( f, "# Machine written; do not edit.\n" );
  fprintf ( f, "# If you do edit, its KEY<tab>VALUE<newline>, nothing extra.\n" );
  fprintf ( f, "\n" );

  // deal with general keys
  confitem_t *ci = page_general;
  while ( ci -> text ) {
    // does this item have a key? a value? if so, try to emit it.
    if ( ci -> key ) {
      char *v = pnd_conf_get_as_char ( h, ci -> key );
      if ( v ) {
	fprintf ( f, "%s\t%s\n", ci -> key, v );
      }
    }
    ci++;
  } // while

  // deal with apps and tabs
  char *v = pnd_box_get_head ( g_conf );
  while ( v ) {

    // does item begin with app or tab tag?
    char *k = pnd_box_get_key ( v );

    if ( k && 
	 ( strncasecmp ( k, "appshow.", 8 ) == 0 ||
	   strncasecmp ( k, "tabshow.", 8 ) == 0 )
       )
    {
      fprintf ( f, "%s\t%s\n", k, v );
    }

    v = pnd_box_get_next ( v );
  } // while

  fclose ( f );

  return ( 1 );
}

void conf_merge_into ( char *fullpath, pnd_conf_handle h ) {
  FILE *f;
  char buffer [ 1024 ];
  char *s;

  f = fopen ( fullpath, "r" );

  if ( ! f ) {
    return;
  }

  while ( fgets ( buffer, 1000, f ) ) {
#if 0
    // trim trailing spaces
    s = strchr ( buffer, ' ' );
    if ( s ) {
      *s = '\0';
    }
#endif
    // and #...
    s = strchr ( buffer, '#' );
    if ( s ) {
      *s = '\0';
    }
    // and newline...
    s = strchr ( buffer, '\n' );
    if ( s ) {
      *s = '\0';
    }

    // if theres anything left..
    if ( buffer [ 0 ] != '\0' ) {

      // assume FOO<tab>BAR<newline> since this really should be machine written, not human screwed with; or if human
      // edited, assume they know what to do :) I even put in some 'docs' in the conf file.
      char *s = strchr ( buffer, '\t' );

      if ( s ) {
	*s = '\0';

	// set it; libpnd conf code already handles 'existant' or 'new'
	pnd_conf_set_char ( h, buffer, s + 1 );

      } // found a <tab>?

    } // anything left?

  } // while

  fclose ( f );

  return;
}

char *conf_determine_location ( pnd_conf_handle h ) {
  static char path [ PATH_MAX ];

  bzero ( path, PATH_MAX );

  if ( ! getenv ( "HOME" ) ) {
    return ( "." ); // wtf?
  }

  snprintf ( path, PATH_MAX - strlen(CONF_PREF_FILENAME) - 1, "%s/%s", getenv ( "HOME" ), CONF_PREF_FILENAME );

  return ( path );
}

void conf_setup_missing ( pnd_conf_handle h ) {

  confitem_t *ci = page_general;

  while ( ci -> text ) {

    // does this item have a default value?
    if ( ci -> def ) {

      // it does, so lets see if we can pull a current value in; if not, set one
      char *v = pnd_conf_get_as_char ( h, ci -> key );

      if ( ! v ) {
	fprintf ( stderr, "pref conf: no value present in config, better set to default; key is '%s' def '%s'\n", ci -> key, ci -> def );
	pnd_conf_set_char ( h, ci -> key, ci -> def );
      }

    } // has def?

    ci++;
  } // while

  return;
}

void conf_reset_to_default ( pnd_conf_handle h ) {

  // reset all keys to default value - if present
  // reset all apps to show
  // reset all tabs to show

  // deal with general keys
  confitem_t *ci = page_general;
  while ( ci -> text ) {

    // does this item have a default value? if so, set it
    if ( ci -> key && ci -> def ) {
      pnd_conf_set_char ( h, ci -> key, ci -> def );
    }

    ci++;
  } // while

  // deal with apps and tabs
  char *v = pnd_box_get_head ( g_conf );
  char *next;
  while ( v ) {
    next = pnd_box_get_next ( v );

    // does item begin with app or tab tag?
    char *k = pnd_box_get_key ( v );

    if ( k && 
	 ( strncasecmp ( k, "appshow.", 8 ) == 0 ||
	   strncasecmp ( k, "tabshow.", 8 ) == 0 )
       )
    {
      pnd_conf_set_char ( g_conf, k, "1" );
    }

    v = next;
  } // while

  return;
}

unsigned int conf_determine_pagelength ( confitem_t *page ) {
  confitem_t *p = page;
  while ( p -> text ) {
    p++;
  }
  return ( p - page );
}
