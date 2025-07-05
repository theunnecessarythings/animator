#pragma once

#include "ecs.h"

class RenderSystem {
public:
  explicit RenderSystem(Registry &r) : reg_(r) {}

  void render(SkCanvas *canvas, float currentTime) {
    std::vector<Entity> bg, fg;

    reg_.each<TransformComponent>([&](Entity e, TransformComponent &) {
      (reg_.has<SceneBackgroundComponent>(e) ? bg : fg).push_back(e);
    });

    auto drawList = [&](const std::vector<Entity> &list) {
      for (Entity e : list)
        if (auto *tr = reg_.get<TransformComponent>(e))
          renderEntity(canvas, currentTime, e, *tr);
    };

    drawList(bg);
    drawList(fg);
  }

private:
  void renderEntity(SkCanvas *c, float time, Entity e, TransformComponent &tr) {
    auto *shape = reg_.get<ShapeComponent>(e);
    auto *material = reg_.get<MaterialComponent>(e);
    if (!shape || !shape->shape || !material)
      return;

    if (auto *anim = reg_.get<AnimationComponent>(e))
      if (time < anim->entryTime || time > anim->exitTime)
        return;

    c->save();
    c->translate(tr.x, tr.y);
    c->rotate(tr.rotation * 180.f / M_PI);
    c->scale(tr.sx, tr.sy);

    shape->shape->render(c, *material);
    c->restore();
  }

  Registry &reg_;
};
