{
  'targets': [
    {
      'target_name': 'libftdi',
      'type': 'static_library',

      'dependencies': [
        'libusb.gypi:libusb'
      ],
      'sources': [
        'libftdi/src/ftdi.c',
        'libftdi/src/ftdi_stream.c'
      ],
      'include_dirs': [
        'libftdi',
        'libftdi/src'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          'libftdi/src',
        ],
      },
    }
  ]
}
