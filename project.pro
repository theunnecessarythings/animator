TEMPLATE       = app
CONFIG        += c++17 widgets opengl export_compile_commands
QT            += widgets opengl concurrent
SKIA_ROOT     = /home/sreeraj/ubuntu/Documents/skia
INCLUDEPATH  += $$SKIA_ROOT sol2/include lua-5.4.8/src include flecs

# Link against the shared Skia library
LIBS += -L$$SKIA_ROOT/out/Shared -lskia \
        -lGL               \
        -lglfw             \
        -lfontconfig       \
        -lfreetype         \
        -ldl                \
        -lpthread           \
        -lm                 \
        -lpng16             \
        -lz                 \
        -lharfbuzz          \
        -lexpat             \
        -ljpeg              \
        -licuuc             \
        -licui18n           \
        -lwebpdemux         \
        -lwebp              \
        -lsharpyuv \
        -L/mnt/ubuntu/home/sreeraj/Documents/lua-5.4.8/src  \
        /home/sreeraj/ubuntu/Documents/animator/lua-5.4.8/src/liblua.a

SOURCES       += src/main.cpp src/scripting.cpp src/commands.cpp src/window.cpp src/render.cpp src/scene.cpp src/canvas.cpp src/shapes.cpp flecs/flecs.c
HEADERS       += include/canvas.h include/window.h include/toolbox.h include/ecs.h include/scene_model.h include/commands.h  \
                include/serialization.h include/cpp_script_interface.h include/script_pch.h include/render.h include/shapes.h include/scripting.h include/scene.h
RESOURCES     += resources/icons.qrc

QMAKE_CXX = clang++
QMAKE_CC = clang
QMAKE_CFLAGS += -std=gnu99
# No special linker flags needed when using shared libraries
QMAKE_LFLAGS += -rdynamic

