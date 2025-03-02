#include <futures/launch.hpp>
//
#include <catch2/catch.hpp>
#include <array>
#include <string>

TEST_CASE("Launch") {
    using namespace futures;

    STATIC_REQUIRE(!detail::is_copy_constructible_v<cfuture<void>>);
    STATIC_REQUIRE(detail::is_copy_constructible_v<shared_cfuture<void>>);

    auto test_launch_function = [](std::string name, auto async_fn) {
        SECTION(name.data()) {
            SECTION("Default executor") {
                SECTION("No return") {
                    SECTION("No args") {
                        int i = 0;
                        auto r = async_fn([&i]() { ++i; });
                        r.wait();
                        REQUIRE(i == 1);
                    }

                    SECTION("With args") {
                        int i = 0;
                        auto r = async_fn([&i](int x) { i = 2 * x; }, 3);
                        r.wait();
                        REQUIRE(i == 6);
                    }
                }
                SECTION("With return") {
                    SECTION("No args") {
                        auto r = async_fn([]() { return 2; });
                        REQUIRE(r.get() == 2);
                    }

                    SECTION("With args") {
                        auto r = async_fn([](int x) { return 2 * x; }, 3);
                        REQUIRE(r.get() == 6);
                    }
                }
                SECTION("Non-trivial return") {
                    SECTION("No args") {
                        auto r = async_fn([]() {
                            return std::string("Hello");
                        });
                        REQUIRE(r.get() == "Hello");
                    }

                    SECTION("With args") {
                        auto r = async_fn(
                            [](int c) { return std::string(5, c); },
                            '_');
                        REQUIRE(r.get() == "_____");
                    }
                }
            }

            SECTION("Custom executor") {
                futures::asio::thread_pool pool(2);
                futures::asio::thread_pool::executor_type ex = pool.executor();

                SECTION("No return") {
                    SECTION("No args") {
                        int i = 0;
                        auto r = async_fn(ex, [&]() { ++i; });
                        r.wait();
                        REQUIRE(i == 1);
                    }

                    SECTION("With args") {
                        int i = 0;
                        auto r = async_fn(
                            ex,
                            [&](int x) { i = 2 * x; },
                            3);
                        r.wait();
                        REQUIRE(i == 6);
                    }
                }
                SECTION("With return") {
                    SECTION("No args") {
                        auto r = async_fn(ex, []() { return 2; });
                        REQUIRE(r.get() == 2);
                    }

                    SECTION("With args") {
                        auto r = async_fn(
                            ex,
                            [](int x) { return 2 * x; },
                            3);
                        REQUIRE(r.get() == 6);
                    }
                }
            }
        }
    };

    using deferred_options
        = future_options<continuable_opt, always_deferred_opt>;
    STATIC_REQUIRE(
        detail::is_same_v<
            deferred_options,
            detail::remove_future_option_t<shared_opt, deferred_options>>);

    test_launch_function("Async", [](auto &&...args) {
        return async(std::forward<decltype(args)>(args)...);
    });

    test_launch_function("Schedule", [](auto &&...args) {
        return schedule(std::forward<decltype(args)>(args)...);
    });
}
