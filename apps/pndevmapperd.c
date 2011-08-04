
/* pndevmapperd exists to watch for a few interesting events and to launch scripts when they occur
 * ie: when the lid closes, should invoke power-mode toggle sh-script
 */

// woot for writing code while sick.

// this code begs a rewrite, but should work fine; its just arranged goofily
// -> I mean, why the racial divide between keys and other events?
// -> now that I've put together pnd_io_evdev, should leverage that; be much cleaner.

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
#include <sys/time.h> // setitimer

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
#include "pnd_device.h"

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
  { KEY_B, "b" },
  { KEY_MENU, "pandora" },
  { KEY_POWER, "power" },
  { KEY_DELETE, "del" },
  { KEY_COMMA, "comma" },
  { KEY_1, "1" },
  { KEY_2, "2" },
  { KEY_3, "3" },
  { KEY_4, "4" },
  { KEY_5, "5" },
  { KEY_6, "6" },
  { KEY_7, "7" },
  { KEY_8, "8" },
  { KEY_9, "9" },
  { KEY_0, "0" },
  { KEY_BRIGHTNESSDOWN, "lcdbrightdown" },
  { KEY_BRIGHTNESSUP, "lcdbrightup" },
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
  unsigned int maxhold;   // maximum hold-time before forcing script invocation

  /* state
   */
  time_t last_trigger_time;
  time_t keydown_time;

} evmap_t;

#define MAXEVENTS 255
evmap_t g_evmap [ MAXEVENTS ];
unsigned int g_evmap_max = 0;
unsigned int g_queued_keyups = 0;

// battery
unsigned char b_threshold = 5;    // %battery
unsigned int b_frequency = 300;   // frequency to check
unsigned int b_blinkfreq = 2;     // blink every 2sec
unsigned int b_blinkdur = 1000;   // blink duration (uSec), 0sec + uSec is assumed
unsigned char b_active = 0;       // 0=inactive, 1=active and waiting to blink, 2=blink is on, waiting to turn off
unsigned char b_shutdown = 1;     // %age battery to force a shutdown!
unsigned int  b_shutdelay = 30;   // delay for shutdown script
unsigned char b_warned = 0;       // Shutdown attempted
char *b_shutdown_script = NULL;
unsigned char bc_enable = 1;      // enable charger control
unsigned char bc_stopcap = 99;    // battery capacity threshold as stop condition 1
unsigned int bc_stopcur = 80000;  // charge current threshold as stop condition 2, in uA
unsigned char bc_startcap = 95;   // battery capacity threshold to resume charging
char *bc_charge_device = NULL;    // charger /sys/class/power_supply/ device, changes between kernel versions

/* get to it
 */
void dispatch_key ( int keycode, int val );
void dispatch_event ( int code, int val );
void sigchld_handler ( int n );
unsigned char set_next_alarm ( unsigned int secs, unsigned int usecs );
void sigalrm_handler ( int n );

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

	// note the script to activate in response
	if ( strchr ( n, ' ' ) ) {
	  char *foo = strdup ( n );
	  char *t = strchr ( foo, ' ' );
	  *t = '\0';
	  g_evmap [ g_evmap_max ].script = foo;
	  g_evmap [ g_evmap_max ].maxhold = atoi ( t + 1 );
	} else {
	  g_evmap [ g_evmap_max ].script = n;
	  g_evmap [ g_evmap_max ].maxhold = 0;
	}

	pnd_log ( pndn_rem, "Registered key %s [%d] to script %s with maxhold %d\n",
		  p -> keyname, p -> keycode, (char*) n, g_evmap [ g_evmap_max ].maxhold );

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

    } else if ( strncmp ( k, "battery.", 8 ) == 0 ) {
      // not consumed here, skip silently

    } else if ( strncmp ( k, "battery_charge.", 15 ) == 0 ) {
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

  // battery conf
  if ( pnd_conf_get_as_int ( evmaph, "battery.threshold" ) != PND_CONF_BADNUM ) {
    b_threshold = pnd_conf_get_as_int ( evmaph, "battery.threshold" );
    pnd_log ( pndn_rem, "Battery threshold set to %u", b_threshold );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery.check_interval" ) != PND_CONF_BADNUM ) {
    b_frequency = pnd_conf_get_as_int ( evmaph, "battery.check_interval" );
    pnd_log ( pndn_rem, "Battery check interval set to %u", b_frequency );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery.blink_interval" ) != PND_CONF_BADNUM ) {
    b_blinkfreq = pnd_conf_get_as_int ( evmaph, "battery.blink_interval" );
    pnd_log ( pndn_rem, "Battery blink interval set to %u", b_blinkfreq );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery.blink_duration" ) != PND_CONF_BADNUM ) {
    b_blinkdur = pnd_conf_get_as_int ( evmaph, "battery.blink_duration" );
    pnd_log ( pndn_rem, "Battery blink duration set to %u", b_blinkdur );
  }
  b_active = 0;
  if ( pnd_conf_get_as_int ( evmaph, "battery.shutdown_threshold" ) != PND_CONF_BADNUM ) {
    b_shutdown = pnd_conf_get_as_int ( evmaph, "battery.shutdown_threshold" );
    pnd_log ( pndn_rem, "Battery shutdown threshold set to %u", b_shutdown );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery.shutdown_delay" ) != PND_CONF_BADNUM ) {
    b_shutdelay = pnd_conf_get_as_int ( evmaph, "battery.shutdown_delay" );
    pnd_log ( pndn_rem, "Battery shutdown delay set to %u", b_shutdelay );
  }
  if ( pnd_conf_get_as_char ( evmaph, "battery.shutdown_script" ) != NULL ) {
    b_shutdown_script = strdup ( pnd_conf_get_as_char ( evmaph, "battery.shutdown_script" ) );
    pnd_log ( pndn_rem, "Battery shutdown script set to %s", b_shutdown_script );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery_charge.enable" ) != PND_CONF_BADNUM ) {
    bc_enable = pnd_conf_get_as_int ( evmaph, "battery_charge.enable" );
    pnd_log ( pndn_rem, "Battery charge enable set to %u", bc_enable );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery_charge.stop_capacity" ) != PND_CONF_BADNUM ) {
    bc_stopcap = pnd_conf_get_as_int ( evmaph, "battery_charge.stop_capacity" );
    pnd_log ( pndn_rem, "Battery charge stop capacity set to %u", bc_stopcap );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery_charge.stop_current" ) != PND_CONF_BADNUM ) {
    bc_stopcur = pnd_conf_get_as_int ( evmaph, "battery_charge.stop_current" );
    pnd_log ( pndn_rem, "Battery charge stop current set to %u", bc_stopcur );
  }
  if ( pnd_conf_get_as_int ( evmaph, "battery_charge.start_capacity" ) != PND_CONF_BADNUM ) {
    bc_startcap = pnd_conf_get_as_int ( evmaph, "battery_charge.start_capacity" );
    pnd_log ( pndn_rem, "Battery charge start capacity set to %u", bc_startcap );
  }
  if ( pnd_conf_get_as_char ( evmaph, "battery_charge.device" ) != NULL ) {
    bc_charge_device = strdup ( pnd_conf_get_as_char ( evmaph, "battery_charge.device" ) );
    pnd_log ( pndn_rem, "Battery charge device set to %s", bc_charge_device );
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
  siggy.sa_flags = SA_RESTART; /* don't need anything */
  sigaction ( SIGCHLD, &siggy, NULL );

  /* set up the battery level warning timers
   */
  siggy.sa_handler = sigalrm_handler;
  siggy.sa_mask = ss; /* implicitly blocks the origin signal */
  siggy.sa_flags = SA_RESTART; /* don't need anything */
  sigaction ( SIGALRM, &siggy, NULL );

  if ( set_next_alarm ( b_frequency, 0 ) ) { // check every 'frequency' seconds
    pnd_log ( pndn_rem, "Checking for low battery every %u seconds\n", b_frequency );
  } else {
    pnd_log ( pndn_error, "ERROR: Couldn't set up timer for every %u seconds\n", b_frequency );
  }

  /* actually try to do something useful
   */

  // stolen in part from notaz :)

  // try to locate the appropriate devices
  int id;
  int fds [ 8 ] = { -1, -1, -1, -1, -1, -1, -1, -1 }; // 0 = keypad, 1 = gpio keys
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

    if ( ioctl (fd, EVIOCGNAME(sizeof(name)), name ) < 0 ) {
      name [ 0 ] = '\0';
    }

    pnd_log ( pndn_rem, "%s maps to %s\n", fname, name );

    if ( strcmp ( name, PND_EVDEV_KEYPAD/*"omap_twl4030keypad"*/ ) == 0 ) {
      fds [ 0 ] = fd;
    } else if ( strcmp ( name, "gpio-keys" ) == 0) {
      fds [ 1 ] = fd;
    } else if ( strcmp ( name, "AT Translated Set 2 keyboard" ) == 0) { // for vmware, my dev environment
      fds [ 0 ] = fd;
    } else if ( strcmp ( name, PND_EVDEV_POWER/*"triton2-pwrbutton"*/ ) == 0) {
      fds [ 2 ] = fd;
    } else if ( strcmp ( name, PND_EVDEV_TS/*"ADS784x Touchscreen"*/ ) == 0) {
      fds [ 3 ] = fd;
    } else if ( strcmp ( name, PND_EVDEV_NUB1/*"vsense66"*/ ) == 0) {
      fds [ 4 ] = fd;
    } else if ( strcmp ( name, PND_EVDEV_NUB1/*"vsense67"*/ ) == 0) {
      fds [ 5 ] = fd;
    } else {
      pnd_log ( pndn_rem, "Ignoring unknown device '%s'\n", name );
      //fds [ 6 ] = fd;
      close ( fd );
      fd = -1;
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
    pnd_log ( pndn_error, "ERROR! Couldn't find either device!\n" );
    //exit ( -2 );
  }

  /* loop forever, watching for events
   */

  while ( 1 ) {
    struct input_event ev[64];

    unsigned int max_fd = 3; /* imaxfd */
    int fd = -1, rd, ret;
    fd_set fdset;

    // set up fd list
    FD_ZERO ( &fdset );

    imaxfd = 0;
    for (i = 0; i < max_fd /*imaxfd*/; i++) {
      if ( fds [ i ] != -1 ) {
	FD_SET( fds [ i ], &fdset );

	if ( fds [ i ] > imaxfd ) {
	  imaxfd = fds [ i ];
	}

      }
    }

    // figure out if we can block forever, or not
    unsigned char do_block = 1;
    struct timeval tv;
    tv.tv_usec = 0;
    tv.tv_sec = 1;

    for ( i = i; i < g_evmap_max; i++ ) {
      if ( g_evmap [ i ].keydown_time && g_evmap [ i ].maxhold ) {
	do_block = 0;
	break;
      }
    }

    // wait for fd's or timeout
    ret = select ( imaxfd + 1, &fdset, NULL, NULL, do_block ? NULL /* no timeout */ : &tv );

    if ( ret == -1 ) {
      pnd_log ( pndn_error, "ERROR! select(2) failed with: %s\n", strerror ( errno ) );
      continue; // retry!

    } else if ( ret == 0 ) { // select returned with timeout (no fd)

      // timeout occurred; should only happen when 1 or more keys are being held down and
      // they're "maxhold" keys, so we have to see if their timer has passed
      unsigned int now = time ( NULL );

      for ( i = i; i < g_evmap_max; i++ ) {

	if ( g_evmap [ i ].keydown_time &&
	     g_evmap [ i ].maxhold &&
	     now - g_evmap [ i ].keydown_time >= g_evmap [ i ].maxhold )
	{
	  keycode_t *k = (keycode_t*) g_evmap [ i ].reqs;
	  dispatch_key ( k -> keycode, 0 /* key up */ );
	}

      } // for

    } else { // an fd was fiddled with

      for ( i = 0; i < max_fd; i++ ) {
	if ( fds [ i ] != -1 && FD_ISSET ( fds [ i ], &fdset ) ) {
	  fd = fds [ i ];
	} // fd is set?
      } // for

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
	    if ( logall >= 0 ) {
	      pnd_log ( pndn_debug, "Key Event: key %s [%d] value %d\n", p -> keyname, p -> keycode, ev [ i ].value );
	    }
	    dispatch_key ( p -> keycode, ev [ i ].value );
	  } else {
	    if ( logall >= 0 ) {
	      pnd_log ( pndn_warning, "Unknown Key Event: keycode %d value %d\n",  ev [ i ].code, ev [ i ].value );
	    }
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
	    if ( logall >= 0 ) {
	      pnd_log ( pndn_debug, "Generic Event: event %s [%d] value %d\n", p -> name, p -> code, ev [ i ].value );
	    }
	    dispatch_event ( p -> code, ev [ i ].value );
	  } else {
	    if ( logall >= 0 ) {
	      pnd_log ( pndn_warning, "Unknown Generic Event: code %d value %d\n",  ev [ i ].code, ev [ i ].value );
	    }
	  }

	} else {
	  pnd_log ( pndn_debug, "DEBUG: Unexpected event type %i received\n", ev[i].type );
	  continue;
	} // type?

      } // for

    } // an fd was touched

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
      unsigned char invoke_it = 0;

      // is this a keydown or a keyup?
      if ( val == 1 ) {
	// keydown
	g_evmap [ i ].keydown_time = time ( NULL );

      } else if ( val == 2 && g_evmap [ i ].keydown_time ) {
	// key is being held; we should check if max-hold is set

	if ( g_evmap [ i ].maxhold &&
	     time ( NULL ) - g_evmap [ i ].keydown_time >= g_evmap [ i ].maxhold )
	{
	  invoke_it = 1;
	}

      } else if ( val == 0 && g_evmap [ i ].keydown_time ) {
	// keyup (while key is down)

	if ( time ( NULL ) - g_evmap [ i ].last_trigger_time >= g_minimum_separation ) {
	  invoke_it = 1;
	} else {
	  pnd_log ( pndn_rem, "Skipping invokation.. falls within minimum_separation threshold\n" );
	}

      } // key up or down?

      if ( invoke_it ) {

	char holdtime [ 128 ];
	sprintf ( holdtime, "%d", (int)( time(NULL) - g_evmap [ i ].keydown_time ) );
	pnd_log ( pndn_rem, "Will attempt to invoke: %s %s\n", g_evmap [ i ].script, holdtime );

	// state
	g_evmap [ i ].keydown_time = 0; // clear the keydown-ness
	g_evmap [ i ].last_trigger_time = time ( NULL );

	// invocation
	int x;

	if ( ( x = fork() ) < 0 ) {
	  pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
	  exit ( -3 );
	}

	if ( x == 0 ) {
	  execl ( g_evmap [ i ].script, g_evmap [ i ].script, holdtime, (char*)NULL );
	  pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", g_evmap [ i ].script );
	  exit ( -4 );
	}

      } // invoke the script!

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

  pnd_log ( pndn_rem, "Dispatching Event..\n" );

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

  pnd_log ( pndn_rem, "     SIGCHLD done ]---\n" );

  return;
}

unsigned char set_next_alarm ( unsigned int secs, unsigned int usecs ) {

  // assume that SIGALRM is already being caught, we just set the itimer here

  struct itimerval itv;

  // if no timer at all, set the 'current' one so it does something; otherwise
  // let it continue..
  getitimer ( ITIMER_REAL, &itv );

  if ( itv.it_value.tv_sec == 0 && itv.it_value.tv_sec == 0 ) {
    itv.it_value.tv_sec = secs;
    itv.it_value.tv_usec = usecs;
  }

  // set the next timer
  //bzero ( &itv, sizeof(struct itimerval) );

  itv.it_interval.tv_sec = secs;
  itv.it_interval.tv_usec = usecs;

  // if next-timer is less than current, set current too
  if ( itv.it_value.tv_sec > itv.it_interval.tv_sec ) {
    itv.it_value.tv_sec = secs;
    itv.it_value.tv_usec = usecs;
  }

  if ( setitimer ( ITIMER_REAL, &itv, NULL /* old value returned here */ ) < 0 ) {
    // sucks
    return ( 0 );
  }
  
  return ( 1 );
}

void sigalrm_handler ( int n ) {

  pnd_log ( pndn_debug, "---[ SIGALRM ]---\n" );

  static time_t last_charge_check, last_charge_worka;
  int batlevel = pnd_device_get_battery_gauge_perc();
  int uamps = 0;
  time_t now;

  pnd_device_get_charge_current ( &uamps );

  if ( batlevel < 0 ) {
#if 0
    // couldn't read the battery level, so just assume low and make blinks?
    batlevel = 4; // low, but not cause a shutdown
#else
    // couldn't read the battery level, so just assume ok!
    batlevel = 50;
#endif
  }

  // first -- are we critical yet? if so, shut down!
  if ( batlevel <= b_shutdown && b_shutdown_script) {

    if ( uamps > 100 ) {
        // critical battery, but charging, so relax.
        b_warned = 0;
    } else {
      if (b_warned == 0) {
          // Avoid warning again till re-powered
          b_warned = 1;
          int x;
          pnd_log ( pndn_error, "Battery Current: %d\n", uamps );
          pnd_log ( pndn_error, "CRITICAL BATTERY LEVEL -- shutdown the system down! Invoke: %s\n",
	      	b_shutdown_script );

          if ( ( x = fork() ) < 0 ) {
	        pnd_log ( pndn_error, "ERROR: Couldn't fork()\n" );
    	    exit ( -3 );
          }

         if ( x == 0 ) {
           char value [ 100 ];
           sprintf ( value, "%d", b_shutdelay );
	   execl ( b_shutdown_script, b_shutdown_script, value, (char*)NULL );
	   pnd_log ( pndn_error, "ERROR: Couldn't exec(%s)\n", b_shutdown_script );
	   exit ( -4 );
         }
      }
    } // charging

  }

  // charge monitoring
  now = time(NULL);
  if ( bc_enable && bc_charge_device != NULL && (unsigned int)(now - last_charge_check) > 60 ) {

    int charge_enabled = pnd_device_get_charger_enable ( bc_charge_device );
    if ( charge_enabled < 0 )
      pnd_log ( pndn_error, "ERROR: Couldn't read charger enable control\n" );
    else {

      if ( charge_enabled && batlevel >= bc_stopcap && 0 < uamps && uamps < bc_stopcur ) {
        pnd_log ( pndn_debug, "Charge stop conditions reached, disabling charging\n" );
        pnd_device_set_charger_enable ( bc_charge_device, 0 );
      }
      else if ( !charge_enabled && batlevel <= bc_startcap ) {
        pnd_log ( pndn_debug, "Charge start conditions reached, enabling charging\n" );
        pnd_device_set_charger_enable ( bc_charge_device, 1 );
      }

      // for some unknown reason it just stops charging randomly (happens once per week or so),
      // and does not restart, resulting in a flat battery if machine is unattended.
      // What seems to help here is writing to chip registers, we can do it here indirectly
      // by writing to enable. Doing it occasionally should do no harm even with missing charger.
      if ( batlevel <= bc_startcap && (unsigned int)(now - last_charge_worka) > 20*60 ) {
        pnd_log ( pndn_debug, "Charge workaround trigger\n" );
        pnd_device_set_charger_enable ( bc_charge_device, 1 );
        last_charge_worka = now;
      }
    }
    last_charge_check = now;
  }

  // is battery warning already active?
  if ( b_active ) {
    // warning is on!

    // is user charging up? if so, stop blinking.
    // perhaps we shoudl check if charger is connected, and not blink at all in that case..
    if ( uamps > 0 ) {
      //Re-arm warning
      b_warned = 0;
      pnd_log ( pndn_debug, "Battery is high again, flipping to non-blinker mode\n" );
      b_active = 0;
      set_next_alarm ( b_frequency, 0 );
      pnd_device_set_led_charger_brightness ( 250 );
      return;
    }

    if ( b_active == 1 ) {
      // turn LED on
      pnd_log ( pndn_debug, "Blink on\n" );
      pnd_device_set_led_charger_brightness ( 200 );
      // set timer to short duration
      b_active = 2;
      set_next_alarm ( 0, b_blinkdur );
    } else if ( b_active == 2 ) {
      // turn LED off
      pnd_log ( pndn_debug, "Blink off\n" );
      pnd_device_set_led_charger_brightness ( 10 );
      // back to longer duration
      b_active = 1;
      set_next_alarm ( b_blinkfreq, 0 );
    }

    return;
  }

  // warning is off..
  if ( batlevel <= b_threshold && uamps < 0 ) {
    // battery seems low, go to active mode
    pnd_log ( pndn_debug, "Battery is low, flipping to blinker mode\n" );
    b_active = 1;
    set_next_alarm ( b_blinkfreq, 0 );
  } // battery level

  return;
}
