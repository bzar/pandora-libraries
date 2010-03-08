
#ifndef h_mmui_h
#define h_mmui_h

/* this code actually _does_ something; this way, at least all the IO routines are in one place, so
 * I know what to replace with something sensible later :)
 * ... ahh, to have time to make this in C++ as an actual abstract interface...
 */

/* staticly cached stuff, for UI
 */

typedef enum {
  IMG_BACKGROUND_800480 = 0,
  IMG_BACKGROUND_TABMASK,
  IMG_DETAIL_PANEL,
  IMG_DETAIL_BG,
  IMG_SELECTED_ALPHAMASK,
  IMG_TAB_SEL,
  IMG_TAB_UNSEL,
  IMG_ICON_MISSING,
  IMG_SELECTED_HILITE,
  IMG_PREVIEW_MISSING,
  IMG_ARROW_UP,
  IMG_ARROW_DOWN,
  IMG_ARROW_SCROLLBAR,
  IMG_MAX, // before this point is loaded; after is generated
  IMG_TRUEMAX
} mm_imgcache_e;

typedef struct {
  mm_imgcache_e id;
  char *confname;
  void /*SDL_Surface*/ *i;
} mm_imgcache_t;

/* ui stuff
 */

typedef enum {
  uisb_none = 0,
  uisb_x = 1,
  uisb_y = (1<<1),
  uisb_a = (1<<2),
  uisb_b = (1<<3),
  uisb_l = (1<<4),
  uisb_r = (1<<5),
  uisb_start = (1<<6),
  uisb_select = (1<<7),
  uisb_max
} ui_sdl_button_e;

unsigned char ui_setup ( void );
unsigned char ui_imagecache ( char *basepath );

#define CHANGED_NOTHING     (0)
#define CHANGED_CATEGORY    (1<<0)  /* changed to different category */
#define CHANGED_SELECTION   (1<<1)  /* changed app selection */
#define CHANGED_DATAUPDATE  (1<<2)  /* deferred preview pic or icon came in */
#define CHANGED_APPRELOAD   (1<<3)  /* new set of applications entirely */
#define CHANGED_EVERYTHING  (0xFFFF) /* redraw it all! */
void ui_render ( unsigned int render_mask );

void ui_loadscreen ( void );        // show screen while loading the menu
void ui_discoverscreen ( unsigned char clearscreen ); // screen to show while scanning for apps
void ui_cachescreen ( unsigned char clearscreen ); // while caching icons, categories and preview-pics-Now-mode

/* internal functions follow
 */

// change the focus
void ui_process_input ( unsigned char block_p );
void ui_push_left ( void );
void ui_push_right ( void );
void ui_push_up ( void );
void ui_push_down ( void );
void ui_push_exec ( void );
void ui_push_ltrigger ( void );
void ui_push_rtrigger ( void );

// ui_render() can register tappable-areas which touchscreen code can then figure out if we made a hit
void ui_register_reset ( void );
void ui_register_tab ( mm_category_t *category, unsigned int x, unsigned int y, unsigned int w, unsigned int h );
void ui_register_app ( pnd_disco_t *app, unsigned int x, unsigned int y, unsigned int w, unsigned int h );

#endif
