// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2014 Canonical LTD. All rights reserved.
// Based on ozone wayland wayland/display.h.
// Original copyright:
// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_DISPLAY_H_
#define OZONE_UI_DISPLAY_H_

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include "base/basictypes.h"
#include "base/strings/string16.h"

#include <list>
#include <map>

#include "ui/gfx/rect.h"

namespace ozoneui {
class Window;

// Maps from window-handle to Window instance.
typedef std::map<unsigned /* handle */, Window*> WindowMap;

// Display encapsulates a connection to an EGL display server which can provide screen information
// and window creation services.
class Display {
public:
  virtual ~Display() {}

  virtual bool Initialized() const = 0;

  // Return the EGLNativeDisplayType instance for this display service.
  // TODO: Use EGLNativeDisplayType once mesa is fixed?
  virtual intptr_t GetNativeDisplay() = 0;

  // Returns a Display owned list of registered windows by handle.
  virtual const WindowMap& GetWindowList() const = 0;

  // TODO: Should this live on Window? ~racarr
  virtual const int32* GetEGLSurfaceProperties(const int32* desired_list) = 0;

  // Begin processing remote events for dispatch to EventConverter.
  virtual void StartProcessingEvents() = 0;
  // Halt processing of remote events. Further processing of events can
  // lead to an error as the EventConverter may then be unprepared to handle such
  // requests.
  virtual void StopProcessingEvents() = 0;
  
  virtual void FlushDisplay() = 0;


  // See comment in ozone_display.cc
  static gfx::Rect LookAheadOutputGeometryHack();
  // Create the main remote connection for the GPU process. ozoneui EGL backends must provide an implementation.
  static Display* GPUProcessDisplayInstance();
  // In some mesa setups we may need a place to force the EGL_PLATFORM environment variable.
  static void MesaEnsureEGLPlatformSelected();

protected:
  Display() {}
  DISALLOW_COPY_AND_ASSIGN(Display);
};

} // namespace ozoneui

#endif  // OZONE_UI_DISPLAY_H_
