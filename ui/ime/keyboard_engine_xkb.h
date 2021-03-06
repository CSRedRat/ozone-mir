// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_IME_KEYBOARD_ENGINE_XKB_H_
#define OZONE_UI_IME_KEYBOARD_ENGINE_XKB_H_

#include <xkbcommon/xkbcommon.h>

#include "base/basictypes.h"

namespace ozoneui {

class KeyboardEngineXKB {
 public:
  KeyboardEngineXKB();
  ~KeyboardEngineXKB();

  void OnKeyboardKeymap(int fd, uint32_t size);
  void OnKeyModifiers(uint32_t mods_depressed,
                      uint32_t mods_latched,
                      uint32_t mods_locked,
                      uint32_t group);
  unsigned ConvertKeyCodeFromEvdev(unsigned hardwarecode);
  bool IgnoreKeyNotify(unsigned hardwarecode, bool pressed);

  uint32_t GetKeyBoardModifiers() const { return keyboard_modifiers_; }
  
  static xkb_keysym_t NormalizeKey(xkb_keysym_t keysym);

 private:
  void InitXKB();
  void FiniXKB();
  bool IsSpecialModifier(unsigned hardwarecode);
  bool IsOnlyCapsLocked() const;

  // Keeps track of the currently active keyboard modifiers. We keep this
  // since we want to advertise keyboard modifiers with mouse events.
  uint32_t keyboard_modifiers_;
  uint32_t mods_depressed_;
  uint32_t mods_latched_;
  uint32_t mods_locked_;
  uint32_t group_;
  int last_key_;
  xkb_keysym_t cached_sym_;

  // keymap used to transform keyboard events.
  struct xkb_keymap *keymap_;
  struct xkb_state *state_;
  struct xkb_context *context_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardEngineXKB);
};

}  // namespace ozoneui

#endif  // OZONE_UI_IME_KEYBOARD_ENGINE_XKB_H_
