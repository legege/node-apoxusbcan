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

#include "node_apoxusbcan.h"

// Part of this work is based on examples provided on the Apox Controls
// website (http://www.apoxcontrols.com/).

#define FTDI_VID 0x0403
#define FTDI_PID 0xf9b8

// Definitions for USB message start and end char
#define USB_DLE 0x10
#define USB_STX 0x02
#define USB_ETX 0x03

#define RX_FRAME_DATA_MAX_LENGTH 32768 // XXX Isn't it a bit too big? There is only one Board or CAN Bus message that's going to fit in here.

#define RAISE_USBCANERROR(input, format, args...) \
      UsbCanError* error = new UsbCanError; \
      snprintf(error->message, sizeof error->message, format, ##args); \
      input->_usbCanErrorQueue.push(error); \
      uv_async_send(&input->_usbCanErrorEmitAsync); 


BoardMessage* CreateBoardMessage(unsigned char* rxFrameData, int rxFrameLength);
CanBusMessage* CreateCanBusMessage(unsigned char* rxFrameData, int rxFrameLength);

enum ReadFrameState {
  RX_FRAME_IDLE,
  RX_FRAME_START,
  RX_FRAME_CONTENT,
  RX_FRAME_CONTENT_NLE,
  RX_FRAME_COMPLETE,
  RX_FRAME_ERROR_EXPECTING_DLE,
  RX_FRAME_ERROR_EXPECTING_STX,
  RX_FRAME_ERROR_EXPECTING_ETX,
  RX_FRAME_ERROR_BAD_CHECKSUM,
  RX_FRAME_ERROR_BUFFER_OVERFLOW
};

static v8::Persistent<v8::FunctionTemplate> s_ct;
static v8::Persistent<v8::String> symbol_error;
static v8::Persistent<v8::String> symbol_boardmessage;
static v8::Persistent<v8::String> symbol_canbusmessage;
static v8::Persistent<v8::String> symbol_emit;

void ApoxUsbCan::Initialize(v8::Handle<v8::Object> target)
{
  v8::HandleScope scope;

  // set the constructor function
  v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New(ApoxUsbCan::New);

  // set the node.js/v8 class name
  s_ct = v8::Persistent<v8::FunctionTemplate>::New(t);
  s_ct->InstanceTemplate()->SetInternalFieldCount(1);
  s_ct->SetClassName(v8::String::NewSymbol("ApoxUsbCan"));

  // registers a class member functions
  NODE_SET_PROTOTYPE_METHOD(s_ct, "open", ApoxUsbCan::Open);
  NODE_SET_PROTOTYPE_METHOD(s_ct, "close", ApoxUsbCan::Close);
  NODE_SET_PROTOTYPE_METHOD(s_ct, "sendBoardMessage", ApoxUsbCan::SendBoardMessage);
  NODE_SET_PROTOTYPE_METHOD(s_ct, "sendCanBusMessage", ApoxUsbCan::SendCanBusMessage);

  // define symboles
  symbol_error = NODE_PSYMBOL("error");
  symbol_boardmessage = NODE_PSYMBOL("boardmessage");
  symbol_canbusmessage = NODE_PSYMBOL("canbusmessage");
  symbol_emit = NODE_PSYMBOL("emit");

  target->Set(v8::String::NewSymbol("ApoxUsbCan"),
              s_ct->GetFunction());
}

v8::Handle<v8::Value> ApoxUsbCan::New(const v8::Arguments& args)
{
  v8::HandleScope scope;

  ApoxUsbCan* hw = new ApoxUsbCan();
  hw->Wrap(args.This());
  return args.This();
}

v8::Handle<v8::Value> ApoxUsbCan::Open(const v8::Arguments& args)
{
  v8::HandleScope scope;

  ApoxUsbCan* input = ObjectWrap::Unwrap<ApoxUsbCan>(args.This());

  if (input->_opened) {
    return scope.Close(v8::Undefined());
  }

  input->_ftdic.usb_read_timeout = 378;
  input->_ftdic.usb_write_timeout = 128;

  if (ftdi_usb_open(&input->_ftdic, FTDI_VID, FTDI_PID) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to open FTDI USB device: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  if (ftdi_usb_reset(&input->_ftdic) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to reset FTDI USB device: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  if (ftdi_usb_purge_buffers(&input->_ftdic) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to purge FTDI USB buffers: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  if (ftdi_write_data_set_chunksize(&input->_ftdic, 2048) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to set FTDI USB write data chunksize: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  if (ftdi_read_data_set_chunksize(&input->_ftdic, 2048) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to set FTDI USB read data chunksize: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  if (ftdi_set_latency_timer(&input->_ftdic, 1) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to set FTDI USB latency timer: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  // Launch the USB read thread
  input->_usbRead = true;
  uv_thread_create(&input->_usbReadThread, UsbReadThread, input);

  // Prepare emit async tasks
  input->_usbCanErrorEmitAsync.data = input;
  uv_async_init(uv_default_loop(), &input->_usbCanErrorEmitAsync, UsbCanErrorEmitter);
  uv_unref((uv_handle_t*)&input->_usbCanErrorEmitAsync); // allow the event loop to exit while this is running

  input->_boardMessageEmitAsync.data = input;
  uv_async_init(uv_default_loop(), &input->_boardMessageEmitAsync, BoardMessageEmitter);
  uv_unref((uv_handle_t*)&input->_boardMessageEmitAsync); // allow the event loop to exit while this is running

  input->_canBusMessageEmitAsync.data = input;
  uv_async_init(uv_default_loop(), &input->_canBusMessageEmitAsync, CanBusMessageEmitter);
  uv_unref((uv_handle_t*)&input->_canBusMessageEmitAsync); // allow the event loop to exit while this is running

  // A hack to keep a reference on the default loop, to let the read thread running in background
  uv_prepare_init(uv_default_loop(), &input->_loopHolder);
  uv_prepare_start(&input->_loopHolder, NULL);

  input->_opened = true;

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> ApoxUsbCan::Close(const v8::Arguments& args)
{
  v8::HandleScope scope;

  ApoxUsbCan* input = ObjectWrap::Unwrap<ApoxUsbCan>(args.This());

  if (!input->_opened) {
    return scope.Close(v8::Undefined());
  }

  // Stop the USB read thread
  if (input->_usbRead) {
    input->_usbRead = false;
    uv_thread_join(&input->_usbReadThread);
  }

  uv_prepare_stop(&input->_loopHolder);

  uv_mutex_lock(&input->_usbWriteMutex);
  uv_mutex_unlock(&input->_usbWriteMutex);
 
  // Close USB
  if (ftdi_usb_close(&input->_ftdic) < 0) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::Concat(v8::String::New("Unable to close FTDI USB device: "),
                                                                              v8::String::New(ftdi_get_error_string(&input->_ftdic))))));
  }

  input->_opened = false;

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> ApoxUsbCan::SendBoardMessage(const v8::Arguments& args)
{
  v8::HandleScope scope;

  ApoxUsbCan* input = ObjectWrap::Unwrap<ApoxUsbCan>(args.This());

  if (!input->_opened) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Device not opened"))));
  }

  if (args.Length() < 1) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong number of arguments"))));
  }

  if (!args[0]->IsNumber()) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong argument type"))));
  }

  unsigned int command = args[0]->ToUint32()->Value();

  if (input->SendBoardMessage(command) < 0) {
    char message[512];
    snprintf(message, sizeof message, "Failed to send message: %s", ftdi_get_error_string(&input->_ftdic));
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New(message))));
  }

  return scope.Close(v8::Undefined());
}

v8::Handle<v8::Value> ApoxUsbCan::SendCanBusMessage(const v8::Arguments& args)
{
  v8::HandleScope scope;

  ApoxUsbCan* input = ObjectWrap::Unwrap<ApoxUsbCan>(args.This());

  if (!input->_opened) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Device not opened"))));
  }

  if (args.Length() < 1) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong number of arguments"))));
  }

  // by default, we assume RTR = false (Remote Transmission Request)
  bool rtr = false;
  int argOffset = 0;

  if (args[0]->IsBoolean()) {
    rtr = args[0]->ToBoolean()->Value();
    argOffset = 1;

    // we need the id next!
    if (args.Length() < 2) {
      return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong number of arguments"))));
    }
  }

  // id is always required
  if (!args[0 + argOffset]->IsNumber()) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong argument type"))));
  }

  unsigned int id = args[0 + argOffset]->Uint32Value();
  bool extendedId = (id >> 11) > 0;
  unsigned char* data = NULL;
  int dataLength = 0;

  // extendedId is optional: we assume the previous detection (29 bits or 11 bits ID)
  // data is optional: we assume empty buffer in this case
  if (args.Length() > 1 + argOffset) {
    // there is an extra argument, but it's not what we expect (Buffer or boolean)
    if (!Buffer::HasInstance(args[1 + argOffset]) && !args[1 + argOffset]->IsBoolean()) {
      return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong argument type"))));
    }

    if (args[1 + argOffset]->IsBoolean()) {
      extendedId = args[1 + argOffset]->ToBoolean()->Value();

      if (args.Length() > 2 + argOffset) {
        // there is an extra argument, but it's not what we expect (Buffer)
        if (!Buffer::HasInstance(args[2 + argOffset])) {
          return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Wrong argument type"))));
        }
        data = (unsigned char*) Buffer::Data(args[2 + argOffset]->ToObject());
        dataLength = (int) Buffer::Length(args[2 + argOffset]->ToObject());
      }
    } else {
      data = (unsigned char*) Buffer::Data(args[1 + argOffset]->ToObject());
      dataLength = (int) Buffer::Length(args[1 + argOffset]->ToObject());
    }
  }

  if (dataLength > 8) {
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New("Too much data, maximum 8 bytes"))));
  }

  if ((input->SendCanBusMessage(rtr, id, extendedId, data, dataLength, 0x00)) < 0) {
    char message[512];
    snprintf(message, sizeof message, "Failed to send message: %s", ftdi_get_error_string(&input->_ftdic));
    return scope.Close(ThrowException(v8::Exception::Error(v8::String::New(message))));
  }

  return scope.Close(v8::Undefined());
}

int ApoxUsbCan::SendBoardMessage(unsigned int command)
{
  unsigned char txFrameData[5];   
  int txFrameLength = 0;

  txFrameData[txFrameLength++] = 0x00;
  txFrameData[txFrameLength++] = command | 0x80;

  return UsbWrite(txFrameData, txFrameLength);
}

int ApoxUsbCan::SendCanBusMessage(bool rtr, unsigned int id, bool extendedId, unsigned char* data, int dataLength, unsigned int txFlags)
{
  unsigned char txFrameData[20];
  int txFrameLength = 0;

  // Outgoing CAN message
  // --------------------
  // [0] ([1][RTR][EXT][unused 0..4])
  // [1] ID MSB
  // [2] ID
  // [3] ID
  // [4] ID LSB
  // [5] FUTURE USE (CANopen or DeviceNet) // ex. Wait for response? Etc..
  // [6] FUTURE USE (CANopen or DeviceNet)
  // [7] RESERVED FOR TX FLAGS 
  // [8] DATA LEN (0-8)
  // [9-16] DATA BYTES 0 to 8 (if needed) (NOT SENT IF RTR is SET)

  txFrameData[txFrameLength++] = 0x80 | (rtr ? 0x40 : 0x00) | (extendedId ? 0x20 : 0x00);
  txFrameData[txFrameLength++] = (unsigned char)(id >> 24) & 0x1f;
  txFrameData[txFrameLength++] = (unsigned char)(id >> 16) & 0xff;
  txFrameData[txFrameLength++] = (unsigned char)(id >> 8) & 0xff;
  txFrameData[txFrameLength++] = (unsigned char)(id) & 0xff;
  txFrameData[txFrameLength++] = 0x00; // future use
  txFrameData[txFrameLength++] = 0x00; // future use
  txFrameData[txFrameLength++] = txFlags;
  txFrameData[txFrameLength++] = dataLength;

  for (int i = 0; i < dataLength && i < 8; i++) {
    txFrameData[txFrameLength++] = data[i];
  }

  return UsbWrite(txFrameData, txFrameLength);
}

ApoxUsbCan::ApoxUsbCan() : ObjectWrap()
{
  _opened = false;
  _usbRead = false;
  ftdi_init(&_ftdic);
  uv_mutex_init(&_usbWriteMutex);
}

ApoxUsbCan::~ApoxUsbCan()
{
  uv_mutex_destroy(&_usbWriteMutex);
  ftdi_deinit(&_ftdic);
}

void ApoxUsbCan::UsbReadThread(void* arg)
{
  ApoxUsbCan *input = static_cast<ApoxUsbCan*>(arg);

  ReadFrameState rxFrameState = RX_FRAME_IDLE;
  unsigned char rxFrameData[RX_FRAME_DATA_MAX_LENGTH];
  int rxFrameLength = 0;
  unsigned char rxFrameChecksum = 0;

  while (input->_usbRead) {
    unsigned char inByte = -1;
    int bytesRead = 0;

    if ((bytesRead = ftdi_read_data(&input->_ftdic, &inByte, 1)) < 0) {
      RAISE_USBCANERROR(input, "Failed to read USB data (%s). Killing read thread!", ftdi_get_error_string(&input->_ftdic));
      input->_usbRead = false;
      break;
    }

    if (bytesRead == 0) {
      continue;
    }

    // IMPORTANT: We assume we're in RUN mode. DOWNLOAD mode is unsupported.

    // Process the byte and try to append to frame data.
    // This section could have implemented with the state pattern, but
    // it's a bit too much for this situation.

    switch (rxFrameState) {
      case RX_FRAME_IDLE:
      case RX_FRAME_COMPLETE:
      case RX_FRAME_ERROR_EXPECTING_DLE:
      case RX_FRAME_ERROR_EXPECTING_STX:
      case RX_FRAME_ERROR_EXPECTING_ETX:
      case RX_FRAME_ERROR_BAD_CHECKSUM:
      case RX_FRAME_ERROR_BUFFER_OVERFLOW:
        if (inByte == USB_DLE) {
          rxFrameState = RX_FRAME_START;
          rxFrameChecksum = 0;
          rxFrameLength = 0;
        } else {
          // Avoid raising with 0xff... for a strange reason, this byte is often thrown after switching to main
          // code or resetting the device. XXX To investigate.
          if (inByte != 0xff) {
            RAISE_USBCANERROR(input, "Error reading USB data: Expecting a DLE byte. Dropping byte 0x%02X", inByte);
          }
          rxFrameState = RX_FRAME_ERROR_EXPECTING_DLE;
        }
        break;
      case RX_FRAME_START:
        if (inByte == USB_STX) {
          rxFrameState = RX_FRAME_CONTENT;
        } else {
          RAISE_USBCANERROR(input, "Error reading USB data: Expecting a STX byte. Dropping byte 0x%02X", inByte);
          rxFrameState = RX_FRAME_ERROR_EXPECTING_STX;
        }
        break;
      case RX_FRAME_CONTENT:
        if (inByte == USB_DLE) {
          rxFrameState = RX_FRAME_CONTENT_NLE;
        } else {
          // We append content in frame buffer and computer checksum
          if (rxFrameLength < RX_FRAME_DATA_MAX_LENGTH) {
            rxFrameData[rxFrameLength] = inByte;
            rxFrameChecksum ^= inByte;
            rxFrameLength++;
          } else {
            RAISE_USBCANERROR(input, "Error reading USB data: Not enough space in buffer. Dropping byte 0x%02X", inByte);
            rxFrameState = RX_FRAME_ERROR_BUFFER_OVERFLOW;
          }
        }
        break;
      case RX_FRAME_CONTENT_NLE:
        if (inByte == USB_ETX) {
          if (rxFrameChecksum == 0) { // checksum should be zero if message was good
            rxFrameState = RX_FRAME_COMPLETE;
            rxFrameLength--; // we are always 1 ahead
          } else {
            RAISE_USBCANERROR(input, "Error reading USB data: Bad frame checksum!");
            rxFrameState = RX_FRAME_ERROR_BAD_CHECKSUM;
          }
        } else if (inByte == USB_STX) {
          RAISE_USBCANERROR(input, "Error reading USB data: Expecting a ETX byte or content byte. Dropping byte 0x%02X", inByte);
          rxFrameState = RX_FRAME_ERROR_EXPECTING_ETX;
        } else {
          // We append content in frame buffer and computer checksum
          if (rxFrameLength < RX_FRAME_DATA_MAX_LENGTH) {
            rxFrameData[rxFrameLength] = inByte;
            rxFrameChecksum ^= inByte;
            rxFrameLength++;
            rxFrameState = RX_FRAME_CONTENT;
          } else {
            RAISE_USBCANERROR(input, "Error reading USB data: Not enough space in buffer. Dropping byte 0x%02X", inByte);
            rxFrameState = RX_FRAME_ERROR_BUFFER_OVERFLOW;
          }
        }
        break;
    }

    // Process complete frame
    if (rxFrameState == RX_FRAME_COMPLETE) {

      if ((rxFrameData[0] == 0x00) || (rxFrameData[0] == 0xff)) {
        // A frame from the USB-CAN board
        BoardMessage* message = CreateBoardMessage(rxFrameData, rxFrameLength);
        input->_boardMessageQueue.push(message);
        uv_async_send(&input->_boardMessageEmitAsync);
      } else {
        // A frame from the CAN bus
        CanBusMessage* message = CreateCanBusMessage(rxFrameData, rxFrameLength);
        input->_canBusMessageQueue.push(message);
        uv_async_send(&input->_canBusMessageEmitAsync);
      }

      rxFrameState = RX_FRAME_IDLE;
    }
  }
}

void ApoxUsbCan::UsbCanErrorEmitter(uv_async_t* w, int status)
{
  v8::HandleScope scope; // Mandatory, otherwise you leak!

  ApoxUsbCan* input = static_cast<ApoxUsbCan*>(w->data);
  
  while (!input->_usbCanErrorQueue.empty()) {
    UsbCanError* error = input->_usbCanErrorQueue.front();

    v8::Local<v8::Value> args[2];
    args[0] = v8::Local<v8::Value>::New(symbol_error);
    args[1] = v8::Local<v8::Value>::New(v8::String::New(error->message));

    MakeCallback(input->handle_, symbol_emit, 2, args);

    input->_usbCanErrorQueue.pop();
    delete error;
  }
}

void ApoxUsbCan::BoardMessageEmitter(uv_async_t* w, int status)
{
  v8::HandleScope scope; // Mandatory, otherwise you leak!

  ApoxUsbCan* input = static_cast<ApoxUsbCan*>(w->data);

  while (!input->_boardMessageQueue.empty()) {
    BoardMessage* message = input->_boardMessageQueue.front();

    v8::Local<v8::Value> args[4];
    args[0] = v8::Local<v8::Value>::New(symbol_boardmessage);
    args[1] = v8::Local<v8::Value>::New(v8::Number::New(message->id));
    args[2] = v8::Local<v8::Value>::New(v8::Number::New(message->command));

    if (message->dataLength > 0) {
      Buffer *slowBuffer = Buffer::New(message->dataLength);
      memcpy(node::Buffer::Data(slowBuffer), (char*)message->data, message->dataLength);
      args[3] = v8::Local<v8::Value>::New(slowBuffer->handle_);
    } else {
      args[3] = v8::Local<v8::Value>::New(v8::Undefined());
    }

    MakeCallback(input->handle_, symbol_emit, 4, args);

    input->_boardMessageQueue.pop();
    delete message;
  }
}

void ApoxUsbCan::CanBusMessageEmitter(uv_async_t* w, int status)
{
  v8::HandleScope scope; // Mandatory, otherwise you leak!

  ApoxUsbCan* input = static_cast<ApoxUsbCan*>(w->data);

  while (!input->_canBusMessageQueue.empty()) {
    CanBusMessage* message = input->_canBusMessageQueue.front();

    v8::Local<v8::Value> args[7];
    args[0] = v8::Local<v8::Value>::New(symbol_canbusmessage);
    args[1] = v8::Local<v8::Value>::New(v8::Number::New(message->timestamp));
    args[2] = v8::Local<v8::Value>::New(v8::Boolean::New(message->rtr));
    args[3] = v8::Local<v8::Value>::New(v8::Number::New(message->id));
    args[4] = v8::Local<v8::Value>::New(v8::Boolean::New(message->extended));
    args[5] = v8::Local<v8::Value>::New(v8::Number::New(message->flags));

    if (message->dataLength > 0) {
      Buffer *slowBuffer = Buffer::New(message->dataLength);
      memcpy(node::Buffer::Data(slowBuffer), (char*)message->data, message->dataLength);
      args[6] = v8::Local<v8::Value>::New(slowBuffer->handle_);
    } else {
      args[6] = v8::Local<v8::Value>::New(v8::Undefined());
    }

    MakeCallback(input->handle_, symbol_emit, 7, args);

    input->_canBusMessageQueue.pop();
    delete message;
  }
}

int ApoxUsbCan::UsbWrite(unsigned char *txFrameData, int txFrameLength) {
  // USB message format
  // ------------------
  // [0] DLE
  // [1] STX
  // [2..n-3] data, but if data contains a DLE, then insert another DLE before it (BYTE Stuffing)
  // [n-2] CSUM
  // [n-1] DLE
  // [n] ETX
  uv_mutex_lock(&_usbWriteMutex);

  unsigned char txBuffer[300]; // XXX Possible buffer overflow, but OK for now (protected by callers)
  int txLength = 0;
  unsigned char txFrameChecksum = 0;

  // Start the transmission
  txBuffer[txLength++] = USB_DLE;
  txBuffer[txLength++] = USB_STX;  // start transmission

  // BYTE Stuff the data and calculate checksum
  for (int i = 0; i < txFrameLength; i++) {
    txFrameChecksum ^= txFrameData[i];

    if (txFrameData[i] == USB_DLE) {
      txBuffer[txLength++] = USB_DLE;
    }
    txBuffer[txLength++] = txFrameData[i];
  }

  // BYTE STUFF checksum if necessary
  if (txFrameChecksum == USB_DLE) {
    txBuffer[txLength++] = USB_DLE;
  }

  // Send the checksum
  txBuffer[txLength++] = txFrameChecksum;

  // Terminate the transmission
  txBuffer[txLength++] = USB_DLE;
  txBuffer[txLength++] = USB_ETX;  // end transmission

  int rc = ftdi_write_data(&_ftdic, txBuffer, txLength);

  uv_mutex_unlock(&_usbWriteMutex);
  return rc; 
}

// ------ Message Factory Methods ------

BoardMessage* CreateBoardMessage(unsigned char* rxFrameData, int rxFrameLength)
{
  // Unsolicited Emergency Message from the board
  // --------------------------------------------
  // [0] 0xFF
  // [1] 0x80
  // [2] ERRORCODE (0-255)
  //
  // Response to config message from board
  // -------------------------------------
  // [0] 0x00
  // [1] command | 0x80
  // [2..n] response data               

  BoardMessage* message = new BoardMessage;
  message->id = rxFrameData[0];
  message->command = rxFrameData[1] & 0x7f;
  message->dataLength = rxFrameLength - 2; // minus two for the first two bytes
  for (int i = 0; i < message->dataLength && i < (int) sizeof(message->data); i++) { 
    message->data[i] = rxFrameData[i + 2];
  }

  return message;
}

CanBusMessage* CreateCanBusMessage(unsigned char* rxFrameData, int rxFrameLength)
{
  // Incoming CAN Message
  // --------------------
  // [0] ([1][RTR][EXT][unused 0..4]) (WARNING: never send back 0xFF, its reserved for emergency)
  // [1] ID MSB
  // [2] ID
  // [3] ID
  // [4] ID LSB
  // [5] TIMESTAMP MSB
  // [6] TIMESTAMP 
  // [7] TIMESTAMP 
  // [8] TIMESTAMP LSB
  // [9] RESERVED FOR RX FLAGS
  // [10] DATA LEN (0-8)
  // [11-18] DATA BYTES 0 to 8 (if needed)

  CanBusMessage *message = new CanBusMessage;
  message->rtr = (rxFrameData[0] & 0x40) ? true : false;
  message->extended = (rxFrameData[0] & 0x20) ? true : false;
  message->id = (((unsigned int) rxFrameData[4] << 24) & 0x1f000000) |
                (((unsigned int) rxFrameData[3] << 16) & 0x00ff0000) |
                (((unsigned int) rxFrameData[2] << 8) & 0x0000ff00) |
                (((unsigned int) rxFrameData[1]) & 0x000000ff);
  message->timestamp = (((unsigned int) rxFrameData[8] << 24) & 0xff000000) |
                       (((unsigned int) rxFrameData[7] << 16) & 0x00ff0000) |
                       (((unsigned int) rxFrameData[6] << 8) & 0x0000ff00) |
                       (((unsigned int) rxFrameData[5]) & 0x000000ff);
  message->flags = rxFrameData[9];

  message->dataLength = rxFrameData[10]; // minus two for the first two bytes
  for (int i = 0; i < message->dataLength && i < (int) sizeof(message->data); i++) { 
    message->data[i] = rxFrameData[i + 11];
  }

  return message;
}

extern "C" {
  void init(v8::Handle<v8::Object> target) {
    ApoxUsbCan::Initialize(target);
  }
  NODE_MODULE(apoxusbcan, init)
}
