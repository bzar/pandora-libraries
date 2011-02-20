
#include <stdio.h>
#include <limits.h> /* for PATH_MAX */
#define __USE_GNU /* for strndup */
#include <string.h> /* for strdup */
#include <stdlib.h> /* getenv */

#include "pnd_container.h"
#include "pnd_conf.h"
#include "pnd_discovery.h"
#include "pnd_notify.h"
#include "pnd_dbusnotify.h"
#include "pnd_logger.h"

#include "mmenu.h"
#include "mmcustom_cats.h"

mmcustom_cat_t mmcustom_complete [ MMCUSTOM_CATS_MAX ];
unsigned int mmcustom_count = 0;

pnd_conf_handle g_custom_h = 0;

char *mmcustom_determine_path ( void ) {
  char *home = getenv ( "HOME" );
  static char path [ PATH_MAX ];
  bzero ( path, PATH_MAX );

  snprintf ( path, PATH_MAX - 1, "%s/%s", home ? home : ".", MMCUSTOM_CATS_PREF_FILENAME );

  return ( path );
}

static unsigned char loaded = 0;
unsigned char mmcustom_setup ( void ) {

  if ( ! loaded ) {
    loaded = 1;

    // inhale conf file
    char *path = mmcustom_determine_path();

    g_custom_h = pnd_conf_fetch_by_path ( path );

    if ( ! g_custom_h ) {
      pnd_log ( pndn_rem, "Custom category conf file %s not found; is okay.\n", path );
      return ( 1 ); // file does not exist most likely
    }

    // find its head; if no head (empty file), bail
    char *iter = pnd_box_get_head ( g_custom_h );

    if ( ! iter ) {
      pnd_log ( pndn_rem, "Custom category conf file %s is empty. Fine.\n", path );
      return ( 1 );
    }

    // walk the conf, plucking out the values
    while ( iter ) {
      char *k = pnd_box_get_key ( iter );

      // does this entry look like it is a custom category?
      if ( strncmp ( k, MMCUSTOM_CATS_SECTION, strlen ( MMCUSTOM_CATS_SECTION ) ) == 0 ) {
	// determine the actual category name part
	k += ( strlen ( MMCUSTOM_CATS_SECTION ) + 1 );

	mmcustom_complete [ mmcustom_count ].cat = strdup ( k );
	if ( iter && strcmp ( iter, MMCUSTOM_CATS_NOCAT ) != 0 ) {
	  mmcustom_complete [ mmcustom_count ].parent_cat = strdup ( iter );
	} else {
	  mmcustom_complete [ mmcustom_count ].parent_cat = NULL;
	}

	mmcustom_count += 1;

      }

      // next!
      iter = pnd_box_get_next ( iter );

    } // while

    //pnd_log ( pndn_rem, "Found %u custom categories.\n", mmcustom_count );

  } // loaded already?

  return ( 1 );
}

void mmcustom_shutdown ( void ) {

  bzero ( mmcustom_complete, sizeof(mmcustom_cat_t) * MMCUSTOM_CATS_MAX );
  mmcustom_count = 0;
  loaded = 0;

  return;
}

unsigned char mmcustom_is_ready ( void ) {
  return ( loaded );
}

unsigned char mmcustom_write ( char *fullpath /* if NULL, uses canonical location */ ) {

  if ( ! fullpath ) {
    fullpath = mmcustom_determine_path();
  }

  FILE *f = fopen ( fullpath, "w" );

  if ( ! f ) {
    return ( 0 );
  }

  int i;
  for ( i = 0; i < mmcustom_count; i++ ) {
    if ( mmcustom_complete [ i ].cat ) {
      fprintf ( f, "%s.%s\t%s\n", MMCUSTOM_CATS_SECTION, mmcustom_complete [ i ].cat, mmcustom_complete [ i ].parent_cat ? mmcustom_complete [ i ].parent_cat : MMCUSTOM_CATS_NOCAT );
    }
  }

  fclose ( f );

  return ( 1 );
}

mmcustom_cat_t *mmcustom_query ( char *name, char *parentcatname ) {
  int i;

  // search for the cat/parent combination
  for ( i = 0; i < mmcustom_count; i++ ) {
    mmcustom_cat_t *p = &(mmcustom_complete [ i ]);

    if ( strcasecmp ( p -> cat, name ) == 0 ) {

      if ( parentcatname == NULL && p -> parent_cat == NULL ) {
	return ( p );
      } else if ( parentcatname && p -> parent_cat && strcasecmp ( p -> parent_cat, parentcatname ) == 0 ) {
	return ( p );
      }

    }

  } // for

  return ( NULL );
}

unsigned int mmcustom_subcount ( char *parentcatname ) {
  unsigned int counter = 0;

  int i;

  // search for the cat/parent combination
  for ( i = 0; i < mmcustom_count; i++ ) {

    if ( mmcustom_complete [ i ].parent_cat &&
	 strcmp ( mmcustom_complete [ i ].parent_cat, parentcatname ) == 0 )
    {
      counter++;
    }

  } // for

  return ( counter );
}

mmcustom_cat_t *mmcustom_register ( char *catname, char *parentcatname ) {
  mmcustom_complete [ mmcustom_count ].cat = strdup ( catname );
  if ( parentcatname ) {
    mmcustom_complete [ mmcustom_count ].parent_cat = strdup ( parentcatname );
  }
  mmcustom_count += 1;
  return ( &(mmcustom_complete [ mmcustom_count - 1 ]) );
}

unsigned int mmcustom_count_subcats ( char *catname ) {
  int i;
  unsigned int counter = 0;

  for ( i = 0; i < mmcustom_count; i++ ) {

    if ( mmcustom_complete [ i ].parent_cat && strcasecmp ( mmcustom_complete [ i ].parent_cat, catname ) == 0 ) {
      counter++;
    }

  }

  return ( counter );
}

void mmcustom_unregister ( char *catname, char *parentcatname ) {
  int i;
  int parent_index = -1;

  if ( parentcatname ) {
    pnd_log ( pndn_warning, "Goal: Remove subcat %s of %s\n", catname, parentcatname );
  } else {
    pnd_log ( pndn_warning, "Goal: Remove parent cat %s and descendants\n", catname );
  }

  for ( i = 0; i < mmcustom_count; i++ ) {

    // killing a parent cat, or just a subcat?
    if ( parentcatname ) {

      // killing a subcat, so match cat+subcat to kill
      if ( mmcustom_complete [ i ].cat && strcmp ( mmcustom_complete [ i ].cat, catname ) == 0 &&
	   mmcustom_complete [ i ].parent_cat && strcasecmp ( mmcustom_complete [ i ].parent_cat, parentcatname ) == 0 )
      {
	pnd_log ( pndn_warning, "  Removing subcat: %s of %s\n", catname, parentcatname );
	free ( mmcustom_complete [ i ].cat );
	mmcustom_complete [ i ].cat = NULL;
	break;
      }

    } else {

      // killing a parent cat, so kill it, and any children of it
      if ( mmcustom_complete [ i ].parent_cat == NULL &&
	   mmcustom_complete [ i ].cat &&
	   strcmp ( mmcustom_complete [ i ].cat, catname ) == 0 )
      {
	// flag the prent for future death (we need its name for now, since the caller is using it)
	parent_index = i;
      }
      else if ( mmcustom_complete [ i ].cat &&
		mmcustom_complete [ i ].parent_cat &&
		strcmp ( mmcustom_complete [ i ].parent_cat, catname ) == 0 )
      {
	// kill children of it
	pnd_log ( pndn_warning, "  Removing cascading subcat: %s of %s\n", mmcustom_complete [ i ].cat, mmcustom_complete [ i ].parent_cat );
	free ( mmcustom_complete [ i ].cat );
	mmcustom_complete [ i ].cat = NULL;
      }

    }

  } // for

  // kill the actual cat itself
  if ( parent_index >= 0 ) {
    pnd_log ( pndn_warning, "  Removing cat: %s\n", catname );
    free ( mmcustom_complete [ parent_index ].cat );
    mmcustom_complete [ parent_index ].cat = NULL;
  }

  return;
}
