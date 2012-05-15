// Copyright (C) 2012, Georges-Etienne Legendre <legege@legege.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var ApoxUsbCan = require('../apoxusbcan').ApoxUsbCan;

var usbcan = new ApoxUsbCan();

usbcan.on('error', function(message) {
  console.log('Oups! Got an error:', message);
});

usbcan.on('canbusmessage', function(timestamp, rtr, id, extended, flags, data) {
  console.log('CANBus message: timestamp =', timestamp, 'rtr =', rtr, 'id =', id, 'extended =', extended, 'flags =', flags, 'data =', data);
});

usbcan.open();

usbcan.switchToMainCode(function(err) {
  if (err) {
    console.log('Failed to switch to main code:', err);
    return;
  }

  console.log('Preparing CAN Bus message');

  // CAN Bus Message (SAE J1939: ISO Request)
  var priority = 6;
  var pgn = 59904;
  var source = 0;
  var destination = 255;
  var canId = (priority << 26) | ((pgn | destination) << 8) | source;
  var canData = new Buffer([0x00, 0xee, 0x00]); // ISO Address Claim

  console.log('Sending CAN Bus message');
  usbcan.sendCanBusMessage(canId, canData);

  // Or, equivalent:
  // var rtr = false; // Remote Transmission Request
  // var extendedCanId = true;
  // usbcan.sendCanBusMessage(rtr, canId, extendedCanId, canData); // rtr, extendedCanId and canData are optional arguments
});

process.on('SIGINT', function () {
  console.log('Got SIGINT... exiting');

  usbcan.reset();
  usbcan.close();
});

process.on('exit', function () {
  usbcan.close();
});

