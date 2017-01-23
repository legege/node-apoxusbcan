{
  'targets': [
    {
      'target_name': 'apoxusbcan',
      'dependencies': [
        'deps/libftdi.gypi:libftdi',
        'deps/libusb.gypi:libusb'
      ],
      "include_dirs": ["<!(node -e \"require('nan')\")"],
      'sources': [
        'src/addon.cc',
        'src/node_apoxusbcan.cc'
      ],
      "conditions": [
        ['OS=="mac"', {
          'xcode_settings': {
            'OTHER_CFLAGS': [ '-std=c++1y', '-stdlib=libc++' ],
            'OTHER_LDFLAGS': [ '-framework', 'CoreFoundation', '-framework', 'IOKit' ],
            'SDKROOT': 'macosx',
            'MACOSX_DEPLOYMENT_TARGET': '10.7',
          },
        }],
      ]
    }
  ]
}
