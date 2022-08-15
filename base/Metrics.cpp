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

#include <memory>
#include <sstream>
#include <variant>

#include "host-common/logging.h"

namespace android {
namespace base {

// These correspond to events defined in
// google3/play/games/battlestar/logging/event_codes.proto
constexpr int64_t kEmulatorGraphicsFreeze = 10009;
constexpr int64_t kEmulatorGraphicsUnfreeze = 10010;
constexpr int64_t kEmulatorGfxstreamVkAbortReason = 10011;
constexpr int64_t kEmulatorGraphicsHangRenderThread = 10024;
constexpr int64_t kEmulatorGraphicsUnHangRenderThread = 10025;
constexpr int64_t kEmulatorGraphicsHangSyncThread = 10026;
constexpr int64_t kEmulatorGraphicsUnHangSyncThread = 10027;

void (*MetricsLogger::add_instant_event_callback)(int64_t event_code) = nullptr;
void (*MetricsLogger::add_instant_event_with_descriptor_callback)(int64_t event_code,
                                                                  int64_t descriptor) = nullptr;
void (*MetricsLogger::add_instant_event_with_metric_callback)(int64_t event_code,
                                                              int64_t metric_value) = nullptr;
void (*MetricsLogger::set_crash_annotation_callback)(const char* key, const char* value) = nullptr;

void logEventHangMetadata(const EventHangMetadata* metadata) {
    ERR("Metadata:");
    ERR("\t file: %s", metadata->file);
    ERR("\t function: %s", metadata->function);
    ERR("\t line: %d", metadata->line);
    ERR("\t msg: %s", metadata->msg);
    if (metadata->data) {
        ERR("\t Additional information:");
        for (auto& [key, value] : *metadata->data) {
            ERR("\t \t %s: %s", key.c_str(), value.c_str());
        }
    }
}

struct MetricTypeVisitor {
    void operator()(const std::monostate /*_*/) const { ERR("MetricEventType not initialized"); }

    void operator()(const MetricEventFreeze freezeEvent) const {
        if (MetricsLogger::add_instant_event_callback) {
            MetricsLogger::add_instant_event_callback(kEmulatorGraphicsFreeze);
        }
    }

    void operator()(const MetricEventUnFreeze unfreezeEvent) const {
        if (MetricsLogger::add_instant_event_with_metric_callback) {
            MetricsLogger::add_instant_event_with_metric_callback(kEmulatorGraphicsUnfreeze,
                                                                  unfreezeEvent.frozen_ms);
        }
    }

    void operator()(const MetricEventHang hangEvent) const {
        // Logging a hang event will trigger a crash report upload. If crash reporting is enabled,
        // the set_annotation_callback will be populated.
        if (MetricsLogger::set_crash_annotation_callback) {
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_file",
                                                         hangEvent.metadata->file);
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_function",
                                                         hangEvent.metadata->function);
            MetricsLogger::set_crash_annotation_callback(
                "gfxstream_hang_line", std::to_string(hangEvent.metadata->line).c_str());
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_msg",
                                                         hangEvent.metadata->msg);
            if (hangEvent.metadata->data) {
                for (auto& [key, value] : *hangEvent.metadata->data) {
                    MetricsLogger::set_crash_annotation_callback(key.c_str(), value.c_str());
                }
            }
        }

        ERR("Logging hang event. Number of tasks already hung: %d", hangEvent.otherHungTasks);
        logEventHangMetadata(hangEvent.metadata);
        if (MetricsLogger::add_instant_event_with_metric_callback) {
            switch (hangEvent.metadata->hangType) {
                case EventHangMetadata::HangType::kRenderThread: {
                    MetricsLogger::add_instant_event_with_metric_callback(
                        kEmulatorGraphicsHangRenderThread, hangEvent.otherHungTasks);
                    break;
                }
                case EventHangMetadata::HangType::kSyncThread: {
                    MetricsLogger::add_instant_event_with_metric_callback(
                        kEmulatorGraphicsHangSyncThread, hangEvent.otherHungTasks);
                    break;
                }
            }
        }

        // We have to unset all annotations since this is not necessarily a fatal crash
        // and we need to ensure we don't pollute future crash reports.
        if (MetricsLogger::set_crash_annotation_callback) {
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_file", "");
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_function", "");
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_line", "");
            MetricsLogger::set_crash_annotation_callback("gfxstream_hang_msg", "");
            if (hangEvent.metadata->data) {
                for (auto& [key, value] : *hangEvent.metadata->data) {
                    MetricsLogger::set_crash_annotation_callback(key.c_str(), "");
                }
            }
        }
    }

    void operator()(const MetricEventUnHang unHangEvent) const {
        ERR("Logging unhang event. Hang time: %d ms", unHangEvent.hung_ms);
        logEventHangMetadata(unHangEvent.metadata);
        if (MetricsLogger::add_instant_event_with_metric_callback) {
            switch (unHangEvent.metadata->hangType) {
                case EventHangMetadata::HangType::kRenderThread: {
                    MetricsLogger::add_instant_event_with_metric_callback(
                        kEmulatorGraphicsUnHangRenderThread, unHangEvent.hung_ms);
                    break;
                }
                case EventHangMetadata::HangType::kSyncThread: {
                    MetricsLogger::add_instant_event_with_metric_callback(
                        kEmulatorGraphicsUnHangSyncThread, unHangEvent.hung_ms);
                    break;
                }
            }
        }
    }

    void operator()(const GfxstreamVkAbort abort) const {
        // Ensure clearcut logs are uploaded before aborting.
        if (MetricsLogger::add_instant_event_with_descriptor_callback) {
            MetricsLogger::add_instant_event_with_descriptor_callback(
                kEmulatorGfxstreamVkAbortReason, abort.abort_reason);
        }

        if (MetricsLogger::set_crash_annotation_callback) {
            MetricsLogger::set_crash_annotation_callback("gfxstream_abort_file", abort.file);
            MetricsLogger::set_crash_annotation_callback("gfxstream_abort_function",
                                                         abort.function);
            MetricsLogger::set_crash_annotation_callback("gfxstream_abort_line",
                                                         std::to_string(abort.line).c_str());
            MetricsLogger::set_crash_annotation_callback(
                "gfxstream_abort_code", std::to_string(abort.abort_reason).c_str());
            MetricsLogger::set_crash_annotation_callback("gfxstream_abort_msg", abort.msg);
        }
    }
};

// MetricsLoggerImpl
class MetricsLoggerImpl : public MetricsLogger {
    void logMetricEvent(MetricEventType eventType) override {
        std::visit(MetricTypeVisitor(), eventType);
    }
};

std::unique_ptr<MetricsLogger> CreateMetricsLogger() {
    return std::make_unique<MetricsLoggerImpl>();
}

}  // namespace base
}  // namespace android