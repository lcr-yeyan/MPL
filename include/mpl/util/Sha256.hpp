#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace mpl {

struct Sha256Digest {
    std::uint32_t state[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::uint64_t byteCount = 0;
    unsigned char buffer[64] = {};
    std::size_t bufferSize = 0;
};

/// Incremental SHA-256 (FIPS 180-4). Used for experiment artifact identity.
class Sha256 {
public:
    void update(std::string_view bytes);
    std::string hexDigest();

private:
    void transform(const unsigned char* block);

    std::uint32_t state_[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                               0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::uint64_t bitCount_ = 0;
    unsigned char buffer_[64] = {};
    std::size_t bufferSize_ = 0;
};

std::string sha256Hex(std::string_view bytes);
std::string sha256FileHex(const std::string& path);

}  // namespace mpl
