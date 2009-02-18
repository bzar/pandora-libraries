
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h>

#include <sys/types.h> /* for open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pnd_device.h"

unsigned char pnd_device_set_clock ( unsigned int c ) {
  char buffer [ 100 ];
  int f;

  sprint ( buffer, "%u", c );

  if ( ( f = open ( PND_DEVICE_PROC_CLOCK, O_RDONLY ) ) < 0 ) {
    return ( 0 );
  }

  if ( write ( f, buffer, strlen ( buffer ) ) < strlen ( buffer ) ) {
    return ( 0 );
  }

  close ( f );

  return ( 1 );
}

unsigned int pnd_device_get_clock ( void ) {
  return ( 0 );
}
