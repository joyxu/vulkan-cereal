// Copyright (C) 2022 The Android Open Source Project
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "base/HealthMonitor.h"

#include <chrono>

#include "base/Metrics.h"
#include "TestClock.h"

namespace emugl {

using android::base::MetricEventHang;
using android::base::MetricEventType;
using android::base::MetricEventUnHang;
using android::base::MetricsLogger;
using android::base::TestClock;
using emugl::kDefaultIntervalMs;
using emugl::kDefaultTimeoutMs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Key;
using ::testing::Le;
using ::testing::MockFunction;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::Test;
using ::testing::VariantWith;

class HealthMonitorTest : public Test {
   protected:
    class MockLogger : public MetricsLogger {
       public:
        MOCK_METHOD(void, logMetricEvent, (MetricEventType eventType), (override));
    };

    HealthMonitorTest() : healthMonitor(logger, SToMs(1)) { TestClock::reset(); }

    ~HealthMonitorTest() { step(1); }

    void step(int seconds) {
        for (int i = 0; i < seconds; i++) {
            TestClock::advance(1);
            healthMonitor.poll().wait();
        }
    }

    int SToMs(int seconds) { return seconds * 1'000; }

    int defaultHangThresholdS = 5;
    MockLogger logger;
    HealthMonitor<TestClock> healthMonitor;
};

TEST_F(HealthMonitorTest, badTimeoutTimeTest) {
    int expectedHangThresholdS = 2;
    int expectedHangDurationS = 5;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventUnHang>(Field(
                        &MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS - 1)),
                                                           Le(SToMs(expectedHangDurationS + 1)))))))
            .Times(1);
    }

    auto id =
        healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(), std::nullopt, 1);
    step(expectedHangThresholdS + expectedHangDurationS);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, startTouchEndEventsTest) {
    EXPECT_CALL(logger, logMetricEvent(_)).Times(0);

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    healthMonitor.touchMonitoredTask(id);
    step(defaultHangThresholdS - 1);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, hangingStartEventTest) {
    EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);

    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + 1);
}

TEST_F(HealthMonitorTest, lateEndEventTest) {
    int expectedHangDurationS = 5;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventUnHang>(Field(
                        &MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS - 1)),
                                                           Le(SToMs(expectedHangDurationS + 1)))))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + expectedHangDurationS);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, taskHangsTwiceTest) {
    int expectedHangDurationS1 = 3;
    int expectedHangDurationS2 = 5;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS1 - 1)),
                                                         Le(SToMs(expectedHangDurationS1 + 1)))))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS2 - 1)),
                                                         Le(SToMs(expectedHangDurationS2 + 1)))))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + expectedHangDurationS1);
    healthMonitor.touchMonitoredTask(id);
    step(defaultHangThresholdS + expectedHangDurationS2);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, taskHangsThriceTest) {
    int expectedHangDurationS1 = 3;
    int expectedHangDurationS2 = 5;
    int expectedHangDurationS3 = 3;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS1 - 1)),
                                                         Le(SToMs(expectedHangDurationS1 + 1)))))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS2 - 1)),
                                                         Le(SToMs(expectedHangDurationS2 + 1)))))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS3 - 1)),
                                                         Le(SToMs(expectedHangDurationS3 + 1)))))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + expectedHangDurationS1);
    healthMonitor.touchMonitoredTask(id);
    step(defaultHangThresholdS + expectedHangDurationS2);
    healthMonitor.touchMonitoredTask(id);
    step(defaultHangThresholdS + expectedHangDurationS3);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, multipleHangingTasksTest) {
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 0))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 1))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 2))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 3))))
            .Times(1);
    }

    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    TestClock::advance(0.2);
    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    TestClock::advance(0.2);
    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    TestClock::advance(0.2);
    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + 1);
}

TEST_F(HealthMonitorTest, oneHangingTaskOutOfTwoTest) {
    EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);

    healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    auto id2 = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    healthMonitor.stopMonitoringTask(id2);
    step(2);
}

TEST_F(HealthMonitorTest, twoTasksHangNonOverlappingTest) {
    int expectedHangDurationS1 = 5;
    int hangThresholdS2 = 10;
    int expectedHangDurationS2 = 2;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS1 - 1)),
                                                         Le(SToMs(expectedHangDurationS1 + 1)))))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(_))).Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS2 - 1)),
                                                         Le(SToMs(expectedHangDurationS2 + 1)))))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + expectedHangDurationS1);
    healthMonitor.stopMonitoringTask(id);
    step(1);
    id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(), std::nullopt,
                                           SToMs(hangThresholdS2));
    step(hangThresholdS2 + expectedHangDurationS2);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, twoTasksHangOverlappingTest) {
    int expectedHangDurationS1 = 5;
    int expectedHangDurationS2 = 8;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 0))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 1))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS1 - 1)),
                                                         Le(SToMs(expectedHangDurationS1 + 1)))))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(
                Field(&MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS2 - 1)),
                                                         Le(SToMs(expectedHangDurationS2 + 1)))))))
            .Times(1);
    }

    auto id1 = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(3);
    auto id2 = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(7);
    healthMonitor.stopMonitoringTask(id1);
    step(5);
    healthMonitor.stopMonitoringTask(id2);
}

TEST_F(HealthMonitorTest, simultaneousTasks) {
    int expectedHangDurationS = 5;
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 0))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(VariantWith<MetricEventHang>(
                                Field(&MetricEventHang::otherHungTasks, 1))))
            .Times(1);
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventUnHang>(Field(
                        &MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS - 1)),
                                                           Le(SToMs(expectedHangDurationS + 1)))))))
            .Times(1);
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventUnHang>(Field(
                        &MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS - 1)),
                                                           Le(SToMs(expectedHangDurationS + 1)))))))
            .Times(1);
    }
    auto id1 = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    auto id2 = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS + expectedHangDurationS);
    healthMonitor.stopMonitoringTask(id1);
    healthMonitor.stopMonitoringTask(id2);
}

TEST_F(HealthMonitorTest, taskHungWithAttachedCallback) {
    MockFunction<std::unique_ptr<HangAnnotations>()> mockCallback;
    std::unique_ptr<HangAnnotations> testAnnotations = std::make_unique<HangAnnotations>();
    testAnnotations->insert({{"key1", "value1"}, {"key2", "value2"}});
    int expectedHangDurationS = 5;
    {
        InSequence s;
        EXPECT_CALL(mockCallback, Call()).WillOnce(Return(ByMove(std::move(testAnnotations))));
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventHang>(Field(
                        &MetricEventHang::metadata,
                        Field(&EventHangMetadata::data,
                              Pointee(AllOf(Contains(Key("key1")), Contains(Key("key2")))))))))
            .Times(1);
        EXPECT_CALL(logger,
                    logMetricEvent(VariantWith<MetricEventUnHang>(Field(
                        &MetricEventUnHang::hung_ms, AllOf(Ge(SToMs(expectedHangDurationS - 1)),
                                                           Le(SToMs(expectedHangDurationS + 1)))))))
            .Times(1);
    }
    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                mockCallback.AsStdFunction());
    step(defaultHangThresholdS + expectedHangDurationS);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, healthyTaskWithParent) {
    EXPECT_CALL(logger, logMetricEvent(_)).Times(0);

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    auto child = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                   std::nullopt, kDefaultTimeoutMs, id);
    step(defaultHangThresholdS - 1);
    healthMonitor.stopMonitoringTask(child);
    step(defaultHangThresholdS - 1);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, threeChainOfHungTasks) {
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 2))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 1))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 0))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 2))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 1))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 0))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    auto child = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                   std::nullopt, kDefaultTimeoutMs, id);
    step(defaultHangThresholdS - 1);
    auto grandchild = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                        std::nullopt, kDefaultTimeoutMs, child);
    step(defaultHangThresholdS + 1);
    healthMonitor.touchMonitoredTask(grandchild);
    step(1);
    healthMonitor.stopMonitoringTask(grandchild);
    healthMonitor.stopMonitoringTask(child);
    healthMonitor.stopMonitoringTask(id);
}

TEST_F(HealthMonitorTest, parentEndsBeforeChild) {
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 1))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 1))))
            .Times(1);
    }

    auto id = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    auto child = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                   std::nullopt, kDefaultTimeoutMs, id);
    healthMonitor.stopMonitoringTask(id);
    step(defaultHangThresholdS + 1);
    healthMonitor.stopMonitoringTask(child);
}

TEST_F(HealthMonitorTest, siblingsHangParentStillHealthy) {
    {
        InSequence s;
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 1))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 1))))
            .Times(1);
        EXPECT_CALL(logger, logMetricEvent(
                                VariantWith<MetricEventHang>(Field(&MetricEventHang::taskId, 2))))
            .Times(1);
        EXPECT_CALL(
            logger,
            logMetricEvent(VariantWith<MetricEventUnHang>(Field(&MetricEventUnHang::taskId, 2))))
            .Times(1);
    }

    auto parent = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>());
    step(defaultHangThresholdS - 1);
    /* 1 */ auto child = healthMonitor.startMonitoringTask(std::make_unique<EventHangMetadata>(),
                                                           std::nullopt, kDefaultTimeoutMs, parent);
    step(defaultHangThresholdS - 1);
    /* 2 */ auto secondChild = healthMonitor.startMonitoringTask(
        std::make_unique<EventHangMetadata>(), std::nullopt, kDefaultTimeoutMs, parent);
    step(2);
    healthMonitor.stopMonitoringTask(child);
    step(defaultHangThresholdS - 1);
    healthMonitor.stopMonitoringTask(secondChild);
    healthMonitor.stopMonitoringTask(parent);
}

}  // namespace emugl
