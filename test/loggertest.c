
#include "pnd_logger.h"

// arbitrary warning level
#define PLOG_LOW    0
#define PLOG_MEDIUM 1
#define PLOG_HIGH   2

int main ( void ) {

  pnd_log ( PLOG_LOW, "low message, should be ignored" );

  /* normal operation -------------- */
  pnd_log_to_stdout();
  pnd_log ( PLOG_LOW, "low message, should go stdout once" );
  /* ------------------------------- */

  /* extra testing vvvvvvvvvvvvvvvvv
   */

  pnd_log_to_stdout();
  pnd_log ( PLOG_LOW, "low message, should go stdout twice" );

  pnd_log_to_nil();
  pnd_log_to_stdout();

  pnd_log_set_pretext ( "loggertest" );
  pnd_log ( PLOG_LOW, "low message, emit once, with pretext" );

  pnd_log_set_filter ( PLOG_MEDIUM );
  pnd_log ( PLOG_LOW, "low message, emit once, filter is medium+" );
  pnd_log ( PLOG_MEDIUM, "medium message, emit once, filter is medium+" );
  pnd_log ( PLOG_HIGH, "high message, emit once, filter is medium+" );

  pnd_log_set_filter ( PLOG_LOW );
  pnd_log ( PLOG_LOW, "low message, emit once, filter is low+ again" );

  return ( 0 );
}
