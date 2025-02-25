#ifndef FAST_JSON_H
#define FAST_JSON_H

#include <node.h>
#include <v8.h>

namespace fast_json {

void Parse(const v8::FunctionCallbackInfo<v8::Value>& args);

}

#endif