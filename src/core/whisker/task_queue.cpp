#include "task_queue.h"
#include <glog/logging.h>

namespace whisker {

TaskQueue::TaskQueue() {
    work_thread = std::thread{&TaskQueue::ProcessTasks, this};
}

TaskQueue::~TaskQueue() {
    if (run_work_thread) {
        FinishQueueSync();
    }
}

std::size_t TaskQueue::GetNumTasks() {
    std::scoped_lock lock(task_queue_mutex);
    return task_queue.size();
}

void TaskQueue::FinishQueueSync() {
    if (const auto num_tasks = GetNumTasks(); num_tasks > 1) {
        LOG(INFO) << "Draining task queue, " << num_tasks << " tasks remaining";
    }
    run_work_thread = false;
    task_queue_cv.notify_one();
    work_thread.join();
}

void TaskQueue::ProcessTasks() {
    const auto should_proceed = [this] { return !task_queue.empty() || !run_work_thread; };
    while (true) {
        std::unique_lock lock(task_queue_mutex);
        task_queue_cv.wait(lock, should_proceed);
        if (task_queue.empty() && !run_work_thread) {
            return;
        }
        const auto task_ptr = std::move(task_queue.front());
        task_queue.pop();
        lock.unlock();
        (*task_ptr)();
    }
}

}  // namespace whisker
