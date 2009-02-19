
/* pndnotifyd - a daemon whose job is to monitor searchpaths for app changes (appearing, disappearing, changing).
 * If a change is found, the discovery code is invoked and apps registered for the launchers to see
 *
 */

// TODO: for daemon mode, need to detach from the terminal
// TODO: Catch HUP and reparse config

#include <stdio.h> // for stdio
#include <unistd.h> // for exit()
#include <stdlib.h> // for exit()
#include <string.h>
#include <time.h> // for time()
#include <ctype.h> // for isdigit()

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_notify.h"
#include "../lib/pnd_pathiter.h"

static unsigned char g_daemon_mode = 0;

int main ( int argc, char *argv[] ) {
  pnd_notify_handle nh;
  char *configpath;
  char *appspath;
  char *searchpath;
  int i;

  unsigned int interval_secs = 60;

  /* iterate across args
   */
  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'd' ) {
      //printf ( "Going daemon mode. Silent running.\n" );
      g_daemon_mode = 1;
    } else if ( isdigit ( argv [ i ][ 0 ] ) ) {
      interval_secs = atoi ( argv [ i ] );
    } else {
      printf ( "%s [-d] [##]\n", argv [ 0 ] );
      printf ( "-d\tDaemon mode; detach from terminal, chdir to /tmp, suppress output. Optional.\n" );
      printf ( "##\tA numeric value is interpreted as number of seconds between checking for filesystem changes. Default %u.\n",
	       interval_secs );
      exit ( 0 );
    }

  }

  if ( ! g_daemon_mode ) {
    printf ( "Interval between checks is %u seconds\n", interval_secs );
  }

  /* parse configs
   */

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

  } else {
    // couldn't find a useful app search path so use the default
    appspath = PND_APPS_SEARCHPATH;
  }

  if ( ! g_daemon_mode ) {
    printf ( "Apps searchpath is '%s'\n", appspath );
  }

  /* set up notifies
   */
  searchpath = appspath;

  nh = pnd_notify_init();

  if ( ! nh ) {
    if ( ! g_daemon_mode ) {
      printf ( "INOTIFY failed to init.\n" );
    }
    exit ( -1 );
  }

  if ( ! g_daemon_mode ) {
    printf ( "INOTIFY is up.\n" );
  }

  SEARCHPATH_PRE
  {

    pnd_notify_watch_path ( nh, buffer, PND_NOTIFY_RECURSE );

    if ( ! g_daemon_mode ) {
      printf ( "Watching path '%s' and its descendents.\n", buffer );
    }

  }
  SEARCHPATH_POST

  /* daemon main loop
   */
  while ( 1 ) {

    // need to rediscover?
    if ( pnd_notify_rediscover_p ( nh ) ) {

      // by this point, the watched directories have notified us that something of relevent
      // has occurred; we should be clever, but we're not, so just re-brute force the
      // discovery and spit out .desktop files..
      if ( ! g_daemon_mode ) {
	printf ( "Time to re-discover!\n" );
      }

      // lets not eat up all the CPU
      // should use an alarm or select() or something
      sleep ( interval_secs );

    } // need to rediscover?

  } // while

  /* shutdown
   */
  pnd_notify_shutdown ( nh );

  return ( 0 );
}
