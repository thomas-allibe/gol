#include "gol.h"
#include <raylib.h>

#ifdef GOL_DEBUG
static bool show_debug = false;
#endif // GOL_DEBUG

int gol_run(int argc, char *argv[]) {

  if (argc > 0)
    printf("%s\n", argv[0]);

  // Initialization
  //--------------------------------------------------------------------------------------

  Rectangle screen = {.width = 800, .height = 450, .x = 0, .y = 0};
  Vector2 position = {0};

  InitWindow((int)screen.width, (int)screen.height, "Game Of Life");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(GOL_FPS); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    if (IsWindowResized()) {
      screen.width = (float)GetRenderWidth();
      screen.height = (float)GetRenderHeight();
    }

#ifdef GOL_DEBUG
    if (IsKeyPressed(KEY_D))
      show_debug = !show_debug;
#endif /* ifdef GOL_DEBUG */

    if (IsKeyDown(KEY_RIGHT))
      position.x += CAMERA_VELOCITY;
    if (IsKeyDown(KEY_LEFT))
      position.x -= CAMERA_VELOCITY;
    if (IsKeyDown(KEY_UP))
      position.y -= CAMERA_VELOCITY;
    if (IsKeyDown(KEY_DOWN))
      position.y += CAMERA_VELOCITY;

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    gol_draw_grid(layout_box(screen, GRAY, BLANK, 10, 5, 0), position, 50);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}

void gol_draw_grid(Rectangle rec, Vector2 pos, float grid_width) {
#ifdef GOL_DEBUG
  char str_coord[15];
#endif /* ifdef GOL_DEBUG */
  Vector2 start_pos, end_pos;

  // Compute interval between pos and the closest lines from the left/top
  Vector2 quotient = {.x = pos.x / grid_width, .y = pos.y / grid_width};
  Vector2 delta = {.x = (quotient.x - floorf(quotient.x)) * grid_width,
                   .y = (quotient.y - floorf(quotient.y)) * grid_width};

  // Draw Vertical Lines
  //
  for (float i = rec.x + delta.x; i <= rec.x + rec.width; i += grid_width) {
    start_pos.x = i;
    start_pos.y = rec.y;
    end_pos.x = start_pos.x;
    end_pos.y = rec.y + rec.height;

    DrawLineV(start_pos, end_pos, LIGHTGRAY);

#ifdef GOL_DEBUG
    if (show_debug) {
      // Moving dot, starting in the center of the screen
      DrawCircle((int)(pos.x + rec.x + rec.width / 2),
                 (int)(pos.y + rec.y + rec.height / 2), 5, RED);

      // x coord on each line
      sprintf(str_coord, "x: %d", (int)start_pos.x);
      DrawTextPro(GetFontDefault(), str_coord,
                  (Vector2){.x = start_pos.x, .y = rec.y + 5},
                  (Vector2){.x = 0, .y = 10}, 90, 10, 2, PURPLE);
    }
#endif /* ifdef GOL_DEBUG                                                      \
        */
  }

  // Draw Horizontal Lines
  //
  for (float i = rec.y + delta.y; i <= rec.y + rec.height; i += grid_width) {
    start_pos.x = rec.x;
    start_pos.y = i;
    end_pos.x = rec.x + rec.width;
    end_pos.y = start_pos.y;

    DrawLineV(start_pos, end_pos, GRID_COLOR);

#ifdef GOL_DEBUG
    if (show_debug) {
      // x coord on each line
      sprintf(str_coord, "y: %d", (int)start_pos.y);
      DrawTextPro(GetFontDefault(), str_coord,
                  (Vector2){.x = rec.x + 5, .y = start_pos.y}, (Vector2){0}, 0,
                  10, 2, PURPLE);
    }
#endif /* ifdef GOL_DEBUG */
  }

#ifdef GOL_DEBUG
  if (show_debug) {
    // Moving dot, starting in the center of the screen
    DrawCircle((int)(pos.x + rec.x + rec.width / 2),
               (int)(pos.y + rec.y + rec.height / 2), 5, RED);
#endif /* ifdef GOL_DEBUG */
  }
}
