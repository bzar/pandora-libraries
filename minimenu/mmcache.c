
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */
#include <limits.h> /* for PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> /* for time() */

#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */

#include <fcntl.h>
#include <limits.h>

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_rotozoom.h"

#define __USE_GNU /* for strcasestr */
#include <unistd.h> /* for unlink */
#include <string.h> /* for making ftw.h happy */

#include "pnd_pxml.h"
#include "pnd_utility.h"
#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "pnd_logger.h"
#include "pnd_desktop.h"
#include "pnd_pndfiles.h"
#include "pnd_apps.h"
#include "../lib/pnd_pathiter.h"
#include "pnd_locate.h"
#include "pnd_notify.h"
#include "pnd_dbusnotify.h"

#include "mmenu.h"
#include "mmapps.h"
#include "mmcache.h"
#include "mmcat.h"
#include "mmui.h"

extern pnd_conf_handle g_conf;
extern unsigned char g_pvwcache;
extern pnd_conf_handle g_desktopconf;

mm_cache_t *g_icon_cache = NULL;
mm_cache_t *g_preview_cache = NULL;

unsigned char cache_preview ( pnd_disco_t *app, unsigned int maxwidth, unsigned int maxheight ) {
  SDL_Surface *s;
  mm_cache_t *c;

  // does this sucker even have a preview?
  if ( ! app -> preview_pic1 ) {
    return ( 1 ); // nothing here, so thats fine
  }

  // check if already cached
  if ( ( c = cache_query_preview ( app -> unique_id ) ) ) {
    return ( 1 ); // already got it
  }

  // not cached, load it up
  //

  // show hourglass
  ui_show_hourglass ( 1 /* updaterect*/ );

  // see if we can mount the pnd/dir
  // does preview file exist?
  //   if so, load it up, size it, cache it
  //   if not, warning and bail
  // unmount it

  // can we mount? or can we find it in preview cache? or an override?
  char fullpath [ PATH_MAX ] = "";
  char filepath [ PATH_MAX ] = "";

  // first, check for preview override
  {
    char ovrfile [ PATH_MAX ];
    char *fooby;
    sprintf ( ovrfile, "%s/%s", app -> object_path, app -> object_filename );
    fooby = strcasestr ( ovrfile, PND_PACKAGE_FILEEXT );
    if ( fooby ) {
      sprintf ( fooby, "_pvw#%u.png", app -> subapp_number );
      struct stat statbuf;
      if ( stat ( ovrfile, &statbuf ) == 0 ) {
	strncpy ( filepath, ovrfile, PATH_MAX );
      } // stat
    } // ovr?
  }

  // if not yet found, try to find in cache
  if ( filepath [ 0 ] == '\0' && g_pvwcache ) {
    static char *cache_findpath = NULL;
    if ( ! cache_findpath ) {
      cache_findpath = pnd_conf_get_as_char ( g_conf, "previewpic.cache_findpath" );
    }
    char buffer [ FILENAME_MAX ];
    sprintf ( buffer, "%s.png", app -> unique_id );
    char *f = pnd_locate_filename ( cache_findpath, buffer );
    if ( f ) {
      strncpy ( filepath, f, PATH_MAX );
    }
  }

  // unique-id to use for the cache mount
  char *uid = app -> unique_id;

  if ( app -> appdata_dirname ) {
    uid = app -> appdata_dirname;
  }

  // if we don't have a file path sorted out yet, means we need to mount and figure it
  if ( ! filepath [ 0 ] ) {
    sprintf ( fullpath, "%s/%s", app -> object_path, app -> object_filename );

    if ( ! pnd_pnd_mount ( pnd_run_script, fullpath, uid ) ) {
      pnd_log ( pndn_debug, "Couldn't mount '%s' for preview\n", fullpath );
      return ( 0 ); // couldn't mount?!
    }

    sprintf ( filepath, "%s/%s/%s", PND_MOUNT_PATH, uid, app -> preview_pic1 );
  }

  // load whatever path we've got
  s = IMG_Load ( filepath );

  if ( ! s ) {
    // unmount it, if mounted
    if ( fullpath [ 0 ] ) {
      pnd_pnd_unmount ( pnd_run_script, fullpath, uid );
    }
    pnd_log ( pndn_debug, "Couldn't open image '%s' for preview\n", filepath );
    return ( 0 );
  }

  // try to copy file to the cache, if we're doing that, and if mounted
  if ( g_pvwcache && fullpath [ 0 ] ) {
    char cacheoutpath [ PATH_MAX ] = "";

    // figure out where we want to write the file to
    if ( cache_find_writable ( app -> object_path, cacheoutpath, PATH_MAX ) ) {
      static char *cache_path = NULL;
      char buffer [ PATH_MAX ];
      if ( ! cache_path ) {
	cache_path = pnd_conf_get_as_char ( g_conf, "previewpic.cache_path" );
      }
      // make the dir
      snprintf ( buffer, PATH_MAX, "%s/%s", cacheoutpath, cache_path );
      struct stat statbuf;
      if ( stat ( buffer, &statbuf ) != 0 ) {
	snprintf ( buffer, PATH_MAX, "/bin/mkdir -p %s/%s", cacheoutpath, cache_path );
	system ( buffer );
      }
      // set up target filename to copy
      snprintf ( buffer, PATH_MAX, "%s/%s/%s.png", cacheoutpath, cache_path, app -> unique_id );
      pnd_log ( pndn_debug, "Found free space to cache preview to here: %s", buffer );
      if ( ! pnd_filecopy ( filepath, buffer ) ) {
	pnd_log ( pndn_error, "ERROR: Copying preview from %s to %s", filepath, buffer );
      }
    } else {
      pnd_log ( pndn_warning, "WARN: Couldn't find a device to cache preview to.\n" );
    }

  } // preview file cache

  // unmount it, if mounted
  if ( fullpath [ 0 ] ) {
    pnd_pnd_unmount ( pnd_run_script, fullpath, app -> unique_id );
  }

  //pnd_log ( pndn_debug, "Image size is %u x %u (max %u x %u)\n", s -> w, s -> h, maxwidth, maxheight );

  // scale
  if ( s -> w < maxwidth ) {
    // scale up?
    if ( pnd_conf_get_as_int_d ( g_conf, "previewpic.scale_up_bool", 1 ) ) {
      SDL_Surface *scaled;
      double scale = (double)maxwidth / (double)s -> w;
      //pnd_log ( pndn_debug, "  Upscaling; scale factor %f\n", scale );
      scaled = rotozoomSurface ( s, 0 /* angle*/, scale /* scale */, 1 /* smooth==1*/ );
      SDL_FreeSurface ( s );
      s = scaled;
    }
  } else if ( s -> w > maxwidth ) {
    SDL_Surface *scaled;
    double scale = (double)maxwidth / (double)s -> w;
    //pnd_log ( pndn_debug, "  Downscaling; scale factor %f\n", scale );
    scaled = rotozoomSurface ( s, 0 /* angle*/, scale /* scale */, 1 /* smooth==1*/ );
    SDL_FreeSurface ( s );
    s = scaled;
  }

  // add to cache
  c = (mm_cache_t*) malloc ( sizeof(mm_cache_t) );
  bzero ( c, sizeof(mm_cache_t) );

  if ( ! g_preview_cache ) {
    g_preview_cache = c;
  } else {
    c -> next = g_preview_cache;
    g_preview_cache = c;
  }

  strncpy ( c -> uniqueid, app -> unique_id, 1000 );
  c -> i = s;

  return ( 1 );
}

unsigned char cache_icon ( pnd_disco_t *app, unsigned char maxwidth, unsigned char maxheight ) {
  SDL_Surface *s;
  mm_cache_t *c;

  // check if already cached
  if ( ( c = cache_query_icon ( app -> unique_id ) ) ) {
    return ( 1 ); // already got it
  }

  // not cached, load it up
  //
  unsigned char *iconbuf = NULL;
  unsigned int buflen = 0;

  // same-path icon override?
  char ovrfile [ PATH_MAX ];
  char *fixpxml;
  sprintf ( ovrfile, "%s/%s", app -> object_path, app -> object_filename );
  fixpxml = strcasestr ( ovrfile, PND_PACKAGE_FILEEXT );
  if ( fixpxml ) {
    strcpy ( fixpxml, ".png" );
    struct stat statbuf;
    if ( stat ( ovrfile, &statbuf ) == 0 ) {
      buflen = statbuf.st_size;
      if ( ( iconbuf = malloc ( statbuf.st_size ) ) ) {
	int fd = open ( ovrfile, O_RDONLY );
	if ( fd >= 0 ) {
	  if ( read ( fd, iconbuf, statbuf.st_size ) != statbuf.st_size ) {
	    free ( iconbuf );
	    close ( fd );
	    return ( 0 );
	  }
	  close ( fd );
	} // open
      } // malloc
    } // stat
  } // ovr?

  // perhaps pndnotifyd has dropped a copy of the icon into /tmp?
#if 1
  {
    static char *iconpath = NULL;

    if ( ! iconpath ) {
      iconpath = pnd_conf_get_as_char ( g_desktopconf, "desktop.iconpath" );
    }

    sprintf ( ovrfile, "%s/%s.png", iconpath, app -> unique_id );

    // making sure the file is at least a few seconds old, to help avoid race condition
    struct stat statbuf;
    if ( stat ( ovrfile, &statbuf ) == 0 && time ( NULL ) - statbuf.st_mtime > 5 ) { // race with pndnotifyd
      buflen = statbuf.st_size;
      if ( ( iconbuf = malloc ( statbuf.st_size ) ) ) {
	int fd = open ( ovrfile, O_RDONLY );
	if ( fd >= 0 ) {
	  if ( read ( fd, iconbuf, statbuf.st_size ) != statbuf.st_size ) {
	    free ( iconbuf );
	    close ( fd );
	    return ( 0 );
	  }
	  close ( fd );
	} // open
      } // malloc
    } // stat

  }
#endif

  // if this is a real pnd file (dir-app or pnd-file-app), then try to pull icon from there
  if ( ! iconbuf ) {

    if (  app -> object_flags & PND_DISCO_GENERATED ) {

      // maybe we can discover this single-file and find an icon?
      if ( strcasestr ( app -> object_filename, PND_PACKAGE_FILEEXT ) ) {

	// looks like a pnd, now what do we do..
	pnd_box_handle h = pnd_disco_file ( app -> object_path, app -> object_filename );

	if ( h ) {
	  pnd_disco_t *d = pnd_box_get_head ( h );
	  iconbuf = pnd_emit_icon_to_buffer ( d, &buflen );
	}

      } // filename has .pnd?

    } else {

      // pull icon into buffer from .pnd if not already found an icon
      iconbuf = pnd_emit_icon_to_buffer ( app, &buflen );

    } // generated?

  } // already got icon?

  if ( ! iconbuf ) {
    return ( 0 );
  }

  // ready up a RWbuffer for SDL
  SDL_RWops *rwops = SDL_RWFromMem ( iconbuf, buflen );

  s = IMG_Load_RW ( rwops, 1 /* free the rwops */ );

  if ( ! s ) {
    return ( 0 );
  }

  free ( iconbuf ); // ditch the icon from ram

  //pnd_log ( pndn_debug, "Image size is %u x %u (max %u x %u)\n", s -> w, s -> h, maxwidth, maxheight );

  // scale the icon?
  if ( s -> w < maxwidth ) {
    // scale up?
    if ( pnd_conf_get_as_int_d ( g_conf, "grid.scale_up_bool", 1 ) ) {
      SDL_Surface *scaled;
      double scale = (double)maxwidth / (double)s -> w;
      //pnd_log ( pndn_debug, "  Upscaling; scale factor %f\n", scale );
      scaled = rotozoomSurface ( s, 0 /* angle*/, scale /* scale */, 1 /* smooth==1*/ );
      SDL_FreeSurface ( s );
      s = scaled;
    }
  } else if ( s -> w > maxwidth ) {
    SDL_Surface *scaled;
    double scale = (double)maxwidth / (double)s -> w;
    //pnd_log ( pndn_debug, "  Downscaling; scale factor %f\n", scale );
    scaled = rotozoomSurface ( s, 0 /* angle*/, scale /* scale */, 1 /* smooth==1*/ );
    SDL_FreeSurface ( s );
    s = scaled;
  }

  // add to cache
  c = (mm_cache_t*) malloc ( sizeof(mm_cache_t) );
  bzero ( c, sizeof(mm_cache_t) );

  if ( ! g_icon_cache ) {
    g_icon_cache = c;
  } else {
    c -> next = g_icon_cache;
    g_icon_cache = c;
  }

  strncpy ( c -> uniqueid, app -> unique_id, 1000 );
  c -> i = s;

  return ( 1 );
}

mm_cache_t *cache_query ( char *id, mm_cache_t *head ) {
  mm_cache_t *iter = head;

  if ( ! id ) {
    return ( NULL );
  }

  while ( iter ) {
    if ( iter -> uniqueid &&
	 strcasecmp ( iter -> uniqueid, id ) == 0 )
    {
      return ( iter );
    }
    iter = iter -> next;
  } // while

  return ( NULL );
}

mm_cache_t *cache_query_icon ( char *id ) {
  return ( cache_query ( id, g_icon_cache ) );
}

mm_cache_t *cache_query_preview ( char *id ) {
  return ( cache_query ( id, g_preview_cache ) );
}

unsigned char cache_find_writable ( char *originpath, char *r_writepath, unsigned int len ) {
  static char *searchpaths = NULL;
  static unsigned int minfree = 0;
  char searchpath [ PATH_MAX ] = "";
  char cmdbuf [ PATH_MAX ];
  FILE *f;
  unsigned int freespace = 0;

  // figure out the mountpoint for the file, and stick that in at front of searchpath
  // so that it will get picked first, if possible.
  char mountpath [ PATH_MAX ];
  if ( pnd_determine_mountpoint ( originpath, mountpath, PATH_MAX ) ) {
    sprintf ( searchpath, "%s:", mountpath );
    //pnd_log ( pndn_debug, "Preferred cache target for %s: %s\n", originpath, mountpath );
  }

  // try to find a device, in order of searchpath, with enough space for this cache-out
  //

  // populate the searchpath
  if ( ! searchpaths ) {
    searchpaths = pnd_conf_get_as_char ( g_conf, "previewpic.cache_searchpath" );
    minfree = pnd_conf_get_as_int_d ( g_conf, "previewpic.cache_minfree", 500 );
  }

  if ( ! searchpaths ) {
    return ( 0 ); // fail!
  }

  strncat ( searchpath, searchpaths, PATH_MAX );

  SEARCHPATH_PRE
  {

    // since I didn't figure out which /sys/block I can pull remaining space from, I'll use df for now :/
    sprintf ( cmdbuf, "/bin/df %s", buffer );

    f = popen ( cmdbuf, "r" );

    if ( f ) {
      while ( fgets ( cmdbuf, PATH_MAX, f ) ) {
	// just eat it up
	// /dev/sdc2              7471392    725260   6366600  11% /media/IMAGE
	if ( sscanf ( cmdbuf, "%*s %*u %*u %u %*u %*s\n", &freespace ) == 1 ) {
	  strncpy ( r_writepath, buffer, len );
	  if ( freespace > minfree ) {
	    pclose ( f );
	    return ( 1 );
	  } // enough free?
	} // df
      } // while
      pclose ( f );
    }

  }
  SEARCHPATH_POST

  return ( 0 );
}
