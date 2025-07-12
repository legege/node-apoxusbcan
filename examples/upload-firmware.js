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

const fs = require('fs');

const USB_START_DOWNLOAD = 0x53; //'S'
const USB_END_DOWNLOAD = 0x45; //'E'
const FIRMWARE_FILE = './examples/firmwares/usbcan4_4_1.HEX';

function readHexFile(fileName, size = 0x10000) {
  let hex_eof = false;
  let extendedaddress = 0;
  const xdata = Buffer.alloc(size, 0xff);

  try {
    const lines = fs.readFileSync(fileName, 'utf8').split(/\r?\n/);

    for (let line of lines) {
      if (hex_eof || !line.startsWith(':')) continue;

      // Get the record length
      let byte_length = parseInt(line.substr(1, 2), 16);

      // Get the address
      let address = parseInt(line.substr(3, 4), 16);

      // Get the record type
      let recordType = line.substr(7, 2);

      if (recordType === '00') { // Data record
        for (let pos = 0; pos < byte_length; pos++) {
          let dataPos = 9 + pos * 2;
          let data_byte = parseInt(line.substr(dataPos, 2), 16);
          let ex_addr = (extendedaddress << 16) | address;
          if (ex_addr < (size - 64)) {
            xdata[ex_addr] = data_byte;
          }
          address++;
        }
      } else if (recordType === '04') { // Extended linear address
        extendedaddress = parseInt(line.substr(9, 4), 16);
      } else if (recordType === '01') { // End of file
        hex_eof = true;
      }
    }
    return xdata;
  } catch (e) {
    console.error('Error reading hex file:', e);
    return null;
  }
}

var usbcan = new ApoxUsbCan();

usbcan.on('error', function(message) {
  console.log('Oups! Got an error:', message);
});

usbcan.open();

const xdata = readHexFile(FIRMWARE_FILE);
if (!xdata) {
  console.error('Failed to read hex file');
  process.exit(1);
}

usbcan.sendBoardMessage(USB_START_DOWNLOAD);

let numpackages = 0;

for (let i = 0; i < 65536; i += 64) {
  let liveone = false;
  let size = 0;
  let wordAddress = i;
  let str = Buffer.alloc(66);

  str[size++] = wordAddress & 0xff;
  str[size++] = (wordAddress >> 8) & 0xff;

  for (let j = 0; j < 64; j++) {
    if (xdata[i + j] !== 0xff) liveone = true;
    str[size++] = xdata[i + j];
  }

  if (liveone) {
    numpackages++;
    usbcan.usbWrite(str.slice(0, size));
  }
}
usbcan.sendBoardMessage(USB_END_DOWNLOAD);


process.on('SIGINT', function () {
  console.log('Got SIGINT... exiting');

  usbcan.reset();
  usbcan.close();
});

process.on('exit', function () {
  usbcan.close();
});
