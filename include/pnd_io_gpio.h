#ifndef GLES2D_EVENT_H
#define GLES2D_EVENT_H

typedef enum {
  pke_pad_up = 1<<0,
  pke_pad_down = 1<<1,
  pke_pad_left = 1<<2,
  pke_pad_right = 1<<3,
  pke_pad_menu = 1<<4,
  pke_pad_x = 1<<5,
  pke_pad_y = 1<<6,
  pke_pad_a = 1<<7,
  pke_pad_b = 1<<8,
  pke_pad_l = 1<<9,
  pke_pad_r = 1<<10,
  pke_pad_max
} pnd_keytype_e;

void PND_Setup_Controls( void ); // call this first
void PND_Close_Controls( void ); // call this last
void PND_SendKeyEvents ( void ); // call this when you want to update status of keys
int PND_Pad_RecentlyPressed ( pnd_keytype_e num ); // after SendKeyEvents is done, call this to check a given key (assuming you're not just using ioctl EVIOCGKEY or SDL joy/keyb to do it)

// internal functions
void PND_ReadEvents ( int fd, int device ); // pull all events from given fd
void PND_CheckEvent ( struct input_event *event, int device ); // examine events, setting state flag as we go
int PND_OpenEventDeviceByID( int event_id );
int PND_OpenEventDeviceByName( char event_name[] );

#endif
