
#ifndef h_pnd_pathiter_h
#define h_pnd_pathiter_h

// man don'y you wish it was python, perl or c++ right about now?
// perl: foreach i ( split ( ':', path ) ) would be sauce right now

// for wordexp(); nice thign to have bundled into libc!
#include <stdio.h>
#include <stdlib.h>
#include <wordexp.h>
// for stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h> // for isdigit()

// this really should be replaced by a function pair .. one to
// start a new search and one to go to next, bailing when done. Maybe
// a state struct. Like we have time. OR perhaps just a single
// func with a callback. Whatever.


// will be set to 0xFFFF for no limit currently set, or a smaller number
// if that is the depth limit
unsigned int pathiter_depthlimit;


#if 1 // globbing is performed

// only iterates actual existing paths; if you want to return
// non-matching paths, see below

#define SEARCHPATH_PRE                            \
  char *end, *colon;                              \
  char *head = searchpath;                        \
  char chunk [ FILENAME_MAX ];                    \
  char *lt;                                       \
                                                  \
  /*fprintf ( stderr, "sp %s\n", searchpath );*/  \
                                                  \
  while ( 1 ) {                                   \
    colon = strchr ( head, ':' );                 \
    end = strchr ( head, '\0' );                  \
                                                  \
    if ( colon && colon < end ) {                 \
      memset ( chunk, '\0', FILENAME_MAX );       \
      strncpy ( chunk, head, colon - head );      \
    } else {                                      \
      strncpy ( chunk, head, FILENAME_MAX - 1 );  \
    }                                             \
                                                  \
    pathiter_depthlimit = 0xFFFF;		  \
                                                  \
    if ( ( lt = strchr ( chunk, '<' ) ) ) {       \
      if ( isdigit ( *(lt+1) ) ) {                \
        pathiter_depthlimit = atoi ( lt + 1 );    \
      }                                           \
      *lt = '\0';                                 \
    }                                             \
                                                  \
    /*fprintf ( stderr, "-> chunk %s limit %d\n", chunk, pathiter_depthlimit );*/ \
                                                  \
    struct stat statbuf;                          \
    wordexp_t _p;                                 \
    char **_w;                                    \
    int _i;                                       \
    char buffer [ FILENAME_MAX ];                 \
                                                  \
    if ( wordexp ( chunk, &_p, 0 ) != 0 ) {       \
      /* ignore this chunk I guess.. */           \
    } else {                                      \
      _w = _p.we_wordv;                           \
                                                  \
      for ( _i=0; _i < _p.we_wordc; _i++ ) {      \
        strcpy ( buffer, _w [ _i ] );             \
	/*fprintf ( stderr, "glob %s\n", buffer );*/	\
	if ( ( stat ( buffer, &statbuf ) == 0 )   \
	  && ( S_ISDIR(statbuf.st_mode) ) )	  \
        { /* user code */

#define SEARCHPATH_POST                           \
        } /* user code */                         \
      } /* for each glob result */	          \
      wordfree ( &_p );                           \
    } /* if wordexp succeeds */		          \
    /* next search path */			  \
    if ( colon && colon < end ) {                 \
      head = colon + 1;                           \
    } else {                                      \
      break; /* done! */                          \
    }                                             \
                                                  \
  } // while


// the following will return even non-matching chunks, but is not doing wordexp() expansion on it

#define SEARCHCHUNK_PRE                           \
  char *end, *colon;                              \
  char *head = searchpath;                        \
  char chunk [ FILENAME_MAX ];                    \
                                                  \
  /*fprintf ( stderr, "sp %s\n", searchpath );*/  \
                                                  \
  while ( 1 ) {                                   \
    colon = strchr ( head, ':' );                 \
    end = strchr ( head, '\0' );                  \
                                                  \
    if ( colon && colon < end ) {                 \
      memset ( chunk, '\0', FILENAME_MAX );       \
      strncpy ( chunk, head, colon - head );      \
    } else {                                      \
      strncpy ( chunk, head, FILENAME_MAX - 1 );  \
    }                                             \
                                                  \
    /*fprintf ( stderr, "-> %s\n", chunk ); */    \
                                                  \
    char buffer [ FILENAME_MAX ];                 \
                                                  \
    strcpy ( buffer, chunk );  			  \
    /*fprintf ( stderr, "glob %s\n", buffer );*/  \
    { /* user code */

#define SEARCHCHUNK_POST                          \
    } /* user code */				  \
    /* next search path */			  \
    if ( colon && colon < end ) {                 \
      head = colon + 1;                           \
    } else {                                      \
      break; /* done! */                          \
    }                                             \
                                                  \
  } // while


#endif // globbing is done


#if 0 // deprecated simple (no globbing/expansion)

#define SEARCHPATH_PRE                            \
  char *end, *colon;                              \
  char *head = searchpath;                        \
  char buffer [ FILENAME_MAX ];                   \
                                                  \
  while ( 1 ) {                                   \
    colon = strchr ( head, ':' );                 \
    end = strchr ( head, '\0' );                  \
                                                  \
    if ( colon && colon < end ) {                 \
      memset ( buffer, '\0', FILENAME_MAX );      \
      strncpy ( buffer, head, colon - head );     \
    } else {                                      \
      strncpy ( buffer, head, FILENAME_MAX - 1 ); \
    }                                             \
                                                  \
    //printf ( "Path to search: '%s'\n", buffer );

#define SEARCHPATH_POST                           \
    /* next search path */			  \
    if ( colon && colon < end ) {                 \
      head = colon + 1;                           \
    } else {                                      \
      break; /* done! */                          \
    }                                             \
                                                  \
  } // while

#endif // deprecated simple


#endif // #ifndef
