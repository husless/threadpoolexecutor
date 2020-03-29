#ifndef _THREAD_SAFE_H
#define _THREAD_SAFE_H

#include <atomic>
#include <condition_variable>
#include <functional> // std::bind
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class MoveOnlyCallable {
private:
    struct FunctorBase {
        virtual ~FunctorBase() = default;
        virtual void invoke() = 0;
    };

    std::unique_ptr<FunctorBase> m_functor;

    template <typename Func>
    struct Functor : FunctorBase {
        Functor(Func&& f) : m_function{std::move(f)} {}
        void invoke() override { m_function(); }
        Func m_function;
    };

public:
    MoveOnlyCallable() = default;
    template <typename Function>
    MoveOnlyCallable(Function&& f) : m_functor{new Functor<Function>(std::move(f))} {}

    MoveOnlyCallable(MoveOnlyCallable&& other) : m_functor{std::move(other.m_functor)} {}
    MoveOnlyCallable& operator=(MoveOnlyCallable&& rhs) {
        m_functor = std::move(rhs.m_functor);
        return *this;
    }
    MoveOnlyCallable(const MoveOnlyCallable&) = delete;
    MoveOnlyCallable& operator=(const MoveOnlyCallable&) = delete;

    void operator()() { m_functor->invoke(); }
};

class ThreadGuard {
public:
    explicit ThreadGuard(std::vector<std::thread>& threads) : m_threads{threads} {}
    ~ThreadGuard() {
        for (auto& t : m_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    std::vector<std::thread>& m_threads;
};

class ThreadPoolExecutor {
private:
    using callee_type = MoveOnlyCallable;
    std::atomic_bool m_done;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<callee_type> m_queue;
    std::vector<std::thread> m_threads;
    ThreadGuard m_guard;

    void worker() {
        while (!m_done) {
            callee_type task;
            {
                std::unique_lock<std::mutex> lock{m_mutex};
                m_cv.wait(lock, [this] { return m_done || !m_queue.empty(); });
                if (m_done) {
                    return;
                }
                task = std::move(m_queue.front());
                m_queue.pop();
            }
            task();
        }
    }

public:
    ThreadPoolExecutor(unsigned max_workers = 0) : m_done{false}, m_guard{m_threads} {
        const unsigned total = std::thread::hardware_concurrency();
        if (max_workers == 0 || max_workers > total) {
            max_workers = total;
        }
        try {
            for (unsigned i = 0; i < max_workers; ++i) {
                m_threads.emplace_back(&ThreadPoolExecutor::worker, this);
            }
        } catch (...) {
            m_done = true;
            throw;
        }
    }
    ~ThreadPoolExecutor() {
        m_done = true;
        m_cv.notify_all();
    }

    template <typename Function, typename... Args>
    auto submit(Function&& f, Args&&... args) {
        using result_type = std::result_of_t<Function(Args...)>;
        std::packaged_task<result_type()> task{
            std::bind(std::forward<Function>(f), std::forward<Args>(args)...)};
        std::future<result_type> future{task.get_future()};
        /**
         *  Instance of std::packaged_task, whose copy assignment operator is deleted, is move-only.
         */
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            m_queue.push(std::move(task));
        }
        m_cv.notify_one();
        return future;
    }
};

#endif
