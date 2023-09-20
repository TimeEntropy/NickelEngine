#pragma once

#ifndef TOML_EXCEPTIONS
#define TOML_EXCEPTIONS 0
#endif
#include "toml++/toml.hpp"

#include "core/assert.hpp"

#include "stb_image.h"
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GECS_ASSERT(expr, msg) Assert(expr, msg)
#include "gecs/gecs.hpp"

#define MIRROW_ASSERT(expr, msg) Assert(expr, msg)
#include "mirrow/drefl/drefl.hpp"
#include "mirrow/serd/dynamic/backends/tomlplusplus.hpp"
#include "mirrow/serd/static/backends/tomlplusplus.hpp"
#include "mirrow/srefl/reflect.hpp"
#include "mirrow/util/const_str.hpp"
#include "mirrow/util/function_traits.hpp"
#include "mirrow/util/misc.hpp"
#include "mirrow/util/type_list.hpp"
#include "mirrow/util/variable_traits.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif