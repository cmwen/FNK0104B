#!/usr/bin/env python3
"""Render actual recorder draw code and IDF glyphs on a host framebuffer."""
from pathlib import Path
import subprocess
import tempfile
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    output = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/recorder-preview.ppm")
    display = (ROOT / "lib/fnk0104b/src/idf_display.cpp").read_text()
    glyphs = display[display.index("constexpr uint8_t kLetters"):display.index("bool transferDone")]
    screen = (ROOT / "apps/20-recorder/src/screen.cpp").read_text()
    colors = screen[screen.index("constexpr uint16_t bg"):screen.index("void touchTask(")]
    text = display[display.index("void idfDisplayText("):display.index("esp_err_t flushIdfDisplay")]
    text = text.replace("int scale)", "int scale = 1)")
    source = '''#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include "screen.hpp"
namespace fnk0104b {
constexpr int kIdfDisplayWidth = 320;
uint16_t pixels[320*240] = {};
void idfDisplayFill(int x, int y, int w, int h, uint16_t color) {
  assert(x >= 0 && y >= 0 && x+w <= 320 && y+h <= 240);
  for(int j=y; j<y+h; ++j) for(int i=x; i<x+w; ++i) pixels[j*320+i] = color;
}
''' + glyphs + text + '}\nnamespace recorder_screen {\n' + colors + '''}
int main(int argc, char** argv) {
  recorder_screen::Snapshot state;
  state.count=4; state.seconds=83; state.peak=2000; state.speech=true;
  for(unsigned i=0;i<3;++i) std::snprintf(state.files[i],24,"REC%08u.opus",4-i);
  for (auto phase : {recorder_screen::Phase::Loading, recorder_screen::Phase::Ready,
      recorder_screen::Phase::Saving, recorder_screen::Phase::Playing,
      recorder_screen::Phase::Error, recorder_screen::Phase::Recording}) {
    state.phase=phase;
    std::snprintf(state.message,48,"Silence saves. Tap STOP to save now.");
    recorder_screen::draw(state);
  }
  FILE* file=std::fopen(argv[1],"wb"); if(!file) return 1;
  std::fprintf(file,"P6\\n320 240\\n255\\n");
  for(auto c : fnk0104b::pixels) {
    uint8_t rgb[]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
    std::fwrite(rgb,1,3,file);
  }
  return std::fclose(file);
}
'''
    with tempfile.TemporaryDirectory(prefix="fnk-recorder-ui-") as folder:
        path = Path(folder)
        (path / "preview.cpp").write_text(source)
        subprocess.run(["c++", "-std=c++17", "-I" + str(ROOT / "apps/20-recorder/src"),
                        str(path / "preview.cpp"), "-o", str(path / "preview")], check=True)
        subprocess.run([str(path / "preview"), str(output)], check=True)
    print(f"All recorder states fit 320x240. Preview: {output}")


if __name__ == "__main__":
    main()
