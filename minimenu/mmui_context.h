#ifndef h_mmui_context_h
#define h_mmui_context_h

/* this header is just an extension to mmui.h really, but rather than clutter that one up thought I'd best
 * split it off.
 */

/* most of this context never changed (or shouldn't change anyway) over the lifetime of the session.
 * Some few values may change however, as noted below, if the detail pane is toggled. (Or possible
 * other theme changes if we add any over time. The rapidity of development is starting to show :)
 */
typedef struct {

  // context that generally doesn't change over a session
  //
  unsigned int screen_width;

  unsigned int font_rgba_r;
  unsigned int font_rgba_g;
  unsigned int font_rgba_b;
  unsigned int font_rgba_a;

  unsigned int grid_offset_x;
  unsigned int grid_offset_y;

  unsigned int icon_offset_x;
  unsigned int icon_offset_y;
  unsigned int icon_max_width;
  unsigned int icon_max_height;
  unsigned int sel_icon_offset_x;
  unsigned int sel_icon_offset_y;

  unsigned int text_width;
  unsigned int text_clip_x;
  unsigned int text_offset_x;
  unsigned int text_offset_y;
  unsigned int text_hilite_offset_y;

  unsigned int arrow_bar_x;
  unsigned int arrow_bar_y;
  unsigned int arrow_bar_clip_w;
  unsigned int arrow_bar_clip_h;
  unsigned int arrow_up_x;
  unsigned int arrow_up_y;
  unsigned int arrow_down_x;
  unsigned int arrow_down_y;

  // calculated
  SDL_Color fontcolor; // calculated; used for default font colour by various widgets

  // context that may well change over the session
  //
  unsigned char row_max;
  unsigned char col_max;

  unsigned int cell_width;
  unsigned int cell_height;

} ui_context_t;

void ui_recache_context ( ui_context_t *c );

#endif
