#pragma once

#include "mpl/lab/ModelAdapter.hpp"

#include <string>

namespace mpl {

/// Offline adapter: returns a candidate supplied by the experiment file. Used
/// for reproducible imports without any API.
class StaticModelAdapter final : public ModelAdapter {
public:
    StaticModelAdapter(std::string source, std::string model = "offline");
    std::string name() const override { return model_; }
    ModelResponse generate(const ModelRequest& request) override;

private:
    std::string source_;
    std::string model_;
};

/// Optional OpenAI-compatible chat-completions adapter. Credentials are read
/// only from the environment:
///   MPL_MODEL_API_KEY, MPL_MODEL_BASE_URL, MPL_MODEL_NAME
/// The key is never logged or written to artifacts.
class OpenAiCompatibleAdapter final : public ModelAdapter {
public:
    OpenAiCompatibleAdapter();
    std::string name() const override;
    ModelResponse generate(const ModelRequest& request) override;

private:
    std::string baseUrl_;
    std::string model_;
    std::string apiKey_;
};

}  // namespace mpl
