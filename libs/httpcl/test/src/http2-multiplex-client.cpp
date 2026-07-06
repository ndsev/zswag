#include "httpcl/http-client.hpp"

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <base-url> <request-count>\n";
        return 2;
    }

    const std::string baseUrl = argv[1];
    const auto requestCount = std::stoi(argv[2]);
    if (requestCount <= 0) {
        std::cerr << "request-count must be positive\n";
        return 2;
    }

    httpcl::CurlHttpClient client;
    std::atomic<int> successfulRequests{0};
    std::vector<std::string> errors(static_cast<size_t>(requestCount));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(requestCount));

    std::mutex startMutex;
    std::condition_variable startCv;
    bool started = false;

    for (int i = 0; i < requestCount; ++i) {
        workers.emplace_back([&, i] {
            {
                std::unique_lock lock(startMutex);
                startCv.wait(lock, [&] { return started; });
            }

            httpcl::Config config;
            auto url = baseUrl + "?id=" + std::to_string(i);
            auto result = client.get(url, config);
            if (result.status == 200 && result.content == "ok") {
                ++successfulRequests;
            } else {
                errors[static_cast<size_t>(i)] =
                    "request " + std::to_string(i) + " returned status="
                    + std::to_string(result.status) + " body='" + result.content + "'";
            }
        });
    }

    {
        std::lock_guard lock(startMutex);
        started = true;
    }
    startCv.notify_all();

    for (auto& worker : workers)
        worker.join();

    if (successfulRequests != requestCount) {
        std::cerr << "Only " << successfulRequests << " / " << requestCount
                  << " requests succeeded.\n";
        for (auto const& error : errors) {
            if (!error.empty())
                std::cerr << error << '\n';
        }
        return 1;
    }

    std::cout << "completed " << successfulRequests << " requests\n";
    return 0;
}
