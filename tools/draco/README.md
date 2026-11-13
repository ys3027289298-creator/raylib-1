# Draco mesh compression support (KHR_draco_mesh_compression)

raylib's glTF loader (`rmodels.c`) can optionally decode meshes compressed with the
[KHR_draco_mesh_compression](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_draco_mesh_compression)
extension, using the [Google Draco](https://github.com/google/draco) library (C++).

The support is **optional** and disabled by default:

 - Without `SUPPORT_DRACO`, uncompressed glTF/GLB assets load exactly as before.
   Draco-compressed primitives are skipped with a diagnosable `WARNING` log
   (no silent empty meshes, no crashes); mixed files still load their
   uncompressed meshes.
 - With `SUPPORT_DRACO`, each Draco-compressed primitive is decoded **once at load
   time** into temporary cgltf accessors and then flows through the standard
   attribute loading path, so materials, skins, morph targets and node transforms
   behave exactly as for uncompressed assets. Peak memory is a small constant
   multiple of the decompressed vertex/index buffers (the decoded Draco mesh is
   released right after extraction, before textures are even touched).

Decoded attributes: `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_0/1`, `COLOR_0`,
`JOINTS_0`, `WEIGHTS_0` plus triangle indices. Corrupted bitstreams, attributes
declared but missing in the stream, out-of-range indices and missing
`extensionsUsed` declarations are reported with `WARNING` logs instead of crashing.

## Building raylib with Draco support

1. Build and install Draco (tested with draco master, 1.5.x):

       git clone https://github.com/google/draco.git
       cmake -S draco -B draco/build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/draco
       cmake --build draco/build -j && cmake --install draco/build

2. Build raylib with `SUPPORT_DRACO`:

   - CMake:

         cmake -S . -B build -DSUPPORT_DRACO=ON -DDRACO_ROOT=/opt/draco
         cmake --build build

     (`DRACO_ROOT` is only needed if Draco is not installed in a default search path;
     a Draco CMake package config is also picked up automatically via `find_package(draco CONFIG)`.)

   - Makefile:

         make -C src SUPPORT_DRACO=TRUE DRACO_ROOT=/opt/draco

   No public API changes: `LoadModel()`/`LoadModelAnimations()` transparently decode
   Draco-compressed `.gltf`/`.glb` files.

## Test assets

`generate_draco_assets.py` (requires `pip install DracoPy numpy`) generates the test
GLBs into `examples/models/resources/models/gltf/draco/`:

    python3 tools/draco/generate_draco_assets.py

 - `draco_cube_multiattr.glb` - indexed cube, POSITION/NORMAL/TANGENT/TEXCOORD_0/COLOR_0,
   material factors, transformed node
 - `draco_skinned.glb` - skinned mesh, POSITION/NORMAL/JOINTS_0/WEIGHTS_0 + skin
 - `draco_corrupted.glb` - truncated Draco bitstream (must fail gracefully)
 - `draco_undeclared.glb` - valid data, extension missing from `extensionsUsed`
 - `draco_mixed.glb` - one Draco mesh + one uncompressed mesh in the same file

## Headless integration test

`test_draco_loading.c` loads all assets through the public `LoadModel()` API and
validates the decoded data. It runs without a display: the rlgl functions used by
`UploadMesh()` are stubbed with the GNU ld `--wrap` mechanism.

From the repository root (after building raylib):

    WRAPS=$(for s in rlLoadVertexArray rlEnableVertexArray rlDisableVertexArray \
        rlUnloadVertexArray rlLoadVertexBuffer rlLoadVertexBufferElement \
        rlUnloadVertexBuffer rlEnableVertexBuffer rlDisableVertexBuffer \
        rlEnableVertexBufferElement rlDisableVertexBufferElement rlSetVertexAttribute \
        rlEnableVertexAttribute rlDisableVertexAttribute rlSetVertexAttributeDefault \
        rlSetVertexAttributeDivisor rlLoadTexture rlUnloadTexture rlGenTextureMipmaps; \
        do echo -n "-Wl,--wrap=$s "; done)

    # raylib built WITH Draco support:
    gcc -DTEST_EXPECT_DRACO tools/draco/test_draco_loading.c -I src -o /tmp/test_draco \
        $WRAPS build-draco/raylib/libraylib.a /opt/draco/lib/libdraco.a \
        -lstdc++ -lGL -lm -lpthread -ldl -lrt -lX11
    /tmp/test_draco        # 30 tests run, 0 failed

    # raylib built WITHOUT Draco support:
    gcc tools/draco/test_draco_loading.c -I src -o /tmp/test_nodraco \
        $WRAPS build-nodraco/raylib/libraylib.a -lGL -lm -lpthread -ldl -lrt -lX11
    /tmp/test_nodraco      # 8 tests run, 0 failed

A graphical example is also available: `examples/models/models_loading_draco.c`
(picked up automatically by the examples build).
