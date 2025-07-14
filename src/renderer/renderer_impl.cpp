#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../base/arena.h"
#include "../base/base.h"
#include "../types.h"
#include "renderer_core.h"
#include "renderer_opengl.h"

// Move the actual implementation from renderer_opengl.cpp here to avoid the include order issue