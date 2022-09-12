#ifndef WHISKER_OVERWRITING_BUFFER_H
#define WHISKER_OVERWRITING_BUFFER_H

#include <condition_variable>
#include <memory>
#include <mutex>

namespace whisker {

// Buffer that accepts a stream of incoming data while maintaining only the latest unread copy.
//
// The expected use case is for a writer to repeatedly write to the buffer from one thread, with the
// reader accessing the latest unread piece of data from another thread.  If no unread data is available,
// the read call will block until a new write has completed.
//
// There should only be one reader and one writer per OverwritingBuffer.

template <typename T>
class OverwritingBuffer final {
  public:
    OverwritingBuffer() = default;

    OverwritingBuffer(const OverwritingBuffer&) = delete;
    OverwritingBuffer& operator=(const OverwritingBuffer&) = delete;

    // this will block until there is data available to be read
    template <typename Reader>
    void Read(const Reader& reader) {
        std::unique_lock lock(index_mutex);
        wait_cv.wait(lock, [this] { return index_newest != -1 && index_reading == -1; });

        index_reading = index_newest;
        index_newest = -1;
        lock.unlock();

        reader(&buffer[index_reading]);

        lock.lock();
        index_reading = -1;
    }

    template <typename Writer>
    void Write(const Writer& writer) {
        std::unique_lock lock(index_mutex);
        for (auto i = 0; i < num_slots; ++i) {
            if (i != index_newest && i != index_reading && index_writing == -1) {
                index_writing = i;
                lock.unlock();

                writer(&buffer[i]);

                lock.lock();
                index_newest = i;
                index_writing = -1;
                wait_cv.notify_one();

                return;
            }
        }
    }

  private:
    static constexpr unsigned short num_slots = 3;
    const std::unique_ptr<T[]> buffer = std::make_unique<T[]>(num_slots);
    short index_newest = -1;
    short index_reading = -1;
    short index_writing = -1;
    std::mutex index_mutex;
    std::condition_variable wait_cv;
};

}  // namespace whisker

#endif  // WHISKER_OVERWRITING_BUFFER_H
