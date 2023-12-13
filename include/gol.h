#ifndef _GOL_H_
#define _GOL_H_

#include "error.h"
#include "fifo.h"
#include "layout.h"
#include "types.h"
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GOL_DEBUG
#define GOL_DEBUG_FONT_SIZE 10.0f
#define GOL_DEBUG_TITLE_FONT_SIZE 32.0f
#define GOL_DEBUG_COLOR BLUE

#define GOL_FPS 60
#define GOL_INITIAL_SCREEN_WIDTH 1480.0
#define GOL_INITIAL_SCREEN_HEIGHT 720.0
#define GOL_INITIAL_GRID_WIDTH 50.0
#define GOL_INITIAL_CYCLE_PERIOD 1.0

#define GOL_ALIVE_CELL_SIZE_RATIO 0.9f

#define GOL_GRID_COLOR LIGHTGRAY
#define GOL_HOVER_COLOR DARKGREEN

typedef u32 Count;

typedef struct GolCellMap {
  Vector2 key; // Cell Corrdinates
  Count value; // Neighbour count (only when updating, else not used)
} GolCellMap;

typedef enum GolCctState {
  gol_cct_quit,
  gol_cct_error,
  gol_cct_compute,
  gol_cct_toggle_cell
} GolCctState;

// Use to send pointers from GolCtx members to Cycle Computation Thread (CCT)
typedef struct GolCctArgs {
  Fifo *fifo;
  Vector2 **alive_cells_render_buffer_1; // Write
  Vector2 **alive_cells_render_buffer_2; // Write
  i32 *buffer_index;                     // Write
  mtx_t *buffer_index_mtx;    // Prevents CCT to change index when main thread
                              // renders alive cells
  double *cycle_period;       // Time in seconds between two cycles. Read Only
  double *cycle_compute_time; // Time to compute a lifecycle. R/W
  u64 *cycle_nb;              // Number of cycle since start. R/W
} GolCctArgs;

typedef struct GolMsgDataToggle {
  Vector2 cell_coord;
} GolMsgDataToggle;

typedef struct GolCtx {
  Rectangle screen;     // Screen bounds
  Rectangle g_screen;   // Game screen bounds
  Rectangle dbg_screen; // Debug screen bounds
  Vector2 cam_pos; // "Camera" position relative to the world, considered at the
                   // top left corner of g_screen
  Vector2 velocity; // Camera velocity

  bool draw_grid;  // Should render grid
  float cell_size; // Width (and height) of a cell

  GolCellMap *alive_cells; // Array of alive cells on the grid.

  bool mouse_on_g_screen;   // Is mouse in g_screen bounds
  Vector2 mouse_cell_coord; // Coordinates of the cell under cursor

  bool play; // If true, plays Game Of Life rules each GOL_CELL_UPDATE_PERIOD

  thrd_t cct;    // Cycle Computation Thread (CCT)
  Fifo cct_fifo; // Cycle Computation Thread fifo

  // These have their adresses shared with the Cycle Computation Thread (CCT)
  //
  Vector2 *alive_cells_render_buffer_1;
  Vector2 *alive_cells_render_buffer_2; // Two cells buffers. When one is
                                        // being built, the other one is used
                                        // for rendering. Main Thread Read Only
  i32 buffer_index;         // Which buffer to render. Main Thread Read Only
  mtx_t buffer_index_mtx;   // Prevents CCT to change index when main thread
                            // renders alive cells
  double cycle_period;      // Time in seconds between two cycles. CCT Read Only
  double cycle_last_update; // Time of the last cell lifecycle update. Main
                            // Thread Read Only
  double cycle_compute_time; // Time to compute a lifecycle. Main
                             // Thread Read Only
  u64 cycle_nb;              // Number of cycle since start. Main
                             // Thread Read Only

  bool show_dbg; // Shoul show debug info ?
} GolCtx;

int gol_run(GolCtx *const self, int argc, char *argv[]);

void gol_init(GolCtx *const self, Error *const err);

void gol_event(GolCtx *const self, Error *const err);
void gol_update(GolCtx *const self);

i32 gol_cct(void *arg);

void gol_draw(GolCtx *const self, Error *const err);
void gol_draw_grid(const GolCtx *const self);
void gol_draw_cells(GolCtx *const self, Error *const err);
void gol_draw_hovered_cell(const GolCtx *const self);
void gol_draw_dbg(const GolCtx *const self);

int gol_deinit(GolCtx *const self, Error *const err);

// Utility
float gol_move_ease(const double cur_time, const double start_time);
void gol_print_rec(const Rectangle rec, const char *const prefix);

#endif // !_GOL_H_
