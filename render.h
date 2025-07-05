#pragma once

#include "ecs.h"

class RenderSystem {
public:
  explicit RenderSystem(Registry &r) : reg_(r) {}

  void render(SkCanvas *canvas, float currentTime) {
    std::vector<Entity> backgroundEntities;
    std::vector<Entity> foregroundEntities;

    reg_.each<TransformComponent>([&](Entity ent, TransformComponent &) {
      if (reg_.has<SceneBackgroundComponent>(ent)) {
        backgroundEntities.push_back(ent);
      } else {
        foregroundEntities.push_back(ent);
      }
    });

    for (Entity ent : backgroundEntities) {
      if (auto *tr = reg_.get<TransformComponent>(ent)) {
        renderEntity(canvas, currentTime, ent, *tr);
      }
    }

    for (Entity ent : foregroundEntities) {
      if (auto *tr = reg_.get<TransformComponent>(ent)) {
        renderEntity(canvas, currentTime, ent, *tr);
      }
    }
  }

private:
  void renderEntity(SkCanvas *canvas, float currentTime, Entity ent,
                    TransformComponent &tr) {
    auto *shapeComp = reg_.get<ShapeComponent>(ent);
    auto *material = reg_.get<MaterialComponent>(ent);

    if (!shapeComp || !shapeComp->shape || !material)
      return;

    auto *animation = reg_.get<AnimationComponent>(ent);
    if (animation && (currentTime < animation->entryTime ||
                      currentTime > animation->exitTime)) {
      return; // Don't render if outside entry/exit times
    }
    canvas->save();
    canvas->translate(tr.x, tr.y);
    canvas->rotate(tr.rotation * 180 / M_PI);
    canvas->scale(tr.sx, tr.sy);

    shapeComp->shape->render(canvas, *material);

    canvas->restore();
  }

  Registry &reg_;
};
