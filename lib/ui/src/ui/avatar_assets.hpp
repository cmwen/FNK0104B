#pragma once

// Board-independent 32x32 RGB565 pixel art, row-major with (0, 0) top left.
#include <stdint.h>

namespace ui {
namespace avatar {

enum class Mood : uint8_t { Idle, Thinking, NeedsInput, Error };

struct Canvas {
  uint16_t pixels[32 * 32];
};

namespace detail {
static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xf8u) << 8) | ((g & 0xfcu) << 3) | (b >> 3));
}

static inline void pixel(Canvas& c, int x, int y, uint16_t color) {
  if (x >= 0 && x < 32 && y >= 0 && y < 32) c.pixels[y * 32 + x] = color;
}

static inline void rect(Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) pixel(c, x, y, color);
}

static inline void roundRect(Canvas& c, int x0, int y0, int x1, int y1,
                             int radius, uint16_t color) {
  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      const int dx = x < x0 + radius ? x0 + radius - x
                   : x > x1 - radius ? x - (x1 - radius) : 0;
      const int dy = y < y0 + radius ? y0 + radius - y
                   : y > y1 - radius ? y - (y1 - radius) : 0;
      if (dx * dx + dy * dy <= radius * radius) pixel(c, x, y, color);
    }
  }
}

static inline void oval(Canvas& c, int cx, int cy, int rx, int ry, uint16_t color) {
  for (int y = -ry; y <= ry; ++y)
    for (int x = -rx; x <= rx; ++x)
      if (x * x * ry * ry + y * y * rx * rx <= rx * rx * ry * ry)
        pixel(c, cx + x, cy + y, color);
}

static inline void line(Canvas& c, int x0, int y0, int x1, int y1, uint16_t color) {
  const int dx = x1 >= x0 ? x1 - x0 : x0 - x1;
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = y1 >= y0 ? y0 - y1 : y1 - y0;
  const int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(c, x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// The robot bobs over a fixed shadow. All its layers use the same offset.
struct RobotPainter {
  Canvas& canvas;
  int rise;
  void pixel(int x, int y, uint16_t color) {
    detail::pixel(canvas, x, y - rise, color);
  }
  void rect(int x0, int y0, int x1, int y1, uint16_t color) {
    detail::rect(canvas, x0, y0 - rise, x1, y1 - rise, color);
  }
  void roundRect(int x0, int y0, int x1, int y1, int radius, uint16_t color) {
    detail::roundRect(canvas, x0, y0 - rise, x1, y1 - rise, radius, color);
  }
  void line(int x0, int y0, int x1, int y1, uint16_t color) {
    detail::line(canvas, x0, y0 - rise, x1, y1 - rise, color);
  }
};

struct Palette { uint16_t bright, light, dark; };
static constexpr Palette kPalettes[] = {
    {rgb(41, 226, 244), rgb(137, 247, 251), rgb(30, 118, 183)},
    {rgb(96, 182, 255), rgb(173, 221, 255), rgb(64, 90, 197)},
    {rgb(179, 113, 249), rgb(224, 183, 255), rgb(104, 61, 190)},
    {rgb(255, 112, 207), rgb(255, 184, 234), rgb(158, 69, 178)},
};
}  // namespace detail

// FNV-1a gives stable identity variation across runs and platforms.
static inline uint32_t hashId(const char* text) {
  uint32_t hash = 2166136261u;
  if (!text) return hash;
  while (*text) {
    hash ^= static_cast<uint8_t>(*text++);
    hash *= 16777619u;
  }
  return hash;
}

// Draws one opaque, floating robot portrait. Its stable identity chooses a
// palette, headset module, and code crest; mood changes only the expression.
static inline void render(Canvas& c, uint32_t identity, Mood mood, uint32_t tick) {
  using namespace detail;
  constexpr uint16_t ink = rgb(22, 17, 57);
  constexpr uint16_t rim = rgb(81, 51, 150);
  constexpr uint16_t shellShadow = rgb(177, 160, 231);
  constexpr uint16_t shell = rgb(244, 245, 255);
  constexpr uint16_t shellHighlight = rgb(255, 255, 255);
  constexpr uint16_t face = rgb(28, 25, 68);
  constexpr uint16_t faceShade = rgb(49, 37, 103);
  constexpr uint16_t ground = rgb(237, 241, 255);
  constexpr uint16_t shadow = rgb(209, 197, 249);
  constexpr uint16_t error = rgb(248, 80, 139);
  const Palette& accent = kPalettes[identity % 4u];
  const unsigned module = (identity >> 8) % 3u;
  const unsigned badge = (identity >> 16) % 4u;
  const unsigned eyeStyle = (identity >> 20) % 3u;
  RobotPainter robot{c, static_cast<int>((tick / 6u) % 2u)};

  // Pale tile and fixed shadow make the one-pixel hover motion visible.
  rect(c, 0, 0, 31, 31, ground);
  roundRect(c, 9, 28, 22, 30, 1, shadow);
  rect(c, 12, 29, 19, 29, rgb(177, 156, 237));
  pixel(c, 2, 7, accent.light); pixel(c, 3, 6, accent.bright);
  pixel(c, 4, 7, accent.light); pixel(c, 3, 8, accent.bright);
  pixel(c, 28, 5, accent.light); pixel(c, 29, 4, accent.bright);
  pixel(c, 30, 5, accent.light); pixel(c, 29, 6, accent.bright);

  // Jet and ear modules are behind the main shell.
  robot.roundRect(12, 23, 19, 27, 2, ink);
  robot.rect(13, 25, 18, 26, accent.dark);
  robot.rect(14, 26, 17, 27, accent.bright);
  robot.roundRect(2, 12, 7, 22, 2, ink);
  robot.roundRect(3, 13, 7, 21, 2, rim);
  robot.rect(3, 16, 5, 19, accent.dark);
  robot.pixel(4, 17, accent.bright);
  robot.roundRect(24, 12, 29, 22, 2, ink);
  robot.roundRect(24, 13, 28, 21, 2, rim);
  robot.rect(26, 16, 28, 19, accent.dark);
  robot.pixel(27, 17, accent.bright);
  if (module == 1u) {
    robot.rect(3, 10, 4, 12, ink);
    robot.pixel(3, 9, accent.bright);
    robot.rect(27, 10, 28, 12, ink);
    robot.pixel(28, 9, accent.bright);
  } else if (module == 2u) {
    robot.rect(2, 15, 3, 19, accent.bright);
    robot.rect(28, 15, 29, 19, accent.bright);
    robot.rect(28, 21, 30, 23, ink);
    robot.rect(29, 21, 30, 22, shell);
    robot.pixel(31, 20, accent.bright);
  }

  // Dark outline, lavender shading, white helmet, and purple visor.
  robot.roundRect(4, 6, 27, 25, 7, ink);
  robot.roundRect(5, 7, 26, 24, 7, rim);
  robot.roundRect(5, 7, 26, 23, 7, shellShadow);
  robot.roundRect(6, 7, 25, 22, 6, shell);
  robot.line(8, 10, 11, 7, shellHighlight);
  robot.line(20, 7, 23, 10, shellHighlight);
  robot.line(8, 23, 12, 25, shellHighlight);
  robot.roundRect(7, 10, 24, 22, 4, ink);
  robot.roundRect(8, 11, 23, 21, 3, faceShade);
  robot.roundRect(8, 12, 23, 21, 3, face);
  robot.rect(10, 11, 21, 11, accent.dark);

  // Every agent gets one of four tiny code crests.
  robot.roundRect(9, 4, 22, 10, 2, ink);
  robot.roundRect(10, 5, 21, 9, 1, accent.dark);
  robot.rect(11, 5, 20, 5, accent.light);
  if (badge == 0u) {
    robot.pixel(12, 7, accent.bright); robot.pixel(13, 6, accent.bright);
    robot.pixel(13, 8, accent.bright); robot.pixel(15, 8, accent.bright);
    robot.pixel(16, 7, accent.bright); robot.pixel(17, 6, accent.bright);
    robot.pixel(19, 6, accent.bright); robot.pixel(20, 7, accent.bright);
    robot.pixel(19, 8, accent.bright);
  } else if (badge == 1u) {
    robot.pixel(13, 7, accent.bright); robot.pixel(14, 6, accent.bright);
    robot.pixel(14, 8, accent.bright); robot.pixel(17, 6, accent.bright);
    robot.pixel(17, 8, accent.bright); robot.pixel(18, 7, accent.bright);
  } else if (badge == 2u) {
    robot.rect(12, 7, 14, 7, accent.bright);
    robot.pixel(17, 6, accent.bright); robot.pixel(18, 7, accent.bright);
    robot.pixel(17, 8, accent.bright);
  } else {
    robot.pixel(14, 6, accent.bright); robot.rect(13, 7, 15, 7, accent.bright);
    robot.pixel(14, 8, accent.light); robot.pixel(18, 7, accent.bright);
  }

  const bool blink = mood == Mood::Idle && tick % 30u == 0u;
  const int gaze = mood == Mood::Thinking
                       ? static_cast<int>((tick / 4u) % 3u) - 1 : 0;
  if (mood == Mood::Error) {
    robot.line(10, 16, 13, 18, error);
    robot.line(18, 18, 21, 16, error);
    robot.pixel(11, 16, shellHighlight);
    robot.pixel(20, 16, shellHighlight);
  } else if (blink) {
    robot.rect(10, 17, 13, 17, accent.bright);
    robot.rect(18, 17, 21, 17, accent.bright);
  } else if (mood == Mood::Idle && eyeStyle == 1u) {
    robot.line(10, 18, 12, 16, accent.bright);
    robot.line(12, 16, 14, 18, accent.bright);
    robot.line(18, 18, 20, 16, accent.bright);
    robot.line(20, 16, 22, 18, accent.bright);
  } else {
    const int eyeXs[] = {10, 18};
    for (int x : eyeXs) {
      robot.roundRect(x, 14, x + 4, 19, 2, accent.dark);
      robot.roundRect(x, 15, x + 4, 19, 2, accent.bright);
      if (mood == Mood::Idle && eyeStyle == 2u) {
        robot.pixel(x + 2, 18, face);
      } else {
        robot.rect(x + 1 + gaze, 16, x + 2 + gaze, 17, face);
      }
      robot.pixel(x + 1 + gaze, 15, shellHighlight);
    }
  }

  if (mood == Mood::Idle) {
    robot.pixel(15, 19, accent.light);
    robot.rect(15, 20, 16, 20, accent.bright);
  } else if (mood == Mood::Thinking) {
    robot.rect(15, 20, 17, 20, accent.light);
    robot.pixel(26, 4, accent.bright);
    robot.pixel(28, 3, accent.bright);
  } else if (mood == Mood::NeedsInput) {
    robot.roundRect(15, 19, 17, 21, 1, accent.bright);
    robot.pixel(16, 20, face);
    if ((tick / 5u) % 2u == 0u) {
      robot.rect(28, 3, 29, 6, accent.dark);
      robot.pixel(28, 8, accent.bright);
      robot.pixel(29, 8, accent.bright);
    }
  } else {
    robot.line(14, 21, 16, 19, error);
    robot.line(16, 19, 18, 21, error);
    if ((tick / 5u) % 2u == 0u) {
      robot.rect(8, 11, 23, 11, error);
      robot.pixel(29, 5, error);
    }
  }
  robot.roundRect(13, 23, 18, 25, 1, ink);
  robot.rect(14, 24, 17, 24, accent.bright);
  pixel(c, 14, 27, accent.light);
  pixel(c, 18, 28, accent.bright);

  // A distinct tile border and large speech symbol keep states legible even
  // when four portraits share the 240-pixel-high display.
  if (mood != Mood::Idle) {
    const uint16_t status = mood == Mood::Thinking ? rgb(42, 202, 237)
                            : mood == Mood::NeedsInput ? rgb(255, 179, 49)
                            : error;
    rect(c, 0, 0, 31, 1, status);
    rect(c, 0, 30, 31, 31, status);
    rect(c, 0, 0, 1, 31, status);
    rect(c, 30, 0, 31, 31, status);
    robot.roundRect(24, 2, 31, 11, 2, ink);
    robot.roundRect(25, 3, 30, 10, 1, shellHighlight);
    if (mood == Mood::Thinking) {
      for (int dot = 0; dot < 3; ++dot) {
        const bool active = static_cast<int>((tick / 3u) % 3u) == dot;
        robot.rect(26 + dot * 2, 6, 26 + dot * 2, 7,
                   active ? status : accent.dark);
      }
    } else if (mood == Mood::NeedsInput) {
      robot.rect(27, 5, 29, 5, status);
      robot.pixel(29, 6, status);
      robot.pixel(28, 7, status);
      robot.pixel(28, 9, status);
    } else {
      robot.rect(28, 4, 28, 7, status);
      robot.pixel(28, 9, status);
    }
  }
}

}  // namespace avatar
}  // namespace ui
