// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/tools/null_invalidation_state_tracker.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "sync/notifier/invalidation_util.h"

namespace syncer {

NullInvalidationStateTracker::NullInvalidationStateTracker() {}
NullInvalidationStateTracker::~NullInvalidationStateTracker() {}

InvalidationStateMap
NullInvalidationStateTracker::GetAllInvalidationStates() const {
  return InvalidationStateMap();
}

void NullInvalidationStateTracker::SetMaxVersionAndPayload(
    const invalidation::ObjectId& id,
    int64 max_invalidation_version,
    const std::string& payload) {
  LOG(INFO) << "Setting max invalidation version for "
          << ObjectIdToString(id) << " to " << max_invalidation_version
          << " with payload " << payload;
}

void NullInvalidationStateTracker::Forget(const ObjectIdSet& ids) {
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    LOG(INFO) << "Forgetting invalidation state for " << ObjectIdToString(*it);
  }
}

std::string NullInvalidationStateTracker::GetBootstrapData() const {
  return std::string();
}

void NullInvalidationStateTracker::SetBootstrapData(const std::string& data) {
  std::string base64_data;
  CHECK(base::Base64Encode(data, &base64_data));
  LOG(INFO) << "Setting bootstrap data to: " << base64_data;
}

void NullInvalidationStateTracker::GenerateAckHandles(
    const ObjectIdSet& ids,
    const scoped_refptr<base::TaskRunner>& task_runner,
    base::Callback<void(const AckHandleMap&)> callback) {
  AckHandleMap ack_handles;
  for (ObjectIdSet::const_iterator it = ids.begin(); it != ids.end(); ++it) {
    ack_handles.insert(std::make_pair(*it, AckHandle::InvalidAckHandle()));
  }
  CHECK(task_runner->PostTask(FROM_HERE, base::Bind(callback, ack_handles)));
}

void NullInvalidationStateTracker::Acknowledge(const invalidation::ObjectId& id,
                                               const AckHandle& ack_handle) {
  LOG(INFO) << "Received ack for " << ObjectIdToString(id);
}

}  // namespace syncer
