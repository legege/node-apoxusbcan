{
  'targets': [
    {
      'target_name': 'apoxusbcan',
      'dependencies': [
        'deps/libftdi/libftdi.gyp:*',
        'deps/libusb/libusb.gyp:*'
      ],
      'sources': [
        'src/node_apoxusbcan.cc'
      ]
    }
  ]
}
