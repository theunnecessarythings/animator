#ifndef TIMELINE_H
#define TIMELINE_H

#include "mobject.h"
#include "animation.h"
#include "easing.h"
#include <vector>

// Represents a single animation track for a Mobject.
struct AnimationTrack {
    Mobject mobject;
    Animation animation;
    EasingFunc easing = linear;
    float start_time = 0.0f;
    float duration = 1.0f;
};

// Calculates the state of all mobjects at a given time.
// This is a pure function that takes the tracks and time, and returns the rendered mobjects.
inline std::vector<Mobject> get_mobjects_at_time(const std::vector<AnimationTrack>& tracks, float current_time) {
    std::vector<Mobject> rendered_mobjects;
    for (const auto& track : tracks) {
        float t = 0;
        float anim_time = (current_time - track.start_time) / track.duration;

        if (anim_time >= 0.0f && anim_time <= 1.0f) {
            t = track.easing(anim_time);
            rendered_mobjects.push_back(track.animation(track.mobject, t));
        } else if (anim_time > 1.0f) {
            rendered_mobjects.push_back(track.animation(track.mobject, 1.0f));
        }
        // If anim_time < 0, the object is not yet visible.
    }
    return rendered_mobjects;
}

#endif // TIMELINE_H
