
/* minimenu
 * aka "2wm" - too weak menu, two week menu, akin to twm
 *
 * Craig wants a super minimal menu ASAP before launch, so lets see what I can put together in 2 weeks with not much
 * free time ;) I'd like to do a fuller ('tiny', but with plugin support and a decent expansion and customizing design..)
 * but later, baby!
 *
 */

/* mmwrapper -- we probably want the menu to exeunt-with-alarums when running applications, to minimize the memory and
 * performance footprint; it could be running in-X or atop a desktop-env, or purely on SDL or framebuffer with nothing..
 * so wrapper will actually do the running, and be tiny.
 * Likewise, if the menu proper dies, the wrapper can fire it up again.
 * A side effect is, people could use the wrappers abilities, and slap another cruddy menu on top of it .. yay for pure
 * curses based or lynx-based menus ;)
 */

/* mmwrapper's lifecycle:
 * 1) launch 'frontend' (mmmenu, could be others)
 * 2) when it exits, pick up the output...
 *    a) either a command (quit, run some app, pick new frontend)
 *    b) or a crash, in which case, back to (1)
 * 3) execute command, if any
 * 4) go to (1)
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

#include "mmwrapcmd.h"

unsigned char g_daemon_mode = 0;
char *g_frontend = NULL;

typedef enum {
  pndn_debug = 0,
  pndn_rem,          // will set default log level to here, so 'debug' is omitted
  pndn_warning,
  pndn_error,
  pndn_none
} pndnotify_loglevels_e;

int main ( int argc, char *argv[] ) {
  int logall = -1; // -1 means normal logging rules; >=0 means log all!
  int i;

  // pull conf, determine frontend; alternate is to check command line.

  // boilerplate stuff from pndnotifyd and pndevmapperd

  /* iterate across args
   */
  for ( i = 1; i < argc; i++ ) {

    if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'd' ) {
      //printf ( "Going daemon mode. Silent running.\n" );
      g_daemon_mode = 1;
    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'l' ) {

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

    } else if ( argv [ i ][ 0 ] == '-' && argv [ i ][ 1 ] == 'f' && argv [ i ][ 2 ] != '\0' )
    {
      g_frontend = argv [ i ] + 2;
    } else {
      //printf ( "Unknown: %s\n", argv [ i ] );
      printf ( "%s [-l[##]] [-d] [-fFRONTENDPATH]\n", argv [ 0 ] );
      printf ( "-d\tDaemon mode; detach from terminal, chdir to /tmp, suppress output. Optional.\n" );
      printf ( "-l#\tLog-it; -l is 0-and-up (or all), and -l2 means 2-and-up (not all); l[0-3] for now. Log goes to /tmp/mmwrapper.log\n" );
      printf ( "-f\tFull path of frontend to run\n" );
      exit ( 0 );
    }

  } // for

  /* enable logging?
   */
  pnd_log_set_pretext ( "mmwrapper" );
  pnd_log_set_flush ( 1 );

  if ( logall == -1 ) {
    // standard logging; non-daemon versus daemon

    if ( g_daemon_mode ) {
      // nada
    } else {
      pnd_log_set_filter ( pndn_rem );
      //pnd_log_set_filter ( pndn_debug );
      pnd_log_to_stdout();
    }

  } else {
    FILE *f;

    f = fopen ( "/tmp/mmwrapper.log", "w" );

    if ( f ) {
      pnd_log_set_filter ( logall );
      pnd_log_to_stream ( f );
      pnd_log ( pndn_rem, "logall mode - logging to /tmp/mmwrapper.log\n" );
    }

    if ( logall == pndn_debug ) {
      pnd_log_set_buried_logging ( 1 ); // log the shit out of it
      pnd_log ( pndn_rem, "logall mode 0 - turned on buried logging\n" );
    }

  } // logall

  pnd_log ( pndn_rem, "%s built %s %s", argv [ 0 ], __DATE__, __TIME__ );

  pnd_log ( pndn_rem, "log level starting as %u", pnd_log_get_filter() );

  pnd_log ( pndn_rem, "Frontend is %s", g_frontend );

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

  // check frontend
  if ( ! g_frontend ) {
    pnd_log ( pndn_error, "ERROR: No frontend specified!\n" );
    exit ( -1 );
  }

  /* actual work now
   */

  // invoke frontend
  // wait for something to come back when it exits

  char cmdbuf [ 1024 ];
  char *c;
  char *args;

  while ( 1 ) {

    // reset
    bzero ( cmdbuf, 1024 );
    args = NULL;

    // invoke frontend
    pnd_log ( pndn_debug, "Invoking frontend: %s\n", g_frontend );

    FILE *fe = popen ( g_frontend, "r" );
    while ( fgets ( cmdbuf, 1000, fe ) ) {
      if ( strstr ( cmdbuf, MM_WATCHIT ) ) {
	break;
      }
      pnd_log ( pndn_debug, "Junk: %s", cmdbuf );
    }
    pclose ( fe );

    if ( ( c = strchr ( cmdbuf, '\n' ) ) ) {
      *c = '\0'; // truncate trailing newline
    }

    if ( ( c = strchr ( cmdbuf, ' ' ) ) ) {
      *c = '\0';
      args = c + 1;
    }

    pnd_log ( pndn_debug, "Command from frontend: '%s' args: '%s'\n", cmdbuf, args ? args : "none" );

    // deal with command
    if ( strcasecmp ( cmdbuf, MM_QUIT ) == 0 ) {
      // time to die!
      pnd_log ( pndn_rem, "Frontend requests shutdown.\n" );
      exit ( 0 );
    } else if ( strcasecmp ( cmdbuf, MM_RUN ) == 0 ) {
      // shell out and run it
      int retval = system ( args );

      if ( retval < 0 ) {
	pnd_log ( pndn_error, "ERROR: Couldn't invoke application specified by frontend: '%s'\n", args );
      } else {
	pnd_log ( pndn_rem, "Invoked application returned %d\n", WEXITSTATUS(retval) );
      }

    } else {
      pnd_log ( pndn_rem, "Unexpected exit of frontend; restarting it!\n" );
      // next time around the loop
    }

  } // while

  return ( 0 );
} // main
