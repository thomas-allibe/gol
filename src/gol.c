#include "gol.h"
#include <raylib.h>
#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#pragma GCC diagnostic pop

i32 gol_run(GolCtx *const self, i32 argc, char *argv[]) {
  (void)argc;
  (void)argv;

  gol_init(self);

#ifdef GOL_DEBUG
  SetTraceLogLevel(LOG_ALL);
#endif /* ifdef GOL_DEBUG */

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {

    gol_event(self);

    gol_update(self);

    BeginDrawing();

    gol_draw(self);

    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context

  return gol_deinit(self);
}

i32 gol_init(GolCtx *const self) {
  self->screen =
      (Rectangle){.width = 1480.0f, .height = 720.0f, .x = 0.0f, .y = 0.0f};
  self->cam_pos = (Vector2){0};
  self->grid_width = 50.0f;
  // self->cells[0] = (Vector2){.x = 0.0f, .y = 0.0f};
  // self->cells[1] = (Vector2){.x = 1.0f, .y = 0.0f};
  // self->cells[2] = (Vector2){.x = 2.0f, .y = 0.0f};
  // arrput(self->cells, (Vector2){.x = 0.0f, .y = 0.0f});
  // arrput(self->cells, self->cam_pos);

#ifdef GOL_DEBUG
  self->show_dbg = true;
#endif /* ifdef GOL_DEBUG */

  InitWindow((i32)self->screen.width, (i32)self->screen.height, "Game Of Life");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(GOL_FPS); // Set our game to run at 60 frames-per-second

  return 0;
}

void gol_event(GolCtx *const self) {
  const double cur_time = GetTime();
  const Vector2 mouse_pos = GetMousePosition();
  const Vector2 mouse_delta = GetMouseDelta();
  const Vector2 mouse_wheel = GetMouseWheelMoveV();

  if (IsWindowResized()) {
    self->screen.width = (float)GetRenderWidth();
    self->screen.height = (float)GetRenderHeight();
  }

  // Enable Debug View
  //
#ifdef GOL_DEBUG
  if (IsKeyPressed(KEY_D)) {
    self->show_dbg = !self->show_dbg;
  }
#endif /* ifdef GOL_DEBUG */

  if (IsKeyPressed(KEY_C)) {
    self->cam_pos.x = 0.0f;
    self->cam_pos.y = 0.0f;
  }

  // Mouse on g_screen
  //
  self->mouse_on_g_screen = CheckCollisionPointRec(mouse_pos, self->g_screen);
  if (self->mouse_on_g_screen) {
    // Compute mouse grid pos coordinate
    //
    self->mouse_grid_pos = (Vector2){
        .x = floorf((mouse_pos.x - self->g_screen.x + self->cam_pos.x) /
                    self->grid_width),
        .y = floorf((mouse_pos.y - self->g_screen.y + self->cam_pos.y) /
                    self->grid_width)};

    if (IsKeyDown(KEY_LEFT_CONTROL)) {
      // Mouse Left + Ctrl: toggle cell
      //
      self->toggle_cell = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    } else {
      // Mouse grid dragging
      //
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        self->velocity.x = -1.0f * mouse_delta.x;
        self->velocity.y = -1.0f * mouse_delta.y;
      } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        self->velocity.x = 0.0f;
        self->velocity.y = 0.0f;
      }
    }
  }

  // Mouse wheel gri_width change
  //
  self->grid_width += mouse_wheel.y;

  // Keyboard Right Key Grid Move
  //
  if (IsKeyDown(KEY_RIGHT)) {
    if (IsKeyPressed(KEY_RIGHT)) {
      self->move_right_start = cur_time;
    }
    self->velocity.x = GOL_CAMERA_MAX_VELOCITY *
                       gol_move_ease(cur_time, self->move_right_start);

  } else if (IsKeyReleased(KEY_RIGHT)) {
    self->move_right_start = 0.0;
    self->velocity.x = 0.0f;
  }

  // Keyboard Left Key Grid Move
  //
  if (IsKeyDown(KEY_LEFT)) {
    if (IsKeyPressed(KEY_LEFT)) {
      self->move_left_start = cur_time;
    }
    self->velocity.x = -1 * GOL_CAMERA_MAX_VELOCITY *
                       gol_move_ease(cur_time, self->move_left_start);

  } else if (IsKeyReleased(KEY_LEFT)) {
    self->move_left_start = 0.0;
    self->velocity.x = 0.0f;
  }
  // Keyboard Up Key Grid Move
  //
  if (IsKeyDown(KEY_UP)) {
    if (IsKeyPressed(KEY_UP)) {
      self->move_up_start = cur_time;
    }
    self->velocity.y = -1 * GOL_CAMERA_MAX_VELOCITY *
                       gol_move_ease(cur_time, self->move_up_start);

  } else if (IsKeyReleased(KEY_UP)) {
    self->move_up_start = 0.0;
    self->velocity.y = 0.0f;
  }

  // Keyboard Down Key Grid Move
  //
  if (IsKeyDown(KEY_DOWN)) {
    if (IsKeyPressed(KEY_DOWN)) {
      self->move_down_start = cur_time;
    }
    self->velocity.y = GOL_CAMERA_MAX_VELOCITY *
                       gol_move_ease(cur_time, self->move_down_start);

  } else if (IsKeyReleased(KEY_DOWN)) {
    self->move_down_start = 0.0;
    self->velocity.y = 0.0f;
  }
}

void gol_update(GolCtx *const self) {
  self->cam_pos.x += self->velocity.x;
  self->cam_pos.y += self->velocity.y;

  if (self->toggle_cell) {
    self->toggle_cell = false;
    bool found = false;
    for (u32 i = 0; i < arrlen(self->cells) && !found; i++) {
      if (self->cells[i].x == self->mouse_grid_pos.x &&
          self->cells[i].y == self->mouse_grid_pos.y) {
        found = true;
        arrdelswap(self->cells, i);
        break;
      }
    }
    if (!found) {
      arrput(self->cells, self->mouse_grid_pos);
    }
  }
}

void gol_draw(GolCtx *const self) {

  ClearBackground(RAYWHITE);

#ifdef GOL_DEBUG
  if (self->show_dbg) {

    layout_start(self->screen, DISP_HORIZ, 2);
    self->g_screen = layout_box(layout_get(), GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    self->dbg_screen = layout_box(layout_get(), RED, BLANK, 10.0f, 5.0f, 5.0f);

    gol_draw_grid(self);
    gol_draw_cells(self);
    gol_draw_hovered_cell(self);
    gol_draw_dbg(self);

  } else
#endif /* ifdef GOL_DEBUG */
  {
    self->g_screen = layout_box(self->screen, GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    gol_draw_grid(self);
    gol_draw_cells(self);
    gol_draw_hovered_cell(self);
  }
}

void gol_draw_grid(const GolCtx *const self) {

  Vector2 start_pos, end_pos;

  i32 i = 0; // For debug

  // Compute interval between cam_pos and the closest lines from the left/top
  const Vector2 grid_frac = {.x = -1.0f * self->cam_pos.x / self->grid_width,
                             .y = -1.0f * self->cam_pos.y / self->grid_width};
  const Vector2 offset = {
      .x = (grid_frac.x - floorf(grid_frac.x)) * self->grid_width,
      .y = (grid_frac.y - floorf(grid_frac.y)) * self->grid_width};

  // Draw Vertical Lines
  //
  // Start by drawing the first leftmost vertical line
  i = 0; // For debug
  for (float x = self->g_screen.x + offset.x;
       x <= self->g_screen.x + self->g_screen.width; x += self->grid_width) {
    start_pos.x = x;
    start_pos.y = self->g_screen.y;
    end_pos.x = x;
    end_pos.y = self->g_screen.y + self->g_screen.height;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // x coord on each line
      DrawTextPro(GetFontDefault(),
                  TextFormat("x: %d", (i32)(truncf(-1.0f * grid_frac.x)) + i),
                  (Vector2){.x = x, .y = self->g_screen.y + 5},
                  (Vector2){.x = 0.0f, .y = 10.0f}, 90.0f, 10.0f, 2.0f, PURPLE);
      i += 1; // Increment
    }
#endif /* ifdef GOL_DEBUG                                                      \
        */
  }

  // Draw Horizontal Lines
  //
  // Start by drawing the first topmost horizontal line
  i = 0; // For debug
  for (float y = self->g_screen.y + offset.y;
       y <= self->g_screen.y + self->g_screen.height; y += self->grid_width) {
    start_pos.x = self->g_screen.x;
    start_pos.y = y;
    end_pos.x = self->g_screen.x + self->g_screen.width;
    end_pos.y = y;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // x coord on each line
      DrawTextPro(GetFontDefault(),
                  TextFormat("y: %d", (i32)(truncf(-1.0f * grid_frac.y)) + i),
                  (Vector2){.x = self->g_screen.x + 5, .y = y}, (Vector2){0},
                  0.0f, 10.0f, 2.0f, PURPLE);
      i += 1; // Increment
    }
#endif /* ifdef GOL_DEBUG */
  }
}

void gol_draw_cells(const GolCtx *const self) {
  const Rectangle cam_window = {.x = self->cam_pos.x,
                                .y = self->cam_pos.y,
                                .width = self->g_screen.width,
                                .height = self->g_screen.height};
  for (u32 i = 0; i < arrlen(self->cells); i++) {
    const Rectangle cell_rec = {.x = self->cells[i].x * self->grid_width,
                                .y = self->cells[i].y * self->grid_width,
                                .width = self->grid_width,
                                .height = self->grid_width};
    if (CheckCollisionRecs(cell_rec, cam_window)) {
      Rectangle cell_to_draw = GetCollisionRec(cell_rec, cam_window);

      cell_to_draw.x = cell_to_draw.x - self->cam_pos.x + self->g_screen.x + 5;
      cell_to_draw.y = cell_to_draw.y - self->cam_pos.y + self->g_screen.y + 5;
      cell_to_draw.width -= 10;
      cell_to_draw.height -= 10;

      DrawRectangleRec(cell_to_draw, BLACK);
    }
  }
}

void gol_draw_hovered_cell(const GolCtx *const self) {
  if (self->mouse_on_g_screen) {
    const Rectangle cam_window = {.x = self->cam_pos.x,
                                  .y = self->cam_pos.y,
                                  .width = self->g_screen.width,
                                  .height = self->g_screen.height};
    const Rectangle cell_rec = {.x = self->mouse_grid_pos.x * self->grid_width,
                                .y = self->mouse_grid_pos.y * self->grid_width,
                                .width = self->grid_width,
                                .height = self->grid_width};
    if (CheckCollisionRecs(cell_rec, cam_window)) {
      Rectangle cell_to_draw = GetCollisionRec(cell_rec, cam_window);

      cell_to_draw.x = cell_to_draw.x - self->cam_pos.x + self->g_screen.x;
      cell_to_draw.y = cell_to_draw.y - self->cam_pos.y + self->g_screen.y;

      DrawRectangleLinesEx(cell_to_draw, 5.0f, GOL_HOVER_COLOR);
    }
  }
}

void gol_draw_dbg(const GolCtx *const self) {
  const Vector2 mouse_pos = GetMousePosition();
  const Vector2 mouse_pos_rel_g = {
      .x = mouse_pos.x - self->g_screen.x,
      .y = mouse_pos.y -
           self->g_screen.y}; // Mouse position relative to g_screen

  // Draw a center cross to g_screen
  //
  DrawLine((i32)(self->g_screen.x + self->g_screen.width / 2.0f),
           (i32)self->g_screen.y,
           (i32)(self->g_screen.x + self->g_screen.width / 2.0f),
           (i32)(self->g_screen.y + self->g_screen.height), RED);
  DrawLine((i32)(self->g_screen.x),
           (i32)(self->g_screen.y + self->g_screen.height / 2.0f),
           (i32)(self->g_screen.x + self->g_screen.width),
           (i32)(self->g_screen.y + self->g_screen.height / 2.0f), RED);

  // Draw Point at Origin
  //
  const Vector2 origin_on_screen = {.x = self->g_screen.x - self->cam_pos.x,
                                    .y = self->g_screen.y - self->cam_pos.y};
  if (CheckCollisionPointRec(origin_on_screen, self->g_screen)) {
    DrawCircle((i32)origin_on_screen.x, (i32)origin_on_screen.y, 15.0f, RED);
  }

  // Draw mouse coordinates relative to screen
  //
  DrawText(TextFormat("s(%d, %d)", (i32)mouse_pos.x, (i32)mouse_pos.y),
           (i32)mouse_pos.x + 20, (i32)mouse_pos.y, GOL_DEBUG_FONT_SIZE, RED);

  // Draw mouse coordinates relative to g_screen
  //
  if (self->mouse_on_g_screen) {
    DrawText(TextFormat("gs(%d, %d)\ng(%d, %d)", (i32)mouse_pos_rel_g.x,
                        (i32)mouse_pos_rel_g.y, (i32)self->mouse_grid_pos.x,
                        (i32)self->mouse_grid_pos.y),
             (i32)mouse_pos.x + 20,
             (i32)(mouse_pos.y + GOL_DEBUG_FONT_SIZE + 5.0f),
             GOL_DEBUG_FONT_SIZE, RED);
  }

  // Draw Debug Panel
  //
  //
  // Draw FPS top left debug screen
  //
  DrawFPS((i32)self->dbg_screen.x, (i32)self->dbg_screen.y);

  layout_start(self->dbg_screen, DISP_VERT, 3);

  const Rectangle title_rec = layout_get();
  const char *const title = "Debug Info:";
  const float title_x_offset =
      (title_rec.width - (float)MeasureText(title, GOL_DEBUG_TITLE_FONT_SIZE)) /
      2.0f;
  DrawText(title, (i32)(title_rec.x + title_x_offset), (i32)title_rec.y,
           GOL_DEBUG_TITLE_FONT_SIZE, RED);

  layout_start(layout_get(), DISP_HORIZ, 3);

  const Rectangle velocity_rec = layout_get();
  DrawText(TextFormat("Camera Velocity (px/s): \n\tx: %f\n\ty: %f",
                      self->velocity.x, self->velocity.y),
           (i32)velocity_rec.x, (i32)velocity_rec.y, GOL_DEBUG_FONT_SIZE, RED);

  const Rectangle cam_coord_rec = layout_get();
  DrawText(TextFormat("Camera: \n\tx: %f\n\ty: %f", self->cam_pos.x,
                      self->cam_pos.y),
           (i32)cam_coord_rec.x, (i32)cam_coord_rec.y, GOL_DEBUG_FONT_SIZE,
           RED);

  const Rectangle screens_rec = layout_get();
  DrawText(
      TextFormat(
          "Screen Rectangles:\n\tApp: {w: %f, h: %f}\n\tGame: {x: %f, y: %f, "
          "w: %f, h: %f}\n\tDebug: {x: %f, y: %f, w: %f, h: %f}",
          self->screen.width, self->screen.height, self->g_screen.x,
          self->g_screen.y, self->g_screen.width, self->g_screen.height,
          self->dbg_screen.x, self->dbg_screen.y, self->dbg_screen.width,
          self->dbg_screen.height),
      (i32)screens_rec.x, (i32)screens_rec.y, GOL_DEBUG_FONT_SIZE, RED);

  const Rectangle cells_rec = layout_get();
  for (u32 i = 0; i < arrlen(self->cells); i++) {
    DrawText(TextFormat("%02d:\t{x: %f, y: %f}", i, self->cells[i].x,
                        self->cells[i].y),
             (int)cells_rec.x,
             (int)cells_rec.y + (int)i * (int)GOL_DEBUG_FONT_SIZE,
             GOL_DEBUG_FONT_SIZE, RED);
  }
}

int gol_deinit(GolCtx *const self) {
  if (self->cells) {
    arrfree(self->cells);
  }
  return EXIT_SUCCESS;
}

float gol_move_ease(const double cur_time, const double start_time) {
  const double delta = cur_time - start_time;

  return (float)(1.0 - exp(-2.0 * delta));
}
void gol_print_rec(const Rectangle rec, const char *const prefix) {
  TraceLog(LOG_DEBUG, "%s {x: %f, y: %f, w: %f, h: %f}\n", prefix, rec.x, rec.y,
           rec.width, rec.height);
}
