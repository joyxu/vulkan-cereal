// Copyright 2021 The Android Open Source Project
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
#include "VirtioGpuTimelines.h"

#include <cinttypes>
#include <cstdio>

#define VGT_ERROR(fmt, ...)                                                   \
    do {                                                                      \
        fprintf(stderr, "%s(%s:%d): " fmt "\n", __func__, __FILE__, __LINE__, \
                ##__VA_ARGS__);                                               \
        fflush(stderr);                                                       \
    } while (0)

using TaskId = VirtioGpuTimelines::TaskId;
using CtxId = VirtioGpuTimelines::CtxId;
using FenceId = VirtioGpuTimelines::FenceId;
using AutoLock = android::base::AutoLock;

VirtioGpuTimelines::VirtioGpuTimelines() : mNextId(0) {}

TaskId VirtioGpuTimelines::enqueueTask(CtxId ctxId) {
    AutoLock lock(mLock);

    TaskId id = mNextId++;
    auto task = std::make_shared<Task>(id, ctxId);
    mTaskIdToTask[id] = task;
    mTimelineQueues[ctxId].emplace_back(std::move(task));
    return id;
}

void VirtioGpuTimelines::enqueueFence(
    CtxId ctxId, FenceId, FenceCompletionCallback fenceCompletionCallback) {
    AutoLock lock(mLock);

    auto fence = std::make_unique<Fence>(fenceCompletionCallback);
    mTimelineQueues[ctxId].emplace_back(std::move(fence));
    poll_locked(ctxId);
}

void VirtioGpuTimelines::notifyTaskCompletion(TaskId taskId) {
    AutoLock lock(mLock);
    auto iTask = mTaskIdToTask.find(taskId);
    if (iTask == mTaskIdToTask.end()) {
        VGT_ERROR("Task(id = %" PRIu64 ") can't be found.",
                  static_cast<uint64_t>(taskId));
        ::std::abort();
    }
    std::shared_ptr<Task> task = iTask->second.lock();
    if (task == nullptr) {
        VGT_ERROR("Task(id = %" PRIu64 ") has been destroyed.",
                  static_cast<uint64_t>(taskId));
        ::std::abort();
    }
    if (task->mId != taskId) {
        VGT_ERROR("Task id mismatch. Expected %" PRIu64 ". Actual %" PRIu64,
                  static_cast<uint64_t>(taskId),
                  static_cast<uint64_t>(task->mId));
        ::std::abort();
    }
    if (task->mHasCompleted) {
        VGT_ERROR("Task(id = %" PRIu64 ") has been set to completed.",
                  static_cast<uint64_t>(taskId));
        ::std::abort();
    }
    task->mHasCompleted = true;
    poll_locked(task->mCtxId);
}

void VirtioGpuTimelines::poll_locked(CtxId ctxId) {
    auto iTimelineQueue = mTimelineQueues.find(ctxId);
    if (iTimelineQueue == mTimelineQueues.end()) {
        VGT_ERROR("Context(id = %" PRIu64 ") doesn't exist.",
                  static_cast<uint64_t>(ctxId));
        ::std::abort();
    }
    std::list<TimelineItem> &timelineQueue = iTimelineQueue->second;
    auto i = timelineQueue.begin();
    for (; i != timelineQueue.end(); i++) {
        // This visitor will signal the fence and return whether the timeline
        // item is an incompleted task.
        struct {
            bool operator()(std::unique_ptr<Fence> &fence) {
                (*fence->mCompletionCallback)();
                return false;
            }
            bool operator()(std::shared_ptr<Task> &task) {
                return !task->mHasCompleted;
            }
        } visitor;
        if (std::visit(visitor, *i)) {
            break;
        }
    }
    timelineQueue.erase(timelineQueue.begin(), i);
}