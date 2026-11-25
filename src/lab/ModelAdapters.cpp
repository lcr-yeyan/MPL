#include "mpl/lab/ModelAdapters.hpp"

#include "mpl/util/JsonValue.hpp"
#include "mpl/util/JsonWriter.hpp"

#include <cstdlib>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>
#endif

namespace mpl {
namespace {

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

std::string stripCodeFence(std::string text) {
    const std::size_t begin = text.find("```");
    if (begin == std::string::npos) {
        return text;
    }
    std::size_t start = text.find('\n', begin);
    if (start == std::string::npos) {
        return text;
    }
    ++start;
    const std::size_t end = text.find("```", start);
    if (end == std::string::npos) {
        return text;
    }
    return text.substr(start, end - start);
}

}  // namespace

StaticModelAdapter::StaticModelAdapter(std::string source, std::string model)
    : source_(std::move(source)), model_(std::move(model)) {}

ModelResponse StaticModelAdapter::generate(const ModelRequest& request) {
    ModelResponse response;
    response.ok = true;
    response.candidateSource = source_;
    response.model = model_;
    response.modelVersion = "offline";
    response.promptVersion = request.promptVersion;
    response.sampling = request.sampling;
    response.modelCalls = 0;
    return response;
}

OpenAiCompatibleAdapter::OpenAiCompatibleAdapter()
    : baseUrl_(envOrEmpty("MPL_MODEL_BASE_URL")),
      model_(envOrEmpty("MPL_MODEL_NAME")),
      apiKey_(envOrEmpty("MPL_MODEL_API_KEY")) {
    if (baseUrl_.empty()) {
        baseUrl_ = "https://api.openai.com/v1";
    }
}

std::string OpenAiCompatibleAdapter::name() const {
    return model_.empty() ? "openai-compatible" : model_;
}

ModelResponse OpenAiCompatibleAdapter::generate(const ModelRequest& request) {
    ModelResponse response;
    response.model = model_;
    response.promptVersion = request.promptVersion;
    response.sampling = request.sampling;
    if (apiKey_.empty()) {
        response.ok = false;
        response.error = "MPL_MODEL_API_KEY is not set";
        return response;
    }
    if (model_.empty()) {
        response.ok = false;
        response.error = "MPL_MODEL_NAME is not set";
        return response;
    }

    JsonWriter body;
    body.beginObject();
    body.key("model").value(model_);
    body.key("temperature").value(0.2);
    if (request.maxOutputTokens > 0) {
        body.key("max_tokens").value(request.maxOutputTokens);
    }
    body.key("messages").beginArray();
    body.beginObject();
    body.key("role").value("system");
    body.key("content").value(
        "You write MPL music programs. Return only one complete MPL program, "
        "no markdown fences.");
    body.endObject();
    body.beginObject();
    body.key("role").value("user");
    body.key("content").value(request.languageContext + "\n\n" + request.prompt +
                              "\n\n" + request.semanticFeedback +
                              (request.previousCandidate.empty()
                                   ? ""
                                   : "\n\nPrevious candidate:\n" +
                                         request.previousCandidate));
    body.endObject();
    body.endArray();
    body.endObject();
    const std::string payload = body.take();

#ifdef _WIN32
    const std::wstring url(baseUrl_.begin(), baseUrl_.end());
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
        response.ok = false;
        response.error = "invalid model base URL";
        return response;
    }
    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) {
        path = L"/";
    }
    path += L"/chat/completions";

    HINTERNET session = WinHttpOpen(L"MPL/0.4", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        response.ok = false;
        response.error = "WinHttpOpen failed";
        return response;
    }
    WinHttpSetTimeouts(session, 30000, 30000, 120000, 120000);
    HINTERNET connection = WinHttpConnect(session, host.c_str(),
                                          components.nPort, 0);
    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        response.ok = false;
        response.error = "WinHttpConnect failed";
        return response;
    }
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS
                            ? WINHTTP_FLAG_SECURE
                            : 0;
    HINTERNET httpRequest = WinHttpOpenRequest(
        connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (httpRequest == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        response.ok = false;
        response.error = "WinHttpOpenRequest failed";
        return response;
    }
    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
                                 std::wstring(apiKey_.begin(), apiKey_.end()) + L"\r\n";
    if (!WinHttpSendRequest(httpRequest, headers.c_str(),
                            static_cast<DWORD>(-1L), const_cast<char*>(payload.data()),
                            static_cast<DWORD>(payload.size()),
                            static_cast<DWORD>(payload.size()), 0) ||
        !WinHttpReceiveResponse(httpRequest, nullptr)) {
        WinHttpCloseHandle(httpRequest);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        response.ok = false;
        response.error = "model HTTP request failed";
        return response;
    }
    std::string reply;
    DWORD available = 0;
    do {
        available = 0;
        if (!WinHttpQueryDataAvailable(httpRequest, &available)) {
            break;
        }
        if (available == 0) {
            break;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(httpRequest, buffer.data(), available, &read)) {
            break;
        }
        reply.append(buffer.data(), read);
    } while (available > 0);
    WinHttpCloseHandle(httpRequest);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    JsonValue root;
    std::string error;
    if (!parseJson(reply, root, error) || !root.isObject()) {
        response.ok = false;
        response.error = "malformed model response";
        return response;
    }
    if (const JsonValue* apiError = root.find("error"); apiError != nullptr) {
        response.ok = false;
        if (const JsonValue* message = apiError->find("message");
            message != nullptr && message->isString()) {
            response.error = message->text;
        } else {
            response.error = "model API error";
        }
        return response;
    }
    if (const JsonValue* usage = root.find("usage"); usage != nullptr) {
        if (const JsonValue* tokens = usage->find("prompt_tokens");
            tokens != nullptr && tokens->isNumber()) {
            response.inputTokens = static_cast<int>(tokens->number);
        }
        if (const JsonValue* tokens = usage->find("completion_tokens");
            tokens != nullptr && tokens->isNumber()) {
            response.outputTokens = static_cast<int>(tokens->number);
        }
        if (const JsonValue* tokens = usage->find("cached_tokens");
            tokens != nullptr && tokens->isNumber()) {
            response.cacheTokens = static_cast<int>(tokens->number);
        }
    }
    response.modelCalls = 1;
    if (const JsonValue* choices = root.find("choices");
        choices != nullptr && choices->isArray() && !choices->array.empty()) {
        const JsonValue& choice = choices->array.front();
        if (const JsonValue* message = choice.find("message");
            message != nullptr && message->isObject()) {
            if (const JsonValue* content = message->find("content");
                content != nullptr && content->isString()) {
                response.candidateSource = stripCodeFence(content->text);
                response.ok = !response.candidateSource.empty();
                if (!response.ok) {
                    response.error = "model returned empty content";
                }
                if (const JsonValue* responseModel = root.find("model");
                    responseModel != nullptr && responseModel->isString()) {
                    response.model = responseModel->text;
                }
                return response;
            }
        }
    }
    response.ok = false;
    response.error = "model response has no choices[0].message.content";
    return response;
#else
    (void)payload;
    response.ok = false;
    response.error = "OpenAI-compatible adapter is only implemented on Windows";
    return response;
#endif
}

}  // namespace mpl
