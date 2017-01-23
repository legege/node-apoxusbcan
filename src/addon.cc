#include <nan.h>
#include "node_apoxusbcan.h"

void InitAll(v8::Local<v8::Object> exports) {
  ApoxUsbCan::Init(exports);
}

NODE_MODULE(apoxusbcan, InitAll)