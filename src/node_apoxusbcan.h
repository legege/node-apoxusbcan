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

#ifndef NODE_APOXUSBCAN_H
#define NODE_APOXUSBCAN_H

#include <nan.h>

#include <ftdi.h>
#include <queue>

typedef struct { 
  char message[512];
} UsbCanError;

typedef struct {  
  unsigned int id;
  unsigned int command;
  unsigned char data[255];
  int dataLength;
} BoardMessage;

typedef struct {
  unsigned int id;
  bool rtr; // remote transmission request (RTR)
  bool extended; // false = 11 bit identifier, true = 29 bit identifier
  unsigned int timestamp;
  unsigned char data[255]; // XXX 8 bytes only?
  int dataLength;
  unsigned int flags;
} CanBusMessage;

class ApoxUsbCan : public Nan::ObjectWrap
{
public:
  static NAN_MODULE_INIT(Init);

  static NAN_METHOD(New);
  static NAN_METHOD(Open);
  static NAN_METHOD(Close);
  static NAN_METHOD(SendBoardMessage);
  static NAN_METHOD(SendCanBusMessage);

  ApoxUsbCan();
  ~ApoxUsbCan();

protected:
  struct ftdi_context _ftdic;

  bool _opened;

  bool _usbRead;
  uv_thread_t _usbReadThread;

  uv_async_t _usbCanErrorEmitAsync;
  std::queue<UsbCanError*> _usbCanErrorQueue;

  uv_async_t _boardMessageEmitAsync;
  std::queue<BoardMessage*> _boardMessageQueue;

  uv_async_t _canBusMessageEmitAsync;
  std::queue<CanBusMessage*> _canBusMessageQueue;

  uv_prepare_t _loopHolder;

  uv_mutex_t _usbWriteMutex;
  
  static void UsbCanErrorEmitter(uv_async_t *w);
  static void BoardMessageEmitter(uv_async_t *w);
  static void CanBusMessageEmitter(uv_async_t *w);

  int SendBoardMessage(unsigned int command);
  int SendCanBusMessage(bool rtr, unsigned int id, bool extendedId, unsigned char* data, int dataLength, unsigned int txFlags);
  int UsbWrite(unsigned char *txFrameData, int txFrameLength);

  static void UsbReadThread(void* arg);

private:
  static Nan::Persistent<v8::Function> constructor;
  Nan::AsyncResource *async_resource;
};

#endif
