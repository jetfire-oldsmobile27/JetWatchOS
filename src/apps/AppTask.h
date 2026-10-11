#pragma once
#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct AppToken {
    std::atomic<bool> alive{true};
};

namespace apptask_detail {

template <typename Work, typename Deliver>
struct Job {
    std::shared_ptr<AppToken> tok;
    Work    work;
    Deliver deliver;
    using R = decltype(std::declval<Work>()());
    R result{};
    std::atomic<bool> delivered{false};
};

template <typename Work, typename Deliver>
void task_entry(void* arg)
{
    using J = Job<Work, Deliver>;

    std::unique_ptr<J> ju(static_cast<J*>(arg));
    J* j = ju.get();                 // ← сначала берём указатель

    struct Finally {
        J* j;
        ~Finally() {
            lv_async_call([](void* p){
                J* j = static_cast<J*>(p);
                if (j->tok->alive && !j->delivered.exchange(true)) {
                    j->deliver(std::move(j->result));
                }
                delete j;
            }, j);
        }
    } fin{ ju.release() };            // ← теперь release отдаёт владение в fin

    try { j->result = j->work(); } catch (...) {}
}

template <typename Work, typename Deliver>
void run(std::shared_ptr<AppToken> tok, Work&& work, Deliver&& deliver)
{
    using J = Job<std::decay_t<Work>, std::decay_t<Deliver>>;
    J* j = new J{ std::move(tok),
                  std::forward<Work>(work),
                  std::forward<Deliver>(deliver) };

    BaseType_t ok = xTaskCreatePinnedToCore(
        task_entry<std::decay_t<Work>, std::decay_t<Deliver>>,
        "apptask", 8 * 1024, j, 5, nullptr, 1);

    if (ok != pdPASS) {
        delete j;
        deliver(work());  // fallback: синхронно
    }
}

template <typename Work>
struct DetachedJob {
    Work work;
};

template <typename Work>
void detached_entry(void* arg)
{
    using J = DetachedJob<Work>;
    std::unique_ptr<J> ju(static_cast<J*>(arg));
    J* j = ju.get();
    try {
        if constexpr (std::is_void_v<decltype(j->work())>) {
            j->work();
        } else {
            (void)j->work();
        }
    } catch (...) {}
    vTaskDelete(nullptr);
}

} // namespace apptask_detail

template <typename Work, typename Deliver>
void apptask_run(std::shared_ptr<AppToken> tok, Work work, Deliver deliver)
{
    apptask_detail::run(std::move(tok), std::move(work), std::move(deliver));
}

template <typename Work>
void apptask_run_detached(Work work)
{
    using W = std::decay_t<Work>;
    using J = apptask_detail::DetachedJob<W>;
    J* j = new J{ std::move(work) };

    BaseType_t ok = xTaskCreatePinnedToCore(
        apptask_detail::detached_entry<W>,
        "apptask", 20 * 1024, j, 5, nullptr, 1);

    if (ok != pdPASS) {
        delete j;
        if constexpr (std::is_void_v<decltype(work())>) {
            work();
        } else {
            (void)work();
        }
    }
}