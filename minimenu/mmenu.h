
#ifndef h_mmenu_h
#define h_mmenu_h

// utility
#define IFNULL(foo,bar) (foo)?(foo):(bar)
extern char *pnd_run_script;
extern char *g_skinpath;

// base searchpath to locate the conf
#define MMENU_CONF "mmenu.conf"
#define MMENU_CONF_SEARCHPATH "/media/*/pandora/mmenu:/media/*/pandora/appdata/mmenu:/etc/pandora/conf:./minimenu"

typedef enum {
  pndn_debug = 0,
  pndn_rem,          // will set default log level to here, so 'debug' is omitted
  pndn_warning,
  pndn_error,
  pndn_none
} pndnotify_loglevels_e;

void emit_and_quit ( char *s );      // normal case; quit and run an app
void emit_and_run ( char *buffer );  // odd case; run an app and stay alive
void exec_raw_binary ( char *fullpath ); // just fork/exec something, without exit

void applications_free ( void );
void applications_scan ( void );

void setup_notifications ( void );

unsigned char cat_is_visible ( pnd_conf_handle h, char *catname );

#endif
