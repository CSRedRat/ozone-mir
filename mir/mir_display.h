// Copyright 2013 The Chromium Authors. All rights reserved.
// Copyright 2014 Canonical LTD. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_MIR_DISPLAY_H_
#define OZONE_MIR_DISPLAY_H_

#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

#include "ozone/ui/egl/display.h"
#include "ozone/ui/events/window_state_change_handler.h"

#include <mir_toolkit/mir_client_library.h>

namespace ozoneui
{
class Screen;
}

namespace ozonemir {

class MirWindow;
class OzoneDisplay;

// MirDisplay is a wrapper around wl_display. Once we get a valid
// wl_display, the Mir server will send different events to register
// the Mir compositor, shell, screens, input devices, ...
class MirDisplay : public ozoneui::Display, public ozoneui::WindowStateChangeHandler {
public:
    MirDisplay(bool gpu_process = true);
    virtual ~MirDisplay();

    bool Initialized() const { return mir_connection_is_valid(connection_); }
    intptr_t GetNativeDisplay() { return reinterpret_cast<intptr_t>(mir_connection_get_egl_native_display(connection_)); }
    
    ozoneui::WindowMap const& GetWindowList() const;
    
    std::list<ozoneui::Screen*> GetScreenList() const;
    ozoneui::Screen* PrimaryScreen() const;
    
    ozoneui::Window* CreateAcceleratedSurface(unsigned w);
    void DestroyWindow(unsigned w);

    const int32* GetEGLSurfaceProperties(const int32* desired_list);
    
    void StartProcessingEvents();
    void StopProcessingEvents();
    
    void FlushDisplay() { /* TODO: May not be necessary for mir ~racarr */ }
    
    // Window state change handler interface.
    void SetWidgetState(unsigned widget, ozoneui::WidgetState state,
                        unsigned width, unsigned height);
    void SetWidgetTitle(unsigned widget, const base::string16& title);
    void SetWidgetAttributes(unsigned widget, unsigned parent,
                             unsigned x, unsigned y,
                             ozoneui::WidgetType type);


protected:
    DISALLOW_COPY_AND_ASSIGN(MirDisplay);

private:
    MirConnection *connection_;

    // screen_list_ may be modified from arbitrary thread in response to mirclient callback
    // and so we need to synchronize access.
    base::Lock mutable screen_list_lock_;

    std::list<ozoneui::Screen*> screen_list_;
    ozoneui::WindowMap window_map_;

    // track if StartProcessingEvents has been called so we can initialize new windows properly.
    bool processing_events_;

    void ClearScreenListLocked(base::AutoLock const&);
    void ClearWindowMap();

    void UpdateScreenList();

    ozonemir::MirWindow* GetWindow(unsigned w);
    
    static void OnDisplayConfigurationChanged(MirConnection *connection, void* context /* Display */);
};

}  // namespace ozonemir

#endif  // OZONE_MIR_DISPLAY_H_
