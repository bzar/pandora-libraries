
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <string.h>

#include <sys/types.h> /* for open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pnd_device.h"

unsigned char pnd_device_open_write_close ( char *name, char *v ) {
  int f;

  if ( ( f = open ( PND_DEVICE_PROC_CLOCK, O_WRONLY /*O_RDONLY*/ ) ) < 0 ) {
    return ( 0 );
  }

  if ( write ( f, buffer, strlen ( buffer ) ) < strlen ( buffer ) ) {
    return ( 0 );
  }

  close ( f );

  return ( 1 );
}

unsigned char pnd_device_open_read_close ( char *name, char *r_buffer, unsigned int buffer_len ) {
  FILE *f;

  f = fopen ( name, "r" );

  if ( ! f ) {
    return ( 0 );
  }

  if ( ! fgets ( r_buffer, buffer_len, f ) ) {
    fclose ( f );
    return ( 0 );
  }

  fclose ( f );

  return ( 1 );
}

unsigned char pnd_device_set_clock ( unsigned int c ) {
  char buffer [ 100 ];

  sprint ( buffer, "%u", c );

  return ( pnd_device_open_write_close ( PND_DEVICE_PROC_CLOCK, buffer ) );
}

unsigned int pnd_device_get_clock ( void ) {
  char buffer [ 100 ];

  if ( pnd_device_open_read_close ( PND_DEVICE_PROC_CLOCK, buffer, 100 ) ) {
    return ( atoi ( buffer ) );
  }

  return ( 0 );
}

unsigned char pnd_device_set_backlight ( unsigned int c ) {
  char buffer [ 100 ];

  sprint ( buffer, "%u", c );

  return ( pnd_device_open_write_close ( PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer ) );
}

unsigned int pnd_device_get_clock ( void ) {
  char buffer [ 100 ];

  if ( pnd_device_open_read_close ( PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer, 100 ) ) {
    return ( atoi ( buffer ) );
  }

  return ( 0 );
}
