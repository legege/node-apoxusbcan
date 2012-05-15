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

usbcan.open();

usbcan.switchToMainCode(function(err) {
  if (err) {
    console.log('Failed to switch to main code:', err);
    return;
  }

  var cb = function(err, version) {
    if (err) {
      console.log('Failed to get hardware version:', err);
      return;
    }
    console.log('Hardware version:', version);

    setTimeout(function() { usbcan.getHardwareVersion(cb) }, 50);
  }
  usbcan.getHardwareVersion(cb);
});

process.on('SIGINT', function () {
  console.log('Got SIGINT... exiting');

  usbcan.reset();
  usbcan.close();
});

process.on('exit', function () {
  usbcan.close();
});

