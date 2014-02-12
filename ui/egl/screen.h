// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2014 Canonical LTD. All rights reserved.
// Based on ozone wayland wayland/screen.h.
// Original copyright:
// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_SCREEN_H_
#define OZONE_UI_SCREEN_H_

#include "ui/gfx/rect.h"

namespace ozoneui {

// Screen objects keep track of the current outputs (screens/monitors)
// that are available to the application.
class Screen {
public:
  virtual ~Screen() {};
  // Returns the active allocation of the screen.
  virtual gfx::Rect Geometry() const = 0;

protected:
  Screen() {}
  DISALLOW_COPY_AND_ASSIGN(Screen);
};

} // namespace ozoneui

#endif  // OZONE_UI_SCREEN_H_
