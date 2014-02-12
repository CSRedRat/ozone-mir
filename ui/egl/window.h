// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2014 Canonical LTD. All rights reserved.
// Based on ozone wayland wayland/shell_surface.h
// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_WINDOW_H_
#define OZONE_UI_WINDOW_H_

#include "ui/gfx/rect.h"
#include "base/strings/string16.h"

namespace ozoneui {

class Window {
  // Creates a window and maps it to handle.
  virtual ~Window() {};

  virtual unsigned Handle() const = 0;

  virtual void RealizeAcceleratedWidget() = 0;

  // Returns pointer to EGLNativeWindowType associated with the window.
  // The Window object owns the pointer.
  virtual intptr_t egl_window() = 0;

protected:
  Window() {}

private:
  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace ozoneui

#endif  // OZONE_UI_WINDOW_H_
