# TODO List

## src/meta.cpp
- [ ] Add font data and other resources here (line 247)

## src/draw/draw.cpp
- [ ] Implement proper stack with history (lines 127, 136, 145, 154)
- [ ] Proper hash implementation (line 330)
- [ ] Use actual texture handle from piece (lines 418, 455)
- [ ] Draw underline/strikethrough if needed (line 463)

## src/renderer/renderer_vulkan_passes.cpp
- [ ] Implement blur pass (line 305)

## src/shaders/glsl/blur.frag
- [ ] Implement blur fragment shader (line 3)

## src/font/font_cache.cpp
- [ ] Update max free sizes in parents (line 331)
- [ ] Implement region release (line 342)
- [ ] Calculate column width (line 408)
- [ ] Implement proper atlas packing with correct data upload (line 497)
- [ ] Implement character position from pixel position (line 558)

## src/font/font_cache.h
- [ ] Replace with renderer texture handle when available (line 23)
- [ ] Replace with renderer texture handle (line 146)

## src/font/font.h
- [ ] Do we need to record height if we are doing the SDF?? (line 32)
- [ ] We want to not use the stbtt types here and have this be under a specific impl when we want to use other providers or when we want to have our own (line 72)

## src/main.cpp
- [ ] Should not need to do this in a main file (line 244)

## src/base/arena.cpp
- [ ] Need to implement getting the large page size (line 16)

## third_party/stb_truetype.h
Note: These are from a third-party library and should probably not be modified directly:
- Various TODOs in stb_truetype.h (lines 923, 1418, 1426, 1514, 1584, 1837, 2027, 2169, 2494, 3184, 3554, 3577, 4886)