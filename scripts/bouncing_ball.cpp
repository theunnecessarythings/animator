#include "script_pch.h"

class BouncingBallScript : public IScript {
private:
  float radius = 40.0f;
  float x = 100.0f, y = 100.0f;
  float vx = 180.0f;
  float vy = -250.0f;
  SkColor color = SK_ColorRED;

  const float bounds_width = 500;
  const float bounds_height = 500;

public:
  BouncingBallScript() = default;
  ~BouncingBallScript() override = default;

  void on_start(flecs::entity entity, flecs::world & /*world*/) override {
    if (entity.has<TransformComponent>()) {
      auto &tc = entity.get_mut<TransformComponent>();
      x = tc.x;
      y = tc.y;
    }
  }

  void on_update(flecs::entity entity, flecs::world & /*world*/, float dt,
                 float /*total_time*/) override {
    // std::cout << "on_update called for BouncingBallScript" << std::endl;
    x += vx * dt;
    y += vy * dt;

    // Handle collisions with the boundaries and change color
    if (x - radius < 0) {
      x = radius;
      vx = -vx;
      color = SK_ColorBLUE;
    } else if (x + radius > bounds_width) {
      x = bounds_width - radius;
      vx = -vx;
      color = SK_ColorGREEN;
    }

    if (y - radius < 0) {
      y = radius;
      vy = -vy;
      color = SK_ColorYELLOW;
    } else if (y + radius > bounds_height) {
      y = bounds_height - radius;
      vy = -vy;
      color = SK_ColorMAGENTA;
    }

    if (entity.has<TransformComponent>()) {
      auto &tc = entity.get_mut<TransformComponent>();
      tc.x = x;
      tc.y = y;
    }
  }

  void on_draw(flecs::entity /*entity*/, flecs::world & /*world*/,
               SkCanvas *canvas) override {
    // std::cout << "on_draw called for BouncingBallScript" << std::endl;
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(color);

    canvas->drawCircle(x, y, radius, paint);
  }
};

// Implement the C-style factory functions that the engine will call.
extern "C" {
IScript *create_script() { return new BouncingBallScript(); }

void destroy_script(IScript *script) { delete script; }
}
