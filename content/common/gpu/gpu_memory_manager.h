// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
#define CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_

#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/public/common/gpu_memory_stats.h"
#include "gpu/command_buffer/service/memory_tracking.h"

namespace content {

class GpuChannelManager;
class GpuMemoryManagerClient;
class GpuMemoryManagerClientState;
class GpuMemoryTrackingGroup;

class CONTENT_EXPORT GpuMemoryManager :
    public base::SupportsWeakPtr<GpuMemoryManager> {
 public:
  enum { kDefaultMaxSurfacesWithFrontbufferSoftLimit = 8 };
  enum ScheduleManageTime {
    // Add a call to Manage to the thread's message loop immediately.
    kScheduleManageNow,
    // Add a Manage call to the thread's message loop for execution 1/60th of
    // of a second from now.
    kScheduleManageLater,
  };

  GpuMemoryManager(GpuChannelManager* channel_manager,
                   uint64 max_surfaces_with_frontbuffer_soft_limit);
  ~GpuMemoryManager();

  // Schedule a Manage() call. If immediate is true, we PostTask without delay.
  // Otherwise PostDelayedTask using a CancelableClosure and allow multiple
  // delayed calls to "queue" up. This way, we do not spam clients in certain
  // lower priority situations. An immediate schedule manage will cancel any
  // queued delayed manage.
  void ScheduleManage(ScheduleManageTime schedule_manage_time);

  // Retrieve GPU Resource consumption statistics for the task manager
  void GetVideoMemoryUsageStats(
      content::GPUVideoMemoryUsageStats* video_memory_usage_stats) const;
  void SetWindowCount(uint32 count);

  GpuMemoryManagerClientState* CreateClientState(
      GpuMemoryManagerClient* client, bool has_surface, bool visible);

  GpuMemoryTrackingGroup* CreateTrackingGroup(
      base::ProcessId pid, gpu::gles2::MemoryTracker* memory_tracker);

 private:
  friend class GpuMemoryManagerTest;
  friend class GpuMemoryTrackingGroup;
  friend class GpuMemoryManagerClientState;

  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           ComparatorTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageBasicFunctionality);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingVisibility);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageManyNotVisibleStubs);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingLastUsedTime);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManageChangingImportanceShareGroup);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestForegroundStubsGetBonusAllocation);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestUpdateAvailableGpuMemory);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           GpuMemoryAllocationCompareTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           StubMemoryStatsForLastManageTests);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestManagedUsageTracking);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestBackgroundCutoff);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestBackgroundMru);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTest,
                           TestUnmanagedTracking);

  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTestNonuniform,
                           BackgroundMru);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTestNonuniform,
                           BackgroundDiscardPersistent);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTestNonuniform,
                           UnmanagedTracking);
  FRIEND_TEST_ALL_PREFIXES(GpuMemoryManagerTestNonuniform,
                           DefaultAllocation);

  typedef std::map<gpu::gles2::MemoryTracker*, GpuMemoryTrackingGroup*>
      TrackingGroupMap;

  typedef std::list<GpuMemoryManagerClientState*> ClientStateList;

  void Manage();
  void SetClientsHibernatedState() const;
  uint64 GetVisibleClientAllocation() const;
  uint64 GetCurrentNonvisibleAvailableGpuMemory() const;
  void AssignSurfacesAllocationsNonuniform();
  void AssignSurfacesAllocationsUniform();
  void AssignNonSurfacesAllocations();

  // Math helper function to compute the maximum value of cap such that
  // sum_i min(bytes[i], cap) <= bytes_sum_limit
  static uint64 ComputeCap(std::vector<uint64> bytes, uint64 bytes_sum_limit);

  // Compute the allocation for clients when visible and not visible.
  void ComputeVisibleSurfacesAllocationsNonuniform();
  void ComputeNonvisibleSurfacesAllocationsNonuniform();

  // Compute the budget for a client. Allow at most bytes_above_required_cap
  // bytes above client_state's required level. Allow at most
  // bytes_above_minimum_cap bytes above client_state's minimum level. Allow
  // at most bytes_overall_cap bytes total.
  uint64 ComputeClientAllocationWhenVisible(
      GpuMemoryManagerClientState* client_state,
      uint64 bytes_above_required_cap,
      uint64 bytes_above_minimum_cap,
      uint64 bytes_overall_cap);
  uint64 ComputeClientAllocationWhenNonvisible(
      GpuMemoryManagerClientState* client_state);

  // Update the amount of GPU memory we think we have in the system, based
  // on what the stubs' contexts report.
  void UpdateAvailableGpuMemory();
  void UpdateUnmanagedMemoryLimits();
  void UpdateNonvisibleAvailableGpuMemory();

  // The amount of video memory which is available for allocation.
  uint64 GetAvailableGpuMemory() const;

  // Minimum value of available GPU memory, no matter how little the GPU
  // reports. This is the default value.
  uint64 GetDefaultAvailableGpuMemory() const;

  // Maximum cap on total GPU memory, no matter how much the GPU reports.
  uint64 GetMaximumTotalGpuMemory() const;

  // The maximum and minimum amount of memory that a client may be assigned.
  uint64 GetMaximumClientAllocation() const;
  uint64 GetMinimumClientAllocation() const {
    return bytes_minimum_per_client_;
  }
  // The default amount of memory that a client is assigned, if it has not
  // reported any memory usage stats yet.
  uint64 GetDefaultClientAllocation() const {
    return bytes_default_per_client_;
  }

  // Get a reasonable memory limit from a viewport's surface area.
  static uint64 CalcAvailableFromViewportArea(int viewport_area);
  static uint64 CalcAvailableFromGpuTotal(uint64 total_gpu_memory);

  // Send memory usage stats to the browser process.
  void SendUmaStatsToBrowser();

  // Get the current number of bytes allocated.
  uint64 GetCurrentUsage() const {
    return bytes_allocated_managed_current_ +
        bytes_allocated_unmanaged_current_;
  }

  // GpuMemoryTrackingGroup interface
  void TrackMemoryAllocatedChange(
      GpuMemoryTrackingGroup* tracking_group,
      uint64 old_size,
      uint64 new_size,
      gpu::gles2::MemoryTracker::Pool tracking_pool);
  void OnDestroyTrackingGroup(GpuMemoryTrackingGroup* tracking_group);
  bool EnsureGPUMemoryAvailable(uint64 size_needed);

  // GpuMemoryManagerClientState interface
  void SetClientStateVisible(
      GpuMemoryManagerClientState* client_state, bool visible);
  void SetClientStateManagedMemoryStats(
      GpuMemoryManagerClientState* client_state,
      const GpuManagedMemoryStats& stats);
  void OnDestroyClientState(GpuMemoryManagerClientState* client);

  // Add or remove a client from its clients list (visible, nonvisible, or
  // nonsurface). When adding the client, add it to the front of the list.
  void AddClientToList(GpuMemoryManagerClientState* client_state);
  void RemoveClientFromList(GpuMemoryManagerClientState* client_state);
  ClientStateList* GetClientList(GpuMemoryManagerClientState* client_state);

  // Interfaces for testing
  void TestingSetUseNonuniformMemoryPolicy(bool use_nonuniform_memory_policy) {
    use_nonuniform_memory_policy_ = use_nonuniform_memory_policy;
  }
  void TestingDisableScheduleManage() { disable_schedule_manage_ = true; }
  void TestingSetAvailableGpuMemory(uint64 bytes) {
    bytes_available_gpu_memory_ = bytes;
    bytes_available_gpu_memory_overridden_ = true;
  }

  void TestingSetMinimumClientAllocation(uint64 bytes) {
    bytes_minimum_per_client_ = bytes;
  }

  void TestingSetDefaultClientAllocation(uint64 bytes) {
    bytes_default_per_client_ = bytes;
  }

  void TestingSetUnmanagedLimitStep(uint64 bytes) {
    bytes_unmanaged_limit_step_ = bytes;
  }

  void TestingSetNonvisibleAvailableGpuMemory(uint64 bytes) {
    bytes_nonvisible_available_gpu_memory_ = bytes;
  }

  GpuChannelManager* channel_manager_;

  // The new memory policy does not uniformly assign memory to tabs, but
  // scales the assignments to the tabs' needs.
  bool use_nonuniform_memory_policy_;

  // A list of all visible and nonvisible clients, in most-recently-used
  // order (most recently used is first).
  ClientStateList clients_visible_mru_;
  ClientStateList clients_nonvisible_mru_;

  // A list of all clients that don't have a surface.
  ClientStateList clients_nonsurface_;

  // All context groups' tracking structures
  TrackingGroupMap tracking_groups_;

  base::CancelableClosure delayed_manage_callback_;
  bool manage_immediate_scheduled_;

  uint64 max_surfaces_with_frontbuffer_soft_limit_;

  // The maximum amount of memory that may be allocated for GPU resources
  uint64 bytes_available_gpu_memory_;
  bool bytes_available_gpu_memory_overridden_;

  // The minimum and default allocations for a single client.
  uint64 bytes_minimum_per_client_;
  uint64 bytes_default_per_client_;

  // The maximum amount of memory that can be allocated for GPU resources
  // in nonvisible renderers.
  uint64 bytes_nonvisible_available_gpu_memory_;

  // The current total memory usage, and historical maximum memory usage
  uint64 bytes_allocated_managed_current_;
  uint64 bytes_allocated_managed_visible_;
  uint64 bytes_allocated_managed_nonvisible_;
  uint64 bytes_allocated_unmanaged_current_;
  uint64 bytes_allocated_historical_max_;

  // If bytes_allocated_unmanaged_current_ leaves the interval [low_, high_),
  // then ScheduleManage to take the change into account.
  uint64 bytes_allocated_unmanaged_high_;
  uint64 bytes_allocated_unmanaged_low_;

  // Update bytes_allocated_unmanaged_low/high_ in intervals of step_.
  uint64 bytes_unmanaged_limit_step_;

  // The number of browser windows that exist. If we ever receive a
  // GpuMsg_SetVideoMemoryWindowCount, then we use this to compute memory
  // allocations, instead of doing more complicated stub-based calculations.
  bool window_count_has_been_received_;
  uint32 window_count_;

  // Used to disable automatic changes to Manage() in testing.
  bool disable_schedule_manage_;

  DISALLOW_COPY_AND_ASSIGN(GpuMemoryManager);
};

}  // namespace content

#endif // CONTENT_COMMON_GPU_GPU_MEMORY_MANAGER_H_
