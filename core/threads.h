#pragma once

#include <functional>

// how many threads share the force pass, 1 keeps everything on the calling thread
// the helper threads start here once and then wait, so nothing gets created in the middle of a step
// in the browser a stopped helper's worker only goes back to the pool once the page gets a turn,
// so calling this twice in a row without letting the page breathe can run out of workers
void set_threads(int k);

// runs the same job on the caller and every helper at once, returns when they have all finished
void run_on_threads(const std::function<void()>& job);
