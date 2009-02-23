
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */

#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */

#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED
#include <ftw.h> /* for nftw, tree walker */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"
#include "pnd_pathiter.h"
#include "pnd_apps.h"
#include "pnd_pndfiles.h"

// need these 'globals' due to the way nftw and ftw work :/
static pnd_box_handle disco_box;
static char *disco_overrides = NULL;

void pnd_disco_destroy ( pnd_disco_t *p ) {

  if ( p -> title_en ) {
    free ( p -> title_en );
  }

  if ( p -> icon ) {
    free ( p -> icon );
  }

  if ( p -> exec ) {
    free ( p -> exec );
  }

  if ( p -> unique_id ) {
    free ( p -> unique_id );
  }

  if ( p -> main_category ) {
    free ( p -> main_category );
  }

  if ( p -> clockspeed ) {
    free ( p -> clockspeed );
  }

  return;
}

static int pnd_disco_callback ( const char *fpath, const struct stat *sb,
				int typeflag, struct FTW *ftwbuf )
{
  unsigned char valid = 0; // 1 for plaintext PXML, 2 for PND...
  pnd_pxml_handle pxmlh = 0;

  //printf ( "disco root callback encountered '%s'\n", fpath );

  // PXML.xml is a possible application candidate (and not a dir named PXML.xml :)
  if ( typeflag & FTW_D ) {
    //printf ( " .. is dir, skipping\n" );
    return ( 0 ); // skip directories and other non-regular files
  }

  // PND/PNZ file and others may be valid as well .. but lets leave that for now
  //   printf ( "%s %s\n", fpath + ftwbuf -> base, PND_PACKAGE_FILEEXT );
  if ( strcasecmp ( fpath + ftwbuf -> base, PXML_FILENAME ) == 0 ) {
    valid = 1;
  } else if ( strcasestr ( fpath + ftwbuf -> base, PND_PACKAGE_FILEEXT "\0" ) ) {
    valid = 2;
  }

  // if not a file of interest, just keep looking until we run out
  if ( ! valid ) {
    //printf ( " .. bad filename, skipping\n" );
    return ( 0 );
  }

  // potentially a valid application
  if ( valid == 1 ) {
    // Plaintext PXML file
    //printf ( "PXML: disco callback encountered '%s'\n", fpath );

    // pick up the PXML if we can
    pxmlh = pnd_pxml_fetch ( (char*) fpath );

  } else if ( valid == 2 ) {
    // PND ... ??
    FILE *f;
    char pxmlbuf [ 32 * 1024 ]; // TBD: assuming 32k pxml accrual buffer is a little lame

    //printf ( "PND: disco callback encountered '%s'\n", fpath );

    // is this a valid .pnd file? The filename is a candidate already, but verify..
    // .. presence of PXML appended, or at least contained within?
    // .. presence of an icon appended after PXML?

    // open it up..
    f = fopen ( fpath, "r" );

    // try to locate the PXML portion
    if ( ! pnd_pnd_seek_pxml ( f ) ) {
      fclose ( f );
      return ( 0 ); // pnd or not, but not to spec. Pwn'd the pnd?
    }

    // accrue it into a buffer
    if ( ! pnd_pnd_accrue_pxml ( f, pxmlbuf, 32 * 1024 ) ) {
      fclose ( f );
      return ( 0 );
    }

    //printf ( "buffer is %s\n", pxmlbuf );
    //fflush ( stdout );

    // by now, we have <PXML> .. </PXML>, try to parse..
    pxmlh = pnd_pxml_fetch_buffer ( (char*) fpath, pxmlbuf );

  }

  // pxmlh is useful?
  if ( pxmlh ) {

    // look for any overrides, if requested
    pnd_pxml_merge_override ( pxmlh, disco_overrides );

    // check for validity and add to resultset if it looks executable
    if ( pnd_is_pxml_valid_app ( pxmlh ) ) {
      char b [ 1024 ]; // TBD: also lame
      pnd_disco_t *p;

      p = pnd_box_allocinsert ( disco_box, (char*) fpath, sizeof(pnd_disco_t) );
      if ( pnd_pxml_get_app_name_en ( pxmlh ) ) {
	p -> title_en = strdup ( pnd_pxml_get_app_name_en ( pxmlh ) );
      }
      if ( pnd_pxml_get_icon ( pxmlh ) ) {
	p -> icon = strdup ( pnd_pxml_get_icon ( pxmlh ) );
      }
      if ( pnd_pxml_get_exec ( pxmlh ) ) {
	snprintf ( b, 1024, "pnd_run_magic %s", pnd_pxml_get_exec ( pxmlh ) );
	p -> exec = strdup ( b );
      }
      if ( pnd_pxml_get_unique_id ( pxmlh ) ) {
	p -> unique_id = strdup ( pnd_pxml_get_unique_id ( pxmlh ) );
      }
      if ( pnd_pxml_get_main_category ( pxmlh ) ) {
	p -> main_category = strdup ( pnd_pxml_get_main_category ( pxmlh ) );
      }
      if ( pnd_pxml_get_clockspeed ( pxmlh ) ) {
	p -> clockspeed = strdup ( pnd_pxml_get_clockspeed ( pxmlh ) ); 
      }

    } else {
      //printf ( "Invalid PXML; skipping.\n" );
    }

    // ditch pxml
    pnd_pxml_delete ( pxmlh );

  } // got a pxmlh

  return ( 0 ); // continue the tree walk
}

pnd_box_handle pnd_disco_search ( char *searchpath, char *overridespath ) {

  //printf ( "Searchpath to discover: '%s'\n", searchpath );

  // alloc a container for the result set
  disco_box = pnd_box_new ( "discovery" );
  disco_overrides = overridespath;

  /* iterate across the paths within the searchpath, attempting to locate applications
   */

  SEARCHPATH_PRE
  {

    // invoke the dir walking function; thankfully Linux includes a pretty good one
    nftw ( buffer,               // path to descend
	   pnd_disco_callback,   // callback to do processing
	   10,                   // no more than X open fd's at once
	   FTW_PHYS );           // do not follow symlinks

  }
  SEARCHPATH_POST

  // return whatever we found, or NULL if nada
  if ( ! pnd_box_get_head ( disco_box ) ) {
    pnd_box_delete ( disco_box );
    disco_box = NULL;
  }

  return ( disco_box );
}

unsigned char pnd_emit_dotdesktop ( char *targetpath, pnd_disco_t *p ) {
  char filename [ FILENAME_MAX ];
  char buffer [ 1024 ];
  FILE *f;

  // specification
  // http://standards.freedesktop.org/desktop-entry-spec

  // validation

  if ( ! p -> unique_id ) {
    return ( 0 );
  }

  if ( ! p -> exec ) {
    return ( 0 );
  }

  // set up

  sprintf ( filename, "%s/%s.desktop", targetpath, p -> unique_id );

  // emit

  //printf ( "EMIT DOTDESKTOP '%s'\n", filename );

  f = fopen ( filename, "w" );

  if ( ! f ) {
    return ( 0 );
  }

  if ( p -> title_en ) {
    snprintf ( buffer, 1020, "Name=%s\n", p -> title_en );
    fprintf ( f, "%s", buffer );
  }

  fprintf ( f, "Type=Application\n" );
  fprintf ( f, "Version=1.0\n" );

  if ( p -> icon ) {
    snprintf ( buffer, 1020, "Icon=%s\n", p -> icon );
    fprintf ( f, "%s", buffer );
  }

#if 0
  if ( p -> description_en ) {
    snprintf ( buffer, 1020, "Comment=%s\n", p -> description_en );
    fprintf ( f, "%s", buffer );
  }
#endif

#if 0
  if ( p -> startdir ) {
    snprintf ( buffer, 1020, "Path=%s\n", p -> startdir );
    fprintf ( f, "%s", buffer );
  } else {
    fprintf ( f, "Path=%s\n", PND_DEFAULT_WORKDIR );
  }
#endif

  if ( p -> exec ) {
    snprintf ( buffer, 1020, "Exec=%s\n", p -> exec );
    fprintf ( f, "%s", buffer );
  }

  fprintf ( f, "_Source=libpnd\n" ); // should we need to know 'who' created the file during trimming

  fclose ( f );

  return ( 1 );
}
