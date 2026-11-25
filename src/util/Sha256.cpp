#include "mpl/util/Sha256.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <vector>

namespace mpl {
namespace {

constexpr std::uint32_t kRound[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

std::uint32_t rotr(std::uint32_t value, std::uint32_t count) {
    return (value >> count) | (value << (32u - count));
}

}  // namespace

void Sha256::update(std::string_view bytes) {
    for (const char character : bytes) {
        buffer_[bufferSize_++] = static_cast<unsigned char>(character);
        if (bufferSize_ == 64) {
            transform(buffer_);
            bufferSize_ = 0;
        }
    }
    bitCount_ += static_cast<std::uint64_t>(bytes.size()) * 8u;
}

void Sha256::transform(const unsigned char* block) {
    std::uint32_t words[64] = {};
    for (int index = 0; index < 16; ++index) {
        words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24) |
                       (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8) |
                       static_cast<std::uint32_t>(block[index * 4 + 3]);
    }
    for (int index = 16; index < 64; ++index) {
        const std::uint32_t s0 = rotr(words[index - 15], 7) ^
                                 rotr(words[index - 15], 18) ^ (words[index - 15] >> 3);
        const std::uint32_t s1 = rotr(words[index - 2], 17) ^
                                 rotr(words[index - 2], 19) ^ (words[index - 2] >> 10);
        words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (int index = 0; index < 64; ++index) {
        const std::uint32_t sum1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t choose = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + sum1 + choose + kRound[index] + words[index];
        const std::uint32_t sum0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::string Sha256::hexDigest() {
    const std::uint64_t bitCount = bitCount_;
    const unsigned char padding = 0x80;
    update(std::string_view(reinterpret_cast<const char*>(&padding), 1));
    const unsigned char zero = 0;
    while (bufferSize_ != 56) {
        if (bufferSize_ == 64) {
            transform(buffer_);
            bufferSize_ = 0;
        }
        buffer_[bufferSize_++] = zero;
    }
    unsigned char length[8];
    for (int index = 0; index < 8; ++index) {
        length[7 - index] = static_cast<unsigned char>((bitCount >> (index * 8)) & 0xFF);
    }
    std::memcpy(buffer_ + 56, length, 8);
    transform(buffer_);
    bufferSize_ = 0;
    std::ostringstream stream;
    for (const std::uint32_t word : state_) {
        stream << std::hex << std::setw(8) << std::setfill('0') << word;
    }
    return stream.str();
}

std::string sha256Hex(std::string_view bytes) {
    Sha256 hash;
    hash.update(bytes);
    return hash.hexDigest();
}

std::string sha256FileHex(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    std::string bytes{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
    return sha256Hex(bytes);
}

}  // namespace mpl
