node-apoxusbcan
===============

A node.js module for Apox Controls USB-CAN devices. Most important features of this device are
supported (e.g. switching to main code, and ability to send/receive CAN Bus messages).

This project has been developed with [node-xanbus](https://github.com/legege/node-xanbus) in mind.
However, it would probably be useful for something else, hence the reason I'm releasing this module
independently.

Installation
------------

From npm:

``` bash
$ npm install apoxcanusb
```

From source:

``` bash
$ git clone https://github.com/legege/node-apoxusbcan.git
$ cd node-apoxusbcan
$ npm install
```

Example
-------

To receive CAN Bus messages:

``` js
var usbcan = new ApoxUsbCan();

usbcan.on('canbusmessage', function(timestamp, rtr, id, extended, flags, data) {
  console.log('CANBus message: timestamp =', timestamp, 'rtr =', rtr, 'id =', id, 'extended =', extended, 'flags =', flags, 'data =', data);
});

usbcan.open();

usbcan.switchToMainCode(function(err) {
  if (err) {
    console.log('Failed to switch to main code:', err);
    return;
  }
});
```

API
---

### ApoxUsbCan

``` js
var usbcan = new ApoxUsbCan();
``` 

#### usbcan.open()

``` js
usbcan.open();
```

  * ignored if `usbcan` is already opened

#### usbcan.close()

``` js
usbcan.close();
```

  * ignored if `usbcan` is not opened or already closed

#### usbcan.reset(callback)

``` js
usbcan.reset(function(err) {
  if (err) {
    console.log('Failed to reset:', err);
    return;
  }
});
```

  * `usbcan` must be opened

#### usbcan.isMainCodeRunning(callback)

``` js
usbcan.isMainCodeRunning(function(err) {
  if (err) {
    console.log('Failed to determine if main code is running:', err);
    return;
  }
});
```

  * `usbcan` must be opened

#### usbcan.switchToMainCode(callback)

``` js
usbcan.switchToMainCode(function(err, mainCodeRunning) {
  if (err) {
    console.log('Failed to switch to main code:', err);
    return;
  }
  console.log('Is main code running?', mainCodeRunning);
});
```

  * `usbcan` must be opened

#### usbcan.getHardwareVersion(callback)

``` js
usbcan.getHardwareVersion(function(err, version) {
  if (err) {
    console.log('Failed to get hardware version:', err);
    return;
  }
  console.log('Hardware version:', version);
});
```

  * `usbcan` must be opened

#### usbcan.getFirmwareVersion(callback)

``` js
usbcan.getFirmwareVersion(function(err, version) {
  if (err) {
    console.log('Failed to get firmware version:', err);
    return;
  }
  console.log('Firmware version:', version);
});
```

  * `usbcan` must be opened

#### usbcan.sendBoardMessage(requestCommand)

``` js
usbcan.sendBoardMessage(0x43);
```

  * It's not recommanded to use this method directly. The list of supported request commands is provided in source code.
  * `usbcan` must be opened

#### usbcan.sendBoardMessageAndReceive(requestCommand, callback, [retryCount, responseMatcher])

``` js
usbcan.sendBoardMessageAndReceive(0x43, function(err, data) {
  if (err) {
    console.log('Failed to send board message:', err);
    return;
  }
});
```
  * It's not recommanded to use this method directly. The list of supported request commands is provided in source code.
  * `retryCount` is optional, but not if `responseMatcher` is provided
  * `responseMatcher` is optional. Example of signature below.

``` js
function(id, responseCommand, data) {
  return id == 0xFF && data[0] == 0x63;
}
```

#### usbcan.sendCanBusMessage([rtr], canId, [extendedCanId], [canData])

``` js
usbcan.sendCanBusMessage(418053888, new Buffer([0x00, 0xee, 0x00]));
usbcan.sendCanBusMessage(418053888, true, new Buffer([0x00, 0xee, 0x00]));
usbcan.sendCanBusMessage(false, 418053888, true, new Buffer([0x00, 0xee, 0x00]));
```

  * `usbcan` must be opened
  * `rtr` is optional (default: `false`)
  * `extendedCanId` is optional (default: detected based on `canId` length)
  * `canData` is optional (default: empty `Buffer`)

#### Event: 'canbusmessage'

``` js
function(timestamp, rtr, id, extended, flags, data) { }
```

#### Event: 'boardmessage'

``` js
function(id, command, data) { }
``` 
 
#### Event: 'error'

``` js
function(message) { }
```

References
----------

  * http://www.apoxcontrols.com/USBCANmenu.htm

License
-------

(The MIT License)

Copyright (C) 2012, Georges-Etienne Legendre <legege@legege.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

A different license may apply to other software included in this package, 
including libftdi and libusb. Please consult their respective license files
for the terms of their individual licenses.
