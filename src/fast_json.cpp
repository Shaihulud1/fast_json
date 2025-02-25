#include <node.h>
#include <uv.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <memory>
#include <iostream>


namespace fast_json {

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::Array;
using v8::Number;
using v8::Context;

// Быстрое преобразование строки в число
double FastAtof(const char* str, size_t length) {
    double result = 0.0;
    double factor = 1.0;
    bool decimal = false;
    bool negative = false;
    
    if (*str == '-') {
        negative = true;
        ++str;
        --length;
    }

    for (size_t i = 0; i < length; ++i) {
        if (str[i] == '.') {
            decimal = true;
            continue;
        }
        if (decimal) {
            factor /= 10.0;
            result += (str[i] - '0') * factor;
        } else {
            result = result * 10.0 + (str[i] - '0');
        }
    }

    return negative ? -result : result;
}

class JsonParser {
public:
    JsonParser(const std::string& jsonStr) : json(jsonStr), length(jsonStr.length()), pos(0) {}

    Local<Value> Parse(Isolate* isolate) {
        pos = 0;
        return ParseValue(isolate);
    }

private:
    void SkipWhitespace() {
        while (pos < length) {
            switch (json[pos]) {
                case ' ':
                case '\t':
                case '\n':
                case '\r':
                    ++pos;
                    break;
                default:
                    return;
            }
        }
    }

    Local<Value> ParseValue(Isolate* isolate) {
        SkipWhitespace();
        if (pos >= length) return v8::Undefined(isolate);

        switch (json[pos]) {
            case '{': return ParseObject(isolate);
            case '[': return ParseArray(isolate);
            case '"': return ParseString(isolate);
            case 't': return ParseTrue(isolate);
            case 'f': return ParseFalse(isolate);
            case 'n': return ParseNull(isolate);
            default: return ParseNumber(isolate);
        }
    }

    Local<Object> ParseObject(Isolate* isolate) {
        Local<Object> obj = Object::New(isolate);
        ++pos; // Skip '{'
        while (pos < length) {
            SkipWhitespace();
            if (json[pos] == '}') {
                ++pos;
                break;
            }
            if (json[pos] != '"') throw std::runtime_error("Expected string key in object");
            
            Local<String> key = ParseString(isolate);
            SkipWhitespace();
            if (json[pos] != ':') throw std::runtime_error("Expected ':' after key in object");
            ++pos;
            Local<Value> value = ParseValue(isolate);
            obj->Set(isolate->GetCurrentContext(), key, value).Check();
            SkipWhitespace();
            if (json[pos] == ',') ++pos;
        }
        return obj;
    }

    Local<Array> ParseArray(Isolate* isolate) {
        std::vector<Local<Value>> values;
        ++pos; // Skip '['
        while (pos < length) {
            SkipWhitespace();
            if (json[pos] == ']') {
                ++pos;
                break;
            }
            values.push_back(ParseValue(isolate));
            SkipWhitespace();
            if (json[pos] == ',') ++pos;
        }
        Local<Array> arr = Array::New(isolate, static_cast<int>(values.size()));
        for (size_t i = 0; i < values.size(); ++i) {
            arr->Set(isolate->GetCurrentContext(), static_cast<uint32_t>(i), values[i]).Check();
        }
        return arr;
    }

    Local<String> ParseString(Isolate* isolate) {
        ++pos; // Skip opening quote
        size_t start = pos;
        while (pos < length && json[pos] != '"') ++pos;
        Local<String> str = String::NewFromUtf8(isolate, json.c_str() + start, v8::NewStringType::kNormal, pos - start).ToLocalChecked();
        ++pos; // Skip closing quote
        return str;
    }

    Local<Value> ParseNumber(Isolate* isolate) {
        size_t start = pos;
        while (pos < length && (std::isdigit(json[pos]) || json[pos] == '.' || json[pos] == 'e' || json[pos] == 'E' || json[pos] == '+' || json[pos] == '-')) ++pos;
        double number = FastAtof(json.c_str() + start, pos - start);
        return Number::New(isolate, number);
    }

    Local<Value> ParseTrue(Isolate* isolate) {
        pos += 4; // Skip 'true'
        return v8::True(isolate);
    }

    Local<Value> ParseFalse(Isolate* isolate) {
        pos += 5; // Skip 'false'
        return v8::False(isolate);
    }

    Local<Value> ParseNull(Isolate* isolate) {
        pos += 4; // Skip 'null'
        return v8::Null(isolate);
    }

    const std::string& json;
    size_t length;
    size_t pos;
};

struct WorkRequest {
    uv_work_t request;
    v8::Persistent<v8::Function> callback;
    std::string json;
    v8::Persistent<v8::Value> result;
    std::string error;
    v8::Isolate* isolate;
};

void ParseAsyncWork(uv_work_t* req) {
    std::cout << "ParseAsyncWork started" << std::endl;
    WorkRequest* work_req = static_cast<WorkRequest*>(req->data);
    
    try {
        std::cout << "Parsing JSON: " << work_req->json << std::endl;
        JsonParser parser(work_req->json);
        work_req->error = ""; // Clear any previous error
    } catch (const std::exception& e) {
        work_req->error = e.what();
        std::cerr << "Exception in ParseAsyncWork: " << e.what() << std::endl;
    } catch (...) {
        work_req->error = "Unknown error occurred";
        std::cerr << "Unknown exception in ParseAsyncWork" << std::endl;
    }
    std::cout << "ParseAsyncWork finished" << std::endl;
}

void ParseAsyncAfter(uv_work_t* req, int status) {
    std::cout << "ParseAsyncAfter started" << std::endl;
    std::unique_ptr<WorkRequest> work_req(static_cast<WorkRequest*>(req->data));
    v8::Isolate* isolate = work_req->isolate;
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Context::Scope context_scope(context);

    v8::Local<v8::Value> argv[2];
    if (!work_req->error.empty()) {
        argv[0] = v8::Exception::Error(v8::String::NewFromUtf8(isolate, work_req->error.c_str()).ToLocalChecked());
        argv[1] = v8::Null(isolate);
    } else {
        argv[0] = v8::Null(isolate);
        JsonParser parser(work_req->json);
        argv[1] = parser.Parse(isolate);
    }

    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(isolate, work_req->callback);
    callback->Call(context, v8::Null(isolate), 2, argv).ToLocalChecked();
    
    work_req->callback.Reset();
    std::cout << "ParseAsyncAfter finished" << std::endl;
}

void ParseAsync(const v8::FunctionCallbackInfo<v8::Value>& args) {
    std::cout << "ParseAsync called" << std::endl;

    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsFunction()) {
        isolate->ThrowException(v8::Exception::TypeError(
            v8::String::NewFromUtf8(isolate, "Wrong arguments").ToLocalChecked()));
        return;
    }

    v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);

    WorkRequest* work_req = new WorkRequest();
    work_req->request.data = work_req;
    work_req->callback.Reset(isolate, callback);
    work_req->isolate = isolate;

    v8::String::Utf8Value str(isolate, args[0]);
    work_req->json = std::string(*str);

    uv_loop_t* loop = uv_default_loop();
    uv_queue_work(loop, &work_req->request, ParseAsyncWork, ParseAsyncAfter);
}

} // namespace fast_json

void Init(v8::Local<v8::Object> exports) {
    NODE_SET_METHOD(exports, "parseAsync", fast_json::ParseAsync);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Init)

