#pragma once

#include "mpl/core/Diagnostic.hpp"
#include "mpl/ir/ProductionIR.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace mpl {

struct PassStats {
    std::string name;
    double milliseconds = 0.0;
};

/// A production-level transformation. Passes report problems through the
/// diagnostics list instead of throwing so that one compile can surface
/// several issues at once.
class Pass {
public:
    virtual ~Pass() = default;

    virtual std::string_view name() const = 0;
    virtual void run(ProductionIR& production, std::vector<Diagnostic>& diagnostics) const = 0;
};

class PassManager {
public:
    void add(std::unique_ptr<Pass> pass);

    std::vector<PassStats> run(ProductionIR& production,
                               std::vector<Diagnostic>& diagnostics) const;

    std::size_t size() const noexcept { return passes_.size(); }

private:
    std::vector<std::unique_ptr<Pass>> passes_;
};

}  // namespace mpl
