
#ifndef h_pnd_pathiter_h
#define h_png_pathiter_h

// man don'y you wish it was python, perl or c++ right about now?
// perl: foreach i ( split ( ':', path ) ) would be sauce right now

// this really should be replaced by a function pair .. one to
// start a new search and one to go to next, bailing when done. Maybe
// a state struct. Like we have time. OR perhaps just a single
// func with a callback. Whatever.

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

#endif
