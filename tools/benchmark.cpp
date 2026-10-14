#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "gravity.h"
#include "octree.h"
#include "scenes.h"

// settings every measurement shares
static const float eps = 0.01f;
static const unsigned seed = 1;
static const int sizes[] = {100, 300, 1000, 3000, 10000, 30000};
static const float thetas[] = {0.3f, 0.5f, 0.7f, 1.0f};

// one tree measurement at one theta
struct TreeRun {
  float theta;
  double ms, mean_error, worst_error;
};

// everything measured for one scene at one body count
struct Row {
  std::string scene;
  int n;
  double brute_ms, build_ms;
  std::vector<TreeRun> tree;
};

// times one force pass in milliseconds: runs it at least 3 times and for at least 0.3 seconds,
// and keeps the fastest run, since anything slower is the machine being busy with something else
template <typename F>
static double time_ms(F pass) {
  double best = 1e300, total = 0;
  for (int runs = 0; runs < 3 || total < 300.0; ++runs) {
    const auto start = std::chrono::steady_clock::now();
    pass();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    best = std::min(best, ms);
    total += ms;
  }
  return best;
}

// measures one scene at one body count with brute force and with the tree at every theta
static Row measure(const std::string& scene, int n) {
  Bodies exact = scene == "clustered" ? clustered_ball(n, seed) : uniform_ball(n, seed);
  Bodies b = exact;
  Octree tree;

  Row row{scene, n, 0, 0, {}};
  row.brute_ms = time_ms([&] { compute_accel(exact, eps); });
  row.build_ms = time_ms([&] { tree.build(b); });

  for (float theta : thetas) {
    TreeRun run{theta, 0, 0, 0};
    run.ms = time_ms([&] { compute_accel_tree(b, tree, eps, theta); });

    // how far each body's tree acceleration is from brute force, as a fraction of the true one
    for (int i = 0; i < n; ++i) {
      const double dx = b.ax[i] - exact.ax[i], dy = b.ay[i] - exact.ay[i], dz = b.az[i] - exact.az[i];
      const double size = std::sqrt(double(exact.ax[i]) * exact.ax[i] + double(exact.ay[i]) * exact.ay[i] +
                                    double(exact.az[i]) * exact.az[i]);
      const double off = std::sqrt(dx * dx + dy * dy + dz * dz) / size;
      run.mean_error += off / n;
      run.worst_error = std::max(run.worst_error, off);
    }
    row.tree.push_back(run);
  }
  return row;
}

// usage: benchmark [out_dir] [commit] [machine]
int main(int argc, char** argv) {
  const std::string out_dir = argc > 1 ? argv[1] : "results";
  const std::string commit = argc > 2 ? argv[2] : "unknown";
  const std::string machine = argc > 3 ? argv[3] : "unknown";

  // opens both files up front so a bad folder fails before the long run, not after it
  FILE* json = std::fopen((out_dir + "/benchmarks.json").c_str(), "w");
  FILE* md = std::fopen((out_dir + "/benchmarks.md").c_str(), "w");
  if (!json || !md) {
    std::fprintf(stderr, "cannot write to %s\n", out_dir.c_str());
    return 1;
  }

  // runs every measurement, printing a line each so the progress is visible
  std::vector<Row> rows;
  for (const char* scene : {"uniform", "clustered"}) {
    for (int n : sizes) {
      rows.push_back(measure(scene, n));
      const Row& r = rows.back();
      std::printf("%s n=%d brute %.3f ms, tree at theta 0.5 %.3f ms\n", scene, n, r.brute_ms, r.tree[1].ms);
      std::fflush(stdout);
    }
  }

  // the json file: how it was measured, then one entry per scene and body count
  std::fprintf(json, "{\n  \"machine\": \"%s\",\n  \"commit\": \"%s\",\n  \"compiler\": \"%s\",\n", machine.c_str(),
               commit.c_str(), __VERSION__);
  std::fprintf(json, "  \"build_flags\": \"%s\",\n  \"softening\": %g,\n  \"seed\": %u,\n", BENCH_FLAGS, eps, seed);
  std::fprintf(json, "  \"timing\": \"one force pass, single thread, fastest of at least 3 runs and 0.3 s\",\n");
  std::fprintf(json, "  \"error\": \"|a_tree - a_brute| / |a_brute| per body, each body against its own acceleration\",\n  \"runs\": [\n");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const Row& r = rows[i];
    std::fprintf(json, "    {\"scene\": \"%s\", \"n\": %d, \"brute_ms\": %.4f, \"tree_build_ms\": %.4f, \"tree\": [",
                 r.scene.c_str(), r.n, r.brute_ms, r.build_ms);
    for (std::size_t t = 0; t < r.tree.size(); ++t) {
      const TreeRun& run = r.tree[t];
      std::fprintf(json, "%s\n      {\"theta\": %g, \"ms\": %.4f, \"speedup\": %.3f, \"mean_error\": %.3e, \"worst_error\": %.3e}",
                   t ? "," : "", run.theta, run.ms, r.brute_ms / run.ms, run.mean_error, run.worst_error);
    }
    std::fprintf(json, "]}%s\n", i + 1 < rows.size() ? "," : "");
  }
  std::fprintf(json, "  ]\n}\n");

  // the markdown file: the same numbers as tables
  std::fprintf(md, "# Brute force against Barnes-Hut\n\nWritten by `tools/benchmark.cpp`. Same numbers as `benchmarks.json`.\n\n");
  std::fprintf(md, "- Machine: %s\n- Commit: %s\n- Compiler: %s\n- Build flags: %s\n", machine.c_str(), commit.c_str(),
               __VERSION__, BENCH_FLAGS);
  std::fprintf(md, "- Softening: %g, seed: %u, bodies at rest with equal mass in a ball of radius 1\n", eps, seed);
  std::fprintf(md, "- Time: one force pass on a single thread, the fastest of at least 3 runs and 0.3 seconds. Tree times include building the tree\n");
  std::fprintf(md, "- Error: |a_tree - a_brute| / |a_brute| for each body, then the mean and the worst body\n");
  std::fprintf(md, "- The worst body is measured against its own acceleration. Where the pulls nearly cancel that is small, so the worst reads far higher than the mean\n");

  for (const char* scene : {"uniform", "clustered"}) {
    // time per force pass, with the speedup over brute force next to each tree time
    std::fprintf(md, "\n## %s scene: milliseconds per force pass\n\n| N | brute force | tree build only |", scene);
    for (float theta : thetas) std::fprintf(md, " theta %g |", theta);
    std::fprintf(md, "\n| --- | --- | --- |");
    for (std::size_t t = 0; t < sizeof(thetas) / sizeof(thetas[0]); ++t) std::fprintf(md, " --- |");
    for (const Row& r : rows) {
      if (r.scene != scene) continue;
      std::fprintf(md, "\n| %d | %.3f | %.3f |", r.n, r.brute_ms, r.build_ms);
      for (const TreeRun& run : r.tree) std::fprintf(md, " %.3f (%.2fx) |", run.ms, r.brute_ms / run.ms);
    }

    // force error for the same runs
    std::fprintf(md, "\n\n## %s scene: force error, mean (worst body)\n\n| N |", scene);
    for (float theta : thetas) std::fprintf(md, " theta %g |", theta);
    std::fprintf(md, "\n| --- |");
    for (std::size_t t = 0; t < sizeof(thetas) / sizeof(thetas[0]); ++t) std::fprintf(md, " --- |");
    for (const Row& r : rows) {
      if (r.scene != scene) continue;
      std::fprintf(md, "\n| %d |", r.n);
      for (const TreeRun& run : r.tree) std::fprintf(md, " %.2e (%.2e) |", run.mean_error, run.worst_error);
    }
    std::fprintf(md, "\n");
  }

  std::fclose(json);
  std::fclose(md);
  return 0;
}
