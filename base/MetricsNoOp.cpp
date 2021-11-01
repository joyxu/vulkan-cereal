// Copyright (C) 2021 The Android Open Source Project
// Copyright (C) 2021 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/Metrics.h"

namespace android {
namespace base {

class MetricsLogLibNoOp : public MetricsLogger {
    void logMetricEvent(MetricEventType eventType) override {}
};

std::unique_ptr<MetricsLogger> CreateMetricsLogger() {
    return std::make_unique<MetricsLogLibNoOp>();
}

}  // namespace base
}  // namespace android