
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */

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
  unsigned char valid = pnd_object_type_unknown;
  pnd_pxml_handle pxmlh = 0;
  unsigned int pxml_close_pos = 0;

  //printf ( "disco root callback encountered '%s'\n", fpath );

  // PXML.xml is a possible application candidate (and not a dir named PXML.xml :)
  if ( typeflag & FTW_D ) {
    //printf ( " .. is dir, skipping\n" );
    return ( 0 ); // skip directories and other non-regular files
  }

  // PND/PNZ file and others may be valid as well .. but lets leave that for now
  //   printf ( "%s %s\n", fpath + ftwbuf -> base, PND_PACKAGE_FILEEXT );
  if ( strcasecmp ( fpath + ftwbuf -> base, PXML_FILENAME ) == 0 ) {
    valid = pnd_object_type_directory;
  } else if ( strcasestr ( fpath + ftwbuf -> base, PND_PACKAGE_FILEEXT "\0" ) ) {
    valid = pnd_object_type_pnd;
  }

  // if not a file of interest, just keep looking until we run out
  if ( ! valid ) {
    //printf ( " .. bad filename, skipping\n" );
    return ( 0 );
  }

  // potentially a valid application
  if ( valid == pnd_object_type_directory ) {
    // Plaintext PXML file
    //printf ( "PXML: disco callback encountered '%s'\n", fpath );

    // pick up the PXML if we can
    pxmlh = pnd_pxml_fetch ( (char*) fpath );

  } else if ( valid == pnd_object_type_pnd ) {
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

#if 1 // icon
    // for convenience, lets skip along past trailing newlines/CR's in hopes of finding icon data?
    {
      unsigned int pos = ftell ( f );
      char pngbuffer [ 16 ]; // \211 P N G \r \n \032 \n
      pngbuffer [ 0 ] = 137;      pngbuffer [ 1 ] = 80;      pngbuffer [ 2 ] = 78;      pngbuffer [ 3 ] = 71;
      pngbuffer [ 4 ] = 13;       pngbuffer [ 5 ] = 10;       pngbuffer [ 6 ] = 26;      pngbuffer [ 7 ] = 10;
      if ( fread ( pngbuffer + 8, 8, 1, f ) == 1 ) {
	if ( memcmp ( pngbuffer, pngbuffer + 8, 8 ) == 0 ) {
	  pxml_close_pos = pos;
	}
      }
    } // icon
#endif

    // by now, we have <PXML> .. </PXML>, try to parse..
    pxmlh = pnd_pxml_fetch_buffer ( (char*) fpath, pxmlbuf );

    // done with file
    fclose ( f );

  }

  // pxmlh is useful?
  if ( pxmlh ) {

    // look for any overrides, if requested
    pnd_pxml_merge_override ( pxmlh, disco_overrides );

    // check for validity and add to resultset if it looks executable
    if ( pnd_is_pxml_valid_app ( pxmlh ) ) {
      pnd_disco_t *p;
      char *fixpxml;

      p = pnd_box_allocinsert ( disco_box, (char*) fpath, sizeof(pnd_disco_t) );

      // base paths
      p -> object_path = strdup ( fpath );

      if ( ( fixpxml = strcasestr ( p -> object_path, PXML_FILENAME ) ) ) {
	*fixpxml = '\0'; // if this is not a .pnd, lop off the PXML.xml at the end
      } else if ( ( fixpxml = strrchr ( p -> object_path, '/' ) ) ) {
	*(fixpxml+1) = '\0'; // for pnd, lop off to last /
      }

      if ( ( fixpxml = strrchr ( fpath, '/' ) ) ) {
	p -> object_filename = strdup ( fixpxml + 1 );
      }

      // png icon path
      p -> pnd_icon_pos = pxml_close_pos;

      // type
      p -> object_type = valid;

      // PXML fields
      if ( pnd_pxml_get_app_name_en ( pxmlh ) ) {
	p -> title_en = strdup ( pnd_pxml_get_app_name_en ( pxmlh ) );
      }
      if ( pnd_pxml_get_icon ( pxmlh ) ) {
	p -> icon = strdup ( pnd_pxml_get_icon ( pxmlh ) );
      }
      if ( pnd_pxml_get_exec ( pxmlh ) ) {
	p -> exec = strdup ( pnd_pxml_get_exec ( pxmlh ) );
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
      if ( pnd_pxml_get_startdir ( pxmlh ) ) {
	p -> startdir = strdup ( pnd_pxml_get_startdir ( pxmlh ) ); 
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

unsigned char pnd_emit_dotdesktop ( char *targetpath, char *pndrun, pnd_disco_t *p ) {
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

#if 0 // we let pnd_run.sh handle this
  if ( p -> startdir ) {
    snprintf ( buffer, 1020, "Path=%s\n", p -> startdir );
    fprintf ( f, "%s", buffer );
  } else {
    fprintf ( f, "Path=%s\n", PND_DEFAULT_WORKDIR );
  }
#endif

  if ( p -> exec ) {

    // basics
    if ( p -> object_type == pnd_object_type_directory ) {
      snprintf ( buffer, 1020, "Exec=%s -p %s -e %s -u", pndrun, p -> object_path, p -> exec );
    } else if ( p -> object_type == pnd_object_type_pnd ) {
      snprintf ( buffer, 1020, "Exec=%s -p %s/%s -e %s -u", pndrun, p -> object_path, p -> object_filename, p -> exec );
    }

    // start dir
    if ( p -> startdir ) {
      strncat ( buffer, " -s ", 1020 );
      strncat ( buffer, p -> startdir, 1020 );
    }

    // newline
    strncat ( buffer, "\n", 1020 );

    // emit
    fprintf ( f, "%s", buffer );
  }

  fprintf ( f, "%s\n", PND_DOTDESKTOP_SOURCE ); // should we need to know 'who' created the file during trimming

  fclose ( f );

  return ( 1 );
}

unsigned char pnd_emit_icon ( char *targetpath, pnd_disco_t *p ) {
  char buffer [ FILENAME_MAX ]; // target filename
  char from [ FILENAME_MAX ];   // source filename
  char bits [ 8 * 1024 ];
  unsigned int bitlen;
  FILE *pnd, *target;

  // prelim .. if a pnd file, and no offset found, discovery code didn't locate icon.. so bail.
  if ( ( p -> object_type == pnd_object_type_pnd ) &&
       ( ! p -> pnd_icon_pos ) )
  {
    return ( 0 ); // discover code didn't find it, so FAIL
  }

  // determine filename for target
  sprintf ( buffer, "%s/%s.png", targetpath, p -> unique_id ); // target

  /* first.. open the source file, by type of application:
   * are we looking through a pnd file or a dir?
   */
  if ( p -> object_type == pnd_object_type_directory ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> icon );
  } else if ( p -> object_type == pnd_object_type_pnd ) {
    sprintf ( from, "%s/%s", p -> object_path, p -> object_filename );
  }

  pnd = fopen ( from, "r" );

  if ( ! pnd ) {
    return ( 0 );
  }

  unsigned int len;

  target = fopen ( buffer, "wb" );

  if ( ! target ) {
    fclose ( pnd );
    return ( 0 );
  }

  fseek ( pnd, 0, SEEK_END );
  len = ftell ( pnd );
  //fseek ( pnd, 0, SEEK_SET );

  fseek ( pnd, p -> pnd_icon_pos, SEEK_SET );

  len -= p -> pnd_icon_pos;

  while ( len ) {

    if ( len > (8*1024) ) {
      bitlen = (8*1024);
    } else {
      bitlen = len;
    }

    if ( fread ( bits, bitlen, 1, pnd ) != 1 ) {
      fclose ( pnd );
      fclose ( target );
      unlink ( buffer );
      return ( 0 );
    }

    if ( fwrite ( bits, bitlen, 1, target ) != 1 ) {
      fclose ( pnd );
      fclose ( target );
      unlink ( buffer );
      return ( 0 );
    }

    len -= bitlen;
  } // while

  fclose ( pnd );
  fclose ( target );

  return ( 1 );
}
