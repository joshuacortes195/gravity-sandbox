#include "threads.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {

// the helper threads and everything they share with the caller
struct Pool {
  std::vector<std::thread> helpers;
  std::mutex lock;
  std::condition_variable wake;
  const std::function<void()>* job = nullptr;
  int round = 0;              // goes up by one each time there is new work
  bool quit = false;          // tells the helpers to stop
  std::atomic<int> busy{0};   // helpers still working on this round

  // what a helper does its whole life: sleep until a new round, do the job, report back
  void helper_loop(int seen) {
    for (;;) {
      const std::function<void()>* todo;
      {
        std::unique_lock<std::mutex> hold(lock);
        wake.wait(hold, [&] { return quit || round != seen; });
        if (quit) return;
        seen = round;
        todo = job;
      }
      (*todo)();
      busy.fetch_sub(1);
    }
  }

  // wakes every helper with the quit flag and waits for them to exit
  void stop() {
    {
      std::lock_guard<std::mutex> hold(lock);
      quit = true;
    }
    wake.notify_all();
    for (std::thread& t : helpers) t.join();
    helpers.clear();
    quit = false;
  }

  // stops the helpers when the program ends, a running std::thread would crash it
  ~Pool() { stop(); }
};

Pool pool;

}  // namespace

void set_threads(int k) {
  pool.stop();
  // the caller is one of the k, so it starts k - 1 helpers
  for (int i = 1; i < k; ++i) pool.helpers.emplace_back(&Pool::helper_loop, &pool, pool.round);
}

void run_on_threads(const std::function<void()>& job) {
  // no helpers, just do it here
  if (pool.helpers.empty()) {
    job();
    return;
  }

  // hands the job to the helpers and wakes them up
  {
    std::lock_guard<std::mutex> hold(pool.lock);
    pool.job = &job;
    pool.busy.store(int(pool.helpers.size()));
    ++pool.round;
  }
  pool.wake.notify_all();

  // the caller does its share too
  job();

  // the browser does not let the page's main thread sleep, so it spins until every helper is done
  // natively it hands its core back between checks, in case there are more threads than cores
  while (pool.busy.load() > 0) {
#ifndef __EMSCRIPTEN__
    std::this_thread::yield();
#endif
  }
}
