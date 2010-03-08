
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"

#include "mmenu.h"
#include "mmcache.h"
#include "mmcat.h"

mm_category_t g_categories [ MAX_CATS ];
unsigned char g_categorycount = 0;

unsigned char category_push ( char *catname, pnd_disco_t *app ) {
  mm_category_t *c;

  // check category list; if found, append app to the end of it.
  // if not found, add it to the category list and plop the app in there.
  // app's are just app-refs, which contain links to the disco-t list -- thus removal is only in one place, and
  // an app can be in multiple categories if we like..
  //

  // find or create category
  //

  if ( ( c = category_query ( catname ) ) ) {
    // category was found..
  } else {
    // category wasn't found..
    pnd_log ( PND_LOG_DEFAULT, "New category '%s'\n", catname );
    g_categories [ g_categorycount ].catname = strdup ( catname );
    g_categories [ g_categorycount ].refs = NULL;
    c = &(g_categories [ g_categorycount ]);
    g_categorycount++;
  }

  // alloc and populate appref
  //
  mm_appref_t *ar = malloc ( sizeof(mm_appref_t) );
  if ( ! ar ) {
    return ( 0 );
  }

  bzero ( ar, sizeof(mm_appref_t) );

  ar -> ref = app;

  // plug it into category
  //   and sort it on insert!
#if 0 // no sorting
  ar -> next = c -> refs;
  c -> refs = ar;
#else // with sorting
  // if no refs at all, or new guy has no title, just stick it in at head
  if ( c -> refs && ar -> ref -> title_en ) {
    mm_appref_t *iter = c -> refs;
    mm_appref_t *last = NULL;

    while ( iter ) {

      if ( iter -> ref -> title_en ) {
	if ( strcmp ( ar -> ref -> title_en, iter -> ref -> title_en ) < 0 ) {
	  // new guy is smaller than the current guy!
	  break;
	}
      } else {
	// since new guy must have a name by here, we're bigger than any guy who does not have a name
	// --> continue
      }

      last = iter;
      iter = iter -> next;
    }

    if ( iter ) {
      // smaller than the current guy, so stitch in
      if ( last ) {
	ar -> next = iter;
	last -> next = ar;
      } else {
	ar -> next = c -> refs;
	c -> refs = ar;
      }
    } else {
      // we're the biggest, just append to last
      last -> next = ar;
    }

  } else {
    ar -> next = c -> refs;
    c -> refs = ar;
  }
#endif
  c -> refcount++;

  return ( 1 );
}

mm_category_t *category_query ( char *catname ) {
  unsigned char i;

  for ( i = 0; i < g_categorycount; i++ ) {

    if ( strcasecmp ( g_categories [ i ].catname, catname ) == 0 ) {
      return ( &(g_categories [ i ]) );
    }

  }

  return ( NULL );
}

void category_dump ( void ) {
  unsigned int i;

  // WHY AREN'T I SORTING ON INSERT?

  // dump
  for ( i = 0; i < g_categorycount; i++ ) {
    pnd_log ( PND_LOG_DEFAULT, "Category %u: '%s' * %u\n", i, g_categories [ i ].catname, g_categories [ i ].refcount );
    mm_appref_t *ar = g_categories [ i ].refs;

    while ( ar ) {
      pnd_log ( PND_LOG_DEFAULT, "  Appref %s\n", IFNULL(ar -> ref -> title_en,"No Name") );
      ar = ar -> next;
    }

  } // for

  return;
}
