#ifndef _THREAD_SAFE_H
#define _THREAD_SAFE_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>
#include <future>
#include <functional> // std::bind
#include <thread>
#include <vector>

template <typename T>
class FineSafeQueue {
private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };

    std::mutex m_head_mutex;
    std::unique_ptr<node> m_head;
    std::mutex m_tail_mutex;
    node* m_tail;
    std::condition_variable m_data_cond;

    node* get_tail() {
        std::lock_guard<std::mutex> tail_lock(m_tail_mutex);
        return m_tail;
    }

    std::unique_ptr<node> pop_head() {
        if (m_head.get() == get_tail()) {
            return nullptr;
        }
        std::unique_ptr<node> head = std::move(m_head);
        m_head = std::move(head->next);
        return head;
    }

    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock<std::mutex> head_lock(m_head_mutex);
        m_data_cond.wait(head_lock, [&] { return m_head.get() != get_tail(); });
        return head_lock;
    }

    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }

    std::unique_ptr<node> try_pop_head() {
        std::lock_guard<std::mutex> head_lock(m_head_mutex);
        return pop_head();
    }

public:
    FineSafeQueue() : m_head(std::make_unique<node>()), m_tail(m_head.get()) {}
    FineSafeQueue(const FineSafeQueue&) = delete;
    FineSafeQueue& operator=(const FineSafeQueue&) = delete;

    std::shared_ptr<T> wait_and_pop() {
        std::unique_ptr<node> const head = wait_pop_head();
        return head->data;
    }

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> head = try_pop_head();
        return head ? head->data : nullptr;
    }

    bool try_pop(T& value) {
        std::unique_ptr<node> const head = try_pop_head();
        if (head) {
            value = std::move(*(head->data));
            return true;
        }
        return false;
    }

    bool empty() const noexcept {
        std::lock_guard<std::mutex> head_lock(m_head_mutex);
        return (m_head.get() == get_tail());
    }

    void push(T new_value) {
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(std::make_unique<node>());
        {
            std::lock_guard<std::mutex> tail_lock(m_tail_mutex);
            m_tail->data = new_data;
            node* const new_tail = p.get();
            m_tail->next = std::move(p);
            m_tail = new_tail;
        }
        m_data_cond.notify_one();
    }
};

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
    FineSafeQueue<callee_type> m_queue;
    std::vector<std::thread> m_threads;
    ThreadGuard m_guard;

    void worker() {
        while (!m_done) {
            callee_type task;
            if (m_queue.try_pop(task)) {
                task();
            } else {
                std::this_thread::yield();
            }
        }
    }

public:
    ThreadPoolExecutor(unsigned max_workers = 0) : m_done(false), m_guard(m_threads) {
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
    ~ThreadPoolExecutor() { m_done = true; }

    template <typename Function, typename... Args>
    auto submit(Function&& f, Args&&... args) {
        using result_type = std::result_of_t<Function(Args...)>;
        std::packaged_task<result_type()> task(
            std::bind(std::forward<Function>(f), std::forward<Args>(args)...));
        std::future<result_type> future(task.get_future());
        /**
         *  Instance of std::packaged_task, whose copy assignment operator is deleted, is move-only.
         */
        m_queue.push(std::move(task));
        return future;
    }
};

#endif

