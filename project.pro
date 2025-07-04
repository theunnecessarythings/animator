TEMPLATE       = app
CONFIG        += c++17 widgets opengl export_compile_commands
QT            += widgets opengl
SKIA_ROOT     = /home/sreeraj/ubuntu/Documents/skia
INCLUDEPATH  += $$SKIA_ROOT
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
        -lsharpyuv
SOURCES       += main.cpp
HEADERS       += canvas.h window.h toolbox.h ecs.h scene_model.h
RESOURCES     += icons.qrc
