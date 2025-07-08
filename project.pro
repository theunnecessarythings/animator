TEMPLATE       = app
CONFIG        += c++17 widgets opengl export_compile_commands
QT            += widgets opengl
SKIA_ROOT     = /home/sreeraj/ubuntu/Documents/skia
INCLUDEPATH  += $$SKIA_ROOT sol2/include lua-5.4.8/src include flecs
LIBS += $$SKIA_ROOT/out/Static/libskia.a \
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
        -L/mnt/ubuntu/home/sreeraj/Documents/lua-5.4.8/src \
        /home/sreeraj/ubuntu/Documents/animator/lua-5.4.8/src/liblua.a
SOURCES       += src/main.cpp src/commands.cpp src/window.cpp src/render.cpp src/scene.cpp src/canvas.cpp src/shapes.cpp flecs/flecs.c
HEADERS       += include/canvas.h include/window.h include/toolbox.h include/ecs.h include/scene_model.h include/commands.h \
                include/serialization.h include/render.h include/shapes.h include/scripting.h include/scene.h
RESOURCES     += resources/icons.qrc

QMAKE_CXX = clang++
QMAKE_CC = clang
QMAKE_CFLAGS += -std=gnu99

