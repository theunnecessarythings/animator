#pragma once
#include "ecs.h"
#include "shapes.h"

class RenderSystem {
public:
  explicit RenderSystem(flecs::world &w) : world_(w) {}

  void render(SkCanvas *canvas, float currentTime) {
    std::vector<Entity> bg, fg;

    // Collect draw order buckets (background first)
    world_.each<TransformComponent>([&](flecs::entity e, TransformComponent &) {
      (e.has<SceneBackgroundComponent>() ? bg : fg).push_back(e);
    });

    auto drawList = [&](const std::vector<Entity> &bucket) {
      for (Entity e : bucket) {
        if (!e.has<TransformComponent>() || !e.has<ShapeComponent>() ||
            !e.has<MaterialComponent>())
          continue;
        auto &tr = e.get_mut<TransformComponent>();
        auto &shapeCmp = e.get_mut<ShapeComponent>();
        auto &mat = e.get_mut<MaterialComponent>();
        if (e.has<AnimationComponent>()) {
          auto anim = e.get<AnimationComponent>();
          if (currentTime < anim.entryTime || currentTime > anim.exitTime)
            continue;
        }

        canvas->save();
        canvas->translate(tr.x, tr.y);
        canvas->rotate(tr.rotation * 180.f / M_PI);
        canvas->scale(tr.sx, tr.sy);
        const PathEffectComponent *pathEffect = nullptr;
        if (e.has<PathEffectComponent>()) {
          pathEffect = &e.get<PathEffectComponent>();
        }
        shapeCmp.shape->render(canvas, mat, pathEffect);
        canvas->restore();
      }
    };

    drawList(bg);
    drawList(fg);
  }

private:
  flecs::world &world_;
};
