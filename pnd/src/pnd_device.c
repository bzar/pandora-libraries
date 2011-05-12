
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

  if ( ( f = open ( name, O_WRONLY /*O_RDONLY*/ ) ) < 0 ) {
    return ( 0 );
  }

  if ( write ( f, v, strlen ( v ) ) < (int)strlen ( v ) ) {
    close ( f );
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

  sprintf ( buffer, "%u", c );

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

  sprintf ( buffer, "%u", c );

  return ( pnd_device_open_write_close ( PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer ) );
}

unsigned int pnd_device_get_backlight ( void ) {
  char buffer [ 100 ];

  if ( pnd_device_open_read_close ( PND_DEVICE_SYS_BACKLIGHT_BRIGHTNESS, buffer, 100 ) ) {
    return ( atoi ( buffer ) );
  }

  return ( 0 );
}

int pnd_device_get_battery_gauge_perc ( void ) {
  char buffer [ 100 ];

  if ( pnd_device_open_read_close ( PND_DEVICE_BATTERY_GAUGE_PERC, buffer, 100 ) ) {
    return ( atoi ( buffer ) );
  }

  return ( -1 );
}

unsigned char pnd_device_get_charge_current ( int *result ) {
  char buffer [ 100 ];

  if ( pnd_device_open_read_close ( PND_DEVICE_CHARGE_CURRENT, buffer, 100 ) ) {
    *result = atoi ( buffer );
    return ( 1 );
  }

  return ( 0 );
}

unsigned char pnd_device_set_led_power_brightness ( unsigned char v ) {
  char buffer [ 100 ];

  sprintf ( buffer, "%u", v );

  return ( pnd_device_open_write_close ( PND_DEVICE_LED_POWER PND_DEVICE_LED_SUFFIX_BRIGHTNESS, buffer ) );
}

unsigned char pnd_device_set_led_charger_brightness ( unsigned char v ) {
  char buffer [ 100 ];

  sprintf ( buffer, "%u", v );

  return ( pnd_device_open_write_close ( PND_DEVICE_LED_CHARGER PND_DEVICE_LED_SUFFIX_BRIGHTNESS, buffer ) );
}
