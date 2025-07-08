#pragma once
#include "ecs.h"
#include "scripting.h"
#include "shapes.h"

class RenderSystem {
public:
  explicit RenderSystem(flecs::world &w, ScriptSystem &ss)
      : world_(w), scriptSystem_(ss) {}

  void render(SkCanvas *canvas, float currentTime);

private:
  flecs::world &world_;
  ScriptSystem &scriptSystem_;
};
