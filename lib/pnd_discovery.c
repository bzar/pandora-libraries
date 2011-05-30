
#include <stdio.h> /* for FILE etc */
#include <stdlib.h> /* for malloc */
#include <unistd.h> /* for unlink */
#include <limits.h> /* for PATH_MAX */
#include <sys/types.h>
#include <sys/stat.h>

#define __USE_GNU /* for strcasestr */
#include <string.h> /* for making ftw.h happy */

#define _XOPEN_SOURCE 500
#define __USE_XOPEN_EXTENDED
#define _GNU_SOURCE
#include <ftw.h> /* for nftw, tree walker */

#include "pnd_container.h"
#include "pnd_pxml.h"
#include "pnd_discovery.h"
#include "pnd_pathiter.h"
#include "pnd_apps.h"
#include "pnd_pndfiles.h"
#include "pnd_logger.h"
#include "pnd_conf.h"

// need these 'globals' due to the way nftw and ftw work :/
static pnd_box_handle disco_box;
static char *disco_overrides = NULL;

void pnd_disco_destroy ( pnd_disco_t *p ) {
  if ( p -> package_id ) {     free ( p -> package_id);   }
  if ( p -> package_version_major ) { free ( p -> package_version_major ); }
  if ( p -> package_version_minor ) { free ( p -> package_version_minor ); }  
  if ( p -> package_version_release ) { free ( p -> package_version_release ); }  
  if ( p -> package_version_build ) { free ( p -> package_version_build ); }    
  if ( p -> title_en ) {       free ( p -> title_en );    }
  if ( p -> unique_id ) {      free ( p -> unique_id );   }
  if ( p -> appdata_dirname ) { free ( p -> appdata_dirname );   }
  if ( p -> icon )     {       free ( p -> icon );        }
  if ( p -> exec )     {       free ( p -> exec );        }
  if ( p -> execargs ) {       free ( p -> execargs );    }
  if ( p -> clockspeed ) {     free ( p -> clockspeed );  }
  if ( p -> startdir ) {       free ( p -> startdir );    }
  if ( p -> option_no_x11 ) {  free ( p -> option_no_x11 );  }
  if ( p -> main_category ) {  free ( p -> main_category );  }
  if ( p -> main_category1 ) { free ( p -> main_category1 ); }
  if ( p -> main_category2 ) { free ( p -> main_category2 ); }
  if ( p -> alt_category ) {   free ( p -> alt_category );   }
  if ( p -> alt_category1 ) {  free ( p -> alt_category1 );  }
  if ( p -> alt_category2 ) {  free ( p -> alt_category2 );  }
  if ( p -> mkdir_sp )      {  free ( p -> mkdir_sp );       }
  if ( p -> info_name )     {  free ( p -> info_name );       }
  if ( p -> info_type )     {  free ( p -> info_type );       }
  if ( p -> info_filename ) {  free ( p -> info_filename );       }
  if ( p -> version_major ) {  free ( p -> version_major );    }
  if ( p -> version_minor ) {  free ( p -> version_minor );    } 
  if ( p -> version_release ) {free ( p -> version_release );  }
  if ( p -> version_build ) {  free ( p -> version_build );    } 
  return;
}

static int pnd_disco_callback ( const char *fpath, const struct stat *sb,
				int typeflag, struct FTW *ftwbuf )
{
  unsigned char valid = pnd_object_type_unknown;
  pnd_pxml_handle pxmlh = 0;
  pnd_pxml_handle *pxmlapps = NULL;
  pnd_pxml_handle *pxmlappiter;
  unsigned int pxml_close_pos = 0;
  unsigned char logit = pnd_log_do_buried_logging();

  if ( logit ) {
    pnd_log ( PND_LOG_DEFAULT, "disco callback encountered '%s'\n", fpath );
  }

  // PXML.xml is a possible application candidate (and not a dir named PXML.xml :)
  if ( typeflag & FTW_D ) {
    if ( logit ) {
      pnd_log ( PND_LOG_DEFAULT, " .. is dir, skipping\n" );
    }
    if ( ftwbuf -> level >= pathiter_depthlimit ) {
      return ( FTW_SKIP_SUBTREE );
    }
    return ( FTW_CONTINUE ); // skip directories and other non-regular files
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
    if ( logit ) {
      pnd_log ( PND_LOG_DEFAULT, " .. bad filename, skipping\n" );
    }
    return ( FTW_CONTINUE );
  }

  // potentially a valid application
  if ( valid == pnd_object_type_directory ) {
    // Plaintext PXML file
    //printf ( "PXML: disco callback encountered '%s'\n", fpath );

    // pick up the PXML if we can
    pxmlapps = pnd_pxml_fetch ( (char*) fpath );

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
      return ( FTW_CONTINUE ); // pnd or not, but not to spec. Pwn'd the pnd?
    }

    // accrue it into a buffer
    if ( ! pnd_pnd_accrue_pxml ( f, pxmlbuf, 32 * 1024 ) ) {
      fclose ( f );
      return ( FTW_CONTINUE );
    }

    //printf ( "buffer is %s\n", pxmlbuf );
    //fflush ( stdout );

#if 1 // icon
    // for convenience, lets skip along past trailing newlines/CR's in hopes of finding icon data?
    {
      char pngbuffer [ 16 ]; // \211 P N G \r \n \032 \n
      pngbuffer [ 0 ] = 137;      pngbuffer [ 1 ] = 80;      pngbuffer [ 2 ] = 78;      pngbuffer [ 3 ] = 71;
      pngbuffer [ 4 ] = 13;       pngbuffer [ 5 ] = 10;       pngbuffer [ 6 ] = 26;      pngbuffer [ 7 ] = 10;

      unsigned char padtests = 20;
      unsigned int padstart = ftell ( f );

      // seek back 10 (should be back into the /PXML> part) to catch any appending-icon-no-line-endings funny business
      fseek ( f, -10, SEEK_CUR );

      while ( padtests ) {

	if ( fread ( pngbuffer + 8, 8, 1, f ) == 1 ) {
	  if ( memcmp ( pngbuffer, pngbuffer + 8, 8 ) == 0 ) {
	    pxml_close_pos = ftell ( f ) - 8;
	    break;
	  } // if
	  fseek ( f, -7, SEEK_CUR ); // seek back 7 (so we're 1 further than we started, since PNG header is 8b)
	} // if read

	padtests --;
      } // while

      if ( ! padtests ) {
	// no icon found, so back to where we started looking
	fseek ( f, padstart, SEEK_SET );
      }

    } // icon
#endif

    // by now, we have <PXML> .. </PXML>, try to parse..
    pxmlapps = pnd_pxml_fetch_buffer ( (char*) fpath, pxmlbuf );

    // done with file
    fclose ( f );

  }

  // pxmlh is useful?
  if ( ! pxmlapps ) {
    return ( FTW_CONTINUE ); // continue tree walk
  }

  // for ovr-file
  char ovrfile [ PATH_MAX ];
  pnd_box_handle ovrh = 0; // 0 didn't try, -1 tried and failed, >0 tried and got

  // iterate across apps in the PXML
  pxmlappiter = pxmlapps;
  while ( 1 ) {
    pxmlh = *pxmlappiter;
    pxmlappiter++;

    if ( ! pxmlh ) {
      break; // all done
    }

    // check for validity and add to resultset if it looks executable
    if ( pnd_is_pxml_valid_app ( pxmlh ) ) {
      pnd_disco_t *p;
      char *fixpxml;
      char *z;

      //pnd_log ( PND_LOG_DEFAULT, "Setting up discovered app %u\n", ((pnd_pxml_t*) pxmlh) -> subapp_number );

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

      // subapp-number
      p -> subapp_number = ((pnd_pxml_t*) pxmlh) -> subapp_number;

      // png icon path
      p -> pnd_icon_pos = pxml_close_pos;

      // type
      p -> object_type = valid;

      // PXML fields
      if ( pnd_pxml_get_package_id ( pxmlh ) ) {
	p -> package_id = strdup ( pnd_pxml_get_package_id ( pxmlh ) );
      }
      if ( pnd_pxml_get_package_version_major ( pxmlh ) ) {
	p -> package_version_major = strdup ( pnd_pxml_get_package_version_major ( pxmlh ) );
      }     
      if ( pnd_pxml_get_package_version_minor ( pxmlh ) ) {
   p -> package_version_minor = strdup ( pnd_pxml_get_package_version_minor ( pxmlh ) );
      } 
      if ( pnd_pxml_get_package_version_release ( pxmlh ) ) {
   p -> package_version_release = strdup ( pnd_pxml_get_package_version_release ( pxmlh ) );
      }
      if ( pnd_pxml_get_package_version_build ( pxmlh ) ) {
   p -> package_version_build = strdup ( pnd_pxml_get_package_version_build ( pxmlh ) );
      }          
      if ( pnd_pxml_get_app_name_en ( pxmlh ) ) {
	p -> title_en = strdup ( pnd_pxml_get_app_name_en ( pxmlh ) );
      }
      if ( pnd_pxml_get_description_en ( pxmlh ) ) {
	p -> desc_en = strdup ( pnd_pxml_get_description_en ( pxmlh ) );
      }
      if ( pnd_pxml_get_icon ( pxmlh ) ) {
	p -> icon = strdup ( pnd_pxml_get_icon ( pxmlh ) );
      }
      if ( pnd_pxml_get_exec ( pxmlh ) ) {
	p -> exec = strdup ( pnd_pxml_get_exec ( pxmlh ) );
      }
      if ( pnd_pxml_get_execargs ( pxmlh ) ) {
	p -> execargs = strdup ( pnd_pxml_get_execargs ( pxmlh ) );
      }
      if ( pnd_pxml_get_exec_option_no_x11 ( pxmlh ) ) {
	p -> option_no_x11 = strdup ( pnd_pxml_get_exec_option_no_x11 ( pxmlh ) );
      }
      if ( pnd_pxml_get_unique_id ( pxmlh ) ) {
	p -> unique_id = strdup ( pnd_pxml_get_unique_id ( pxmlh ) );
      }
      if ( pnd_pxml_get_appdata_dirname ( pxmlh ) ) {
	p -> appdata_dirname = strdup ( pnd_pxml_get_appdata_dirname ( pxmlh ) );
      }
      if ( pnd_pxml_get_clockspeed ( pxmlh ) ) {
	p -> clockspeed = strdup ( pnd_pxml_get_clockspeed ( pxmlh ) ); 
      }
      if ( pnd_pxml_get_startdir ( pxmlh ) ) {
	p -> startdir = strdup ( pnd_pxml_get_startdir ( pxmlh ) ); 
      }
      // category kruft
      if ( pnd_pxml_get_main_category ( pxmlh ) ) {
	p -> main_category = strdup ( pnd_pxml_get_main_category ( pxmlh ) );
      }
      if ( pnd_pxml_get_subcategory1 ( pxmlh ) ) {
	p -> main_category1 = strdup ( pnd_pxml_get_subcategory1 ( pxmlh ) );
      }
      if ( pnd_pxml_get_subcategory2 ( pxmlh ) ) {
	p -> main_category2 = strdup ( pnd_pxml_get_subcategory2 ( pxmlh ) );
      }
      if ( pnd_pxml_get_altcategory ( pxmlh ) ) {
	p -> alt_category = strdup ( pnd_pxml_get_altcategory ( pxmlh ) );
      }
      if ( pnd_pxml_get_altsubcategory1 ( pxmlh ) ) {
	p -> alt_category1 = strdup ( pnd_pxml_get_altsubcategory1 ( pxmlh ) );
      }
      if ( pnd_pxml_get_altsubcategory2 ( pxmlh ) ) {
	p -> alt_category2 = strdup ( pnd_pxml_get_altsubcategory2 ( pxmlh ) );
      }
      // preview pics
      if ( ( z = pnd_pxml_get_previewpic1 ( pxmlh ) ) ) {
	p -> preview_pic1 = strdup ( z );
      }
      if ( ( z = pnd_pxml_get_previewpic2 ( pxmlh ) ) ) {
	p -> preview_pic2 = strdup ( z );
      }
      // version
      if ( pnd_pxml_get_version_major ( pxmlh ) ) {
   p -> version_major = strdup ( pnd_pxml_get_version_major ( pxmlh ) );
      }
      if ( pnd_pxml_get_version_minor ( pxmlh ) ) {
   p -> version_minor = strdup ( pnd_pxml_get_version_minor ( pxmlh ) );
      }     
      if ( pnd_pxml_get_version_release ( pxmlh ) ) {
   p -> version_release = strdup ( pnd_pxml_get_version_release ( pxmlh ) );
      }   
      if ( pnd_pxml_get_version_build ( pxmlh ) ) {
   p -> version_build = strdup ( pnd_pxml_get_version_build ( pxmlh ) );
      }   
      // mkdirs
      if ( pnd_pxml_get_mkdir ( pxmlh ) ) {
	p -> mkdir_sp = strdup ( pnd_pxml_get_mkdir ( pxmlh ) );
      }
      // info
      if ( pnd_pxml_get_info_src ( pxmlh ) ) {
	p -> info_filename = strdup ( pnd_pxml_get_info_src ( pxmlh ) );
      }
      if ( pnd_pxml_get_info_name ( pxmlh ) ) {
	p -> info_name = strdup ( pnd_pxml_get_info_name ( pxmlh ) );
      }
      if ( pnd_pxml_get_info_type ( pxmlh ) ) {
	p -> info_type = strdup ( pnd_pxml_get_info_type ( pxmlh ) );
      }

      // look for any PXML overrides, if requested
      if ( disco_overrides ) {
	pnd_pxml_merge_override ( pxmlh, disco_overrides );
      }

      // handle ovr overrides
      // try to load a same-path-as-pnd override file
      if ( ovrh == 0 ) {
	sprintf ( ovrfile, "%s/%s", p -> object_path, p -> object_filename );
	fixpxml = strcasestr ( ovrfile, PND_PACKAGE_FILEEXT );
	if ( fixpxml ) {
	  strcpy ( fixpxml, PXML_SAMEPATH_OVERRIDE_FILEEXT );
	  struct stat statbuf;
	  if ( stat ( ovrfile, &statbuf ) == 0 ) {
	    ovrh = pnd_conf_fetch_by_path ( ovrfile );

	    if ( ! ovrh ) {
	      // couldn't pull conf out of file, so don't try again
	      ovrh = (void*)(-1);
	    }

	  } else {
	    ovrh = (void*)(-1); // not found, don't try again
	  } // stat
	} // can find .pnd
      } // tried ovr yet?

      // is ovr file open?
      if ( ovrh != 0 && ovrh != (void*)(-1) ) {
	// pull in appropriate values
	char key [ 100 ];
	char *v;

	// set the flag regardless, so its for all subapps
	p -> object_flags |= PND_DISCO_FLAG_OVR;

	// title
	snprintf ( key, 100, "Application-%u.title", p -> subapp_number );
	if ( ( v = pnd_conf_get_as_char ( ovrh, key ) ) ) {
	  if ( p -> title_en ) {
	    free ( p -> title_en );
	  }
	  p -> title_en = strdup ( v );
	}

	// clockspeed
	snprintf ( key, 100, "Application-%u.clockspeed", p -> subapp_number );
	if ( ( v = pnd_conf_get_as_char ( ovrh, key ) ) ) {
	  if ( p -> clockspeed ) {
	    free ( p -> clockspeed );
	  }
	  p -> clockspeed = strdup ( v );
	}

	// appdata dirname
	snprintf ( key, 100, "Application-%u.appdata", p -> subapp_number );
	if ( ( v = pnd_conf_get_as_char ( ovrh, key ) ) ) {
	  if ( p -> appdata_dirname ) {
	    free ( p -> appdata_dirname );
	  }
	  p -> appdata_dirname = strdup ( v );
	}

	// categories
	snprintf ( key, 100, "Application-%u.maincategory", p -> subapp_number );
	if ( ( v = pnd_conf_get_as_char ( ovrh, key ) ) ) {
	  if ( p -> main_category ) {
	    free ( p -> main_category );
	  }
	  p -> main_category = strdup ( v );
	}
	snprintf ( key, 100, "Application-%u.maincategorysub1", p -> subapp_number );
	if ( ( v = pnd_conf_get_as_char ( ovrh, key ) ) ) {
	  if ( p -> main_category1 ) {
	    free ( p -> main_category1 );
	    p -> main_category1 = NULL;
	  }
	  if ( strcasecmp ( v, "NoSubcategory" ) != 0 ) {
	    p -> main_category1 = strdup ( v );
	  }
	}

      } // got ovr conf loaded?

    } else {
      //printf ( "Invalid PXML; skipping.\n" );
    }

    // ditch pxml
    pnd_pxml_delete ( pxmlh );

  } // while pxmlh is good

  // free up ovr
  if ( ovrh != 0 && ovrh != (void*)(-1) ) {
    pnd_box_delete ( ovrh );
  }

  // free up the applist
  free ( pxmlapps );

  return ( FTW_CONTINUE ); // continue the tree walk
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
	   FTW_PHYS | FTW_ACTIONRETVAL );   // do not follow symlinks

  }
  SEARCHPATH_POST

  // return whatever we found, or NULL if nada
  if ( ! pnd_box_get_head ( disco_box ) ) {
    pnd_box_delete ( disco_box );
    disco_box = NULL;
  }

  return ( disco_box );
}

pnd_box_handle pnd_disco_file ( char *path, char *filename ) {
  struct stat statbuf;

  // set up container
  disco_overrides = NULL;
  disco_box = pnd_box_new ( "discovery" );

  // path
  char fullpath [ PATH_MAX ];
  sprintf ( fullpath, "%s/%s", path, filename );

  // fake it
  if ( stat ( fullpath, &statbuf ) < 0 ) {
    return ( 0 );
  }

  struct FTW ftw;
  ftw.base = strlen ( path );
  ftw.level = 0;

  pnd_disco_callback ( fullpath, &statbuf, FTW_F, &ftw );

  // return whatever we found, or NULL if nada
  if ( ! pnd_box_get_head ( disco_box ) ) {
    pnd_box_delete ( disco_box );
    disco_box = NULL;
  }

  return ( disco_box );
}
