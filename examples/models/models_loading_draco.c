/*******************************************************************************************
*
*   raylib [models] example - Load Draco-compressed glTF models (KHR_draco_mesh_compression)
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   NOTE: Draco-compressed glTF models require raylib to be built with SUPPORT_DRACO
*         and the Draco library (https://github.com/google/draco), otherwise the
*         compressed meshes are skipped with a warning. Test assets can be regenerated
*         with tools/draco/generate_draco_assets.py
*
*   Example originally created with raylib 6.5, last time updated with raylib 6.5
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2026 raylib technologies (@raylibtech)
*
********************************************************************************************/

#include "raylib.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [models] example - draco compressed gltf loading");

    // Define the camera to look into our 3d world
    Camera camera = { 0 };
    camera.position = (Vector3){ 6.0f, 6.0f, 6.0f };
    camera.target = (Vector3){ 0.0f, 2.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // Load Draco-compressed glTF models (KHR_draco_mesh_compression)
    Model cubeModel = LoadModel("resources/models/gltf/draco/draco_cube_multiattr.glb");
    Model skinnedModel = LoadModel("resources/models/gltf/draco/draco_skinned.glb");

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            BeginMode3D(camera);

                if (cubeModel.meshCount > 0) DrawModel(cubeModel, (Vector3){ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
                if (skinnedModel.meshCount > 0) DrawModel(skinnedModel, (Vector3){ 3.0f, 0.0f, 0.0f }, 1.0f, WHITE);

                DrawGrid(10, 1.0f);

            EndMode3D();

            DrawText("Draco-compressed glTF models (KHR_draco_mesh_compression)", 10, 10, 20, DARKGRAY);
            if (cubeModel.meshCount == 0) DrawText("Draco support not enabled (SUPPORT_DRACO), meshes skipped", 10, 40, 10, MAROON);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadModel(cubeModel);
    UnloadModel(skinnedModel);

    CloseWindow();
    //--------------------------------------------------------------------------------------

    return 0;
}
