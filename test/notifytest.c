
#include <stdio.h> // for stdio
#include <unistd.h> // for exit()
#include <stdlib.h> // for exit()

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_notify.h"

int main ( int argc, char *argv[] ) {

  pnd_notify_handle nh;

  nh = pnd_notify_init();

  if ( ! nh ) {
    printf ( "INOTIFY failed to init.\n" );
    exit ( 0 );
  }

  printf ( "INOTIFY is up.\n" );

  /* do it!
   */

  unsigned int countdown = 10;

  while ( countdown ) {
    printf ( "Countdown = %u\n", countdown );

    if ( pnd_notify_rediscover_p ( nh ) ) {
      printf ( "Must do a rediscover!\n" );
      break;
    }

    countdown--;
  }

  /* do it!
   */

  pnd_notify_shutdown ( nh );

  return ( 0 );
}
