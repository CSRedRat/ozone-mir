// Copyright 2013 Intel Corporation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ozone/ui/ime/keyboard_engine_xkb.h"

#include <sys/mman.h>

#include "ozone/ui/events/keyboard_codes_ozone.h"
#include "ui/events/event.h"

namespace ozonewayland {

KeyboardEngineXKB::KeyboardEngineXKB() : keyboard_modifiers_(0),
    mods_depressed_(0),
    mods_latched_(0),
    mods_locked_(0),
    group_(0),
    last_key_(-1),
    cached_sym_(XKB_KEY_NoSymbol),
    keymap_(NULL),
    state_(NULL),
    context_(NULL) {
}

KeyboardEngineXKB::~KeyboardEngineXKB() {
  FiniXKB();
}

void KeyboardEngineXKB::OnKeyboardKeymap(int fd, uint32_t size) {
  char *map_str =
      reinterpret_cast<char*>(mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0));
  if (map_str == MAP_FAILED)
    return;

  InitXKB();
  keymap_ = xkb_map_new_from_string(context_,
                                    map_str,
                                    XKB_KEYMAP_FORMAT_TEXT_V1,
                                    (xkb_map_compile_flags)0);
  munmap(map_str, size);
  if (!keymap_)
    return;

  state_ = xkb_state_new(keymap_);
  if (state_) {
    xkb_map_unref(keymap_);
    keymap_ = NULL;
  }
}

void KeyboardEngineXKB::OnKeyModifiers(uint32_t mods_depressed,
                                        uint32_t mods_latched,
                                        uint32_t mods_locked,
                                        uint32_t group) {
  if (!state_)
    return;

  if ((mods_depressed_ == mods_depressed) && (mods_locked_ == mods_locked)
      && (mods_latched_ == mods_latched) && (group_ == group)) {
    return;
  }

  mods_depressed_ = mods_depressed;
  mods_locked_ = mods_locked;
  mods_latched_ = mods_latched;
  group_ = group;
  xkb_state_update_mask(state_,
                        mods_depressed_,
                        mods_latched_,
                        mods_locked_,
                        0,
                        0,
                        group_);

  keyboard_modifiers_ = 0;
  if (xkb_state_mod_name_is_active(
      state_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE))
    keyboard_modifiers_ |= ui::EF_SHIFT_DOWN;

  if (xkb_state_mod_name_is_active(
      state_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))
    keyboard_modifiers_ |= ui::EF_CONTROL_DOWN;

  if (xkb_state_mod_name_is_active(
      state_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))
    keyboard_modifiers_ |= ui::EF_ALT_DOWN;

  if (xkb_state_mod_name_is_active(
      state_, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE))
    keyboard_modifiers_ |= ui::EF_CAPS_LOCK_DOWN;
}

unsigned KeyboardEngineXKB::ConvertKeyCodeFromEvdev(unsigned hardwarecode) {
  if (hardwarecode == last_key_)
    return cached_sym_;

  const xkb_keysym_t *syms;
  xkb_keysym_t sym;
  uint32_t code = hardwarecode + 8;
  uint32_t num_syms = xkb_key_get_syms(state_, code, &syms);
  if (num_syms == 1)
    sym = syms[0];
  else
    sym = XKB_KEY_NoSymbol;

  last_key_ = hardwarecode;
  cached_sym_ = NormalizeKey(cached_sym_);

  return cached_sym_;
}

bool KeyboardEngineXKB::IgnoreKeyNotify(
         unsigned hardwarecode, bool pressed) {
  // If the key is pressed or it's a special modifier key i.e altgr, we cannot
  // ignore it.
  // TODO(kalyan): Handle all needed cases here.
  if (pressed || IsSpecialModifier(hardwarecode))
    return false;

  // No modifiers set, we don't have to deal with any special cases. Ignore the
  // release events.
  if (!keyboard_modifiers_ || IsOnlyCapsLocked())
    return true;

  return false;
}

void KeyboardEngineXKB::InitXKB() {
  if (context_)
    return;

  context_ = xkb_context_new((xkb_context_flags)0);
}

void KeyboardEngineXKB::FiniXKB() {
  if (state_) {
    xkb_state_unref(state_);
    state_ = NULL;
  }

  if (keymap_) {
    xkb_map_unref(keymap_);
    keymap_ = NULL;
  }

  if (context_) {
    xkb_context_unref(context_);
    context_ = NULL;
  }
}

bool KeyboardEngineXKB::IsSpecialModifier(unsigned hardwarecode) {
    switch (ConvertKeyCodeFromEvdev(hardwarecode)) {
    case XKB_KEY_ISO_Level3_Shift:  // altgr
      return true;
    break;
    default:
    return false;
  }
}

bool KeyboardEngineXKB::IsOnlyCapsLocked() const {
  if ((keyboard_modifiers_ & ui::EF_CONTROL_DOWN) != 0)
    return false;

  if ((keyboard_modifiers_ & ui::EF_ALT_DOWN) != 0)
    return false;

  if ((keyboard_modifiers_ & ui::EF_SHIFT_DOWN) != 0)
    return false;

  return true;
}

xkb_keysym_t KeyboardEngineXKB::NormalizeKey(xkb_keysym_t keysym) {
  if ((keysym >= XKB_KEY_A && keysym <= XKB_KEY_Z) ||
       (keysym >= XKB_KEY_a && keysym <= XKB_KEY_z) ||
         (keysym >= XKB_KEY_0 && keysym <= XKB_KEY_9))
    return keysym;

  if (keysym >= XKB_KEY_KP_0 && keysym <= XKB_KEY_KP_9) {
    // Numpad Number-keys can be represented by a keysym value of 0-9 nos.
    return  XKB_KEY_0 + (keysym - XKB_KEY_KP_0);
  } else if (keysym > 0x01000100 && keysym < 0x01ffffff) {
    // Any UCS character in this range will simply be the character's
    // Unicode number plus 0x01000000.
    return  keysym - 0x001000000;
  } else if (keysym >= XKB_KEY_F1 && keysym <= XKB_KEY_F24) {
    return  OZONEACTIONKEY_F1 + (keysym - XKB_KEY_F1);
  } else if (keysym >= XKB_KEY_KP_F1 && keysym <= XKB_KEY_KP_F4) {
      return  OZONEACTIONKEY_F1 + (keysym - XKB_KEY_KP_F1);
  } else {
      switch (keysym) {
        case XKB_KEY_dead_circumflex:
          return  OZONECHARCODE_CARET_CIRCUMFLEX;
        case XKB_KEY_dead_diaeresis:
          return  OZONECHARCODE_SPACING_DIAERESIS;
        case XKB_KEY_dead_perispomeni:
          return  OZONECHARCODE_TILDE;
        case XKB_KEY_dead_acute:
          return  OZONECHARCODE_SPACING_ACUTE;
        case XKB_KEY_dead_grave:
          return  OZONECHARCODE_GRAVE_ASSCENT;
        case XKB_KEY_endash:
          return  OZONECHARCODE_ENDASH;
        case XKB_KEY_singlelowquotemark:
          return  OZONECHARCODE_SINGLE_LOW_QUOTATION_MARK;
        case XKB_KEY_dead_cedilla:
          return  OZONECHARCODE_SPACING_CEDILLA;
        case XKB_KEY_KP_Equal:
          return  OZONECHARCODE_EQUAL;
        case XKB_KEY_KP_Multiply:
          return  OZONECHARCODE_MULTIPLY;
        case XKB_KEY_KP_Add:
          return  OZONECHARCODE_PLUS;
        case XKB_KEY_KP_Separator:
          return  OZONECHARCODE_COMMA;
        case XKB_KEY_KP_Subtract:
          return  OZONECHARCODE_MINUS;
        case XKB_KEY_KP_Decimal:
        case XKB_KEY_period:
          return  OZONECHARCODE_PERIOD;
        case XKB_KEY_KP_Divide:
          return  OZONECHARCODE_DIVISION;
        case XKB_KEY_Delete:
        case XKB_KEY_KP_Delete:
          return  OZONEACTIONKEY_DELETE;
        case XKB_KEY_KP_Tab:
        case XKB_KEY_ISO_Left_Tab:
        case XKB_KEY_Tab:
        case XKB_KEY_3270_BackTab:
          return  OZONEACTIONKEY_TAB;
        case XKB_KEY_Sys_Req:
        case XKB_KEY_Escape:
          return  OZONEACTIONKEY_ESCAPE;
        case XKB_KEY_Linefeed:
          return  OZONECHARCODE_LINEFEED;
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
        case XKB_KEY_ISO_Enter:
          return  OZONEACTIONKEY_RETURN;
        case XKB_KEY_KP_Space:
        case XKB_KEY_space:
          return  OZONEACTIONKEY_SPACE;
        case XKB_KEY_dead_caron:
          return  OZONECHARCODE_CARON;
        case XKB_KEY_BackSpace:
          return  OZONEACTIONKEY_BACK;
        case XKB_KEY_dead_doubleacute:
          return  OZONECHARCODE_DOUBLE_ACUTE_ACCENT;
        case XKB_KEY_dead_horn:
          return  OZONECHARCODE_COMBINING_HORN;
        case XKB_KEY_oe:
          return  OZONECHARCODE_LSMALL_OE;
        case XKB_KEY_OE:
          return  OZONECHARCODE_LOE;
        case XKB_KEY_idotless:
          return  OZONECHARCODE_LSMALL_DOT_LESS_I;
        case XKB_KEY_kra:
          return  OZONECHARCODE_LSMALL_KRA;
        case XKB_KEY_dead_stroke:
          return  OZONECHARCODE_MINUS;
        case XKB_KEY_eng:
          return  OZONECHARCODE_LSMALL_ENG;
        case XKB_KEY_ENG:
          return  OZONECHARCODE_LENG;
        case XKB_KEY_leftsinglequotemark:
          return  OZONECHARCODE_LEFT_SINGLE_QUOTATION_MARK;
        case XKB_KEY_rightsinglequotemark:
          return  OZONECHARCODE_RIGHT_SINGLE_QUOTATION_MARK;
        case XKB_KEY_dead_belowdot:
          return  OZONECHARCODE_COMBINING_DOT_BELOW;
        case XKB_KEY_dead_belowdiaeresis:
          return  OZONECHARCODE_COMBINING_DIAERESIS_BELOW;
        case XKB_KEY_Clear:
        case XKB_KEY_KP_Begin:
          return  OZONEACTIONKEY_CLEAR;
        case XKB_KEY_Home:
        case XKB_KEY_KP_Home:
          return  OZONEACTIONKEY_HOME;
        case XKB_KEY_End:
        case XKB_KEY_KP_End:
          return  OZONEACTIONKEY_END;
        case XKB_KEY_Page_Up:
        case XKB_KEY_KP_Page_Up:  // aka XKB_KEY_KP_Prior
          return  OZONEACTIONKEY_PRIOR;
        case XKB_KEY_Page_Down:
        case XKB_KEY_KP_Page_Down:  // aka XKB_KEY_KP_Next
          return  OZONEACTIONKEY_NEXT;
        case XKB_KEY_Left:
        case XKB_KEY_KP_Left:
          return  OZONEACTIONKEY_LEFT;
        case XKB_KEY_Right:
        case XKB_KEY_KP_Right:
          return  OZONEACTIONKEY_RIGHT;
        case XKB_KEY_Down:
        case XKB_KEY_KP_Down:
          return  OZONEACTIONKEY_DOWN;
        case XKB_KEY_Up:
        case XKB_KEY_KP_Up:
          return  OZONEACTIONKEY_UP;
        case XKB_KEY_Kana_Lock:
        case XKB_KEY_Kana_Shift:
          return  OZONEACTIONKEY_KANA;
        case XKB_KEY_Hangul:
          return  OZONEACTIONKEY_HANGUL;
        case XKB_KEY_Hangul_Hanja:
          return  OZONEACTIONKEY_HANJA;
        case XKB_KEY_Kanji:
          return  OZONEACTIONKEY_KANJI;
        case XKB_KEY_Henkan:
          return  OZONEACTIONKEY_CONVERT;
        case XKB_KEY_Muhenkan:
          return  OZONEACTIONKEY_NONCONVERT;
        case XKB_KEY_Zenkaku_Hankaku:
          return  OZONEACTIONKEY_DBE_DBCSCHAR;
        case XKB_KEY_ISO_Level5_Shift:
          return  OZONEACTIONKEY_OEM_8;
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
          return  OZONEACTIONKEY_SHIFT;
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
          return  OZONEACTIONKEY_CONTROL;
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
          return  OZONEACTIONKEY_MENU;
        case XKB_KEY_ISO_Level3_Shift:
          return  OZONEACTIONKEY_ALTGR;
        case XKB_KEY_Multi_key:
          return  OZONEACTIONKEY_COMPOSE;
        case XKB_KEY_Pause:
          return  OZONEACTIONKEY_PAUSE;
        case XKB_KEY_Caps_Lock:
          return  OZONEACTIONKEY_CAPITAL;
        case XKB_KEY_Num_Lock:
          return  OZONEACTIONKEY_NUMLOCK;
        case XKB_KEY_Scroll_Lock:
          return  OZONEACTIONKEY_SCROLL;
        case XKB_KEY_Select:
          return  OZONEACTIONKEY_SELECT;
        case XKB_KEY_Print:
          return  OZONEACTIONKEY_PRINT;
        case XKB_KEY_Execute:
          return  OZONEACTIONKEY_EXECUTE;
        case XKB_KEY_Insert:
        case XKB_KEY_KP_Insert:
          return  OZONEACTIONKEY_INSERT;
        case XKB_KEY_Help:
          return  OZONEACTIONKEY_HELP;
        case XKB_KEY_Super_L:
          return  OZONEACTIONKEY_LWIN;
        case XKB_KEY_Super_R:
          return  OZONEACTIONKEY_RWIN;
        case XKB_KEY_Menu:
          return  OZONEACTIONKEY_APPS;
        case XKB_KEY_XF86Tools:
          return  OZONEACTIONKEY_F13;
        case XKB_KEY_XF86Launch5:
          return  OZONEACTIONKEY_F14;
        case XKB_KEY_XF86Launch6:
          return  OZONEACTIONKEY_F15;
        case XKB_KEY_XF86Launch7:
          return  OZONEACTIONKEY_F16;
        case XKB_KEY_XF86Launch8:
          return  OZONEACTIONKEY_F17;
        case XKB_KEY_XF86Launch9:
          return  OZONEACTIONKEY_F18;

        // For supporting multimedia buttons on a USB keyboard.
        case XKB_KEY_XF86Back:
          return  OZONEACTIONKEY_BROWSER_BACK;
        case XKB_KEY_XF86Forward:
          return  OZONEACTIONKEY_BROWSER_FORWARD;
        case XKB_KEY_XF86Reload:
          return  OZONEACTIONKEY_BROWSER_REFRESH;
        case XKB_KEY_XF86Stop:
          return  OZONEACTIONKEY_BROWSER_STOP;
        case XKB_KEY_XF86Search:
          return  OZONEACTIONKEY_BROWSER_SEARCH;
        case XKB_KEY_XF86Favorites:
          return  OZONEACTIONKEY_BROWSER_FAVORITES;
        case XKB_KEY_XF86HomePage:
          return  OZONEACTIONKEY_BROWSER_HOME;
        case XKB_KEY_XF86AudioMute:
          return  OZONEACTIONKEY_VOLUME_MUTE;
        case XKB_KEY_XF86AudioLowerVolume:
          return  OZONEACTIONKEY_VOLUME_DOWN;
        case XKB_KEY_XF86AudioRaiseVolume:
          return  OZONEACTIONKEY_VOLUME_UP;
        case XKB_KEY_XF86AudioNext:
          return  OZONEACTIONKEY_MEDIA_NEXT_TRACK;
        case XKB_KEY_XF86AudioPrev:
          return  OZONEACTIONKEY_MEDIA_PREV_TRACK;
        case XKB_KEY_XF86AudioStop:
          return  OZONEACTIONKEY_MEDIA_STOP;
        case XKB_KEY_XF86AudioPlay:
          return  OZONEACTIONKEY_MEDIA_PLAY_PAUSE;
        case XKB_KEY_XF86Mail:
          return  OZONEACTIONKEY_MEDIA_LAUNCH_MAIL;
        case XKB_KEY_XF86LaunchA:
          return  OZONEACTIONKEY_MEDIA_LAUNCH_APP1;
        case XKB_KEY_XF86LaunchB:
        case XKB_KEY_XF86Calculator:
          return  OZONEACTIONKEY_MEDIA_LAUNCH_APP2;
        case XKB_KEY_XF86WLAN:
          return  OZONEACTIONKEY_WLAN;
        case XKB_KEY_XF86PowerOff:
          return  OZONEACTIONKEY_POWER;
        case XKB_KEY_XF86MonBrightnessDown:
          return  OZONEACTIONKEY_BRIGHTNESS_DOWN;
        case XKB_KEY_XF86MonBrightnessUp:
          return  OZONEACTIONKEY_BRIGHTNESS_UP;
        case XKB_KEY_XF86KbdBrightnessDown:
          return  OZONEACTIONKEY_KBD_BRIGHTNESS_DOWN;
        case XKB_KEY_XF86KbdBrightnessUp:
          return  OZONEACTIONKEY_KBD_BRIGHTNESS_UP;
        case XKB_KEY_emptyset:
        case XKB_KEY_NoSymbol:
          return  OZONECHARCODE_NULL;
        default:
          break;
    }
  }
}

}  // namespace ozonewayland
