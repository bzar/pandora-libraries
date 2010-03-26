
#ifndef h_mmenu_h
#define h_mmenu_h

// utility
#define IFNULL(foo,bar) (foo)?(foo):(bar)
extern char *pnd_run_script;
extern char *g_skinpath;

// base searchpath to locate the conf
#define MMENU_CONF "mmenu.conf"
#define MMENU_CONF_SEARCHPATH "/etc/pandora/conf:./minimenu"

typedef enum {
  pndn_debug = 0,
  pndn_rem,          // will set default log level to here, so 'debug' is omitted
  pndn_warning,
  pndn_error,
  pndn_none
} pndnotify_loglevels_e;

void emit_and_quit ( char *s );

void applications_free ( void );
void applications_scan ( void );

#endif
