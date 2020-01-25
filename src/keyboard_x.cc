/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Microsoft Corporation. All rights reserved.
 *  Licensed under the MIT License. See License.txt in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "keymapping.h"
#include "string_conversion.h"
#include "common.h"

#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon.h>

#include "../deps/chromium/macros.h"
#include "../deps/chromium/x/keysym_to_unicode.h"

namespace {

class KeyModifierMaskToXModifierMask {
 public:
  static KeyModifierMaskToXModifierMask& GetInstance() {
    static KeyModifierMaskToXModifierMask instance;
    return instance;
  }

  void Initialize() {
    alt_modifier = 0;
    meta_modifier = 0;
    num_lock_modifier = 0;
    mode_switch_modifier = 0;

    const char *keymap_str = "help";
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_string(
	context, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1,
	XKB_KEYMAP_COMPILE_NO_FLAGS);
    struct xkb_state *state = xkb_state_new(keymap);
    
    
    /*XModifierKeymap* mod_map = XGetModifierMapping(display);*/
    int max_mod_keys = mod_map->max_keypermod;
    for (int mod_index = 0; mod_index <= 8; ++mod_index) {
      for (int key_index = 0; key_index < max_mod_keys; ++key_index) {
        int key = mod_map->modifiermap[mod_index * max_mod_keys + key_index];
        if (!key) {
          continue;
        }

        int keysym = xkb_state_key_get_one_sym(xkb_state, key);
        if (!keysym) {
          continue;
        }

        // TODO: Also check for XK_ISO_Level3_Shift	0xFE03

        if (keysym == XKB_KEY_Alt_L || keysym == XKB_KEY_Alt_R) {
          alt_modifier = 1 << mod_index;
        }
        if (keysym == XKB_KEY_Mode_switch) {
          mode_switch_modifier = 1 << mod_index;
        }
        if (keysym == XKB_KEY_Meta_L || keysym == XKB_KEY_Super_L || keysym == XKB_KEY_Meta_R || keysym == XKB_KEY_Super_R) {
          meta_modifier = 1 << mod_index;
        }
        if (keysym == XKB_KEY_Num_Lock) {
          num_lock_modifier = 1 << mod_index;
        }
      }
    }

    XFreeModifiermap(mod_map);
  }

  int XModFromKeyMod(int keyMod) {
    int x_modifier = 0;

    // Ctrl + Alt => AltGr
    if (keyMod & kControlKeyModifierMask && keyMod & kAltKeyModifierMask) {
      x_modifier |= mode_switch_modifier;//alt_r_modifier;
    } else if (keyMod & kControlKeyModifierMask) {
      x_modifier |= ControlMask;
    } else if (keyMod & kAltKeyModifierMask) {
      x_modifier |= alt_modifier;
    }

    if (keyMod & kShiftKeyModifierMask) {
      x_modifier |= ShiftMask;
    }

    if (keyMod & kMetaKeyModifierMask) {
      x_modifier |= meta_modifier;
    }

    if (keyMod & kNumLockKeyModifierMask) {
      x_modifier |= num_lock_modifier;
    }

    return x_modifier;
  }

 private:
  KeyModifierMaskToXModifierMask() {
    Initialize();
  }

  int alt_modifier;
  int meta_modifier;
  int num_lock_modifier;
  int mode_switch_modifier;

  DISALLOW_COPY_AND_ASSIGN(KeyModifierMaskToXModifierMask);
};

std::string GetStrFromXEvent(const XEvent* xev) {
  const XKeyEvent* xkey = &xev->xkey;
  KeySym keysym = XK_VoidSymbol;
  XLookupString(const_cast<XKeyEvent*>(xkey), NULL, 0, &keysym, NULL);
  uint16_t character = ui::GetUnicodeCharacterFromXKeySym(keysym);

  if (!character)
    return std::string();

  wchar_t value = character;

  return vscode_keyboard::UTF16toUTF8(&value, 1);
}

} // namespace


namespace vscode_keyboard {

#define USB_KEYMAP(usb, evdev, xkb, win, mac, code, id) {usb, xkb, code}
#define USB_KEYMAP_DECLARATION const KeycodeMapEntry usb_keycode_map[] =
#include "../deps/chromium/keycode_converter_data.inc"
#undef USB_KEYMAP
#undef USB_KEYMAP_DECLARATION

napi_value _GetKeyMap(napi_env env, napi_callback_info info) {

  napi_value result;
  NAPI_CALL(env, napi_create_object(env, &result));

  Display *display;
  if (!(display = XOpenDisplay(""))) {
    return result;
  }

  KeyModifierMaskToXModifierMask *mask_provider = &KeyModifierMaskToXModifierMask::GetInstance();
  mask_provider->Initialize();

  size_t cnt = sizeof(usb_keycode_map) / sizeof(usb_keycode_map[0]);

  for (size_t i = 0; i < cnt; ++i) {
    const char *code = usb_keycode_map[i].code;
    int native_keycode = usb_keycode_map[i].native_keycode;

    if (!code || native_keycode <= 0) {
      continue;
    }

    napi_value entry;
    NAPI_CALL(env, napi_create_object(env, &entry));

    key_event->keycode = native_keycode;
    {
      key_event->state = 0;
      std::string value = GetStrFromXEvent(&event);
      NAPI_CALL(env, napi_set_named_property_string_utf8(env, entry, "value", value.c_str()));
    }

    {
      key_event->state = mask_provider->XModFromKeyMod(kShiftKeyModifierMask);
      std::string withShift = GetStrFromXEvent(&event);
      NAPI_CALL(env, napi_set_named_property_string_utf8(env, entry, "withShift", withShift.c_str()));
    }

    {
      key_event->state = mask_provider->XModFromKeyMod(kControlKeyModifierMask | kAltKeyModifierMask);
      std::string withAltGr = GetStrFromXEvent(&event);
      NAPI_CALL(env, napi_set_named_property_string_utf8(env, entry, "withAltGr", withAltGr.c_str()));
    }

    {
      key_event->state = mask_provider->XModFromKeyMod(kShiftKeyModifierMask | kControlKeyModifierMask | kAltKeyModifierMask);
      std::string withShiftAltGr = GetStrFromXEvent(&event);
      NAPI_CALL(env, napi_set_named_property_string_utf8(env, entry, "withShiftAltGr", withShiftAltGr.c_str()));
    }

    NAPI_CALL(env, napi_set_named_property(env, result, code, entry));
  }

  return result;
}

napi_value _GetCurrentKeyboardLayout(napi_env env, napi_callback_info info) {

  napi_value result;
  NAPI_CALL(env, napi_get_null(env, &result));

  Display *display;
  if (!(display = XOpenDisplay(""))) {
    return result;
  }

  XkbRF_VarDefsRec vdr;
  char *tmp = NULL;
  int res = XkbRF_GetNamesProp(display, &tmp, &vdr);
  if (res) {
    NAPI_CALL(env, napi_create_object(env, &result));

    NAPI_CALL(env, napi_set_named_property_string_utf8(env, result, "model", vdr.model ? vdr.model : ""));
    NAPI_CALL(env, napi_set_named_property_string_utf8(env, result, "layout", vdr.layout ? vdr.layout : ""));
    NAPI_CALL(env, napi_set_named_property_string_utf8(env, result, "variant", vdr.variant ? vdr.variant : ""));
    NAPI_CALL(env, napi_set_named_property_string_utf8(env, result, "options", vdr.options ? vdr.options : ""));
    NAPI_CALL(env, napi_set_named_property_string_utf8(env, result, "rules", tmp ? tmp : ""));
  }

  XFlush(display);
  XCloseDisplay(display);
  return result;
}

napi_value _OnDidChangeKeyboardLayout(napi_env env, napi_callback_info info) {
  return napi_fetch_undefined(env);
}

napi_value _isISOKeyboard(napi_env env, napi_callback_info info) {
  return napi_fetch_undefined(env);
}

} // namespace vscode_keyboard
