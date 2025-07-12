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

var util = require('util');
var events = require('events');
var apoxusbcan = require('bindings')('apoxusbcan.node');

// Make sure the addon inherit from EventEmitter
apoxusbcan.ApoxUsbCan.prototype.__proto__ = events.EventEmitter.prototype

var MESSAGE_CALLBACK_TIMEOUT = 1000;

// These are the board commands. Not all of them are used, but are listed here for
// future reference.

var WHICH_CODE_IS_RUNNING = 0x00;
var RESET_CPU  = 0x01;
var GET_TX_ERR_CNT = 0x02;
var GET_RX_ERR_CNT = 0x03;
var GET_CANSTAT = 0x04;
var GET_COMSTAT = 0x05;
var GET_MSGFILTER1 = 0x06;
var GET_MSGFILTER2 = 0x07;
var SET_MSGFILTER1 = 0x08;
var SET_MSGFILTER2 = 0x09;

var IS_RCV_BUFFER_EMPTY = 0x0a;
var IS_TX_BUFFER_EMPTY  = 0x0b;
var IS_TX_PENDING = 0x0c;
var GET_CANCON = 0x0d;
var GET_CIOCON = 0x0e;
var SET_SYNC_COUNT = 0x10;
var SET_SYNC_MSG = 0x11;
var TURN_SYNC_ON = 0x12;
var TURN_SYNC_OFF = 0x13;

var SET_CONFIG_MODE = 0x20;
var SET_LOOPBACK_MODE = 0x21;
var SET_NORMAL_MODE = 0x22;
var SET_SLEEP_MODE = 0x23;
var SET_LISTEN_MODE = 0x24;
var ABORT_ALL_TX = 0x26;
var SET_BAUD_1MEG = 0x30;
var SET_BAUD_500K = 0x31;
var SET_BAUD_250K = 0x32;
var SET_BAUD_125K = 0x33;
var GET_BAUD_REGS = 0x34;
var RESET_MICRO = 0x42;
var GET_HARDWARE_VERSION = 0x43;
var GET_FIRMWARE_VERSION = 0x44;

// A trick to extend an addon class
var ApoxUsbCan = exports.ApoxUsbCan = function () {
  var p = new apoxusbcan.ApoxUsbCan(); 
  p.__proto__ = ApoxUsbCan.prototype;
  p.setMaxListeners(0); // we know what we're doing
  return p; 
}; 

ApoxUsbCan.prototype = { 
  __proto__: apoxusbcan.ApoxUsbCan.prototype, 
  constructor: ApoxUsbCan
};

// This method can be used to send generic board message with an expected response.
// The callback, retryCount and responseMatcher are optional arguments.
ApoxUsbCan.prototype.sendBoardMessageAndReceive = function(requestCommand, callback, retryCount, responseMatcher) {
  var self = this;

  var messageCallback = function(id, responseCommand, data) {
    var match = responseMatcher ? responseMatcher(id, responseCommand, data) : (requestCommand == responseCommand & 0x7F);
    if (match) {
      clearTimeout(messageCallbackTimeout);
      if (callback) callback(null, data);
      self.removeListener('boardmessage', messageCallback);
    }
  };

  self.sendBoardMessage(requestCommand);

  var messageCallbackTimeoutFn = function() {
    if (retryCount > 0) {
      self.sendBoardMessage(requestCommand);

      messageCallbackTimeout = setTimeout(messageCallbackTimeoutFn, MESSAGE_CALLBACK_TIMEOUT);
      retryCount--;
    } else {
      if (callback) callback("No response received for command " + requestCommand);
      self.removeListener('boardmessage', messageCallback);
    }
  };

  var messageCallbackTimeout = setTimeout(messageCallbackTimeoutFn, MESSAGE_CALLBACK_TIMEOUT);

  self.addListener('boardmessage', messageCallback);
};

ApoxUsbCan.prototype.getHardwareVersion = function(callback) {
  this.sendBoardMessageAndReceive(GET_HARDWARE_VERSION, function(err, data) {
    if (err) {
      if (callback) callback(err);
    } else {
      if (callback) callback(null, data.toString());
    }
  });
};

ApoxUsbCan.prototype.getFirmwareVersion = function(callback) {
  this.sendBoardMessageAndReceive(GET_FIRMWARE_VERSION, function(err, data) {
    if (err) {
      if (callback) callback(err);
    } else {
      if (callback) callback(null, data.toString());
    }
  });
};

ApoxUsbCan.prototype.switchToMainCode = function(callback) {
  var self = this;
  this.isMainCodeRunning(function(err, isMainCodeRunning) {
    if (err) {
      if (callback) callback(err);
    } else {
      if (!isMainCodeRunning) {
        self.sendBoardMessageAndReceive(0x52, callback, 0, function(id, responseCommand, data) {
          return id == 0xFF && data[0] == 0x63;
        });
      } else {
        if (callback) callback(null);
      }
    }
  });
};

ApoxUsbCan.prototype.isMainCodeRunning = function(callback) {
  this.sendBoardMessageAndReceive(WHICH_CODE_IS_RUNNING, function(err, data) {
    if (err) {
      if (callback) callback(err);
    } else {
      if (data[0] == 0xCC) {
        if (callback) callback(null, true);
      } else if (data[0] == 0x55) {
        if (callback) callback(null, false);
      } else {
        if (callback) callback("Unexpected running code: " + data[0]);
      }
    }
  }, 2); // notice here the retry!
};

ApoxUsbCan.prototype.reset = function(callback) {
  this.sendBoardMessage(RESET_MICRO);

  if (callback) {
    callback(null);
  }
};


