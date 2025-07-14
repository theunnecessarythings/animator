#include "camera.h"
#include "script_pch.h"

#include "animation.h"
#include "easing.h"
#include "group.h"
#include "mobject.h"
#include "shapes.h"
#include "text.h"
#include "renderer.h"
#include "timeline.h"

#include <iostream>
#include <vector>

class Main : public IScript {
private:
  SkFont font;
  float current_time = 0;
  std::vector<AnimationTrack> animation_tracks;

public:
  Main() {
    sk_sp<SkFontMgr> fontMgr = CREATE_FONT_MGR();
    sk_sp<SkTypeface> face =
        fontMgr->matchFamilyStyle("Playfair Display", SkFontStyle());
    if (!face) {
      face = fontMgr->legacyMakeTypeface(nullptr, SkFontStyle());
    }
    font = SkFont(face, 48);
  }

  ~Main() override = default;

  void on_start(flecs::entity entity, flecs::world &world) override {
    std::cout << "Full feature showcase script started." << std::endl;

    // 1. Basic Shapes and Animations
    auto circle = create_circle({-300, -200}, 50).set_fill_color(SK_ColorBLUE);
    add_animation({circle, fade_in(), ease_in_sine, 0.0f, 1.0f});
    add_animation({circle, move_to({-300, 200}), ease_in_out_back, 1.0f, 2.0f});

    auto square = create_square({-150, -200}, 80).set_stroke_color(SK_ColorGREEN).set_stroke_width(8.0f);
    add_animation({square, rotate(360), ease_out_expo, 0.5f, 2.5f});

    // 2. More Shapes and Easing
    auto triangle = create_regular_polygon({0, -200}, 3, 60).set_fill_color(SK_ColorYELLOW);
    add_animation({triangle, scale(2.0), ease_in_back, 1.0f, 2.0f});

    auto arrow = create_arrow({150, -250}, {150, -150}).set_stroke_color(SK_ColorCYAN);
    add_animation({arrow, show_creation(), linear, 1.5f, 2.5f});

    // 3. Text Rendering
    auto text = create_text("Gemini", font, {300, -200}).set_fill_color(SK_ColorMAGENTA);
    add_animation({text, fade_in(), linear, 2.0f, 2.0f});

    // 4. Advanced Animations: Transform
    auto start_shape = create_circle({-300, 100}, 50).set_fill_color(SK_ColorRED).set_stroke_width(5.0f);
    auto end_shape = create_square({-150, 100}, 100).set_fill_color(SK_ColorGREEN).set_stroke_width(10.0f);
    add_animation(
        {start_shape, transform(end_shape), ease_in_out_sine, 3.0f, 2.0f});

    // 5. Grouping
    auto star = create_regular_polygon({0, 100}, 5, 50).set_stroke_color(SK_ColorWHITE);
    auto small_circle = create_circle({0, 100}, 20).set_fill_color(SK_ColorGRAY);
    auto group = group_mobjects({star, small_circle});
    add_animation({group, rotate(-360), ease_in_out_quad, 3.5f, 2.0f});
    add_animation({group, move_to({200, 100}), linear, 3.5f, 2.0f});

    // 6. Chained Animations
    auto chained_mobject = create_square({-200, 200}, 50).set_fill_color(SK_ColorORANGE);
    std::vector<Animation> chain_animations = {
        move_to({-100, 200}),
        scale(1.5f),
        rotate(180),
        move_to({0, 200})
    };
    add_animation({chained_mobject, chain(chain_animations), ease_in_out_quad, 6.0f, 4.0f});

    // 7. Styling with Shaders and Effects
    auto styled_circle = create_circle({-100, 0}, 40).set_fill_color(SK_ColorRED);
    add_animation({styled_circle, fade_in(), linear, 8.0f, 1.0f});

    auto gradient_square = create_square({100, 0}, 80);
    SkPoint pts[] = {{100, 0}, {180, 80}};
    SkColor colors[] = {SK_ColorBLUE, SK_ColorGREEN};
    sk_sp<SkShader> linear_sh = SkGradientShader::MakeLinear(pts, colors, nullptr, 2, SkTileMode::kClamp);
    gradient_square = gradient_square.set_shader(linear_sh).set_fill_color(SK_ColorWHITE); // Set fill color for shader to work
    add_animation({gradient_square, fade_in(), linear, 8.5f, 1.0f});

    auto blurred_text = create_text("Blur", font, {0, 200});
    sk_sp<SkMaskFilter> blur_filter = SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle, 5.0f);
    blurred_text = blurred_text.set_mask_filter(blur_filter).set_fill_color(SK_ColorWHITE);
    add_animation({blurred_text, fade_in(), linear, 9.0f, 1.0f});

  }

  void on_update(flecs::entity entity, flecs::world &world, float dt, float total_time) override {
    current_time = total_time;
  }

  void on_draw(flecs::entity entity, flecs::world &world, SkCanvas *canvas) override {
    auto mobjects_to_draw = get_mobjects_at_time(animation_tracks, current_time);
    for (const auto& m : mobjects_to_draw) {
        draw_mobject(canvas, m);
    }
  }

private:
  void add_animation(const AnimationTrack& track) {
    animation_tracks.push_back(track);
  }
};

// --- Factory Functions --- //
extern "C" {
IScript *create_script() { return new Main(); }
void destroy_script(IScript *script) { delete script; }
}
