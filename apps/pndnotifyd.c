
/* pndnotifyd - a daemon whose job is to monitor searchpaths for app changes (appearing, disappearing, changing).
 * If a change is found, the discovery code is invoked and apps registered for the launchers to see
 *
 */

// TODO: Catch HUP and reparse config
// TODO: Should perhaps direct all printf's through a vsprintf handler to avoid redundant "if ! g_daemon_mode"
// TODO: During daemon mode, should perhaps syslog or log errors

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
char *appspath;
char *overridespath;
// daemon stuff
char *searchpath = NULL;
char *dotdesktoppath = NULL;
char *iconpath = NULL;
char *notifypath = NULL;
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

int main ( int argc, char *argv[] ) {
  // behaviour
  unsigned char scanonlaunch = 1;
  unsigned int interval_secs = 10;
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
    pnd_log ( pndn_rem, "log level starting as %u", pnd_log_get_filter() );
  }

  pnd_log ( pndn_rem, "Interval between checks is %u seconds\n", interval_secs );

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

  /* parse configs
   */

  consume_configuration();

  /* startup
   */

  pnd_log ( pndn_rem, "Apps searchpath is '%s'\n", appspath );
  pnd_log ( pndn_rem, "PXML overrides searchpath is '%s'\n", overridespath );
  pnd_log ( pndn_rem, ".desktop files emit to '%s'\n", dotdesktoppath );
  pnd_log ( pndn_rem, ".desktop icon files emit to '%s'\n", iconpath );
  pnd_log ( pndn_rem, "Notify searchpath is '%s'\n", notifypath );

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
      pnd_box_handle applist;
      time_t createtime = time ( NULL ); // all 'new' .destops are created at or after this time; prev are old.

      // if this was a forced scan, lets not do that next iteration
      if ( scanonlaunch ) {
	scanonlaunch = 0;
      }

      // by this point, the watched directories have notified us that something of relevent
      // has occurred; we should be clever, but we're not, so just re-brute force the
      // discovery and spit out .desktop files..
      pnd_log ( pndn_rem, "------------------------------------------------------\n" );
      pnd_log ( pndn_rem, "Changes within watched paths .. performing re-discover!\n" );

      // run the discovery
      applist = pnd_disco_search ( appspath, overridespath );

      // list the found apps (if any)
      if ( applist ) {
	pnd_disco_t *d = pnd_box_get_head ( applist );

	while ( d ) {

	  pnd_log ( pndn_rem, "Found app: %s\n", pnd_box_get_key ( d ) );

	  // check if icon already exists (from a previous extraction say); if so, we needn't
	  // do it again
	  char existingpath [ FILENAME_MAX ];
	  sprintf ( existingpath, "%s/%s.png", iconpath, d -> unique_id );

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

	    pnd_log ( pndn_rem, "  Icon not already present, so trying to write it! %s\n", existingpath );

	    // attempt to create icon files; if successful, alter the disco struct to contain new
	    // path, otherwise leave it alone (since it could be a generic icon reference..)
	    if ( pnd_emit_icon ( iconpath, d ) ) {
	      // success; fix up icon path to new one..
	      if ( d -> icon ) {
		free ( d -> icon );
	      }
	      d -> icon = strdup ( existingpath );
	    } else {
	      pnd_log ( pndn_rem, "  WARN: Couldn't write out icon %s\n", existingpath );
	    }

	  } // icon already exists?

	  // create the .desktop file
	  if ( pnd_emit_dotdesktop ( dotdesktoppath, pndrun, d ) ) {
	    // add a watch onto the newly created .desktop?
#if 0
	    char buffer [ FILENAME_MAX ];
	    sprintf ( buffer, "%s/%s", dotdesktoppath, d -> unique_id );
	    pnd_notify_watch_path ( nh, buffer, PND_NOTIFY_RECURSE );
#endif
	  } else {
	    pnd_log ( pndn_rem, "ERROR: Error creating .desktop file for app: %s\n", pnd_box_get_key ( d ) );
	  }

	  // next!
	  d = pnd_box_get_next ( d );

	} // while applist

      } else {

	pnd_log ( pndn_rem, "No applications found in search path\n" );

      } // got apps?

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

	if ( ( dir = opendir ( dotdesktoppath ) ) ) {
	  struct dirent *dirent;
	  struct stat dirs;
	  char buffer [ FILENAME_MAX ];

	  while ( ( dirent = readdir ( dir ) ) ) {

	    // file is a .desktop?
	    if ( strstr ( dirent -> d_name, ".desktop" ) == NULL ) {
	      continue;
	    }

	    // figure out full path
	    sprintf ( buffer, "%s/%s", dotdesktoppath, dirent -> d_name );

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
    // should use an alarm or select() or something
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

    appspath = pnd_conf_get_as_char ( apph, PND_APPS_KEY );

    if ( ! appspath ) {
      appspath = PND_APPS_SEARCHPATH;
    }

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
    appspath = PND_APPS_SEARCHPATH;
    overridespath = PND_PXML_OVERRIDE_SEARCHPATH;
    notifypath = PND_APPS_NOTIFYPATH;
  }

  // attempt to figure out where to drop dotfiles
  pnd_conf_handle desktoph;

  desktoph = pnd_conf_fetch_by_id ( pnd_conf_desktop, configpath );

  if ( desktoph ) {
    dotdesktoppath = pnd_conf_get_as_char ( desktoph, PND_DOTDESKTOP_KEY );

    if ( ! dotdesktoppath ) {
      dotdesktoppath = PND_DOTDESKTOP_DEFAULT;
    }

    iconpath = pnd_conf_get_as_char ( desktoph, PND_DOTDESKTOPICONS_KEY );

    if ( ! iconpath ) {
      iconpath = PND_DOTDESKTOPICONS_DEFAULT;
    }

  } else {
    dotdesktoppath = PND_DOTDESKTOPICONS_DEFAULT;
  }

  // try to locate a runscript and optional hupscript

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

  /* handle globbing or variable substitution
   */
  dotdesktoppath = pnd_expand_tilde ( strdup ( dotdesktoppath ) );
  iconpath = pnd_expand_tilde ( strdup ( iconpath ) );

  /* validate paths
   */
  mkdir ( dotdesktoppath, 0777 );
  mkdir ( iconpath, 0777 );

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
