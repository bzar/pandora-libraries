
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h> /* abs() */
#include <linux/input.h> /* struct input_event */
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>

#include "pnd_device.h"
#include "pnd_io_evdev.h"

typedef struct {
  int fd;
  // state info
  union {
    pnd_nubstate_t nub;
    unsigned int buttons;
  } state;
} pnd_evmap_t;
static pnd_evmap_t evmap [ pnd_evdev_max ];
static unsigned char evmap_first = 1;
static pnd_evdev_e evmapback [ 10 ]; // map fd back to which device

unsigned char pnd_evdev_open ( pnd_evdev_e device ) {

  // if first, reset global struct
  if ( evmap_first ) {
    evmap_first = 0; // don't repeat
    unsigned char i;
    for ( i = 0; i < pnd_evdev_max; i++ ) {
      evmap [ i ].fd = -1; // not opened
    }
  } // if first

  // do it
  switch ( device ) {

  case pnd_evdev_dpads:
    if ( ( evmap [ pnd_evdev_dpads ].fd = pnd_evdev_open_by_name ( PND_EVDEV_GPIO ) ) >= 0 ) {
      evmapback [ evmap [ pnd_evdev_dpads ].fd ] = pnd_evdev_dpads;
      return ( 1 );
    }
    break;

  case pnd_evdev_nub1:
    if ( ( evmap [ pnd_evdev_nub1 ].fd = pnd_evdev_open_by_name ( PND_EVDEV_NUB1 ) ) >= 0 ) {
      evmapback [ evmap [ pnd_evdev_nub1 ].fd ] = pnd_evdev_nub1;
      return ( 1 );
    }
    break;

  case pnd_evdev_nub2:
    if ( ( evmap [ pnd_evdev_nub2 ].fd = pnd_evdev_open_by_name ( PND_EVDEV_NUB2 ) ) >= 0 ) {
      evmapback [ evmap [ pnd_evdev_nub2 ].fd ] = pnd_evdev_nub2;
      return ( 1 );
    }
    break;

  case pnd_evdev_power:
    if ( ( evmap [ pnd_evdev_power ].fd = pnd_evdev_open_by_name ( PND_EVDEV_POWER ) ) >= 0 ) {
      evmapback [ evmap [ pnd_evdev_power ].fd ] = pnd_evdev_power;
      return ( 1 );
    }
    break;

  case pnd_evdev_max:
    return ( 0 ); // shutup gcc
  } // switch

  // error opening, or not supported in this bit of code, or wtf
  return ( 0 );
}

void pnd_evdev_close ( pnd_evdev_e device ) {
  // if open
  if ( evmap [ device ].fd >= 0 ) {
    close ( evmap [ device ].fd ); // close it
    evmap [ device ].fd = -1; // flag as closed
  }
  return;
}

void pnd_evdev_closeall ( void ) {
  unsigned char i;

  for ( i = 0; i < pnd_evdev_max; i++ ) {
    pnd_evdev_close ( i );
  } // for

  return;
}

int pnd_evdev_get_fd ( unsigned char device ) {
  return ( evmap [ device ].fd );
}

int pnd_evdev_open_by_name ( char *devname ) {
  int fd;
  char fname [ 64 ];
  char name[256] = { 0, };
  unsigned char id;

  // keep opening event# devices until we run out
  for ( id = 0; ; id++ ) {

    // set temp filename
    snprintf ( fname, sizeof ( fname ), "/dev/input/event%i", id );

    // try open, or bail out if no more
    if ( ( fd = open ( fname, O_RDONLY | O_NDELAY ) ) < 0 ) {
      return ( -1 );
    }

    // fetch the devices name
    if ( ioctl (fd, EVIOCGNAME(sizeof(name)), name ) < 0 ) {
      close ( fd ); // couldn't get the name?!
      continue; // pretty odd to not have a name, but maybe the next device does..
    }

    // are we done?
    if ( strcmp ( name, devname ) == 0 ) {
      return ( fd );
    }

    // next!
    close ( fd );
  }

  return ( -1 ); // couldn't find it
}

unsigned char pnd_evdev_catchup ( unsigned char blockp ) {
  fd_set fdset;
  int maxfd = -99;
  int i;

  // if not blocking, we want a zero duration timeout
  struct timeval tv;
  struct timeval *ptv;
  bzero ( &tv, sizeof(struct timeval) ); // set to 0s, ie: return immediately

  if ( blockp ) {
    ptv = NULL;
  } else {
    ptv = &tv;
  }

  // clear watch fd's
  FD_ZERO ( &fdset );

  // for all our open evdev's, flag them in the watch fd list
  for ( i = 0; i < pnd_evdev_max; i++ ) {
    if ( evmap [ i ].fd >= 0 ) {
      // set the fd into the select set
      FD_SET( evmap [ i ].fd, &fdset );
      // select(2) needs to know the highest fd
      if ( evmap [ i ].fd > maxfd ) {
	maxfd = evmap [ i ].fd;
      } // if
    } // if
  } // for

  if ( maxfd < 0 ) {
    return ( 0 ); // nothing to do
  }

  int ret;

  ret = select ( maxfd + 1, &fdset, NULL, NULL, ptv );

  if ( ret < 0 ) {
    return ( 0 ); // something bad
  } else if ( ret == 0 ) {
    return ( 1 ); // all good, nothing here
  }

  // all good, and something in the queue..
  for ( i = 0; i < pnd_evdev_max; i++ ) {
    // if open..
    if ( evmap [ i ].fd >= 0 ) {
      // if pending in the queue
      if ( FD_ISSET ( evmap [ i ].fd, &fdset ) ) {

	int rd, fd;
	int j;
	struct input_event ev[64];

	fd = evmap [ i ].fd;

	// pull events from the queue; max 64 events
	rd = read ( fd, ev, sizeof(struct input_event) * 64 );
	if ( rd < (int) sizeof(struct input_event) ) {
	  // less than a whole event-struct? bad!
	  break;
	}

	// for each event in the pulled events, parse it
	for (j = 0; j < rd / sizeof(struct input_event); j++ ) {

	  // SYN, ignore
	  if ( ev[j].type == EV_SYN ) {
	    continue;

	  } else if ( ev[j].type == EV_KEY ) {
	    // KEY event -- including dpads

	    // store val for key
	    // keycode: ev[j].code -> keycode, see linux/input.h
	    // state val: ev[j].value -> 0keyup, 1keydown

#if 0 // as of mid-March; notaz did a recent keycode change so had to refigure it out
	    printf ( "evdev\tkey %d\tvalue %d\n", ev[j].code, ev[j].value );
	    // a 102    b 107    x 109     y 104
	    // ltrigger 54           rtrigger 97
	    // start  56      select 29    pandora 139
#endif

	    unsigned int state = evmap [ pnd_evdev_dpads ].state.buttons;

	    switch ( ev[j].code ) {

	    case KEY_UP:     state &= ~pnd_evdev_up; if ( ev[j].value ) state |= pnd_evdev_up; break;
	    case KEY_DOWN:   state &= ~pnd_evdev_down; if ( ev[j].value ) state |= pnd_evdev_down; break;
	    case KEY_LEFT:   state &= ~pnd_evdev_left; if ( ev[j].value ) state |= pnd_evdev_left; break;
	    case KEY_RIGHT:  state &= ~pnd_evdev_right; if ( ev[j].value ) state |= pnd_evdev_right; break;
	    case KEY_PAGEDOWN: /*KEY_X*/     state &= ~pnd_evdev_x; if ( ev[j].value ) state |= pnd_evdev_x; break;
	    case KEY_PAGEUP: /*KEY_Y*/       state &= ~pnd_evdev_y; if ( ev[j].value ) state |= pnd_evdev_y; break;
	    case KEY_HOME: /*KEY_A*/         state &= ~pnd_evdev_a; if ( ev[j].value ) state |= pnd_evdev_a; break;
	    case KEY_END: /*KEY_B*/          state &= ~pnd_evdev_b; if ( ev[j].value ) state |= pnd_evdev_b; break;

	    case KEY_RIGHTSHIFT: // ltrigger
	      state &= ~pnd_evdev_ltrigger; if ( ev[j].value ) state |= pnd_evdev_ltrigger; break;
	    case KEY_RIGHTCTRL: // rtrigger
	      state &= ~pnd_evdev_rtrigger; if ( ev[j].value ) state |= pnd_evdev_rtrigger; break;

	      // start
	      // select
	      // pandora
	    case KEY_LEFTALT:     state &= ~pnd_evdev_start; if ( ev[j].value ) state |= pnd_evdev_start; break;
	    case KEY_LEFTCTRL:     state &= ~pnd_evdev_select; if ( ev[j].value ) state |= pnd_evdev_select; break;
	    case KEY_MENU:     state &= ~pnd_evdev_pandora; if ( ev[j].value ) state |= pnd_evdev_pandora; break;

	    } // switch

	    evmap [ pnd_evdev_dpads ].state.buttons = state;

	  } else if ( ev[j].type == EV_SW ) {
	    // SWITCH event

	  } else if ( ev[j].type == EV_ABS ) {
	    // ABS .. ie: nub

	    // vsense66 -> nub1 (left)
	    // vsense67 -> nub2 (right)
	    pnd_nubstate_t *pn = NULL;
	    if ( evmapback [ fd ] == pnd_evdev_nub1 ) {
	      pn = &( evmap [ pnd_evdev_nub1 ].state.nub );
	    } else {
	      pn = &( evmap [ pnd_evdev_nub2 ].state.nub );
	    }

	    if ( ev[j].code == ABS_X ) {
	      pn -> x = ev[j].value;
	    } else if ( ev[j].code == ABS_Y ) {
	      pn -> y = ev[j].value;
	    } else {
	      //printf("unexpected EV_ABS code: %i\n", ev[i].code);
	    }

	  } else {
	    // unknown?
	    continue;
	  } // event type?

	} // for each pulled event

      } // if pending
    } // if open
  } // for all device..

  return ( 1 );
}

unsigned int pnd_evdev_dpad_state ( pnd_evdev_e device ) {
  if ( evmap [ device ].fd < 0 ) {
    return ( -1 );
  }
  return ( evmap [ device ].state.buttons );
}

int pnd_evdev_nub_state ( pnd_evdev_e nubdevice, pnd_nubstate_t *r_nubstate ) {
  if ( evmap [ nubdevice ].fd < 0 ) {
    return ( -1 );
  }
  memcpy ( r_nubstate, &(evmap [ nubdevice ].state.nub), sizeof(pnd_nubstate_t) );
  return ( 1 );
}
