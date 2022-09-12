#ifndef WHISKER_TASK_QUEUE_H
#define WHISKER_TASK_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace whisker {

class TaskQueue final {
  public:
    using Task = std::function<void()>;

    TaskQueue();
    ~TaskQueue();

    TaskQueue(const TaskQueue&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;

    template <typename T>
    void AddTask(T&& task) {
        std::scoped_lock lock(task_queue_mutex);
        task_queue.emplace(std::make_unique<Task>(std::forward<T>(task)));
        task_queue_cv.notify_one();
    }

    std::size_t GetNumTasks();
    void FinishQueueSync();

  private:
    void ProcessTasks();

    std::queue<std::unique_ptr<Task>> task_queue;
    std::mutex task_queue_mutex;
    std::condition_variable task_queue_cv;
    std::thread work_thread;
    std::atomic_bool run_work_thread = true;
};

}  // namespace whisker

#endif  // WHISKER_TASK_QUEUE_H
