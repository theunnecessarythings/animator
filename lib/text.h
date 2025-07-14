#ifndef TEXT_H
#define TEXT_H

#include "include/core/SkFont.h"
#include "include/core/SkPath.h"
#include "include/core/SkTextBlob.h"
#include "mobject.h"
#include <vector>

// Creates a text mobject, centered at the given position.
inline Mobject create_text(const std::string &text, const SkFont &font,
                           const SkPoint &pos) {
  Mobject m;
  SkPath text_path;

  // Get glyphs for the text
  std::vector<SkGlyphID> glyphs(text.length());
  font.textToGlyphs(text.c_str(), text.length(), SkTextEncoding::kUTF8, glyphs);

  // Get positions for the glyphs
  std::vector<SkPoint> glyph_pos(text.length());
  font.getPos(glyphs, glyph_pos, {0, 0});

  // Iterate through glyphs and add their paths to the main text_path
  for (size_t i = 0; i < glyphs.size(); ++i) {
    SkPath glyph_path;
    if (font.getPath(glyphs[i], &glyph_path)) {
      // Offset the glyph path by its position
      glyph_path.offset(glyph_pos[i].x(), glyph_pos[i].y());
      text_path.addPath(glyph_path);
    }
  }

  // Center the entire text path at the origin
  SkRect bounds = text_path.getBounds();
  text_path.offset(-bounds.centerX(), -bounds.centerY());

  // Move the centered path to the desired position
  text_path.offset(pos.x(), pos.y());

  m.path = text_path;
  return m;
}

#endif // TEXT_H
