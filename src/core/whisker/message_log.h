#ifndef WHISKER_MESSAGE_LOG_H
#define WHISKER_MESSAGE_LOG_H

#include <cstdint>
#include <memory>
#include <string>

namespace google::protobuf {
class MessageLite;
}

namespace whisker {

constexpr std::uint64_t message_log_default_header = 0x3130676F6C6B7377;  // little-endian 'wsklog01'

class MessageLogReader {
  public:
    virtual ~MessageLogReader() = default;

    virtual bool Read(google::protobuf::MessageLite& message) = 0;

    virtual float GetReadPercent() = 0;

    static std::shared_ptr<MessageLogReader> CreateInstance(const std::string& log_file_path,
                                                            std::uint64_t magic_header = message_log_default_header);
};

class MessageLogWriter {
  public:
    virtual ~MessageLogWriter() = default;

    virtual void Write(std::shared_ptr<const google::protobuf::MessageLite> message) = 0;

    virtual std::size_t GetNumPendingWrites() = 0;

    static std::shared_ptr<MessageLogWriter> CreateInstance(const std::string& log_file_path,
                                                            std::uint64_t magic_header = message_log_default_header);
};

}  // namespace whisker

#endif  // WHISKER_MESSAGE_LOG_H
