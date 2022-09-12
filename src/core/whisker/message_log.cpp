#include "message_log.h"
#include <filesystem>
#include <mutex>
#include <utility>
#include <glog/logging.h>
#include <google/protobuf/message_lite.h>
#include <zlib.h>
#include <whisker/task_queue.h>

namespace whisker {

class MessageLogReaderImpl final : public MessageLogReader {
  public:
    MessageLogReaderImpl(const std::string& log_file_path, std::uint64_t magic_header) : log_file_path(log_file_path) {
        LOG(INFO) << "Opening message log file " << std::filesystem::absolute(log_file_path).lexically_normal()
                  << " for reading";

        log_file = gzopen(log_file_path.c_str(), "rb");
        PCHECK(log_file) << "Error opening message log file";

        file_size = std::filesystem::file_size(log_file_path);

        std::uint64_t log_file_header;
        CHECK(ReadValue(log_file_header)) << "Error reading header value from message log file";
        CHECK_EQ(log_file_header, magic_header) << "Unexpected header value in message log file";
    }

    ~MessageLogReaderImpl() override {
        CHECK_EQ(gzclose_r(log_file), Z_OK) << "Error closing message log file";
        LOG(INFO) << "Closed message log file " << std::filesystem::absolute(log_file_path).lexically_normal();
    }

  private:
    bool Read(google::protobuf::MessageLite& message) override {
        std::scoped_lock lock(log_file_mutex);
        std::uint32_t message_size;
        if (!ReadValue(message_size)) {
            return false;
        }
        buffer.resize(message_size);
        if (gzfread(buffer.data(), message_size, 1, log_file) != 1) {
            return false;
        }
        return message.ParseFromString(buffer);
    }

    float GetReadPercent() override {
        std::scoped_lock lock(log_file_mutex);
        return static_cast<float>(gzoffset(log_file)) / file_size;
    }

    template <typename T>
    bool ReadValue(T& value) {
        constexpr auto width = sizeof(T);
        unsigned char buf[width];
        if (gzfread(buf, width, 1, log_file) == 1) {
            value = 0;
            for (auto i = (width - 1); i > 0; --i) {
                value |= buf[i];
                value <<= 8;
            }
            value |= buf[0];
            return true;
        } else {
            return false;
        }
    }

    const std::string log_file_path;
    gzFile log_file;
    std::mutex log_file_mutex;
    std::uintmax_t file_size;
    std::string buffer;
};

std::shared_ptr<MessageLogReader> MessageLogReader::CreateInstance(const std::string& log_file_path,
                                                                   std::uint64_t magic_header) {
    return std::make_shared<MessageLogReaderImpl>(log_file_path, magic_header);
}

class MessageLogWriterImpl final : public MessageLogWriter {
  public:
    MessageLogWriterImpl(const std::string& log_file_path, std::uint64_t magic_header) : log_file_path(log_file_path) {
        LOG(INFO) << "Opening message log file " << std::filesystem::absolute(log_file_path).lexically_normal()
                  << " for writing";

        log_file = gzopen(log_file_path.c_str(), "wb");
        PCHECK(log_file) << "Error opening message log file";

        WriteValue(magic_header);
    }

    ~MessageLogWriterImpl() override {
        task_queue.FinishQueueSync();  // do this in destructor to ensure class members outlive queue
        CHECK_EQ(gzclose_w(log_file), Z_OK) << "Error closing message log file";
        LOG(INFO) << "Closed message log file " << std::filesystem::absolute(log_file_path).lexically_normal();
    }

  private:
    void Write(std::shared_ptr<const google::protobuf::MessageLite> message) override {
        task_queue.AddTask([this, message = std::move(message)] {
            message->SerializeToString(&serialization_buffer);
            CHECK_LE(serialization_buffer.size(), UINT32_MAX) << "Messages larger than UINT32_MAX are unsupported";
            WriteValue(static_cast<std::uint32_t>(serialization_buffer.size()));
            WriteBuffer(serialization_buffer.c_str(), serialization_buffer.size());
        });
    }

    std::size_t GetNumPendingWrites() override { return task_queue.GetNumTasks(); }

    template <typename T>
    void WriteValue(T value) {
        constexpr auto width = sizeof(T);
        char buf[width];
        for (unsigned int i = 0; i < width; ++i) {
            buf[i] = ((value >> (i * 8)) & 0xff);
        }
        WriteBuffer(buf, width);
    }

    void WriteBuffer(const char* buffer, std::size_t size) {
        CHECK_EQ(gzfwrite(buffer, size, 1, log_file), 1) << "Error writing to message log file";
    }

    const std::string log_file_path;
    gzFile log_file;
    TaskQueue task_queue;
    std::string serialization_buffer;
};

std::shared_ptr<MessageLogWriter> MessageLogWriter::CreateInstance(const std::string& log_file_path,
                                                                   std::uint64_t magic_header) {
    return std::make_shared<MessageLogWriterImpl>(log_file_path, magic_header);
}

}  // namespace whisker
