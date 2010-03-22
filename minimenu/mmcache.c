
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */
#include <limits.h> /* for PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>

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

#include "mmenu.h"
#include "mmapps.h"
#include "mmcache.h"

extern pnd_conf_handle g_conf;

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

  // see if we can mount the pnd/dir
  // does preview file exist?
  //   if so, load it up, size it, cache it
  //   if not, warning and bail
  // unmount it

  // can we mount?
  char fullpath [ PATH_MAX ];
  char filepath [ PATH_MAX ];

  sprintf ( fullpath, "%s/%s", app -> object_path, app -> object_filename );

  if ( ! pnd_pnd_mount ( pnd_run_script, fullpath, app -> unique_id ) ) {
    pnd_log ( pndn_debug, "Couldn't mount '%s' for preview\n", fullpath );
    return ( 0 ); // couldn't mount?!
  }

  sprintf ( filepath, "%s/%s/%s", PND_MOUNT_PATH, app -> unique_id, app -> preview_pic1 );
  s = IMG_Load ( filepath );

  pnd_pnd_unmount ( pnd_run_script, fullpath, app -> unique_id );

  if ( ! s ) {
    pnd_log ( pndn_debug, "Couldn't open image '%s' for preview\n", filepath );
    return ( 0 );
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

  // same-path override?
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

  // pull icon into buffer from .pnd
  if ( ! iconbuf ) {
    iconbuf = pnd_emit_icon_to_buffer ( app, &buflen );
  }

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
