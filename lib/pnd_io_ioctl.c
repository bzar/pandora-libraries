
#include <linux/input.h>
#include <sys/ioctl.h>
#include <strings.h>
#include "pnd_io_ioctl.h"

int pnd_is_key_down ( int fd, int key ) {
  unsigned int size = KEY_MAX / 8 + 1;
  unsigned char buf [ size ];
  bzero ( buf, size );

  if ( ioctl ( fd, EVIOCGKEY(size), buf ) < 0 ) {
    return ( -1 ); // error
  }

  if ( buf [ key / 8 ] & ( 1<<(key%8) ) ) {
    return ( 1 ); // down
  }

  return ( 0 ); // not down
}
