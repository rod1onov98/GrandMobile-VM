#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <iomanip>

static std::string b64encode(const uint8_t* data, size_t len) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 3 <= len; i += 3) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(chars[(n >> 18) & 0x3F]);
        out.push_back(chars[(n >> 12) & 0x3F]);
        out.push_back(chars[(n >> 6) & 0x3F]);
        out.push_back(chars[n & 0x3F]);
    }
    size_t rem = len - i;
    if (rem == 1) {
        uint32_t n = data[i] << 16;
        out.push_back(chars[(n >> 18) & 0x3F]);
        out.push_back(chars[(n >> 12) & 0x3F]);
        out.push_back('='); out.push_back('=');
    }
    else if (rem == 2) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
        out.push_back(chars[(n >> 18) & 0x3F]);
        out.push_back(chars[(n >> 12) & 0x3F]);
        out.push_back(chars[(n >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

static std::vector<uint8_t> encrypt_native_raw(const std::string& input, int32_t seed) {
    std::vector<uint8_t> buf(input.begin(), input.end());
    size_t total_len = buf.size();
    if (total_len == 0) return buf;
    uint32_t key = static_cast<uint32_t>(seed);
    auto init_states = [](uint32_t a3) -> std::pair<uint32_t, uint32_t> {
        uint32_t s1 = 0x62098F3C ^ ((30025 | (a3 << 13)) ^ ((a3 & 3) * 8));
        uint32_t s2 = (s1 << 16) | 0x448C;
        return { s1, s2 };
        };

    size_t num_full_blocks = total_len / 64;
    size_t rem_len = total_len % 64;
    uint32_t a3 = key;
    for (size_t block_idx = 0; block_idx < num_full_blocks; ++block_idx) {
        size_t local_24 = 0;
        for (int outer = 0; outer < 8; ++outer) {
            auto [state1, state2] = init_states(a3);
            for (int inner = 0; inner < 2; ++inner) {
                size_t addr = block_idx * 64 + inner * 4 + local_24;
                uint32_t gamma = 0x7890CFB3 ^ a3;
                uint32_t v;
                memcpy(&v, buf.data() + addr, 4);
                v ^= gamma;
                memcpy(buf.data() + addr, &v, 4);
                a3 ^= state1;
                state1 = state1 * 0x06511073 + state2;
                state2 ^= static_cast<uint32_t>(inner);
            }
            local_24 += 8;
        }
    }
    if (rem_len > 0) {
        auto [state, _s2] = init_states(key);
        (void)_s2;
        uint32_t step = key * 0x06511073;
        for (size_t j = 0; j < rem_len; ++j) {
            size_t idx = total_len - 1 - j;
            uint8_t gamma = static_cast<uint8_t>(state & 0xFF);
            buf[idx] ^= gamma;
            state += step;
        }
    }
    return buf;
}

static std::string encrypt_native(const std::string& input, int32_t seed) {
    std::vector<uint8_t> buf = encrypt_native_raw(input, seed);
    return b64encode(buf.data(), buf.size());
}