
// this is pulled from cpasjuste and/or pickle

#if defined (_PANDORA) || !defined (EMULATOR)

/* cribbed from pnd_keytypes.h so as to make it unnecessary */
  #define BITS_PER_LONG (sizeof(long) * 8)
  #define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
  #include <string.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <stdio.h>
/* back to reality */

#include <stdlib.h> /* abs() */
#include <linux/input.h> /* struct input_event */

#include "pnd_io_gpio.h"
//#include "pnd_keytype.h"
#include "pnd_device.h"

unsigned char GLES2D_Pad [ pke_pad_max ];

char event_name[30];
int fd_usbk, fd_usbm, fd_gpio, fd_pndk, fd_nub1, fd_nub2, fd_ts, rd, i, j, k;
struct input_event ev[64];
int version;
unsigned short id[4];
unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
char dev_name[256] = "Unknown";
int absolute[5];

char pnd_nub1[9]  = PND_EVDEV_NUB1; //"vsense66";
char pnd_nub2[9]  = PND_EVDEV_NUB2; //"vsense67";
char pnd_key[19]  = PND_EVDEV_KEYPAD; //"omap_twl4030keypad";
char pnd_gpio[10] = "gpio-keys";

#define DEV_NUB1 0
#define DEV_NUB2 1
#define DEV_PNDK 2
#define DEV_GPIO 3

#define NUB1_CUTOFF 100
#define NUB2_CUTOFF 100
#define NUB2_SCALE  10

void PND_Setup_Controls ( void ) {
  //printf( "Setting up Pandora Controls\n" );

  // Static Controls
  // Pandora keyboard
  fd_pndk = PND_OpenEventDeviceByName(pnd_key);
  // Pandora buttons
  fd_gpio = PND_OpenEventDeviceByName(pnd_gpio);
  // Pandora analog nub's
  fd_nub1 = PND_OpenEventDeviceByName(pnd_nub1);
  fd_nub2 = PND_OpenEventDeviceByName(pnd_nub2);

}

void PND_Close_Controls ( void ) {
  //printf( "Closing Pandora Controls\n" );

  if( fd_pndk > 0 )
    close(fd_pndk );
  if( fd_gpio > 0 )
    close(fd_gpio );
  if( fd_nub1 > 0 )
    close(fd_nub1 );
  if( fd_nub2 > 0 )
    close(fd_nub2 );

}

void PND_SendKeyEvents ( void ) {
  PND_ReadEvents( fd_pndk, DEV_PNDK );
  PND_ReadEvents( fd_gpio, DEV_GPIO );
  PND_ReadEvents( fd_nub1, DEV_NUB1 );
  PND_ReadEvents( fd_nub2, DEV_NUB2 );
}

void PND_ReadEvents ( int fd, int device ) {

  if ( fd != 0 ) {

    rd = read ( fd, ev, sizeof(struct input_event) * 64 );

    if ( rd > (int) sizeof(struct input_event) ) {
      for (i = 0; i < rd / sizeof(struct input_event); i++) {
	PND_CheckEvent ( &ev[i], device );
      }
    }

  } // got fd?

  return;
}

void PND_CheckEvent ( struct input_event *event, int device ) {
  int value;

  // printf( "Device %d Type %d Code %d Value %d\n", device, event->type, event->code, event->value );

  value	= event->value;

  switch( event->type )	{

  case EV_KEY:

    switch( event->code ) {

    case KEY_UP:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_up] = 1;
      }	else {
	GLES2D_Pad[pke_pad_up] = 0;
      }
      break;

    case KEY_DOWN:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_down] = 1;
      } else {
	GLES2D_Pad[pke_pad_down] = 0;
      }
      break;

    case KEY_LEFT:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_left] = 1;
      }	else {
	GLES2D_Pad[pke_pad_left] = 0;
      }
      break;

    case KEY_RIGHT:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_right] = 1;
      }	else {
	GLES2D_Pad[pke_pad_right] = 0;
      }
      break;

    case KEY_MENU:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_menu] = 1;
      }	else {
	GLES2D_Pad[pke_pad_menu] = 0;
      }
      break;

    case BTN_START:
      if ( event->value ) printf("START\n");
      break;

    case BTN_SELECT:
      if ( event->value ) printf("SELECT\n");
      break;

    case BTN_X:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_x] = 1;
      }	else {
	GLES2D_Pad[pke_pad_x] = 0;
      }
      break;

    case BTN_Y:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_y] = 1;
      }	else {
	GLES2D_Pad[pke_pad_y] = 0;
      }
      break;

    case BTN_A:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_a] = 1;
      }	else {
	GLES2D_Pad[pke_pad_a] = 0;
      }
      break;

    case BTN_B:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_b] = 1;
      }	else {
	GLES2D_Pad[pke_pad_b] = 0;
      }
      break;

    case BTN_TL:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_l] = 1;
      }	else {
	GLES2D_Pad[pke_pad_l] = 0;
      }
      break;

    case BTN_TR:
      if ( event->value ) {
	GLES2D_Pad[pke_pad_r] = 1;
      }	else {
	GLES2D_Pad[pke_pad_r] = 0;
      }
      break;

    default:
      break;
    }
    break;

  case EV_ABS:

    switch ( device ) {

    case DEV_NUB1:
      if ( event->code == ABS_X ) {
	//printf( "nub1 x %3d\n", value );
	if( abs(value) > NUB1_CUTOFF ) {
	  if( value > 0 ) {
	    value = 1;
	  } else if( value < 0 ) {
	    value = 1;
	  }
	} else {
	}
      }

      if( event->code == ABS_Y ) {
	//printf( "nub1 y %3d\n", value );
	if( abs(value) > NUB1_CUTOFF ) {
	  if( value > 0 ) {
	    value = 1;
	  } else if( value < 0 ) {
	    value = 1;
	  }
	} else {
	}
      }
      break;

    case DEV_NUB2:
      if(event->code == ABS_X) {
	//printf( "nub2 x %3d\n", value );
	if( abs(value) > NUB2_CUTOFF ) {
	  if( value > 0 ) {
	    value = 1;
	  } else if( value < 0 ) {
	    value = 1;
	  }
	} else {
	}
      }

      if(event->code == ABS_Y) {
	//printf( "nub2 y %3d\n", value );
	if( abs(value) > NUB2_CUTOFF ) {
	  if( value > 0 ) {
	    value = 1;
	  } else if( value < 0 ) {
	    value = 1;
	  }
	} else {
	}
      }
      break;

    }
    break;
  }

  return;
}

int PND_OpenEventDeviceByID ( int event_id ) {
  int fd;

  snprintf( event_name, sizeof(event_name), "/dev/input/event%d", event_id );
  printf( "Device: %s\n", event_name );
  if ((fd = open(event_name, O_RDONLY |  O_NDELAY)) < 0) {
    perror("ERROR: Could not open device");
    return 0;
  }

  if (ioctl(fd, EVIOCGVERSION, &version)) {
    perror("evtest: can't get version");
    return 0;
  }

  printf("Input driver version is %d.%d.%d\n",
	 version >> 16, (version >> 8) & 0xff, version & 0xff);

  ioctl(fd, EVIOCGID, id);
  printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
	 id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

  ioctl(fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
  printf("Input device name: \"%s\"\n", dev_name);

  return fd;
}

int PND_OpenEventDeviceByName ( char device_name[] ) {
  int fd;

  for (i = 0; 1; i++) {

    snprintf( event_name, sizeof(event_name), "/dev/input/event%d", i );
    printf( "Device: %s\n", event_name );
    if ((fd = open(event_name, O_RDONLY |  O_NDELAY)) < 0) {
      perror("ERROR: Could not open device");
      return 0;
    }

    if (fd < 0) break; /* no more devices */

    ioctl(fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
    if (strcmp(dev_name, device_name) == 0) {

      if (ioctl(fd, EVIOCGVERSION, &version)) {
	perror("evtest: can't get version");
	return 0;
      }

      printf("Input driver version is %d.%d.%d\n",
	     version >> 16, (version >> 8) & 0xff, version & 0xff);

      ioctl(fd, EVIOCGID, id);
      printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
	     id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

      ioctl(fd, EVIOCGNAME(sizeof(dev_name)), dev_name);
      printf("Input device name: \"%s\"\n", dev_name);

      return fd;
    }

    close(fd); /* we don't need this device */
  }

  return 0;
}

int PND_Pad_RecentlyPressed ( pnd_keytype_e num ) {
  static int GLES2D_Pad_old [ pke_pad_max ];

  if ( !GLES2D_Pad_old[num] && GLES2D_Pad[num] ) {
    GLES2D_Pad_old[num] = GLES2D_Pad[num];
    return 1;
  }

  GLES2D_Pad_old[num] = GLES2D_Pad[num];

  return 0;
}

#endif
