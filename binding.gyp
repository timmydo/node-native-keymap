{
  "targets": [
    {
      "target_name": "keymapping",
      "sources": [
        "src/string_conversion.cc",
        "src/keymapping.cc"
      ],
      "conditions": [
        ['OS=="linux"', {
          "sources": [
            "deps/chromium/x/keysym_to_unicode.cc",
            "src/keyboard_x.cc"
          ],
          "include_dirs": [
            "<!@(${PKG_CONFIG:-pkg-config} xkbcommon --cflags | sed s/-I//g)"
          ],
          "libraries": [
            "<!@(${PKG_CONFIG:-pkg-config} xkbcommon --libs)"
          ]
        }],
        ['OS=="freebsd"', {
          "sources": [
            "deps/chromium/x/keysym_to_unicode.cc",
            "src/keyboard_x.cc"
          ],
          "include_dirs": [
            "/usr/local/include"
          ],
          "link_settings": {
            "libraries": [
              "-lxkbcommon",
              "-L/usr/local/lib"
            ]
          }
        }],
        ['OS=="win"', {
          "sources": [
            "src/keyboard_win.cc"
          ]
        }],
        ['OS=="mac"', {
          "sources": [
            "src/keyboard_mac.mm"
          ],
          'link_settings' : {
            'libraries' : [
              '-framework Cocoa'
            ]
          }
        }]
      ]
    }
  ]
}
