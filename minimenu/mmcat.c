
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

#include "pnd_conf.h"
#include "pnd_logger.h"
#include "pnd_pxml.h"
#include "pnd_container.h"
#include "pnd_discovery.h"
#include "../lib/pnd_pathiter.h"

#include "mmenu.h"
#include "mmcache.h"
#include "mmcat.h"
#include "freedesktop_cats.h"

// all categories known -- visible, hidden, subcats, whatever.
pnd_box_handle m_categories = NULL;
unsigned int m_categorycount = 0;

// all categories being published right now -- a subset copied from m_categories
mm_category_t *g_categories [ MAX_CATS ];
unsigned char g_categorycount;

// category mappings
mm_catmap_t g_catmaps [ MAX_CATS ];
unsigned char g_catmapcount = 0;

extern pnd_conf_handle g_conf;

void category_init ( void ) {
  m_categories = pnd_box_new ( "Schrodinger's cat" );
  return;
}

unsigned char category_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, char *fspath, unsigned char visiblep ) {
  mm_category_t *c;

  // check category list; if found, append app to the end of it.
  // if not found, add it to the category list and plop the app in there.
  // app's are just app-refs, which contain links to the disco-t list -- thus removal is only in one place, and
  // an app can be in multiple categories if we like..
  //

  // find or create category
  //

  if ( ( c = pnd_box_find_by_key ( m_categories, catname ) ) ) {
    // category was found..
  } else {
    // category wasn't found..
    //pnd_log ( PND_LOG_DEFAULT, "New category '%s'\n", catname );
    c = pnd_box_allocinsert ( m_categories, catname, sizeof(mm_category_t) );
    c -> catname = strdup ( catname );
    if ( parentcatname ) {
      c -> parent_catname = strdup ( parentcatname );
    }

    if ( visiblep ) {
      c -> catflags = CFNORMAL;
    } else {
      c -> catflags = CFHIDDEN;
    }

    // if user prefers subcats-as-folders, lets reflag this sucker
    if ( parentcatname && pnd_conf_get_as_int_d ( g_conf, "tabs.subcat_as_folders", 1 ) ) {
      //printf ( "subcat: %s parent: %s\n", catname, parentcatname ? parentcatname : "none" );
      c -> catflags = CFSUBCAT;
    }

    c -> refs = NULL;

    if ( fspath ) {
      c -> fspath = strdup ( fspath );
    }

    m_categorycount++;
  }

  if ( ! app ) {
    return ( 1 ); // create cat, but skip app
  }

  // alloc and populate appref
  //
  mm_appref_t *ar = malloc ( sizeof(mm_appref_t) );
  if ( ! ar ) {
    return ( 0 );
  }

  bzero ( ar, sizeof(mm_appref_t) );

  ar -> ref = app;
  ar -> ovrh = ovrh;

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
	if ( cat_sort_score ( c, ar, iter ) < 0 ) {
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

int cat_sort_score ( mm_category_t *cat, mm_appref_t *s1, mm_appref_t *s2 ) {

  // are we in a directory browser, or looking at pnd-files?
  if ( cat -> fspath ) {

    if ( s1 == s2 ) {
      return ( 0 ); // equal

    } else if ( s1 -> ref -> object_type == pnd_object_type_directory &&
		s2 -> ref -> object_type == pnd_object_type_directory )
    {
      // both are directories, be nice
      return ( strcmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
    } else if ( s1 -> ref -> object_type == pnd_object_type_directory &&
		s2 -> ref -> object_type != pnd_object_type_directory )
    {
      return ( -1 ); // dir on the left is earlier than file on the right
    } else if ( s1 -> ref -> object_type != pnd_object_type_directory &&
		s2 -> ref -> object_type == pnd_object_type_directory )
    {
      return ( 1 ); // dir on the right is earlier than file on the left
    } else {
      // file on file
      return ( strcmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
    }

  }

  return ( strcasecmp ( s1 -> ref -> title_en, s2 -> ref -> title_en ) );
}

void category_dump ( void ) {

  // WHY AREN'T I SORTING ON INSERT?

  // dump
  mm_category_t *iter = pnd_box_get_head ( m_categories );
  unsigned int counter = 0;

  while ( iter ) {

    pnd_log ( PND_LOG_DEFAULT, "Category %u: '%s' * %u\n", counter, iter -> catname, iter -> refcount );
    mm_appref_t *ar = iter -> refs;

    while ( ar ) {
      pnd_log ( PND_LOG_DEFAULT, "  Appref %s\n", IFNULL(ar -> ref -> title_en,"No Name") );
      ar = ar -> next;
    }

    iter = pnd_box_get_next ( iter );
    counter++;
  }

  return;
}

void category_freeall ( void ) {
  mm_category_t *c, *cnext;
  mm_appref_t *iter, *next;

  c = pnd_box_get_head ( m_categories );

  while ( c ) {
    cnext = pnd_box_get_next ( c );

    // wipe 'em
    iter = c -> refs;

    while ( iter ) {
      next = iter -> next;
      free ( iter );
      iter = next;
    }

    c -> refs = NULL;

    if ( c -> catname ) {
      free ( c -> catname );
      c -> catname = NULL;
    }

    if ( c -> fspath ) {
      free ( c -> fspath );
      c -> fspath = NULL;
    }

    pnd_box_delete_node ( m_categories, c );

    // next
    c = cnext;
  }

  return;
}

unsigned char category_map_setup ( void ) {

  char *searchpath = pnd_box_get_head ( g_conf );

  if ( ! searchpath ) {
    return ( 0 );
  }

  // look through conf for useful keys
  while ( searchpath ) {
    char *k = pnd_box_get_key ( searchpath );

    // does this key look like a category mapping key?
    if ( strncasecmp ( k, "categories.@", 12 ) == 0 ) {
      k += 12;

      // iterate across 'words' in v, assigning catmaps to them
      SEARCHCHUNK_PRE
      {
	//pnd_log ( pndn_debug, "target(%s) from(%s)\n", k, buffer );

	category_push ( k, NULL /* parent cat */, NULL, 0, NULL /* fspath */, 1 );
	g_catmaps [ g_catmapcount ].target = pnd_box_find_by_key ( m_categories, k );
	g_catmaps [ g_catmapcount ].from = strdup ( buffer );
	g_catmapcount++;

      }
      SEARCHCHUNK_POST

    } // if key looks like catmap

    searchpath = pnd_box_get_next ( searchpath );
  } // while each conf key

  return ( 1 );
}

mm_category_t *category_map_query ( char *cat ) {
  unsigned char i;

  for ( i = 0; i < g_catmapcount; i++ ) {
    if ( strcasecmp ( g_catmaps [ i ].from, cat ) == 0 ) {
      return ( g_catmaps [ i ].target );
    }
  }

  return ( NULL );
}

unsigned char category_meta_push ( char *catname, char *parentcatname, pnd_disco_t *app, pnd_conf_handle ovrh, unsigned char visiblep ) {
  mm_category_t *cat;
#if 0 // prepending
  char catnamebuffer [ 512 ] = "";
#endif

  if ( ! catname ) {
    return ( 1 ); // fine, just nada
  }

  //fprintf ( stderr, "meta push: '%s'\n", catname );

  // push bad categories into Other (if we're not targeting All right now)
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.good_cats_only", 1 ) ) {

    // don't audit All
    if ( strncmp ( catname, "All ", 4 ) != 0 ) {

      // if this is a parent cat..
      if ( catname && ! parentcatname ) {

	// if bad, shove it to Other
	if ( ! freedesktop_check_cat ( catname ) ) {
	  parentcatname = NULL;
	  catname = BADCATNAME;
	  visiblep = cat_is_visible ( g_conf, catname );
	}

      } else if ( catname && parentcatname ) {
	// this is a subcat

	// if parent is bad, then we probably already pushed it over, so don't do it again.
	// if its parent is okay, but subcat is bad, push it to other. (ie: lets avoid duplication in Other)
	if ( ! freedesktop_check_cat ( parentcatname ) ) {
	  // skip
	  return ( 1 );

	} else if ( ! freedesktop_check_cat ( catname ) ) {
	  parentcatname = NULL;
	  catname = BADCATNAME;
	  visiblep = cat_is_visible ( g_conf, catname );
	}

      } // parent or child cat?

    } // not All

  } // good cats only?

  // if invisible, and a parent category name is known, prepend it for ease of use
#if 0 // prepending
  if ( ! visiblep && parentcatname ) {
    snprintf ( catnamebuffer, 500, "%s.%s", parentcatname, catname );
    catname = catnamebuffer;
  }
#endif

  // do we honour cat mapping at all?
  if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_on", 0 ) ) {

    // is this guy mapped?
    cat = category_map_query ( catname );

    if ( cat ) {
      category_push ( cat -> catname, parentcatname /* parent cat */, app, ovrh, NULL /* fspath */, visiblep );
      goto meta_done;
    }

    // not mapped.. but default?
    if ( pnd_conf_get_as_int_d ( g_conf, "categories.map_default_on", 0 ) ) {
      char *def = pnd_conf_get_as_char ( g_conf, "categories.map_default_cat" );
      if ( def ) {
	category_push ( def, parentcatname /* parent cat */, app, ovrh, NULL /* fspath */, visiblep );
	goto meta_done;
      }
    }

  } // cat map is desired?

  // not default, just do it
  category_push ( catname, parentcatname /* parent cat */, app, ovrh, NULL /* fspath */, visiblep );

  // if subcats as folders, then lets just make up a dummy app that pretends to be a folder,
  // and stuff it into the parent cat
  if ( parentcatname && pnd_conf_get_as_int_d ( g_conf, "tabs.subcat_as_folders", 1 ) ) {

    // it is implicit that since we're talking parentcat, its already been created in a previous call
    // therefore, we need to..
    // i) find the parent cat
    // ii) ensure it already has a faux-disco container
    // iii) ensure that disco container doesn't already contain a disco-entry for this subcat
    // iv) create the dummy app folder by pushing the disco into the apprefs as normal
    // v) create a dummy '..' for going back up, in the child

    mm_category_t *pcat = pnd_box_find_by_key ( m_categories, parentcatname );

    if ( ! pcat -> disco ) {
      pcat -> disco = pnd_box_new ( pcat -> catname );
    }

    // if this subcat is already in the faux-disco list, then its probably already
    // handled so we needn't concern ourselves anymore. If not, then we can
    // create it and push it into the parent as a new 'app'
    pnd_disco_t *disco = pnd_box_find_by_key ( pcat -> disco, catname );

    if ( ! disco ) {

      disco = pnd_box_allocinsert ( pcat -> disco, catname, sizeof(pnd_disco_t) );
      if ( disco ) {

	// create the subcat faux-disco entry, and register into parent cat
	char uid [ 30 ];
	sprintf ( uid, "%p", catname );

	disco -> unique_id = strdup ( uid );
	if ( strchr ( catname, '.' ) ) {
	  disco -> title_en = strdup ( strchr ( catname, '.' ) + 1 );
	} else {
	  disco -> title_en = strdup ( catname );
	}
	disco -> object_flags = PND_DISCO_GENERATED;
	disco -> object_type = pnd_object_type_directory; // suggest to Grid that its a dir
	disco -> object_path = strdup ( catname );

	category_push ( parentcatname, NULL /* parent cat */, disco, 0 /*ovrh*/, NULL /* fspath */, 1 /* visible */ );

	// create .. faux-disco entry into child cat
	disco = pnd_box_allocinsert ( pcat -> disco, catname, sizeof(pnd_disco_t) );

	sprintf ( uid, "%p", uid );

	disco -> unique_id = strdup ( uid );
	disco -> title_en = strdup ( ".." );
	disco -> object_flags = PND_DISCO_GENERATED;
	disco -> object_type = pnd_object_type_directory; // suggest to Grid that its a dir

	category_push ( catname, parentcatname /* parent cat */, disco, 0 /*ovrh*/, NULL /* fspath */, 1 /* visible */ );

      } // making faux disco entries

    } // disco already exist?

  } // subcat as folder?

  // hack :(
 meta_done:

  //fprintf ( stderr, "cat meta-push : vis[%30s,%d b] : tally; vis %d invis %d\n", catname, visiblep, g_categorycount, _categories_inviscount );

  return ( 1 );
}

unsigned char category_fs_restock ( mm_category_t *cat ) {

  if ( ! cat -> fspath ) {
    return ( 1 ); // not a filesystem browser tab
  }

  // clear any existing baggage
  //

  // apprefs
  mm_appref_t *iter = cat -> refs, *next;
  while ( iter ) {
    next = iter -> next;
    free ( iter );
    iter = next;
  }
  cat -> refs = NULL;

  // discos
  if ( cat -> disco ) {
    pnd_disco_t *p = pnd_box_get_head ( cat -> disco );
    pnd_disco_t *n;
    while ( p ) {
      n = pnd_box_get_next ( p );
      pnd_disco_destroy ( p );
      p = n;
    }
    pnd_box_delete ( cat -> disco );
  }

  // rescan the filesystem
  //

  //pnd_log ( pndn_debug, "Restocking cat %s with path %s\n", cat -> catname, cat -> fspath );
  DIR *d;

  if ( ( d = opendir ( cat -> fspath ) ) ) {
    struct dirent *de = readdir ( d );

    pnd_disco_t *disco;
    char uid [ 100 ];

    cat -> disco = pnd_box_new ( cat -> catname );

    while ( de ) {

      struct stat buffy;
      char fullpath [ PATH_MAX ];
      sprintf ( fullpath, "%s/%s", cat -> fspath, de -> d_name );
      int statret = stat ( fullpath, &buffy );

      // if file is executable somehow or another
      if ( statret == 0 &&
	   buffy.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)
	 )
      {
	// determine unique-id
	sprintf ( uid, "%d", (int) de -> d_ino );
	disco = NULL;

	switch ( de -> d_type ) {

	case DT_DIR:
	  if ( strcmp ( de -> d_name, "." ) == 0 ) {
	    // ignore ".", but ".." is fine
	  } else {
	    disco = pnd_box_allocinsert ( cat -> disco, uid, sizeof(pnd_disco_t) );
	    disco -> object_type = pnd_object_type_directory; // suggest to Grid that its a dir
	  }
	  break;
	case DT_UNKNOWN:
	case DT_REG:
	  disco = pnd_box_allocinsert ( cat -> disco, uid, sizeof(pnd_disco_t) );
	  disco -> object_type = pnd_object_type_unknown; // suggest to Grid that its a file
	  break;

	} // switch

	// found a directory or executable?
	if ( disco ) {
	  // register with current category
	  disco -> unique_id = strdup ( uid );
	  disco -> title_en = strdup ( de -> d_name );
	  disco -> object_flags = PND_DISCO_GENERATED;
	  disco -> object_path = strdup ( cat -> fspath );
	  disco -> object_filename = strdup ( de -> d_name );
	  category_push ( cat -> catname, NULL /* parent cat */, disco, 0 /* no ovr */, NULL /* fspath already set */, 1 /* visible */ );
	  // if a override icon exists, cache it up
	  cache_icon ( disco, pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_width", 50 ),
		       pnd_conf_get_as_int_d ( g_conf, "grid.icon_max_height", 50 ) );
	}

      } // stat

      // next
      de = readdir ( d );
    }

    closedir ( d );
  }

  return ( 1 );
}

static int catname_cmp ( const void *p1, const void *p2 ) {
  //mm_category_t *c1 = (mm_category_t*) p1;
  //mm_category_t *c2 = (mm_category_t*) p2;
  mm_category_t *c1 = *( (mm_category_t**) p1 );
  mm_category_t *c2 = *( (mm_category_t**) p2 );

  if ( ( isalnum ( c1 -> catname [ 0 ] ) ) && ( ! isalnum ( c1 -> catname [ 1 ] ) ) ) {
    return ( -1 );
  } else if ( ( ! isalnum ( c1 -> catname [ 0 ] ) ) && ( isalnum ( c1 -> catname [ 1 ] ) ) ) {
    return ( 1 );
  } else if ( ( ! isalnum ( c1 -> catname [ 0 ] ) ) && ( ! isalnum ( c1 -> catname [ 1 ] ) ) ) {
    return ( 0 );
  }

  int i = strcasecmp ( c1 -> catname, c2 -> catname );
  //printf ( "cat name compare %p %s to %p %s = %d\n", p1, c1 -> catname, p2, c2 -> catname, i );

  return ( i );
}

void category_sort ( void ) {
  // we probably don't want to sort tab categories, since the user may have specified an ordering
  // But we can sort invisi-cats, to make them easier to find, and ordered by parent category

#if 0
  qsort ( _categories_invis, _categories_inviscount, sizeof(mm_category_t), catname_cmp );
#endif

#if 1
  qsort ( g_categories, g_categorycount, sizeof(mm_category_t*), catname_cmp );
#endif

  return;
}

void category_publish ( unsigned int filter_mask, char *param ) {
  unsigned char interested;

  // clear published categories
  memset ( g_categories, '\0', sizeof(mm_category_t*) * MAX_CATS );
  g_categorycount = 0;

  // figure out the start
  mm_category_t *iter = pnd_box_get_head ( m_categories );

  // for each category we know...
  while ( iter ) {

    interested = 0;

    // is this category desired?
    if ( filter_mask == CFALL ) {
      interested = 1;
    } else if ( filter_mask == CFBYNAME ) {
      if ( strcasecmp ( iter -> catname, param ) == 0 ) {
	interested = 1;
      }
    } else if ( iter -> catflags == filter_mask ) {
      interested = 1;
    } // if

    if ( interested ) {
      // set us up the bomb; notice that we're just duplicating the pointers, not making
      // any new data here; none of this should ever be free'd!
      g_categories [ g_categorycount ] = iter;
      g_categorycount++;
    }

    // next
    iter = pnd_box_get_next ( iter );
  }

  // dump
#if 0
  unsigned int i;
  for ( i = 0; i < g_categorycount; i++ ) {
    printf ( "Unsorted cat %d %p: %s\n", i, &(g_categories [ i ]), g_categories [ i ] -> catname );
  }
#endif

  // sort published categories
  category_sort();

  return;
}

unsigned int category_count ( unsigned int filter_mask ) {
  mm_category_t *iter = pnd_box_get_head ( m_categories );
  unsigned int count = 0;

  // for each category we know...
  while ( iter ) {

    // is this category desired?
    if ( iter -> catflags == filter_mask ) {
      count++;
    } // if

    // next
    iter = pnd_box_get_next ( iter );
  }

  return ( count );
}

int category_index ( char *catname ) {
  unsigned char i;
 
  for ( i = 0; i < g_categorycount; i++ ) {
 
    if ( strcasecmp ( g_categories [ i ] -> catname, catname ) == 0 ) {
      return ( i );
    }
 
  }
 
  return ( -1 );
}
