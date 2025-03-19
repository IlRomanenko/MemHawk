#pragma once

#include <absl/base/optimization.h>
#include <absl/base/prefetch.h>

#define likely(x) ABSL_PREDICT_TRUE(x)
#define unlikely(x) ABSL_PREDICT_FALSE(x)
