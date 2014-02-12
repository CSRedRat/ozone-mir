// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2014 Canonical LTD. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/mir/mir_display.h"
#include "ozone/mir/mir_window.h"
#include <EGL/egl.h>

#include "ozone/ui/egl/screen.h"
#include "base/logging.h"

namespace om = ozonemir;

namespace {

struct MirScreen : public ozoneui::Screen {
  MirScreen(MirDisplayOutput const& output)
    : bounds(output.position_x, output.position_y,
             output.modes[output.current_mode].horizontal_resolution,
             output.modes[output.current_mode].vertical_resolution) {
  }
  gfx::Rect Geometry() const {
    return bounds;
  }

  gfx::Rect const bounds;

};

static om::MirDisplay *instance_;

}

om::MirDisplay::MirDisplay(bool gpu_process)
  : processing_events_(false) {

  if (gpu_process)
    ozonewayland::WindowStateChangeHandler::SetInstance(this);
  connection_ = mir_connect_sync(NULL, "Chromium-Ozone");
  
  UpdateScreenList();
}

om::MirDisplay::~MirDisplay() {
  ozonewayland::WindowStateChangeHandler::SetInstance(0);

  mir_connection_release(connection_);
  
  ClearScreenListLocked(base::AutoLock(screen_list_lock_));
  ClearWindowMap();
}

void om::MirDisplay::ClearWindowMap() {
  for (std::map<unsigned, ozoneui::Window*>::iterator it = window_map_.begin(); 
       it != window_map_.end();
       it++) {
    delete it->second;
  }
  window_map_.clear();
}

void om::MirDisplay::ClearScreenListLocked(base::AutoLock const& /* screen_list_lock_ */) {
    for (std::list<ozoneui::Screen*>::iterator it = screen_list_.begin(); 
         it != screen_list_.end();
         it++) {
      delete *it;
    }
    screen_list_.clear();
}

void om::MirDisplay::UpdateScreenList() {
  DCHECK(mir_connection_is_valid(connection_));

  MirDisplayConfiguration* config = mir_connection_create_display_config(connection_);

  { // Acquire screen_list_lock_
    base::AutoLock lock(screen_list_lock_);
    ClearScreenListLocked(lock);
    for (unsigned i = 0; i < config->num_outputs; i++) {
      MirDisplayOutput const& output = config->outputs[i];
      if (output.used)
        screen_list_.push_back(new MirScreen(output));
    }
  } // Release screen_list_lock_

  mir_display_config_destroy(config);
}

std::list<ozoneui::Screen*> om::MirDisplay::GetScreenList() const {
  base::AutoLock lock(screen_list_lock_);
  return screen_list_;
}

ozoneui::Screen* om::MirDisplay::PrimaryScreen() const {
  base::AutoLock lock(screen_list_lock_);
  if (screen_list_.empty())
      return 0;
  else
      return *screen_list_.begin();
}

ozoneui::WindowMap const& om::MirDisplay::GetWindowList() const {
  return window_map_;
}

ozoneui::Window* om::MirDisplay::CreateAcceleratedSurface(unsigned w) {
  DCHECK(mir_connection_is_valid(connection_));
  om::MirWindow *mw = new om::MirWindow(connection_, w);

  if (processing_events_)
    mw->StartProcessingEvents();
  window_map_[w] = mw;

  return mw;
}

void om::MirDisplay::DestroyWindow(unsigned w) {
  // TODO: Safe? ~racarr
  om::MirWindow* win = static_cast<om::MirWindow*>(window_map_[w]);
  DCHECK(win);

  delete win;
  window_map_.erase(w);
}

ozonemir::MirWindow* om::MirDisplay::GetWindow(unsigned w) {
  return static_cast<om::MirWindow*>(window_map_[w]);
}

const int32* om::MirDisplay::GetEGLSurfaceProperties(const int32* desired_list) {
  static const EGLint kConfigAttribs[] = {
    EGL_BUFFER_SIZE, 32,
    EGL_ALPHA_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };

  return kConfigAttribs;
}

void om::MirDisplay::StartProcessingEvents() {
  processing_events_ = true;

  DCHECK(mir_connection_is_valid(connection_));

  // Begin processing display configuration events.
  mir_connection_set_display_config_change_callback(connection_, &om::MirDisplay::OnDisplayConfigurationChanged, this);

  // Begin processing input and window management events.
  for (ozoneui::WindowMap::iterator w = window_map_.begin(); w != window_map_.end(); w++) {
    om::MirWindow* mw = static_cast<om::MirWindow*>(w->second);
    mw->StartProcessingEvents();
  }
}

void om::MirDisplay::StopProcessingEvents() {
  processing_events_ = false;

  for (ozoneui::WindowMap::iterator w = window_map_.begin(); w != window_map_.end(); w++) {
    om::MirWindow* mw = static_cast<om::MirWindow*>(w->second);
    mw->StopProcessingEvents();
  }
}

gfx::Rect ozoneui::Display::LookAheadOutputGeometryHack() {
  om::MirDisplay *disp = new om::MirDisplay(false /* not in gpu_process */);
  DCHECK(disp->PrimaryScreen());

  return disp->PrimaryScreen()->Geometry();
}

ozoneui::Display* ozoneui::Display::GPUProcessDisplayInstance() {
  if(!instance_)
      instance_ = new om::MirDisplay();
  return instance_;
}

void om::MirDisplay::OnDisplayConfigurationChanged(MirConnection *connection, void *context) {
  om::MirDisplay *disp = static_cast<om::MirDisplay*>(context);
  
  // TODO: It would be nice if mir client just let us unregister the callback. ~racarr
  if (disp->processing_events_)
    disp->UpdateScreenList();
}

void om::MirDisplay::SetWidgetState(unsigned w, ozonewayland::WidgetState state,
                                    unsigned width, unsigned height) {
  switch (state) {
    case ozonewayland::CREATE:
    {
      CreateAcceleratedSurface(w);
      break;
    }
    case ozonewayland::FULLSCREEN:
    {
      om::MirWindow* widget = GetWindow(w);
      widget->SetFullscreen();
      widget->Resize(width, height);
      break;
    }
    case ozonewayland::MAXIMIZED:
    {
      om::MirWindow* widget = GetWindow(w);
      widget->Maximize();
      break;
    }
    case ozonewayland::MINIMIZED:
    {
      om::MirWindow* widget = GetWindow(w);
      widget->Minimize();
      break;
    }
    case ozonewayland::RESTORE:
    {
      om::MirWindow* widget = GetWindow(w);
      widget->Restore();
      widget->Resize(width, height);
      break;
    }
    case ozonewayland::ACTIVE:
      NOTIMPLEMENTED();
      break;
    case ozonewayland::INACTIVE:
      NOTIMPLEMENTED();
      break;
    case ozonewayland::SHOW:
      NOTIMPLEMENTED();
      break;
    case ozonewayland::HIDE:
      NOTIMPLEMENTED();
      break;
    case ozonewayland::RESIZE:
    {
      om::MirWindow* window = GetWindow(w);
      DCHECK(window);
      window->Resize(width, height);
      break;
    }
    case ozonewayland::DESTROYED:
    {
      DestroyWindow(w);
      if (window_map_.empty())
        StopProcessingEvents();
      break;
    }
    default:
      break;
  }
}

void om::MirDisplay::SetWidgetTitle(unsigned widget, const base::string16& title) {
  om::MirWindow* window = GetWindow(widget);
  DCHECK(window);
  
  window->SetWindowTitle(title);
}

void om::MirDisplay::SetWidgetAttributes(unsigned widget, unsigned parent,
                                         unsigned x, unsigned y,
                                         ozonewayland::WidgetType type) {
  om::MirWindow* window = GetWindow(widget);
  // TODO:  om::MirWindow* parent_window = GetWindow(parent);
  DCHECK(window);
  // TODO: Support popup with x and y in mir.
  window->SetWindowType(type);
}

void ozoneui::Display::MesaEnsureEGLPlatformSelected() {
  // EGL Platform auto detection is working fine with Mir so lets not confuse things by
  // setting an env variable we do not need.
}
