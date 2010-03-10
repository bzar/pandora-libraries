
/* minimenu
 * aka "2wm" - too weak menu, two week menu, akin to twm
 *
 * Craig wants a super minimal menu ASAP before launch, so lets see what I can put together in 2 weeks with not much
 * free time ;) I'd like to do a fuller ('tiny', but with plugin support and a decent expansion and customizing design..)
 * but later, baby!
 *
 */

/* mmenu - This is the actual menu
 * The goal of this app is to show a application picker screen of some sort, allow the user to perform some useful
 * activities (such as set clock speed, say), and request an app to run, or shutdown, etc.
 * To keep the memory footprint down, when invoking an application, the menu _exits_, and simply spits out
 * an operation for mmwrapper to perform. In the case of no wrapper, the menu will just exit, which is handy for
 * debugging.
 */

/* mmenu lifecycle:
 * 1) determine app list (via pnd scan, .desktop scan, whatever voodoo)
 * 2) show a menu, allow user to interact:
 *    a) user picks an application to run, or -> exit, pass shell run line to wrapper
 *    b) user requests menu shutdown -> exit, tell wrapper to exit as well
 *    c) user performsn some operation (set clock, copy files, whatever) -> probably stays within the menu
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_utility.h"
#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_device.h"

#include "mmenu.h"
#include "mmwrapcmd.h"
#include "mmapps.h"
#include "mmcache.h"
#include "mmcat.h"
#include "mmui.h"

pnd_box_handle *g_active_apps = NULL;
unsigned int g_active_appcount = 0;
char g_username [ 128 ]; // since we have to wait for login (!!), store username here
pnd_conf_handle g_conf = 0;
pnd_conf_handle g_desktopconf = 0;

char *pnd_run_script = NULL;
char *g_skinpath = NULL;
unsigned char g_x11_present = 1; // >0 if X is present

int main ( int argc, char *argv[] ) {
  int logall = -1; // -1 means normal logging rules; >=0 means log all!
  int i;

  // boilerplate stuff from pndnotifyd and pndevmapperd

  /* iterate across args
   */
  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'l' ) {

      if ( isdigit ( argv [ i ][ 2 ] ) ) {
	unsigned char x = atoi ( argv [ i ] + 2 );
	if ( x >= 0 &&
	     x < pndn_none )
	{
	  logall = x;
	}
      } else {
	logall = 0;
      }

    } else {
      //printf ( "Unknown: %s\n", argv [ i ] );
      printf ( "%s [-l##]\n", argv [ 0 ] );
      printf ( "-l#\tLog-it; -l is 0-and-up (or all), and -l2 means 2-and-up (not all); l[0-3] for now. Log goes to /tmp/mmenu.log\n" );
      printf ( "-f\tFull path of frontend to run\n" );
      exit ( 0 );
    }

  } // for

  /* enable logging?
   */
  pnd_log_set_pretext ( "mmenu" );
  pnd_log_set_flush ( 1 );

  if ( logall == -1 ) {
    // standard logging; non-daemon versus daemon

#if 1 // HACK: set debug level to high on desktop, but not on pandora; just a convenience while devving, until the conf file is read
    struct stat statbuf;
    if ( stat ( PND_DEVICE_BATTERY_GAUGE_PERC, &statbuf ) == 0 ) {
      // on pandora
      pnd_log_set_filter ( pndn_error );
    } else {
      pnd_log_set_filter ( pndn_debug );
    }
#endif

    pnd_log_to_stdout();

  } else {
    FILE *f;

    f = fopen ( "/tmp/mmenu.log", "w" );

    if ( f ) {
      pnd_log_set_filter ( logall );
      pnd_log_to_stream ( f );
      pnd_log ( pndn_rem, "logall mode - logging to /tmp/mmenu.log\n" );
    }

    if ( logall == pndn_debug ) {
      pnd_log_set_buried_logging ( 1 ); // log the shit out of it
      pnd_log ( pndn_rem, "logall mode 0 - turned on buried logging\n" );
    }

  } // logall

  pnd_log ( pndn_rem, "%s built %s %s", argv [ 0 ], __DATE__, __TIME__ );

  pnd_log ( pndn_rem, "log level starting as %u", pnd_log_get_filter() );

  // wait for a user to be logged in - we should probably get hupped when a user logs in, so we can handle
  // log-out and back in again, with SDs popping in and out between..
  pnd_log ( pndn_rem, "Checking to see if a user is logged in\n" );
  while ( 1 ) {
    if ( pnd_check_login ( g_username, 127 ) ) {
      break;
    }
    pnd_log ( pndn_debug, "  No one logged in yet .. spinning.\n" );
    sleep ( 2 );
  } // spin
  pnd_log ( pndn_rem, "Looks like user '%s' is in, continue.\n", g_username );

  /* conf file
   */
  g_conf = pnd_conf_fetch_by_name ( MMENU_CONF, MMENU_CONF_SEARCHPATH );

  if ( ! g_conf ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fetch conf file '%s'!\n", MMENU_CONF );
    emit_and_quit ( MM_QUIT );
  }

  g_desktopconf = pnd_conf_fetch_by_id ( pnd_conf_desktop, PND_CONF_SEARCHPATH );

  if ( ! g_desktopconf ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fetch desktop conf file\n" );
    emit_and_quit ( MM_QUIT );
  }

  // redo log filter
  pnd_log_set_filter ( pnd_conf_get_as_int_d ( g_conf, "minimenu.loglevel", pndn_error ) );

  /* setup
   */

  // X11?
  if ( pnd_conf_get_as_char ( g_conf, "minimenu.x11_present_sh" ) ) {
    FILE *fx = popen ( pnd_conf_get_as_char ( g_conf, "minimenu.x11_present_sh" ), "r" );
    char buffer [ 100 ];
    if ( fx ) {
      if ( fgets ( buffer, 100, fx ) ) {
	if ( atoi ( buffer ) ) {
	  g_x11_present = 1;
	  pnd_log ( pndn_rem, "X11 seems to be present [pid %u]\n", atoi(buffer) );
	} else {
	  g_x11_present = 0;
	  pnd_log ( pndn_rem, "X11 seems NOT to be present\n" );
	}
      } else {
	  g_x11_present = 0;
	  pnd_log ( pndn_rem, "X11 seems NOT to be present\n" );
      }
      pclose ( fx );
    }
  } // x11?

  // pnd_run.sh
  pnd_run_script = pnd_locate_filename ( pnd_conf_get_as_char ( g_conf, "minimenu.pndrun" ), "pnd_run.sh" );

  if ( ! pnd_run_script ) {
    pnd_log ( pndn_error, "ERROR: Couldn't locate pnd_run.sh!\n" );
    emit_and_quit ( MM_QUIT );
  }

  pnd_run_script = strdup ( pnd_run_script ); // so we don't lose it next pnd_locate

  pnd_log ( pndn_rem, "Found pnd_run.sh at '%s'\n", pnd_run_script );

  // figure out skin path
  if ( ! pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ) ||
       ! pnd_conf_get_as_char ( g_conf, "minimenu.font" )
     )
  {
    pnd_log ( pndn_error, "ERROR: Couldn't set up skin!\n" );
    emit_and_quit ( MM_QUIT );
  }

  g_skinpath = pnd_locate_filename ( pnd_conf_get_as_char ( g_conf, MMENU_ARTPATH ),
				     pnd_conf_get_as_char ( g_conf, "minimenu.font" ) );

  if ( ! g_skinpath ) {
    pnd_log ( pndn_error, "ERROR: Couldn't locate skin font!\n" );
    emit_and_quit ( MM_QUIT );
  }

  g_skinpath = strdup ( g_skinpath ); // so we don't lose it next pnd_locate

  * strstr ( g_skinpath, pnd_conf_get_as_char ( g_conf, "minimenu.font" ) ) = '\0';

  pnd_log ( pndn_debug, "Looks like skin is at '%s'\n", g_skinpath );

  // attempt to set up UI
  if ( ! ui_setup() ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up the UI!\n" );
    emit_and_quit ( MM_QUIT );
  }

  // show load screen
  ui_loadscreen();

  // set up static image cache
  if ( ! ui_imagecache ( g_skinpath ) ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up static UI image cache!\n" );
    emit_and_quit ( MM_QUIT );
  }

  /* inhale applications, icons, categories, etc
   */

  // show disco screen
  ui_discoverscreen ( 1 /* clear screen */ );

  // determine current app list, cache icons
  pnd_log ( pndn_debug, "Looking for pnd applications here: %s\n", pnd_conf_get_as_char ( g_desktopconf, "desktop.searchpath" ) );
  g_active_apps = pnd_disco_search ( pnd_conf_get_as_char ( g_desktopconf, "desktop.searchpath" ), NULL ); // ignore overrides for now
  g_active_appcount = pnd_box_get_size ( g_active_apps );

  unsigned char maxwidth, maxheight;
  maxwidth = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_ICON_MAX_WIDTH, 50 );
  maxheight = pnd_conf_get_as_int_d ( g_conf, MMENU_DISP_ICON_MAX_HEIGHT, 50 );

  // show cache screen
  ui_cachescreen ( 1 /* clear screen */, NULL );

  pnd_log ( pndn_debug, "Found pnd applications, and caching icons:\n" );
  pnd_disco_t *iter = pnd_box_get_head ( g_active_apps );
  while ( iter ) {
    //pnd_log ( pndn_debug, "  App: '%s'\n", IFNULL(iter->title_en,"No Name") );

    // update cachescreen
    ui_cachescreen ( 1 /* clear screen */, IFNULL(iter->title_en,"No Name") );

    // cache the icon
    if ( iter -> pnd_icon_pos &&
	 ! cache_icon ( iter, maxwidth, maxheight ) )
    {
      pnd_log ( pndn_warning, "  Couldn't load icon: '%s'\n", IFNULL(iter->title_en,"No Name") );
    }

    // cache the preview --> SHOULD DEFER
    if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.load_previews_now", 0 ) > 0 ) {
      // load the preview pics now!
      if ( iter -> preview_pic1 &&
	   ! cache_preview ( iter, pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_width", 200 ), pnd_conf_get_as_int_d ( g_conf, "previewpic.cell_height", 180 ) ) )
      {
	pnd_log ( pndn_warning, "  Couldn't load preview pic: '%s' -> '%s'\n", IFNULL(iter->title_en,"No Name"), iter -> preview_pic1 );
      }
    } // preview now?

    // push the categories .. or suppress application
    //
    if ( ( pnd_pxml_get_x11 ( iter -> option_no_x11 ) != pnd_pxml_x11_required ) ||
	 ( pnd_pxml_get_x11 ( iter -> option_no_x11 ) == pnd_pxml_x11_required && g_x11_present == 1 )
       )
    {

      // push to All category
      // we do this first, so first category is always All
      if ( ! category_push ( g_x11_present ? CATEGORY_ALL "    (X11)" : CATEGORY_ALL "   (No X11)", iter ) ) {
	pnd_log ( pndn_warning, "  Couldn't categorize to All: '%s'\n", IFNULL(iter -> title_en, "No Name") );
      }

      // main categories
      if ( iter -> main_category && pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat", 1 ) ) {
	if ( ! category_push ( iter -> main_category, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> main_category, IFNULL(iter -> title_en, "No Name") );
	}
      }

      if ( iter -> main_category1 && pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat1", 0 ) ) {
	if ( ! category_push ( iter -> main_category1, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> main_category1, IFNULL(iter -> title_en, "No Name") );
	}
      }

      if ( iter -> main_category2 && pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat2", 0 ) ) {
	if ( ! category_push ( iter -> main_category2, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> main_category2, IFNULL(iter -> title_en, "No Name") );
	}
      }

      // alt categories
      if ( iter -> alt_category && pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat", 0 ) ) {
	if ( ! category_push ( iter -> alt_category, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> alt_category, IFNULL(iter -> title_en, "No Name") );
	}
      }

      if ( iter -> alt_category1 && pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat1", 0 ) ) {
	if ( ! category_push ( iter -> alt_category1, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> alt_category1, IFNULL(iter -> title_en, "No Name") );
	}
      }

      if ( iter -> alt_category2 && pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat2", 0 ) ) {
	if ( ! category_push ( iter -> alt_category2, iter ) ) {
	  pnd_log ( pndn_warning, "  Couldn't categorize to %s: '%s'\n", iter -> alt_category2, IFNULL(iter -> title_en, "No Name") );
	}
      }

    } // register with categories or filter out

    // next
    iter = pnd_box_get_next ( iter );
  } // while

  // dump categories
  //category_dump();

  /* actual work now
   */

  while ( 1 ) { // forever!

    // show the menu, or changes thereof
    ui_render ( CHANGED_NOTHING );

    // wait for input or time-based events (like animations)
    // deal with inputs
    ui_process_input ( 1 /* block */ );

    // sleep? block?
    usleep ( 5000 );

  } // while

  return ( 0 );
}

void emit_and_quit ( char *s ) {
  printf ( "%s\n", s );
  exit ( 0 );
}
