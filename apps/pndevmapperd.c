
/* pndevmapperd exists to watch for a few interesting events and to launch scripts when they occur
 * ie: when the lid closes, should invoke power-mode toggle sh-script
 */

// woot for writing code while sick.

// this code begs a rewrite, but should work fine; its just arranged goofily
// -> I mean, why the racial divide between keys and other events

#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */
#include <string.h> /* for strdup */
#include <unistd.h>    // for exit()
#include <sys/types.h> // for umask
#include <sys/stat.h>  // for umask
#include <fcntl.h> // for open(2)
#include <errno.h> // for errno
#include <time.h> // for time(2)
#include <ctype.h> // for isdigit
#include <signal.h> // for sigaction
#include <sys/wait.h> // for wait

#include <linux/input.h> // for keys
//#include "../../kernel-rip/input.h" // for keys

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_pndfiles.h"
#include "pnd_pxml.h"
#include "pnd_logger.h"
#include "pnd_utility.h"
#include "pnd_notify.h"

// daemon and logging
//
unsigned char g_daemon_mode = 0;
unsigned int g_minimum_separation = 1;

typedef enum {
  pndn_debug = 0,
  pndn_rem,          // will set default log level to here, so 'debug' is omitted
  pndn_warning,
  pndn_error,
  pndn_none
} pndnotify_loglevels_e;

// key/event definition
//
typedef struct {
  int keycode;
  char *keyname;
} keycode_t;

keycode_t keycodes[] = {
  { KEY_A, "a" },
  { KEY_MENU, "pandora" },
  { KEY_POWER, "power" },
  { -1, NULL }
};

typedef struct {
  int type;
  int code;
  char *name;
} generic_event_t;

generic_event_t generics[] = {
  { EV_SW, 0, "lid-toggle" }, // expecting value 1 (lid close) or 0 (lid open)
  { -1, -1, NULL }
};

// event-to-sh mapping
//
typedef struct {

  unsigned char key_p; // 1 if its a key, otherwise an event

  /* template information
   */
  void *reqs;          // scancode/etc for the event in question

  char *script;        // script to invoke
  //unsigned int hold_min; // minimum hold-time to trigger

  /* state
   */
  time_t last_trigger_time;
  time_t keydown_time;

} evmap_t;

#define MAXEVENTS 255
evmap_t g_evmap [ MAXEVENTS ];
unsigned int g_evmap_max = 0;

/* get to it
 */
void dispatch_key ( int keycode, int val );
void dispatch_event ( int code, int val );
void sigchld_handler ( int n );

static void usage ( char *argv[] ) {
  printf ( "%s [-d]\n", argv [ 0 ] );
  printf ( "-d\tDaemon mode; detach from terminal, chdir to /tmp, suppress output. Optional.\n" );
  printf ( "-l#\tLog-it; -l is 0-and-up (or all), and -l2 means 2-and-up (not all); l[0-3] for now. Log goes to /tmp/pndevmapperd.log\n" );
  return;
}

int main ( int argc, char *argv[] ) {
  int i;
  int logall = -1; // -1 means normal logging rules; >=0 means log all!

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
    } else {
      usage ( argv );
      exit ( 0 );
    }

  } // for

  /* enable logging?
   */
  pnd_log_set_pretext ( "pndevmapperd" );
  pnd_log_set_flush ( 1 );

  if ( logall == -1 ) {
    // standard logging; non-daemon versus daemon

    if ( g_daemon_mode ) {
      // nada
    } else {
      pnd_log_set_filter ( pndn_rem );
      pnd_log_to_stdout();
    }

  } else {
    FILE *f;

    f = fopen ( "/tmp/pndevmapperd.log", "w" );

    if ( f ) {
      pnd_log_set_filter ( logall );
      pnd_log_to_stream ( f );
      pnd_log ( pndn_rem, "logall mode - logging to /tmp/pndevmapperd.log\n" );
    }

    if ( logall == pndn_debug ) {
      pnd_log_set_buried_logging ( 1 ); // log the shit out of it
      pnd_log ( pndn_rem, "logall mode 0 - turned on buried logging\n" );
    }

  } // logall

  pnd_log ( pndn_rem, "%s built %s %s", argv [ 0 ], __DATE__, __TIME__ );

  pnd_log ( pndn_rem, "log level starting as %u", pnd_log_get_filter() );

  // basic daemon set up
  if ( g_daemon_mode ) {

    // set a CWD somewhere else
    chdir ( "/tmp" );

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

  /* hmm, seems to not like working right after boot.. do we depend on another daemon or
   * on giving kernel time to init something, or ... wtf?
   * -- lets give the system some time to wake up
   */
  { // delay

    // this one works for pndnotifyd, which actually needs INOTIFYH..
    //

    // check if inotify is awake yet; if not, try waiting for awhile to see if it does
    pnd_log ( pndn_rem, "Starting INOTIFY test; should be instant, but may take awhile...\n" );

    if ( ! pnd_notify_wait_until_ready ( 120 /* seconds */ ) ) {
      pnd_log ( pndn_error, "ERROR: INOTIFY refuses to be useful and quite awhile has passed. Bailing out.\n" );
      return ( -1 );
    }

    pnd_log ( pndn_rem, "INOTIFY seems to be useful, whew.\n" );

    // pndnotifyd also waits for user to log in .. pretty excessive, especially since
    // what if user wants to close the lid while at the log in screen? for now play the
    // odds as thats pretty unliekly usage scenariom but is clearly not acceptible :/
    //

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

  } // delay

  /* inhale config or die trying
   */
  char *configpath;

  // attempt to fetch a sensible default searchpath for configs
  configpath = pnd_conf_query_searchpath();

  // attempt to fetch the apps config. since it finds us the runscript
  pnd_conf_handle evmaph;

  evmaph = pnd_conf_fetch_by_id ( pnd_conf_evmap, configpath );

  if ( ! evmaph ) {
    // couldn't locate conf, just bail
    pnd_log ( pndn_error, "ERROR: Couldn't locate conf file\n" );
    exit ( -1 );
  }

  /* iterate across conf, stocking the event map
   */
  void *n = pnd_box_get_head ( evmaph );

  while ( n ) {
    char *k = pnd_box_get_key ( n );
    //printf ( "key %s\n", k );

    if ( strncmp ( k, "keys.", 5 ) == 0 ) {
      k += 5;

      // keys should really push push generic-events onto the table, since they;'re just a special case of them
      // to make things easier to read

      // figure out which keycode we're talking about
      keycode_t *p = keycodes;
      while ( p -> keycode != -1 ) {
	if ( strcasecmp ( p -> keyname, k ) == 0 ) {
	  break;
	}
	p++;
      }

      if ( p -> keycode != -1 ) {
	g_evmap [ g_evmap_max ].key_p = 1;    // its a key, not an event
	g_evmap [ g_evmap_max ].reqs = p;     // note the keycode
	g_evmap [ g_evmap_max ].script = n;   // note the script to activate in response
	pnd_log ( pndn_rem, "Registered key %s [%d] to script %s\n", p -> keyname, p -> keycode, (char*) n );
	g_evmap_max++;
      } else {
	pnd_log ( pndn_warning, "WARNING! Key '%s' is not handled by pndevmapperd yet! Skipping.", k );
      }

    } else if ( strncmp ( k, "events.", 7 ) == 0 ) {
      k += 7;

      // yes, key events could really be defined in this generic sense, and really we could just let people
      // put the code and so on right in the conf, but trying to keep it easy on people; maybe should
      // add a 'generic' section to conf file and just let folks redefine random events that way
      // Really, it'd be nice if the /dev/input/events could spit out useful text, and just use scripts
      // to respond without a daemon per se; for that matter, pnd-ls and pnd-map pnd-dotdesktopemitter
      // should just exist as scripts rather than daemons, but whose counting?

      // figure out which keycode we're talking about
      generic_event_t *p = generics;
      while ( p -> code != -1 ) {
	if ( strcasecmp ( p -> name, k ) == 0 ) {
	  break;
	}
	p++;
      }

      if ( p -> code != -1 ) {
	g_evmap [ g_evmap_max ].key_p = 0;    // its an event, not a key
	g_evmap [ g_evmap_max ].reqs = p;     // note the keycode
	g_evmap [ g_evmap_max ].script = n;   // note the script to activate in response
	pnd_log ( pndn_rem, "Registered generic event %s [%d] to script %s\n", p -> name, p -> code, (char*) n );
	g_evmap_max++;
      } else {
	pnd_log ( pndn_warning, "WARNING! Generic event '%s' is not handled by pndevmapperd yet! Skipping.", k );
      }

    } else if ( strncmp ( k, "pndevmapperd.", 7 ) == 0 ) {
      // not consumed here, skip silently

    } else {
      // uhhh
      pnd_log ( pndn_warning, "Unknown config key '%s'; skipping.\n", k );
    }

    n = pnd_box_get_next ( n );
  } // while

  if ( pnd_conf_get_as_int ( evmaph, "pndevmapperd.loglevel" ) != PND_CONF_BADNUM ) {
    pnd_log_set_filter ( pnd_conf_get_as_int ( evmaph, "pndevmapperd.loglevel" ) );
    pnd_log ( pndn_rem, "config file causes loglevel to change to %u", pnd_log_get_filter() );
  }

  if ( pnd_conf_get_as_int ( evmaph, "pndevmapperd.minimum_separation" ) != PND_CONF_BADNUM ) {
    g_minimum_separation = pnd_conf_get_as_int ( evmaph, "pndevmapperd.minimum_separation" );
    pnd_log ( pndn_rem, "config file causes minimum_separation to change to %u", g_minimum_separation );
  }

  /* do we have anything to do?
   */
  if ( ! g_evmap_max ) {
    // uuuh, nothing to do?
    pnd_log ( pndn_warning, "WARNING! No events configured to watch, so just spinning wheels...\n" );
    exit ( -1 );
  } // spin

  /* set up sigchld -- don't want zombies all over; well, we do, but not process zombies
   */
  sigset_t ss;
  sigemptyset ( &ss );

  struct sigaction siggy;
  siggy.sa_handler = sigchld_handler;
  siggy.sa_mask = ss; /* implicitly blocks the origin signal */
  siggy.sa_flags = 0; /* don't need anything */

  sigaction ( SIGCHLD, &siggy, NULL );

  /* actually try to do something useful
   */

  // stolen in part from notaz :)

  // try to locate the appropriate devices
  int id;
  int fds [ 5 ] = { -1, -1, -1, -1, -1 }; // 0 = keypad, 1 = gpio keys
  int imaxfd = 0;

  for ( id = 0; ; id++ ) {
    char fname[64];
    char name[256] = { 0, };
    int fd;

    snprintf ( fname, sizeof ( fname ), "/dev/input/event%i", id );
    fd = open ( fname, O_RDONLY );

    if ( fd == -1 ) {
      break;
    }

    ioctl (fd, EVIOCGNAME(sizeof(name)), name );

    if ( strcmp ( name, "omap_twl4030keypad" ) == 0 ) {
      fds [ 0 ] = fd;
    } else if ( strcmp ( name, "gpio-keys" ) == 0) {
      fds [ 1 ] = fd;
    } else if ( strcmp ( name, "AT Translated Set 2 keyboard" ) == 0) { // for vmware, my dev environment
      fds [ 0 ] = fd;
    } else {
      pnd_log ( pndn_rem, "Ignoring unknown device '%s'\n", name );
      close ( fd );
      continue;
    }

    if (imaxfd < fd) imaxfd = fd;
  } // for

  if ( fds [ 0 ] == -1 ) {
    pnd_log ( pndn_warning, "WARNING! Couldn't find keypad device\n" );
  }

  if ( fds [ 1 ] == -1 ) {
    pnd_log ( pndn_warning, "WARNING! couldn't find button device\n" );
  }

  if ( fds [ 0 ] == -1 && fds [ 1 ] == -1 ) {
    pnd_log ( pndn_error, "ERROR! Couldn't find either device; exiting!\n" );
    exit ( -2 );
  }

  /* loop forever, watching for events
   */

  while ( 1 ) {
    struct input_event ev[64];

    int fd = -1, rd, ret;
    fd_set fdset;

    FD_ZERO ( &fdset );

    for (i = 0; i < 2; i++) {
      if ( fds [ i ] != -1 ) {
	FD_SET( fds [ i ], &fdset );
      }
    }

    ret = select ( imaxfd + 1, &fdset, NULL, NULL, NULL );

    if ( ret == -1 ) {
      pnd_log ( pndn_error, "ERROR! select(2) failed with: %s\n", strerror ( errno ) );
      break;
    }

    for ( i = 0; i < 2; i++ ) {
      if ( fds [ i ] != -1 && FD_ISSET ( fds [ i ], &fdset ) ) {
	fd = fds [ i ];
      }
    }

    /* buttons or keypad */
    rd = read ( fd, ev, sizeof(struct input_event) * 64 );
    if ( rd < (int) sizeof(struct input_event) ) {
      pnd_log ( pndn_error, "ERROR! read(2) input_event failed with: %s\n", strerror ( errno ) );
      break;
    }

    for (i = 0; i < rd / sizeof(struct input_event); i++ ) {

      if ( ev[i].type == EV_SYN ) {
	continue;
      } else if ( ev[i].type == EV_KEY ) {

	// do we even know about this key at all?
	keycode_t *p = keycodes;
	while ( p -> keycode != -1 ) {
	  if ( p -> keycode == ev [ i ].code ) {
	    break;
	  }
	  p++;
	}

	// if we do, hand it off to dispatcher to look up if we actually do something with it
	if ( p -> keycode != -1 ) {
	  pnd_log ( pndn_debug, "Key Event: key %s [%d] value %d\n", p -> keyname, p -> keycode, ev [ i ].value );
	  dispatch_key ( p -> keycode, ev [ i ].value );
	} else {
	  pnd_log ( pndn_warning, "Unknown Key Event: keycode %d value %d\n",  ev [ i ].code, ev [ i ].value );
	}

      } else if ( ev[i].type == EV_SW ) {

	// do we even know about this event at all?
	generic_event_t *p = generics;
	while ( p -> code != -1 ) {
	  if ( p -> code == ev [ i ].code ) {
	    break;
	  }
	  p++;
	}

	// if we do, hand it off to dispatcher to look up if we actually do something with it
	if ( p -> code != -1 ) {
	  pnd_log ( pndn_debug, "Generic Event: event %s [%d] value %d\n", p -> name, p -> code, ev [ i ].value );
	  dispatch_event ( p -> code, ev [ i ].value );
	} else {
	  pnd_log ( pndn_warning, "Unknown Generic Event: code %d value %d\n",  ev [ i ].code, ev [ i ].value );
	}

      } else {
	pnd_log ( pndn_debug, "DEBUG: Unexpected event type %i received\n", ev[i].type );
	continue;
      } // type?

    } // for

  } // while

  for (i = 0; i < 2; i++) {
    if ( i != 2 && fds [ i ] != -1 ) {
      close (fds [ i ] );
    }
  }

  return ( 0 );
} // main

// this should really register the keystate and time, and then another func to monitor
// time-passage and check the registered list and act on the event..
void dispatch_key ( int keycode, int val ) {
  unsigned int i;

  // val decodes as:
  // 1 - down (pressed)
  // 2 - down again (hold)
  // 0 - up (released)

  for ( i = 0; i < g_evmap_max; i++ ) {

    if ( ( g_evmap [ i ].key_p ) &&
	 ( ((keycode_t*) (g_evmap [ i ].reqs)) -> keycode == keycode ) &&
	 ( g_evmap [ i ].script ) )
    {

      // is this a keydown or a keyup?
      if ( val == 1 ) {
	// keydown
	g_evmap [ i ].keydown_time = time ( NULL );

      } else if ( val == 0 ) {
	// keyup

	char holdtime [ 128 ];
	sprintf ( holdtime, "%d", (int)( time(NULL) - g_evmap [ i ].keydown_time ) );

	if ( time ( NULL ) - g_evmap [ i ].last_trigger_time >= g_minimum_separation ) {
	  int x;

	  g_evmap [ i ].last_trigger_time = time ( NULL );

	  pnd_log ( pndn_rem, "Will attempt to invoke: %s %s\n", g_evmap [ i ].script, holdtime );

	  if ( ( x = fork() ) < 0 ) {
	    pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
	    exit ( -3 );
	  }

	  if ( x == 0 ) {
	    execl ( g_evmap [ i ].script, g_evmap [ i ].script, holdtime, (char*)NULL );
	    pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", g_evmap [ i ].script );
	    exit ( -4 );
	  }

	} else {
	  pnd_log ( pndn_rem, "Skipping invokation.. falls within minimum_separation threshold\n" );
	}

      } // key up or down?

      return;
    } // found matching event for keycode

  } // while

  return;
}

void dispatch_event ( int code, int val ) {
  unsigned int i;

  // LID val decodes as:
  // 1 - closing
  // 0 - opening

  for ( i = 0; i < g_evmap_max; i++ ) {

    if ( ( g_evmap [ i ].key_p == 0 ) &&
	 ( ((generic_event_t*) (g_evmap [ i ].reqs)) -> code == code ) &&
	 ( g_evmap [ i ].script ) )
    {

      // just hand the code to the script (ie: 0 or 1 to script)
      if ( time ( NULL ) - g_evmap [ i ].last_trigger_time >= g_minimum_separation ) {
	int x;
	char value [ 100 ];

	sprintf ( value, "%d", val );

	g_evmap [ i ].last_trigger_time = time ( NULL );

	pnd_log ( pndn_rem, "Will attempt to invoke: %s %s\n", g_evmap [ i ].script, value );

	if ( ( x = fork() ) < 0 ) {
	  pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
	  exit ( -3 );
	}

	if ( x == 0 ) {
	  execl ( g_evmap [ i ].script, g_evmap [ i ].script, value, (char*)NULL );
	  pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", g_evmap [ i ].script );
	  exit ( -4 );
	}

      } else {
	pnd_log ( pndn_rem, "Skipping invokation.. falls within minimum_separation threshold\n" );
      }

      return;
    } // found matching event for keycode

  } // while

  return;
}

void sigchld_handler ( int n ) {

  pnd_log ( pndn_rem, "---[ SIGCHLD received ]---\n" );

  int status;
  wait ( &status );

  return;
}
