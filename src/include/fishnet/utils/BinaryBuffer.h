#pragma once

/*
 * FishNet Binary Buffer
 *
 * Read/write utility for network packet serialization.
 * Supports: U8, U16 BE/LE, U24 LE, U32 BE, U64 BE, raw bytes, strings.
 * Tracks a read position for sequential parsing.
 * Throws std::out_of_range on buffer overrun.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>

namespace fishnet {

class BinaryBuffer {
public:
    BinaryBuffer() = default;

    explicit BinaryBuffer(size_t reserveSize) {
        data_.reserve(reserveSize);
    }

    BinaryBuffer(const uint8_t* data, size_t len)
        : data_(data, data + len) {}

    // Write operations

    void writeU8(uint8_t value) {
        data_.push_back(value);
    }

    void writeU16BE(uint16_t value) {
        data_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void writeU16LE(uint16_t value) {
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
        data_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    void writeU24LE(uint32_t value) {
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
        data_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    }

    void writeU32BE(uint32_t value) {
        data_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        data_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        data_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data_.push_back(static_cast<uint8_t>(value & 0xFF));
    }

    void writeU64BE(uint64_t value) {
        for (int i = 7; i >= 0; --i) {
            data_.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFF));
        }
    }

    void writeBytes(const uint8_t* src, size_t len) {
        data_.insert(data_.end(), src, src + len);
    }

    void writeString(const std::string& str) {
        writeU16BE(static_cast<uint16_t>(str.size()));
        data_.insert(data_.end(), str.begin(), str.end());
    }

    // Read operations

    uint8_t readU8() {
        checkRead(1);
        return data_[readPos_++];
    }

    uint16_t readU16BE() {
        checkRead(2);
        uint16_t val = (static_cast<uint16_t>(data_[readPos_]) << 8) |
                        static_cast<uint16_t>(data_[readPos_ + 1]);
        readPos_ += 2;
        return val;
    }

    uint16_t readU16LE() {
        checkRead(2);
        uint16_t val = static_cast<uint16_t>(data_[readPos_]) |
                       (static_cast<uint16_t>(data_[readPos_ + 1]) << 8);
        readPos_ += 2;
        return val;
    }

    uint32_t readU24LE() {
        checkRead(3);
        uint32_t val = static_cast<uint32_t>(data_[readPos_]) |
                       (static_cast<uint32_t>(data_[readPos_ + 1]) << 8) |
                       (static_cast<uint32_t>(data_[readPos_ + 2]) << 16);
        readPos_ += 3;
        return val;
    }

    uint32_t readU32BE() {
        checkRead(4);
        uint32_t val = (static_cast<uint32_t>(data_[readPos_]) << 24) |
                       (static_cast<uint32_t>(data_[readPos_ + 1]) << 16) |
                       (static_cast<uint32_t>(data_[readPos_ + 2]) << 8) |
                        static_cast<uint32_t>(data_[readPos_ + 3]);
        readPos_ += 4;
        return val;
    }

    uint64_t readU64BE() {
        checkRead(8);
        uint64_t val = 0;
        for (int i = 7; i >= 0; --i) {
            val |= static_cast<uint64_t>(data_[readPos_++]) << (8 * i);
        }
        return val;
    }

    std::vector<uint8_t> readBytes(size_t len) {
        checkRead(len);
        std::vector<uint8_t> result(data_.begin() + readPos_,
                                     data_.begin() + readPos_ + len);
        readPos_ += len;
        return result;
    }

    std::string readString() {
        uint16_t len = readU16BE();
        checkRead(len);
        std::string result(data_.begin() + readPos_,
                           data_.begin() + readPos_ + len);
        readPos_ += len;
        return result;
    }

    void skip(size_t bytes) {
        checkRead(bytes);
        readPos_ += bytes;
    }

    // Accessors

    const uint8_t* data() const { return data_.data(); }
    uint8_t* data() { return data_.data(); }
    size_t size() const { return data_.size(); }
    size_t readPosition() const { return readPos_; }
    size_t remaining() const { return data_.size() - readPos_; }
    bool empty() const { return data_.empty(); }

    void setReadPosition(size_t pos) { readPos_ = pos; }
    void clear() { data_.clear(); readPos_ = 0; }

    std::vector<uint8_t>& rawBuffer() { return data_; }
    const std::vector<uint8_t>& rawBuffer() const { return data_; }

private:
    std::vector<uint8_t> data_;
    size_t readPos_ = 0;

    void checkRead(size_t bytes) const {
        if (readPos_ + bytes > data_.size()) {
            throw std::out_of_range("BinaryBuffer: read beyond end of buffer");
        }
    }
};

} // namespace fishnet
