// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_FUCHSIA_SESSION_CONNECTION_H_
#define FLUTTER_SHELL_PLATFORM_FUCHSIA_SESSION_CONNECTION_H_

#include <fuchsia/ui/gfx/cpp/fidl.h>
#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/optional.h>
#include <lib/fit/function.h>
#include <lib/ui/scenic/cpp/resources.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "flutter/flow/compositor_context.h"
#include "flutter/flow/scene_update_context.h"
#include "flutter/fml/closure.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/trace_event.h"
#include "vulkan_surface_producer.h"

namespace flutter_runner {

using on_frame_presented_event =
    std::function<void(fuchsia::scenic::scheduling::FramePresentedInfo)>;

// The component residing on the raster thread that is responsible for
// maintaining the Scenic session connection and presenting node updates.
class SessionConnection final {
 public:
  SessionConnection(std::string debug_label,
                    fuchsia::ui::views::ViewToken view_token,
                    scenic::ViewRefPair view_ref_pair,
                    fidl::InterfaceHandle<fuchsia::ui::scenic::Session> session,
                    fml::closure session_error_callback,
                    on_frame_presented_event on_frame_presented_callback,
                    zx_handle_t vsync_event_handle);

  ~SessionConnection();

  bool has_metrics() const { return scene_update_context_.has_metrics(); }

  const fuchsia::ui::gfx::MetricsPtr& metrics() const {
    return scene_update_context_.metrics();
  }

  void set_metrics(const fuchsia::ui::gfx::Metrics& metrics) {
    fuchsia::ui::gfx::Metrics metrics_copy;
    metrics.Clone(&metrics_copy);
    scene_update_context_.set_metrics(
        fidl::MakeOptional(std::move(metrics_copy)));
  }

  void set_enable_wireframe(bool enable);

  flutter::SceneUpdateContext& scene_update_context() {
    return scene_update_context_;
  }

  scenic::ContainerNode& root_node() { return root_node_; }
  scenic::View* root_view() { return &root_view_; }

  void Present(flutter::CompositorContext::ScopedFrame* frame);

  void OnSessionSizeChangeHint(float width_change_factor,
                               float height_change_factor);

  VulkanSurfaceProducer* vulkan_surface_producer() {
    return surface_producer_.get();
  }

  static fml::TimePoint CalculateNextLatchPoint(
      fml::TimePoint present_requested_time,
      fml::TimePoint now,
      fml::TimePoint last_latch_point_targeted,
      fml::TimeDelta flutter_frame_build_time,
      fml::TimeDelta vsync_interval,
      std::deque<std::pair<fml::TimePoint, fml::TimePoint>>&
          future_presentation_infos);

 private:
  const std::string debug_label_;
  scenic::Session session_wrapper_;

  scenic::View root_view_;
  scenic::EntityNode root_node_;

  std::unique_ptr<VulkanSurfaceProducer> surface_producer_;
  flutter::SceneUpdateContext scene_update_context_;
  on_frame_presented_event on_frame_presented_callback_;

  zx_handle_t vsync_event_handle_;

  fml::TimePoint last_latch_point_targeted_ =
      fml::TimePoint::FromEpochDelta(fml::TimeDelta::Zero());
  fml::TimePoint present_requested_time_ =
      fml::TimePoint::FromEpochDelta(fml::TimeDelta::Zero());

  std::deque<std::pair<fml::TimePoint, fml::TimePoint>>
      future_presentation_infos_ = {};

  bool initialized_ = false;

  // A flow event trace id for following |Session::Present| calls into
  // Scenic.  This will be incremented each |Session::Present| call.  By
  // convention, the Scenic side will also contain its own trace id that
  // begins at 0, and is incremented each |Session::Present| call.
  uint64_t next_present_trace_id_ = 0;
  uint64_t next_present_session_trace_id_ = 0;
  uint64_t processed_present_session_trace_id_ = 0;

  // The maximum number of frames Flutter sent to Scenic that it can have
  // outstanding at any time. This is equivalent to how many times it has
  // called Present2() before receiving an OnFramePresented() event.
  static constexpr int kMaxFramesInFlight = 3;
  int frames_in_flight_ = 0;

  int frames_in_flight_allowed_ = 0;

  bool present_session_pending_ = false;

  void EnqueueClearOps();

  void PresentSession();

  static void ToggleSignal(zx_handle_t handle, bool raise);

  FML_DISALLOW_COPY_AND_ASSIGN(SessionConnection);
};

}  // namespace flutter_runner

#endif  // FLUTTER_SHELL_PLATFORM_FUCHSIA_SESSION_CONNECTION_H_
