
#ifndef h_mmenu_h
#define h_mmenu_h

// utility
#define IFNULL(foo,bar) (foo)?(foo):(bar)
extern char *pnd_run_script;
extern char *g_skinpath;

// base searchpath to locate the conf
#define MMENU_CONF "mmenu.conf"
#define MMENU_CONF_SEARCHPATH "/etc/pandora/conf:./minimenu"

// keys
#define MMENU_ARTPATH "minimenu.static_art_searchpath"

#define MMENU_GRID_FONT "grid.font"
#define MMENU_GRID_FONTSIZE "grid.font_ptsize"

#define MMENU_DISP_COLMAX "grid.col_max"
#define MMENU_DISP_ROWMAX "grid.row_max"
#define MMENU_DISP_ICON_MAX_WIDTH "grid.icon_max_width"
#define MMENU_DISP_ICON_MAX_HEIGHT "grid.icon_max_height"

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
