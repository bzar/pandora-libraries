
#include <stdio.h> // for stdio
#include <unistd.h> // for exit()
#include <stdlib.h> // for exit()
#include <time.h> // for time

#include "pnd_conf.h"
#include "pnd_dbusnotify.h"

int main ( int argc, char *argv[] ) {
  pnd_dbusnotify_handle h;

  h = pnd_dbusnotify_init();

  time_t start = time ( NULL );

  while ( time ( NULL ) - start < 20 ) {
    printf ( "Tick %u\n", (unsigned int) ( time ( NULL ) - start ) );

    if ( pnd_dbusnotify_rediscover_p ( h ) ) {
      printf ( "Event!\n" );
    }

    fflush ( stdout );
    sleep ( 1 );

  } // while

  pnd_dbusnotify_shutdown ( h );

  return ( 0 );
}
