// This is the precompiled header for C++ scripts.
// It includes all common and heavy-header libraries that scripts might need.
#pragma once

// C++ Standard Library
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Flecs
#define FLECS_CPP
#include <flecs.h>

// Skia
#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImage.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathEffect.h"
#include "include/effects/SkCornerPathEffect.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkDiscretePathEffect.h"

// Engine Headers
#include "cpp_script_interface.h"
#include "ecs.h"
