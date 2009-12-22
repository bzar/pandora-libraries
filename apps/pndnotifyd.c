
/* pndnotifyd - a daemon whose job is to monitor searchpaths for app changes (appearing, disappearing, changing).
 * If a change is found, the discovery code is invoked and apps registered for the launchers to see
 *
 */

#include <stdio.h>     // for stdio
#include <unistd.h>    // for exit()
#include <stdlib.h>    // for exit()
#define __USE_GNU /* for strcasestr */
#include <string.h>
#include <time.h>      // for time()
#include <ctype.h>     // for isdigit()
#include <sys/types.h> // for umask
#include <sys/stat.h>  // for umask
#include <dirent.h>    // for opendir()
#include <signal.h>    // for sigaction

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_notify.h"
#include "../lib/pnd_pathiter.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_pxml.h"
#include "pnd_utility.h"
#include "pnd_desktop.h"
#include "pnd_logger.h"

// this piece of code was simpler once; but need to grow it a bit and in a rush
// moving all these to globals rather than refactor the code a bit; tsk tsk..

// op mode; emitting stdout or no?
static unsigned char g_daemon_mode = 0;

typedef enum {
  pndn_debug = 0,
  pndn_rem,          // will set default log level to here, so 'debug' is omitted
  pndn_warning,
  pndn_error,
  pndn_none
} pndnotify_loglevels_e;

// like discotest
char *configpath;
char *overridespath;
// daemon stuff
char *searchpath = NULL;
char *notifypath = NULL;
time_t createtime = 0; // all 'new' .destops are created at or after this time; prev are old.
// dotfiles; this used to be a single pai .. now two pairs, a little unwieldy; pnd_box it up?
char *desktop_dotdesktoppath = NULL;
char *desktop_iconpath = NULL;
char *desktop_appspath = NULL;
char *menu_dotdesktoppath = NULL;
char *menu_iconpath = NULL;
char *menu_appspath = NULL;
// pnd runscript
char *run_searchpath; // searchpath to find pnd_run.sh
char *run_script;     // name of pnd_run.sh script from config
char *pndrun;         // full path to located pnd_run.sh
char *pndhup = NULL;  // full path to located pnd_hup.sh
// notifier handle
pnd_notify_handle nh = 0;

// constants
#define PNDNOTIFYD_LOGLEVEL "pndnotifyd.loglevel"

// decl's
void consume_configuration ( void );
void setup_notifications ( void );
void sighup_handler ( int n );
void process_discoveries ( pnd_box_handle applist, char *emitdesktoppath, char *emiticonpath );
unsigned char perform_discoveries ( char *appspath, char *overridespath,
				    char *emitdesktoppath, char *emiticonpath );

int main ( int argc, char *argv[] ) {
  // behaviour
  unsigned char scanonlaunch = 1;
  unsigned int interval_secs = 5;
  // misc
  int i;

  /* iterate across args
   */
  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'd' ) {
      //printf ( "Going daemon mode. Silent running.\n" );
      g_daemon_mode = 1;
    } else if ( isdigit ( argv [ i ][ 0 ] ) ) {
      interval_secs = atoi ( argv [ i ] );
    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'n' ) {
      scanonlaunch = 0;
    } else {
      printf ( "%s [-d] [##]\n", argv [ 0 ] );
      printf ( "-d\tDaemon mode; detach from terminal, chdir to /tmp, suppress output. Optional.\n" );
      printf ( "-n\tDo not scan on launch; default is to run a scan for apps when %s is invoked.\n", argv [ 0 ] );
      printf ( "##\tA numeric value is interpreted as number of seconds between checking for filesystem changes. Default %u.\n",
	       interval_secs );
      printf ( "Signal: HUP the process to force reload of configuration and reset the notifier watch paths\n" );
      exit ( 0 );
    }

  } // for

  /* enable logging?
   */
  if ( g_daemon_mode ) {
    // nada
  } else {
    pnd_log_set_filter ( pndn_rem );
    pnd_log_set_pretext ( "pndnotifyd" );
    pnd_log_to_stdout();
    pnd_log_set_flush ( 1 );
    pnd_log ( pndn_rem, "log level starting as %u", pnd_log_get_filter() );
  }

  pnd_log ( pndn_rem, "Interval between checks is %u seconds\n", interval_secs );

  // check if inotify is awake yet; if not, try waiting for awhile to see if it does
  pnd_log ( pndn_rem, "Starting INOTIFY test; should be instant, but may take awhile...\n" );

  if ( ! pnd_notify_wait_until_ready ( 120 /* seconds */ ) ) {
    pnd_log ( pndn_error, "ERROR: INOTIFY refuses to be useful and quite awhile has passed. Bailing out.\n" );
    return ( -1 );
  }

  pnd_log ( pndn_rem, "INOTIFY seems to be useful, whew.\n" );

  // basic daemon set up
  if ( g_daemon_mode ) {

    // set a CWD somewhere else
#if 0
    chdir ( "/tmp" );
#endif

    // detach from terminal
    if ( ( i = fork() ) < 0 ) {
      pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
      exit ( i );
    }
    if ( i ) {
      exit ( 0 ); // exit parent
    }
    setsid();

    // umask
    umask ( 022 ); // emitted files can be rwxr-xr-x
    
  } // set up daemon

  // wait for a user to be logged in - we should probably get hupped when a user logs in, so we can handle
  // log-out and back in again, with SDs popping in and out between..
  pnd_log ( pndn_rem, "Checking to see if a user is logged in\n" );
  char tmp_username [ 128 ];
  while ( 1 ) {
    if ( pnd_check_login ( tmp_username, 127 ) ) {
      break;
    }
    pnd_log ( pndn_debug, "  No one logged in yet .. spinning.\n" );
    sleep ( 2 );
  } // spin
  pnd_log ( pndn_rem, "Looks like user '%s' is in, continue.\n", tmp_username );

  /* parse configs
   */

  consume_configuration();

  /* startup
   */

  pnd_log ( pndn_rem, "PXML overrides searchpath is '%s'\n", overridespath );
  pnd_log ( pndn_rem, "Notify searchpath is '%s'\n", notifypath );

  pnd_log ( pndn_rem, "Desktop apps ---------------------------------\n" );
  pnd_log ( pndn_rem, "Apps searchpath is '%s'\n", desktop_appspath );
  pnd_log ( pndn_rem, ".desktop files emit to '%s'\n", desktop_dotdesktoppath );
  pnd_log ( pndn_rem, ".desktop icon files emit to '%s'\n", desktop_iconpath );

  pnd_log ( pndn_rem, "Menu apps ---------------------------------\n" );
  pnd_log ( pndn_rem, "Apps searchpath is '%s'\n", menu_appspath );
  pnd_log ( pndn_rem, ".desktop files emit to '%s'\n", menu_dotdesktoppath );
  pnd_log ( pndn_rem, ".desktop icon files emit to '%s'\n", menu_iconpath );

  /* set up signal handler
   */
  sigset_t ss;
  sigemptyset ( &ss );

  struct sigaction siggy;
  siggy.sa_handler = sighup_handler;
  siggy.sa_mask = ss; /* implicitly blocks the origin signal */
  siggy.sa_flags = 0; /* don't need anything */

  sigaction ( SIGHUP, &siggy, NULL );

  /* set up notifies
   */

  // if we're gong to scan on launch, it'll set things right up and then set up notifications.
  // if no scan on launch, we need to set the notif's up now.
  //if ( ! scanonlaunch ) {
  setup_notifications();
  //}

  /* daemon main loop
   */
  while ( 1 ) {

    // need to rediscover?
    if ( scanonlaunch ||
	 pnd_notify_rediscover_p ( nh ) )
    {
      createtime = time ( NULL ); // all 'new' .destops are created at or after this time; prev are old.

      // if this was a forced scan, lets not do that next iteration
      if ( scanonlaunch ) {
	scanonlaunch = 0;
      }

      // by this point, the watched directories have notified us that something of relevent
      // has occurred; we should be clever, but we're not, so just re-brute force the
      // discovery and spit out .desktop files..
      pnd_log ( pndn_rem, "------------------------------------------------------\n" );
      pnd_log ( pndn_rem, "Changes within watched paths .. performing re-discover!\n" );

      /* run the discovery
       */

      pnd_log ( pndn_rem, "Scanning desktop paths----------------------------\n" );
      if ( ! perform_discoveries ( desktop_appspath, overridespath, desktop_dotdesktoppath, desktop_iconpath ) ) {
	pnd_log ( pndn_rem, "No applications found in desktop search path\n" );
      }

      if ( menu_appspath && menu_dotdesktoppath && menu_iconpath ) {
	pnd_log ( pndn_rem, "Scanning menu paths----------------------------\n" );
	if ( ! perform_discoveries ( menu_appspath, overridespath, menu_dotdesktoppath, menu_iconpath ) ) {
	  pnd_log ( pndn_rem, "No applications found in menu search path\n" );
	}
      }

      // if we've got a hup script located, lets invoke it
      if ( pndhup ) {
	pnd_log ( pndn_rem, "Invoking hup script '%s'.\n", pndhup );
	pnd_exec_no_wait_1 ( pndhup, NULL );
      }

      // since its entirely likely new directories have been found (ie: SD with a directory structure was inserted)
      // we should re-apply watches to catch all these new directories; ie: user might use on-device browser to
      // drop in new applications, or use the shell to juggle them around, or any number of activities.
      //setup_notifications();

    } // need to rediscover?

    // lets not eat up all the CPU
    // should use an alarm or select() or something -- I mean really, why aren't I putting interval_secs into
    // the select() call above in pnd_notify_whatsitcalled()? -- but lets not break this right before release shall we
    // NOTE: Oh right, I remember now -- inotify will spam when a card is inserted, and it will not be instantaneoous..
    // the events will dribble in over a second. So this sleep is a lame trick that generally works. I really should
    // do select(), and then when it returns just spin for a couple seconds slurping up events until no more and a thresh-hold
    // time is hit, but this will do for now. I suck.
    sleep ( interval_secs );

  } // while

  /* shutdown
   */
  pnd_notify_shutdown ( nh );

  return ( 0 );
}

void consume_configuration ( void ) {

  // attempt to fetch a sensible default searchpath for configs
  configpath = pnd_conf_query_searchpath();

  // attempt to fetch the apps config to pick up a searchpath
  pnd_conf_handle apph;

  apph = pnd_conf_fetch_by_id ( pnd_conf_apps, configpath );

  if ( apph ) {

    overridespath = pnd_conf_get_as_char ( apph, PND_PXML_OVERRIDE_KEY );

    if ( ! overridespath ) {
      overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
    }

    notifypath = pnd_conf_get_as_char ( apph, PND_APPS_NOTIFY_KEY );

    if ( ! notifypath ) {
      notifypath = PND_APPS_NOTIFYPATH;
    }

  } else {
    // couldn't find a useful app search path so use the default
    overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
    notifypath = PND_APPS_NOTIFYPATH;
  }

  // attempt to figure out where to drop dotfiles .. now that we're going
  // multi-target we see the limit of my rudimentary conf-file parser; should
  // just parse to an array of targets, rather that hardcoding two, but
  // on the other hand, don't likely see the need for more than two? (famous
  // last words.)
  pnd_conf_handle desktoph;

  desktoph = pnd_conf_fetch_by_id ( pnd_conf_desktop, configpath );

  // for 'desktop' main applications
  if ( desktoph ) {
    desktop_dotdesktoppath = pnd_conf_get_as_char ( desktoph, PND_DESKTOP_DOTDESKTOP_PATH_KEY );
    desktop_iconpath = pnd_conf_get_as_char ( desktoph, PND_DESKTOP_ICONS_PATH_KEY );
    desktop_appspath = pnd_conf_get_as_char ( desktoph, PND_DESKTOP_SEARCH_KEY );
  }

  if ( ! desktop_dotdesktoppath ) {
    desktop_dotdesktoppath = PND_DESKTOP_DOTDESKTOP_PATH_DEFAULT;
  }

  if ( ! desktop_iconpath ) {
    desktop_iconpath = PND_DESKTOP_ICONS_PATH_DEFAULT;
  }

  if ( ! desktop_appspath ) {
    desktop_appspath = PND_DESKTOP_SEARCH_PATH_DEFAULT;
  }

  // for 'menu' applications
  if ( desktoph ) {
    menu_dotdesktoppath = pnd_conf_get_as_char ( desktoph, PND_MENU_DOTDESKTOP_PATH_KEY );
    menu_iconpath = pnd_conf_get_as_char ( desktoph, PND_MENU_ICONS_PATH_KEY );
    menu_appspath = pnd_conf_get_as_char ( desktoph, PND_MENU_SEARCH_KEY );
  }

  /* try to locate a runscript and optional hupscript
   */

  if ( apph ) {
    run_searchpath = pnd_conf_get_as_char ( apph, PND_PNDRUN_SEARCHPATH_KEY );
    run_script = pnd_conf_get_as_char ( apph, PND_PNDRUN_KEY );
    pndrun = NULL;

    if ( ! run_searchpath ) {
      run_searchpath = PND_APPS_SEARCHPATH;
      run_script = PND_PNDRUN_FILENAME;
    }

    if ( pnd_conf_get_as_int ( apph, PNDNOTIFYD_LOGLEVEL ) != PND_CONF_BADNUM ) {
      pnd_log_set_filter ( pnd_conf_get_as_int ( apph, PNDNOTIFYD_LOGLEVEL ) );
      pnd_log ( pndn_rem, "config file causes loglevel to change to %u", pnd_log_get_filter() );
    }

  } else {
    run_searchpath = NULL;
    run_script = NULL;
    pndrun = PND_PNDRUN_DEFAULT;
  }

  if ( ! pndrun ) {
    pndrun = pnd_locate_filename ( run_searchpath, run_script );

    if ( pndrun ) {
      pndrun = strdup ( pndrun ); // so we don't just use the built in buffer; next locate will overwrite it
    } else {
      pndrun = PND_PNDRUN_DEFAULT;
    }

  }

  if ( desktoph && run_searchpath ) {
    char *t;

    if ( ( t = pnd_conf_get_as_char ( desktoph, PND_PNDHUP_KEY ) ) ) {
      pndhup = pnd_locate_filename ( run_searchpath, t );

      if ( pndhup ) {
	pndhup = strdup ( pndhup ); // so we don't just use the built in buffer; next locate will overwrite it
      }

#if 0 // don't enable this; if no key in config, we don't want to bother hupping
    } else {
      pndhup = pnd_locate_filename ( run_searchpath, PND_PNDHUP_FILENAME );
#endif
    }

  }

  // debug
  if ( run_searchpath ) pnd_log ( pndn_rem, "Locating pnd run in %s\n", run_searchpath );
  if ( run_script ) pnd_log ( pndn_rem, "Locating pnd runscript as %s\n", run_script );
  if ( pndrun ) pnd_log ( pndn_rem, "pndrun is %s\n", pndrun );
  if ( pndhup ) {
    pnd_log ( pndn_rem, "pndhup is %s\n", pndhup );
  } else {
    pnd_log ( pndn_rem, "No pndhup found (which is fine.)\n" );
  }

  /* cheap hack, maybe it'll help when pndnotifyd is coming up before the rest of the system and before
   * the user is formally logged in
   */
  pnd_log ( pndn_rem, "Setting a default $HOME to non-root user\n" );
  {
    DIR *dir;

    if ( ( dir = opendir ( "/home" ) ) ) {
      struct dirent *dirent;

      while ( ( dirent = readdir ( dir ) ) ) {
	pnd_log ( pndn_rem, "  Found user homedir '%s'\n", dirent -> d_name );

	// file is a .desktop?
	if ( dirent -> d_name [ 0 ] == '.' ) {
	  continue;
	} else if ( strcmp ( dirent -> d_name, "root" ) == 0 ) {
	  continue;
	}

	// a non-root user is found
	char buffer [ 200 ];
	sprintf ( buffer, "/home/%s", dirent -> d_name );
	pnd_log ( pndn_rem, "  Going with '%s'\n", buffer );
	setenv ( "HOME", buffer, 1 /* overwrite */ );

      } // while iterating through dir listing

      closedir ( dir );
    } // opendir?

  } // scope

  /* handle globbing or variable substitution
   */
  desktop_dotdesktoppath = pnd_expand_tilde ( strdup ( desktop_dotdesktoppath ) );
  desktop_iconpath = pnd_expand_tilde ( strdup ( desktop_iconpath ) );
  mkdir ( desktop_dotdesktoppath, 0777 );
  mkdir ( desktop_iconpath, 0777 );

  if ( menu_dotdesktoppath ) {
    menu_dotdesktoppath = pnd_expand_tilde ( strdup ( menu_dotdesktoppath ) );
    mkdir ( menu_dotdesktoppath, 0777 );
  }
  if ( menu_iconpath ) {
    menu_iconpath = pnd_expand_tilde ( strdup ( menu_iconpath ) );
    mkdir ( menu_iconpath, 0777 );
  }

  // done
  return;
}

void setup_notifications ( void ) {
  searchpath = notifypath;

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

void sighup_handler ( int n ) {

  pnd_log ( pndn_rem, "---[ SIGHUP received ]---\n" );

  // reparse config files
  consume_configuration();

  // re set up the notifier watches
  setup_notifications();

  return;
}

// This very recently was inline code; just slight refactor to functionize it so that it can be
// reused in a couple of places. Simple code with simple design quickly became too large for
// its simple design; should revisit a lot of these little things..
void process_discoveries ( pnd_box_handle applist, char *emitdesktoppath, char *emiticonpath ) {
  pnd_disco_t *d = pnd_box_get_head ( applist );

  while ( d ) {

    pnd_log ( pndn_rem, "Found app: %s\n", pnd_box_get_key ( d ) );

    // check if icon already exists (from a previous extraction say); if so, we needn't
    // do it again
    char existingpath [ FILENAME_MAX ];
    sprintf ( existingpath, "%s/%s.png", emiticonpath, d -> unique_id );

    struct stat dirs;
    if ( stat ( existingpath, &dirs ) == 0 ) {
      // icon seems to exist, so just crib the location into the .desktop

      pnd_log ( pndn_rem, "  Found icon already existed, so reusing it! %s\n", existingpath );

      if ( d -> icon ) {
	free ( d -> icon );
      }
      d -> icon = strdup ( existingpath );

    } else {
      // icon seems unreadable or does not exist; lets try to create it..

      pnd_log ( pndn_debug, "  Icon not already present, so trying to write it! %s\n", existingpath );

      // attempt to create icon files; if successful, alter the disco struct to contain new
      // path, otherwise leave it alone (since it could be a generic icon reference..)
      if ( pnd_emit_icon ( emiticonpath, d ) ) {
	// success; fix up icon path to new one..
	if ( d -> icon ) {
	  free ( d -> icon );
	}
	d -> icon = strdup ( existingpath );
      } else {
	pnd_log ( pndn_debug, "  WARN: Couldn't write out icon %s\n", existingpath );
      }

    } // icon already exists?

    // create the .desktop file
    if ( pnd_emit_dotdesktop ( emitdesktoppath, pndrun, d ) ) {
      // add a watch onto the newly created .desktop?
#if 0
      char buffer [ FILENAME_MAX ];
      sprintf ( buffer, "%s/%s", emitdesktoppath, d -> unique_id );
      pnd_notify_watch_path ( nh, buffer, PND_NOTIFY_RECURSE );
#endif
    } else {
      pnd_log ( pndn_rem, "ERROR: Error creating .desktop file for app: %s\n", pnd_box_get_key ( d ) );
    }

    // next!
    d = pnd_box_get_next ( d );

  } // while applist

  return;
}

// returns true if any applications were found
unsigned char perform_discoveries ( char *appspath, char *overridespath,              // args to do discovery
				    char *emitdesktoppath, char *emiticonpath )       // args to do emitting
{
  pnd_box_handle applist;

  // attempt to auto-discover applications in the given path
  applist = pnd_disco_search ( appspath, overridespath );

  if ( applist ) {
    process_discoveries ( applist, emitdesktoppath, emiticonpath );
  }

  // run a clean up, to remove any dotdesktop files that we didn't
  // just now create (that seem to have been created by pndnotifyd
  // previously.) This allows SD eject (or .pnd remove) to remove
  // an app from the launcher
  //   NOTE: Could opendir and iterate across all .desktop files,
  // removing any that have Source= something else, and that the
  // app name is not in the list found in applist box above. But
  // a cheesy simple way right now is to just remove .desktop files
  // that have a last mod time prior to the time we stored above.
  {
    DIR *dir;

    if ( ( dir = opendir ( emitdesktoppath ) ) ) {
      struct dirent *dirent;
      struct stat dirs;
      char buffer [ FILENAME_MAX ];

      while ( ( dirent = readdir ( dir ) ) ) {

	// file is a .desktop?
	if ( strstr ( dirent -> d_name, ".desktop" ) == NULL ) {
	  continue;
	}

	// figure out full path
	sprintf ( buffer, "%s/%s", emitdesktoppath, dirent -> d_name );

	// file was previously created by libpnd; check Source= line
	// logic: default to 'yes' (in case we can't open the file for some reason)
	//        if we can open the file, default to no and look for the source flag we added; if
	//          that matches then we know its libpnd created, otherwise assume not.
	unsigned char source_libpnd = 1;
	{
	  char line [ 256 ];
	  FILE *grep = fopen ( buffer, "r" );
	  if ( grep ) {
	    source_libpnd = 0;
	    while ( fgets ( line, 255, grep ) ) {
	      if ( strcasestr ( line, PND_DOTDESKTOP_SOURCE ) ) {
		source_libpnd = 2;
	      }
	    } // while
	    fclose ( grep );
	  }
	}
	if ( source_libpnd ) {
#if 0
	  pnd_log ( pndn_rem,
		    "File '%s' appears to have been created by libpnd so candidate for delete: %u\n", buffer, source_libpnd );
#endif
	} else {
#if 0
	  pnd_log ( pndn_rem, "File '%s' appears NOT to have been created by libpnd, so leave it alone\n", buffer );
#endif
	  continue; // skip deleting it
	}

	// file is 'new'?
	if ( stat ( buffer, &dirs ) == 0 ) {
	  if ( dirs.st_mtime >= createtime ) {
#if 0
	    pnd_log ( pndn_rem, "File '%s' seems 'new', so leave it alone.\n", buffer );
#endif
	    continue; // skip deleting it
	  }
	}

	// by this point, the .desktop file must be 'old' and created by pndnotifyd
	// previously, so can remove it
	pnd_log ( pndn_rem, "File '%s' seems nolonger relevent; removing it.\n", dirent -> d_name );
	unlink ( buffer );

      } // while getting filenames from dir

      closedir ( dir );
    }

  } // purge old .desktop files

  //WARN: MEMORY LEAK HERE
  pnd_log ( pndn_debug, "pndnotifyd - memory leak here - perform_discoveries()\n" );
  if ( applist ) {
    pnd_box_delete ( applist ); // does not free the disco_t contents!
  }

  return ( 1 );
}
