
#include <stdio.h> /* for printf, NULL */
#include <stdlib.h> /* for free */
#include <string.h> /* for strdup */

#include "pnd_conf.h"
#include "pnd_container.h"
#include "pnd_apps.h"
#include "pnd_discovery.h"
#include "pnd_locate.h"
#include "pnd_pndfiles.h"
#include "pnd_pxml.h"

static void usage ( char *argv[] ) {
  printf ( "%s\tObtain a description, install guide or other info about a pnd-file\n", argv [ 0 ] );
  printf ( "\n" );
  printf ( "%s path-to-pndfile [section] [section-2...]\n", argv [ 0 ] );
  printf ( "\n" );
  printf ( "section\tOptional. If not specified, general description is shown. (Section 'description')\n" );
  printf ( "\tIf present, show the named section of the PXML -- such as to obtain install instructions etc.\n" );
  printf ( "pndfile\tRequired. Full path to the pnd-file to execute.\n" );
  return;
}

#define SECTIONMAX 100

int main ( int argc, char *argv[] ) {
  char *pndfile = NULL;
  char *section [ SECTIONMAX ];
  unsigned char sections = 0;
  int i;

  for ( i = 1; i < argc; i++ ) {

    if ( ! pndfile ) {
      pndfile = argv [ i ];
      continue;
    }

    section [ sections++ ] = argv [ i ];

    if ( sections == SECTIONMAX ) {
      break;
    }

  } // for args

  if ( ! pndfile ) {
    usage ( argv );
    exit ( 0 );
  }

  if ( ! sections ) {
    section [ sections++ ] = "description";
  }

  // summary
  printf ( "Pndfile\t%s\n", pndfile );
  printf ( "Sections to include:\n" );
  for ( i = 0; i < sections; i++ ) {
    printf ( "- %s\n", section [ i ] );
  } // for
  printf ( "\n" );

  // pull PXML
  unsigned int pxmlbuflen = 96 * 1024; // lame, need to calculate it
  char *pxmlbuf = malloc ( pxmlbuflen );
  if ( ! pxmlbuf ) {
    printf ( "ERROR: RAM exhausted!\n" );
    exit ( 0 );
  }
  memset ( pxmlbuf, '\0', pxmlbuflen );

  FILE *f = fopen ( pndfile, "r" );
  if ( ! f ) {
    printf ( "ERROR: Couldn't open pndfile %s!\n", pndfile );
    exit ( 0 );
  }

  pnd_pxml_handle h = NULL;
  pnd_pxml_handle *apps = NULL;
  if ( pnd_pnd_seek_pxml ( f ) ) {
    if ( pnd_pnd_accrue_pxml ( f, pxmlbuf, pxmlbuflen ) ) {
      apps = pnd_pxml_fetch_buffer ( "pnd_run", pxmlbuf );
    }
  }

  fclose ( f );

  if ( ! apps ) {
    printf ( "ERROR: Couldn't pull PXML.xml from the pndfile.\n" );
    exit ( 0 );
  }

  // iterate across apps
  while ( *apps ) {
    h = *apps;

    // display sections
    for ( i = 0; i < sections; i++ ) {
      char *t;

      if ( strcasecmp ( section [ i ], "description" ) == 0 ) {

	printf ( "Section: %s\n", section [ i ] );

	if ( ( t = pnd_pxml_get_description_en ( h ) ) ) {
	  printf ( "%s\n", t );
	} else {
	  printf ( "Not supplied by PXML.xml in the pnd-file\n" );
	}

      }

    } // for

    // next
    apps++;
  } // while

  return ( 0 );
} // main
