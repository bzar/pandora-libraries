
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

#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */
#include <limits.h> /* for PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>
#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */
#include <strings.h>
#include <ctype.h>
#include <sys/wait.h>
#include <dirent.h>
#include <signal.h> // for sigaction

#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_utility.h"
#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_device.h"
#include "pnd_pndfiles.h"
#include "../lib/pnd_pathiter.h"
#include "pnd_notify.h"
#include "pnd_dbusnotify.h"
#include "pnd_apps.h"

#include "mmenu.h"
#include "mmwrapcmd.h"
#include "mmapps.h"
#include "mmcache.h"
#include "mmcat.h"
#include "mmui.h"
#include "mmconf.h"

pnd_box_handle g_active_apps = NULL;
unsigned int g_active_appcount = 0;
char g_username [ 128 ]; // since we have to wait for login (!!), store username here
pnd_conf_handle g_conf = 0;
pnd_conf_handle g_desktopconf = 0;

char *pnd_run_script = NULL;
unsigned char g_x11_present = 1; // >0 if X is present
unsigned char g_catmap = 0; // if 1, we're doing category mapping
unsigned char g_pvwcache = 0; // if 1, we're trying to do preview caching
unsigned char g_autorescan = 1; // default to auto rescan

pnd_dbusnotify_handle dbh = 0; // if >0, dbusnotify is active
pnd_notify_handle nh = 0; // if >0, inotify is active

char g_skin_selected [ 100 ] = "default";
char *g_skinpath = NULL; // where 'skin_selected' is located .. the fullpath including skin-dir-name
pnd_conf_handle g_skinconf = NULL;

void sigquit_handler ( int n );
unsigned char cat_is_visible ( pnd_conf_handle h, char *catname );
unsigned char app_is_visible ( pnd_conf_handle h, char *uniqueid );

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

  /* conf files
   */

  // mmenu conf
  g_conf = pnd_conf_fetch_by_name ( MMENU_CONF, MMENU_CONF_SEARCHPATH );

  if ( ! g_conf ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fetch conf file '%s'!\n", MMENU_CONF );
    emit_and_quit ( MM_QUIT );
  }

  // override mmenu conf via user preference conf
  conf_merge_into ( conf_determine_location ( g_conf ), g_conf );
  conf_setup_missing ( g_conf );

  // desktop conf for app finding preferences
  g_desktopconf = pnd_conf_fetch_by_id ( pnd_conf_desktop, PND_CONF_SEARCHPATH );

  if ( ! g_desktopconf ) {
    pnd_log ( pndn_error, "ERROR: Couldn't fetch desktop conf file\n" );
    emit_and_quit ( MM_QUIT );
  }

  /* set up quit signal handler
   */
  sigset_t ss;
  sigemptyset ( &ss );

  struct sigaction siggy;
  siggy.sa_handler = sigquit_handler;
  siggy.sa_mask = ss; /* implicitly blocks the origin signal */
  siggy.sa_flags = SA_RESTART; /* don't need anything */
  sigaction ( SIGQUIT, &siggy, NULL );

  /* category conf file
   */
  {
    char *locater = pnd_locate_filename ( pnd_conf_get_as_char ( g_conf, "categories.catmap_searchpath" ),
					  pnd_conf_get_as_char ( g_conf, "categories.catmap_confname" ) );

    if ( locater ) {
      pnd_log ( pndn_rem, "Found category conf at '%s'\n", locater );
      pnd_conf_handle h = pnd_conf_fetch_by_path ( locater );
      if ( h ) {
	// lets just merge the skin conf onto the regular conf, so it just magicly works
	pnd_box_append ( g_conf, h );
      }
    } else {
      pnd_log ( pndn_debug, "No additional category conf file found.\n" );
    }

  } // cat conf

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

  // auto rescan?
  if ( pnd_conf_get_as_int ( g_conf, "minimenu.auto_rescan" ) != PND_CONF_BADNUM ) {
    g_autorescan = pnd_conf_get_as_int ( g_conf, "minimenu.auto_rescan" );
  }
  pnd_log ( pndn_debug, "application rescan is set to: %s\n", g_autorescan ? "auto" : "manual" );

  // figure out what skin is selected (or default)
  FILE *f;
  char *s = strdup ( pnd_conf_get_as_char ( g_conf, "minimenu.skin_selected" ) );
  s = pnd_expand_tilde ( s );
  if ( ( f = fopen ( s, "r" ) ) ) {
    char buffer [ 100 ];
    if ( fgets ( buffer, 100, f ) ) {
      // see if that dir can be located
      if ( strchr ( buffer, '\n' ) ) {
	* strchr ( buffer, '\n' ) = '\0';
      }
      char *found = pnd_locate_filename ( pnd_conf_get_as_char ( g_conf, "minimenu.skin_searchpath" ), buffer );
      if ( found ) {
	strncpy ( g_skin_selected, buffer, 100 );
	g_skinpath = strdup ( found );
      } else {
	pnd_log ( pndn_warning, "Couldn't locate skin named '%s' so falling back.\n", buffer );
      }
    }
    fclose ( f );
  }
  free ( s );

  if ( g_skinpath ) {
    pnd_log ( pndn_rem, "Skin is selected: '%s'\n", g_skin_selected );
  } else {
    pnd_log ( pndn_rem, "Skin falling back to: '%s'\n", g_skin_selected );

    char *found = pnd_locate_filename ( pnd_conf_get_as_char ( g_conf, "minimenu.skin_searchpath" ),
					g_skin_selected );
    if ( found ) {
      g_skinpath = strdup ( found );
    } else {
      pnd_log ( pndn_error, "Couldn't locate skin named '%s'.\n", g_skin_selected );
      emit_and_quit ( MM_QUIT );
    }

  }
  pnd_log ( pndn_rem, "Skin path determined to be: '%s'\n", g_skinpath );

  // lets see if this skin-path actually has a skin conf
  {
    char fullpath [ PATH_MAX ];
    sprintf ( fullpath, "%s/%s", g_skinpath, pnd_conf_get_as_char ( g_conf, "minimenu.skin_confname" ) );
    g_skinconf = pnd_conf_fetch_by_path ( fullpath );
  }

  // figure out skin path if we didn't get it already
  if ( ! g_skinconf ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up skin!\n" );
    emit_and_quit ( MM_QUIT );
  }

  // lets just merge the skin conf onto the regular conf, so it just magicly works
  pnd_box_append ( g_conf, g_skinconf );

  // did user override the splash image?
  char *splash = pnd_conf_get_as_char ( g_conf, "minimenu.force_wallpaper" );
  if ( splash ) {
    // we've got a filename, presumably; lets see if it exists
    struct stat statbuf;
    if ( stat ( splash, &statbuf ) == 0 ) {
      // file seems to exist, lets set our override to that..
      pnd_conf_set_char ( g_conf, "graphics.IMG_BACKGROUND_800480", splash );
    }
  }

  // attempt to set up UI
  if ( ! ui_setup() ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up the UI!\n" );
    emit_and_quit ( MM_QUIT );
  }

  // show load screen
  ui_loadscreen();

  // store flag if we're doing preview caching or not
  if ( pnd_conf_get_as_int_d ( g_conf, "previewpic.do_cache", 0 ) ) {
    g_pvwcache = 1;
  }

  // set up static image cache
  if ( ! ui_imagecache ( g_skinpath ) ) {
    pnd_log ( pndn_error, "ERROR: Couldn't set up static UI image cache!\n" );
    emit_and_quit ( MM_QUIT );
  }

  // init categories
  category_init();

  // create all cat
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.do_all_cat", 1 ) ) {
    category_push ( g_x11_present ? CATEGORY_ALL "    (X11)" : CATEGORY_ALL "   (No X11)", NULL /*app*/, 0, NULL /* fspath */, 1 /* visible */ );
  }

  // set up category mappings
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_on", 0 ) ) {
    g_catmap = category_map_setup();
  }

  /* inhale applications, icons, categories, etc
   */
  applications_scan();

  /* actual work now
   */
  unsigned char block = 1;

  if ( g_autorescan ) {
    block = 0;

    // set up notifications
    dbh = pnd_dbusnotify_init();
    pnd_log ( pndn_debug, "Setting up dbusnotify\n" );
    //setup_notifications();

  } // set up rescan

  /* set speed to minimenu run-speed, now that we're all set up
   */
#if 0 /* something crashes at high speed image caching.. */
  int use_mm_speed = pnd_conf_get_as_int_d ( g_conf, "minimenu.use_mm_speed", 0 );
  if ( use_mm_speed > 0 ) {
    int mm_speed = pnd_conf_get_as_int_d ( g_conf, "minimenu.mm_speed", -1 );
    if ( mm_speed > 50 && mm_speed < 800 ) {
      char buffer [ 512 ];
      snprintf ( buffer, 500, "sudo /usr/pandora/scripts/op_cpuspeed.sh %d", mm_speed );
      system ( buffer );
    }
  } // do speed change?
#endif

  // do it!
  while ( 1 ) { // forever!

    // show the menu, or changes thereof
    ui_render();

    // wait for input or time-based events (like animations)
    // deal with inputs
    ui_process_input ( block /* block */ );

    // did a rescan event trigger?
    if ( g_autorescan ) {
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
      }

    } // rescan?

    // sleep? block?
    usleep ( 100000 /*5000*/ );

  } // while

  return ( 0 );
}

void emit_and_quit ( char *s ) {
  printf ( "%s\n", s );
  // shutdown notifications
  if ( g_autorescan ) {

    if ( dbh ) {
      pnd_dbusnotify_shutdown ( dbh );
    }
    if ( nh ) {
      pnd_notify_shutdown ( nh );
    }

  }

  exit ( 0 );
}

static unsigned int is_dir_empty ( char *fullpath ) {
  DIR *d = opendir ( fullpath );

  if ( ! d ) {
    return ( 0 ); // not empty, since we don't know
  }

  struct dirent *de = readdir ( d );

  while ( de ) {

    if ( strcmp ( de -> d_name, "." ) == 0 ) {
      // irrelevent
    } else if ( strcmp ( de -> d_name, ".." ) == 0 ) {
      // irrelevent
    } else {
      // something else came in, so dir must not be empty
      closedir ( d );
      return ( 0 ); 
    }

    de = readdir ( d );
  }

  closedir ( d );

  return ( 1 ); // dir is empty
}

void applications_free ( void ) {

  // free up all our category apprefs, but keep the preview and icon cache's..
  category_freeall();

  // free up old disco_t
  if ( g_active_apps ) {
    pnd_disco_t *p = pnd_box_get_head ( g_active_apps );
    pnd_disco_t *n;
    while ( p ) {
      n = pnd_box_get_next ( p );
      pnd_disco_destroy ( p );
      p = n;
    }
    pnd_box_delete ( g_active_apps );
  }

  return;
}

void applications_scan ( void ) {

  // has user disabled pnd scanning, by chance?
  if ( ! pnd_conf_get_as_int_d ( g_conf, "filesystem.do_pnd_disco", 1 ) ) {
    goto dirbrowser_scan; // skip pnd's
  }

  // show disco screen
  ui_discoverscreen ( 1 /* clear screen */ );

  // determine current app list, cache icons
  // - ignore overrides for now

  g_active_apps = 0;
  pnd_box_handle merge_apps = 0;

  // desktop apps?
  if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.desktop_apps", 1 ) ) {
    pnd_log ( pndn_debug, "Looking for pnd applications here: %s\n",
	      pnd_conf_get_as_char ( g_desktopconf, "desktop.searchpath" ) );
    g_active_apps = pnd_disco_search ( pnd_conf_get_as_char ( g_desktopconf, "desktop.searchpath" ), NULL );
  }

  // menu apps?
  if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.menu_apps", 1 ) ) {
    pnd_log ( pndn_debug, "Looking for pnd applications here: %s\n",
	      pnd_conf_get_as_char ( g_desktopconf, "menu.searchpath" ) );
    merge_apps = pnd_disco_search ( pnd_conf_get_as_char ( g_desktopconf, "menu.searchpath" ), NULL );
  }

  // merge lists
  if ( merge_apps ) {
    if ( g_active_apps ) {
      // the key from pnd_disco_search() is the _path_, so easy to look for duplicates
      // this is pretty inefficient, being linked lists; perhaps should switch to hash tables when
      // we expect thousands of apps.. or at least an index or something.
      void *a = pnd_box_get_head ( merge_apps );
      void *nexta = NULL;
      while ( a ) {
	nexta = pnd_box_get_next ( a );

	// if the key for the node is also found in active apps, toss out the merging one
	if ( pnd_box_find_by_key ( g_active_apps, pnd_box_get_key ( a ) ) ) {
	  //fprintf ( stderr, "Merging app id '%s' is duplicate; discarding it.\n", pnd_box_get_key ( a ) );
	  pnd_box_delete_node ( merge_apps, a );
	}

	a = nexta;
      }

      // got menu apps, and got desktop apps, merge
      pnd_box_append ( g_active_apps, merge_apps );
    } else {
      // got menu apps, had no desktop apps, so just assign
      g_active_apps = merge_apps;
    }
  }

  // aux apps?
  char *aux_apps = NULL;
  merge_apps = 0;
  aux_apps = pnd_conf_get_as_char ( g_conf, "minimenu.aux_searchpath" );
  if ( aux_apps && aux_apps [ 0 ] ) {
    pnd_log ( pndn_debug, "Looking for pnd applications here: %s\n", aux_apps );
    merge_apps = pnd_disco_search ( aux_apps, NULL );
  }

  // merge aux apps
  if ( merge_apps ) {
    if ( g_active_apps ) {

      // LAME: snipped from above; should just catenate the 3 sets of searchpaths into a
      // master searchpath, possibly removing duplicate paths _then_, and keep all this much
      // more efficient

      // the key from pnd_disco_search() is the _path_, so easy to look for duplicates
      // this is pretty inefficient, being linked lists; perhaps should switch to hash tables when
      // we expect thousands of apps.. or at least an index or something.
      void *a = pnd_box_get_head ( merge_apps );
      void *nexta = NULL;
      while ( a ) {
	nexta = pnd_box_get_next ( a );

	// if the key for the node is also found in active apps, toss out the merging one
	if ( pnd_box_find_by_key ( g_active_apps, pnd_box_get_key ( a ) ) ) {
	  fprintf ( stderr, "Merging app id '%s' is duplicate; discarding it.\n", pnd_box_get_key ( a ) );
	  pnd_box_delete_node ( merge_apps, a );
	}

	a = nexta;
      }

      pnd_box_append ( g_active_apps, merge_apps );
    } else {
      g_active_apps = merge_apps;
    }
  }

  // do it
  g_active_appcount = pnd_box_get_size ( g_active_apps );

  unsigned char maxwidth, maxheight;
  maxwidth = pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 );
  maxheight = pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_height", 50 );

  // show cache screen
  ui_cachescreen ( 1 /* clear screen */, NULL );

  pnd_log ( pndn_debug, "Found pnd applications, and caching icons:\n" );
  pnd_disco_t *iter = pnd_box_get_head ( g_active_apps );
  unsigned int itercount = 0;
  while ( iter ) {
    //pnd_log ( pndn_debug, "  App: '%s'\n", IFNULL(iter->title_en,"No Name") );

    // update cachescreen
    // ... every 5 filenames, just to avoid slowing it too much
    if ( itercount % 5 == 0 ) {
      ui_cachescreen ( 0 /* clear screen */, IFNULL(iter->title_en,"No Name") );
    }

    // if an ovr was flagged by libpnd, lets go inhale it just so we've got the
    // notes handy, since notes are not handled by libpnd proper
    pnd_conf_handle ovrh = 0;
    if ( iter -> object_flags & PND_DISCO_FLAG_OVR ) {
      char ovrfile [ PATH_MAX ];
      char *fixpxml;
      sprintf ( ovrfile, "%s/%s", iter -> object_path, iter -> object_filename );
      fixpxml = strcasestr ( ovrfile, PND_PACKAGE_FILEEXT );
      strcpy ( fixpxml, PXML_SAMEPATH_OVERRIDE_FILEEXT );

      ovrh = pnd_conf_fetch_by_path ( ovrfile );

#if 0
      if ( ovrh ) {
	pnd_log ( pndn_debug, "Found ovr file for %s # %u\n", iter -> object_filename, iter -> subapp_number );
      }
#endif

    } // ovr

    // cache the icon, unless deferred
    if ( pnd_conf_get_as_int_d ( g_conf, "minimenu.load_icons_later", 0 ) == 0 ) {
      if ( iter -> pnd_icon_pos &&
	   ! cache_icon ( iter, maxwidth, maxheight ) )
      {
	pnd_log ( pndn_warning, "  Couldn't load icon: '%s'\n", IFNULL(iter->title_en,"No Name") );
      }
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
 
      if ( iter -> title_en == NULL || iter -> title_en [ 0 ] == '\0' ) {
	// null title; just skip it.
      } else {

	// push to All category
	// we do this first, so first category is always All
	if ( pnd_conf_get_as_int_d ( g_conf, "categories.do_all_cat", 1 ) ) {
	  category_push ( g_x11_present ? CATEGORY_ALL "    (X11)" : CATEGORY_ALL "   (No X11)", iter, ovrh, NULL /* fspath */, 1 /* visible */ );
	} // all?

	// is this app suppressed? if not, show it in whatever categories the user is allowing
	if ( iter -> unique_id && app_is_visible ( g_conf, iter -> unique_id ) ) {

	  // main categories
	  category_meta_push ( iter -> main_category, NULL /* no parent cat */, iter, ovrh, cat_is_visible ( g_conf, iter -> main_category ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat", 1 ) );
	  category_meta_push ( iter -> main_category1, iter -> main_category, iter, ovrh, cat_is_visible ( g_conf, iter -> main_category1 ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat1", 0 ) );
	  category_meta_push ( iter -> main_category2, iter -> main_category, iter, ovrh, cat_is_visible ( g_conf, iter -> main_category2 ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_maincat2", 0 ) );
	  // alt categories
	  category_meta_push ( iter -> alt_category, NULL /* no parent cat */, iter, ovrh, cat_is_visible ( g_conf, iter -> alt_category ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat", 0 ) );
	  category_meta_push ( iter -> alt_category1, iter -> alt_category, iter, ovrh, cat_is_visible ( g_conf, iter -> alt_category1 ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat1", 0 ) );
	  category_meta_push ( iter -> alt_category2, iter -> alt_category, iter, ovrh, cat_is_visible ( g_conf, iter -> alt_category2 ) ); //pnd_conf_get_as_int_d ( g_conf, "tabs.top_altcat2", 0 ) );

	} // app is visible?

      } // has title?

    } // register with categories or filter out

    // next
    iter = pnd_box_get_next ( iter );
    itercount++;
  } // while

 dirbrowser_scan:

  // set up filesystem browser tabs
  if ( pnd_conf_get_as_int_d ( g_conf, "filesystem.do_browser", 0 ) ) {
    char *searchpath = pnd_conf_get_as_char ( g_conf, "filesystem.tab_searchpaths" );

    SEARCHPATH_PRE
    {
      char *c, *tabname;
      c = strrchr ( buffer, '/' );
      if ( c && (*(c+1)!='\0') ) {
	tabname = c;
      } else {
	tabname = buffer;
      }

      // check if dir is empty; if so, skip it.
      if ( ! is_dir_empty ( buffer ) ) {
	category_push ( tabname /* tab name */, NULL /* app */, 0 /* override */, buffer /* fspath */, 1 /* visible */ );
      }

    }
    SEARCHPATH_POST

  } // set up fs browser tabs

  // dump categories
  //category_dump();

  // publish desired categories
  category_publish ( CFNORMAL, NULL );

  // let deferred icon cache go now
  ui_post_scan();

  return;
}

static char _vbuf [ 512 ];
unsigned char cat_is_visible ( pnd_conf_handle h, char *catname ) {
  snprintf ( _vbuf, 500, "tabshow.%s", catname );
  return ( pnd_conf_get_as_int_d ( g_conf, _vbuf, 1 ) ); // default to 'show' when unknown
}

unsigned char app_is_visible ( pnd_conf_handle h, char *uniqueid ) {
  snprintf ( _vbuf, 500, "appshow.%s", uniqueid );
  return ( pnd_conf_get_as_int_d ( g_conf, _vbuf, 1 ) ); // default to 'show' when unknown
}

void sigquit_handler ( int n ) {
  pnd_log ( pndn_rem, "SIGQUIT received; graceful exit.\n" );
  emit_and_quit ( MM_QUIT );
}

void setup_notifications ( void ) {

  // figure out notify path
  char *configpath;
  char *notifypath = NULL;

  configpath = pnd_conf_query_searchpath();

  pnd_conf_handle apph;

  apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

  if ( apph ) {

    notifypath = pnd_conf_get_as_char ( apph, PND_APPS_NOTIFY_KEY );

    if ( ! notifypath ) {
      notifypath = PND_APPS_NOTIFYPATH;
    }

  }

  // given notify path.. ripped from pndnotifyd :(
  char *searchpath = notifypath;

  // if this is first time through, we can just set it up; for subsequent times
  // through, we need to close existing fd and re-open it, since we're too lame
  // to store the list of watches and 'rm' them
#if 1
  if ( nh ) {
    pnd_notify_shutdown ( nh );
    nh = 0;
  }
#endif

  // set up a new set of notifies
  if ( ! nh ) {
    nh = pnd_notify_init();
  }

  if ( ! nh ) {
    pnd_log ( pndn_rem, "INOTIFY failed to init.\n" );
    exit ( -1 );
  }

#if 0
  pnd_log ( pndn_rem, "INOTIFY is up.\n" );
#endif

  SEARCHPATH_PRE
  {

    pnd_log ( pndn_rem, "Watching path '%s' and its descendents.\n", buffer );
    pnd_notify_watch_path ( nh, buffer, PND_NOTIFY_RECURSE );

  }
  SEARCHPATH_POST

  return;
}

// for Pleng
// Goal: normally menu will quit when an app is invoked, but there are cases when some folks
// may configure their system and want mmenu to live instead (save on restart time, don't care
// about RAM, using a multitasking tray/window manager setup...), so instead of 'exit and emit'
// here we just run the app directly and cross fingers!
void emit_and_run ( char *buffer ) {

  // run the bloody thing
  int f;

  if ( ( f = fork() ) < 0 ) {
    // error forking
  } else if ( f > 0 ) {
    // parent
  } else {
    // child, do it
    execl ( "/bin/sh", "/bin/sh", "-c", buffer + strlen(MM_RUN) + 1, (char*) NULL );
  } 

  return;
}

// this code was swiped from pnd_utility pnd_exec_no_wait_1 as it became a little too minimenu-specific to remain there
void exec_raw_binary ( char *fullpath ) {
  int i;

  if ( ( i = fork() ) < 0 ) {
    printf ( "ERROR: Couldn't fork()\n" );
    return;
  }

  if ( i ) {
    return; // parent process, don't care
  }

  // child process, do something
  execl ( "/bin/sh", "/bin/sh", "-c", fullpath, (char*) NULL );
  //execl ( fullpath, fullpath, (char*) NULL );

  // error invoking something, and we're the child process, so just die before all hell breaks lose with us thinking we're the (second!) parent on return!
  exit ( -1 );

  // getting here is an error
  //printf ( "Error attempting to run %s\n", fullpath );

  return;
}
