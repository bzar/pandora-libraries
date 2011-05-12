
#include <stdarg.h> // va-args
#include <stdlib.h> // malloc/free
#include <string.h> // strdup
#include <time.h>
#include "pnd_logger.h"

static char *log_pretext = NULL;
static unsigned char log_filterlevel = 0;
static unsigned char log_flushafter = 0;
static time_t log_first = 0;

typedef enum {
  pndl_nil = 0,
  pndl_stream,
  pndl_syslog,
  pndl_callback
} pnd_logger_e;

typedef struct {
  pnd_logger_e type;
  union {
    FILE *stream;
    pnd_log_callback_f callback;
  };
} pnd_log_target_t;

#define PND_LOG_MAX 5
static pnd_log_target_t log_targets [ PND_LOG_MAX ]; // implicitly nil

static int pnd_log_empty_slot ( void ) {
  unsigned char i;

  for ( i = 0; i < PND_LOG_MAX; i++ ) {
    if ( log_targets [ i ].type == pndl_nil ) {
      return ( i );
    }
  }

  return ( -1 );
}

unsigned char pnd_log_set_filter ( unsigned char newlevel ) {
  unsigned char foo = log_filterlevel;
  log_filterlevel = newlevel;
  return ( foo );
}

unsigned char pnd_log_get_filter ( void ) {
  return ( log_filterlevel );
}

void pnd_log_set_flush ( unsigned char x ) {
  log_flushafter = x;
  return;
}

void pnd_log_set_pretext ( char *pre ) {

  if ( log_pretext ) {
    free ( log_pretext );
    log_pretext = NULL;
  }

  if ( pre ) {
    log_pretext = strdup ( pre );
  }

  return;
}

void pnd_log_to_nil ( void ) {
  memset ( log_targets, '\0', sizeof(pnd_log_target_t) * PND_LOG_MAX );
  return;
}

unsigned char pnd_log_to_stream ( FILE *f ) {
  int i = pnd_log_empty_slot();

  if ( i < 0 ) {
    return ( 0 ); // fail!
  }

  log_targets [ i ].type = pndl_stream;
  log_targets [ i ].stream = f;

  return ( 1 );
}

unsigned char pnd_log_to_syslog ( char *facility ) {
  (void)facility;
  return ( 0 ); // NYI
}

unsigned char pnd_log_to_callback ( pnd_log_callback_f f, void *userdata ) {
  (void)f;
  (void)userdata;
  return ( 0 ); // NYI
}

unsigned char pnd_log_to_stdout ( void ) {
  return ( pnd_log_to_stream ( stdout ) );
}

unsigned char pnd_log_to_stderr ( void ) {
  return ( pnd_log_to_stream ( stderr ) );
}

unsigned char pnd_log_max_targets ( void ) {
  return ( PND_LOG_MAX );
}

static void pnd_log_emit ( unsigned char level, char *message ) {
  unsigned char i;

  // iterate across targets and attempt to emit
  for ( i = 0; i < PND_LOG_MAX; i++ ) {

    switch ( log_targets [ i ].type ) {

    case pndl_nil:
      // nop
      break;

    case pndl_stream:
      // pretext
      if ( log_pretext ) {
        fprintf ( log_targets [ i ].stream, "%s ", log_pretext );
      }
      // log level
      fprintf ( log_targets [ i ].stream, "%u %u\t", level, (unsigned int) (time ( NULL ) - log_first) );
      // message
      if ( message ) {
        fprintf ( log_targets [ i ].stream, "%s", message );
        if ( strchr ( message, '\n' ) == NULL ) {
          fprintf ( log_targets [ i ].stream, "\n" );
        }
        if ( log_flushafter ) {
          fflush ( log_targets [ i ].stream );
        }
      }
      break;

    case pndl_syslog:
      // NYI
      break;

    case pndl_callback:
      // NYI
      break;

    } // switch

  } // for

  return;
}

unsigned char pnd_log ( unsigned char level, char *fmt, ... ) {

  if ( log_first == 0 ) {
    log_first = time ( NULL );
  }

  if ( level == PND_LOG_FORCE ) {
    // always proceed
  } else if ( level < log_filterlevel ) {
    return ( 0 ); // too low level
  }

  // format the actual log string
  int n, size = 100;
  char *p, *np;
  va_list ap;

  if ( ( p = malloc ( size ) ) == NULL ) {
    return ( 0 ); // fail!
  }

  while  ( 1 ) {

    /* Try to print in the allocated space. */
    va_start ( ap, fmt );
    n = vsnprintf ( p, size, fmt, ap );
    va_end ( ap );

    /* If that worked, return the string. */
    if ( n > -1 && n < size ) {
      pnd_log_emit ( level, p );
      break;
    }

    /* Else try again with more space. */
    if ( n > -1 )   /* glibc 2.1 */
      size = n + 1; /* precisely what is needed */
    else           /* glibc 2.0 */
      size *= 2;  /* twice the old size */
    if ( ( np = realloc ( p, size ) ) == NULL ) {
      free(p);
      return ( 0 ); // fail!
    } else {
      p = np;
    }

  } // while

  if ( p ) {
    free ( p );
  }

  return ( 1 );
}


static unsigned char _do_buried_logging = 0;
void pnd_log_set_buried_logging ( unsigned char yesno ) {
  _do_buried_logging = yesno;
  return;
}

unsigned char pnd_log_do_buried_logging ( void ) {
  if ( _do_buried_logging == 1 ) {
    return ( 1 );
  }
  return ( 0 );
}
