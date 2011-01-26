
#ifndef h_freedesktop_cats_h
#define h_freedesktop_cats_h

unsigned char freedesktop_check_cat ( char *name );

extern char *freedesktop_approved_cats[];

#define BADCATNAME "Other" /* irony: Other is itself not a freedesktop category */

#endif
