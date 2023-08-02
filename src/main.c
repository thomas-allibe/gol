#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;

  Vector2 pos = {0};
  bool close = false;

  InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose() &&
         !close) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    if (IsKeyDown(KEY_RIGHT))
      pos.x += 5;
    if (IsKeyDown(KEY_LEFT))
      pos.x -= 5;
    if (IsKeyDown(KEY_UP))
      pos.y -= 5;
    if (IsKeyDown(KEY_DOWN))
      pos.y += 5;
    if (IsKeyDown(KEY_ESCAPE))
      close = true;

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    ClearBackground(RAYWHITE);

    DrawCircle(pos.x, pos.y, 5, RED);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
