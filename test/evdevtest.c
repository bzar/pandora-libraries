
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "pnd_device.h"
#include "pnd_io_evdev.h"

int main ( void ) {

  if ( ! pnd_evdev_open ( pnd_evdev_dpads ) ) {
    printf ( "Couldn't open dpads\n" );
    exit ( 0 );
  }

  if ( ! pnd_evdev_open ( pnd_evdev_nub1 ) ) {
    printf ( "Couldn't open nub1\n" );
    exit ( 0 );
  }

  if ( ! pnd_evdev_open ( pnd_evdev_nub2 ) ) {
    printf ( "Couldn't open nub2\n" );
    exit ( 0 );
  }

  while ( 1 ) {

    // poll and process
    if ( ! pnd_evdev_catchup ( 1 /* block */ ) ) {
      printf ( "Couldn't catch up events\n" );
      exit ( 0 );
    }

    // check state
    int s;

    s = pnd_evdev_dpad_state ( pnd_evdev_dpads );

    if ( s & pnd_evdev_up ) { printf ( "d-pad up\n" ); }
    if ( s & pnd_evdev_down ) { printf ( "d-pad down\n" ); }
    if ( s & pnd_evdev_left ) { printf ( "d-pad left\n" ); }
    if ( s & pnd_evdev_right ) { printf ( "d-pad right\n" ); }

    if ( s & pnd_evdev_x ) { printf ( "d-pad x\n" ); }
    if ( s & pnd_evdev_y ) { printf ( "d-pad y\n" ); }
    if ( s & pnd_evdev_a ) { printf ( "d-pad a\n" ); }
    if ( s & pnd_evdev_b ) { printf ( "d-pad b\n" ); }
    if ( s & pnd_evdev_ltrigger ) { printf ( "d-pad ltrigger\n" ); }
    if ( s & pnd_evdev_rtrigger ) { printf ( "d-pad rtrigger\n" ); }

    if ( s & pnd_evdev_start ) { printf ( "d-pad start\n" ); }
    if ( s & pnd_evdev_select ) { printf ( "d-pad select\n" ); }
    if ( s & pnd_evdev_pandora ) { printf ( "d-pad pandora\n" ); }

    pnd_nubstate_t ns;

    pnd_evdev_nub_state ( pnd_evdev_nub1, &ns );
    if ( ns.x || ns.y ) {
      printf ( "ns1 x / y: %d / %d\n", ns.x, ns.y );
    }

    pnd_evdev_nub_state ( pnd_evdev_nub2, &ns );
    if ( ns.x || ns.y ) {
      printf ( "ns2 x / y: %d / %d\n", ns.x, ns.y );
    }

    // sleep
    //usleep ( 500000 );

  } // while

  pnd_evdev_closeall();

  return ( 0 );
}
