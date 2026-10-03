// The fuzzer's main(): runs a range of seeds, each one a reproducible test case in its own process.
// Fuzzing is a large range, reproducing is a range of one.
// Usage: rx_fuzzer <first seed> [count] [processes]

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fuzz_harness.h"

static int reportFd = -1;

// Sends the violation to the parent and ends this test case
void fuzzViolation(const std::string &kind, const std::string &detail)
{
    const std::string msg = kind + "\n" + detail;
    if (write(reportFd, msg.data(), msg.size()) < 0)
        perror("write");
    _exit(1);
}

// The seed alone decides the test case. Keep this stable or recorded seeds stop reproducing.
static std::vector<uint8_t> inputFromSeed(uint64_t seed)
{
    constexpr size_t MIN_INPUT_BYTES = FUZZ_HEADER_BYTES + 1;
    constexpr size_t MAX_INPUT_BYTES = 256;
    constexpr uint8_t OPCODE_DELIVER = 0;
    // How healthy the link is: the share of opcodes that just deliver the packet. The rest are random.
    static const unsigned deliverPercent[] = {0, 50, 80, 95};

    std::mt19937_64 rng(seed);
    const unsigned deliver = deliverPercent[rng() % std::size(deliverPercent)];
    std::vector<uint8_t> in(MIN_INPUT_BYTES + rng() % (MAX_INPUT_BYTES - MIN_INPUT_BYTES + 1));
    for (size_t i = 0; i < FUZZ_HEADER_BYTES; i++)
        in[i] = rng();
    for (size_t i = FUZZ_HEADER_BYTES; i < in.size(); i++)
        in[i] = (rng() % 100 < deliver) ? OPCODE_DELIVER : rng();
    return in;
}

struct Running
{
    uint64_t seed;
    int reportFd;
};

struct Result
{
    std::string kind;
    std::string detail;
};

static std::map<pid_t, Running> running;
static std::map<uint64_t, Result> results;

// Starts one seed's test case in a child process
static void startSeed(uint64_t seed)
{
    const std::vector<uint8_t> in = inputFromSeed(seed);
    trace("seed %llu, input:", (unsigned long long)seed);
    for (auto b : in)
        trace(" %02x", b);
    trace("\n");

    // The firmware keeps its state in globals with no way to reset them, so every seed gets a fresh process
    int fds[2];
    if (pipe(fds) != 0)
    {
        perror("pipe");
        exit(2);
    }
    const pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(2);
    }
    if (pid == 0)
    {
        close(fds[0]);
        reportFd = fds[1];
        fuzzRunInput(in.data(), in.size());
        _exit(0);
    }
    close(fds[1]);
    running[pid] = {seed, fds[0]};
}

// Waits for any child to finish and collects what it reported
static void finishOne()
{
    int status = 0;
    const pid_t pid = waitpid(-1, &status, 0);
    const Running done = running[pid];
    running.erase(pid);

    std::string report;
    char buf[1024];
    for (ssize_t n; (n = read(done.reportFd, buf, sizeof(buf))) > 0;)
        report.append(buf, n);
    close(done.reportFd);

    Result result;
    if (!report.empty())
    {
        const size_t nl = report.find('\n');
        result.kind = report.substr(0, nl);
        result.detail = report.substr(nl + 1);
    }
    else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        result.kind = "crash";
        result.detail = "the firmware crashed, rerun the seed for the sanitizer report";
    }
    results[done.seed] = result;
}

// Runs the seeds, several at a time, and prints the first few failing seeds of each kind and the totals
int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <first seed> [count] [processes]\n", argv[0]);
        return 2;
    }
    const uint64_t first = strtoull(argv[1], nullptr, 0);
    const uint64_t count = argc > 2 ? strtoull(argv[2], nullptr, 0) : 1;
    const size_t jobs = argc > 3 ? strtoull(argv[3], nullptr, 0) : 1;
    constexpr unsigned MAX_REPORTS_PER_KIND = 3;
    fuzzInit();

    std::map<std::string, uint64_t> failures;
    uint64_t nextToStart = first;
    uint64_t nextToReport = first;
    while (nextToReport < first + count)
    {
        while (running.size() < jobs && nextToStart < first + count)
            startSeed(nextToStart++);
        finishOne();

        // Report in seed order, so the output does not depend on which child finished first
        for (auto it = results.find(nextToReport); it != results.end(); it = results.find(nextToReport))
        {
            const Result &result = it->second;
            if (!result.kind.empty() && failures[result.kind]++ < MAX_REPORTS_PER_KIND)
            {
                printf("seed %llu: %s\n", (unsigned long long)nextToReport, result.detail.c_str());
                fflush(stdout);
            }
            results.erase(it);
            nextToReport++;
        }
    }

    std::vector<std::pair<uint64_t, std::string>> totals;
    for (const auto &f : failures)
        totals.push_back({f.second, f.first});
    std::sort(totals.rbegin(), totals.rend());
    for (const auto &t : totals)
        printf("seeds %llu..%llu: %llu x %s\n", (unsigned long long)first, (unsigned long long)(first + count - 1),
            (unsigned long long)t.first, t.second.c_str());
    return failures.empty() ? 0 : 1;
}
