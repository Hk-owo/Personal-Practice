//
// Created by lacas on 2026/2/20.
//

#ifndef TIME_WHEEL_PRECISION_TEST_H
#define TIME_WHEEL_PRECISION_TEST_H

#include "TimeWheel.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <cmath>

struct PrecisionResult {
    std::string test_name;
    uint expected_ms;
    uint actual_ms;
    int error_ms;
    double error_percent;
    bool success;
};

class TimeWheelPrecisionTest {
private:
    static std::chrono::steady_clock::time_point start_time;
    static std::vector<PrecisionResult> results;

    static std::string now() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
        auto timer = std::chrono::system_clock::to_time_t(now);
        std::tm bt = *std::localtime(&timer);
        char buf[64];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                 bt.tm_hour, bt.tm_min, bt.tm_sec, (int)ms.count());
        return std::string(buf);
    }

    static void record_result(const std::string& name, uint expected, uint actual) {
        int error = (int)actual - (int)expected;
        double error_pct = (std::abs(error) * 100.0) / expected;
        bool success = std::abs(error) <= 10; // 允许10ms误差

        results.push_back({name, expected, actual, error, error_pct, success});

        std::cout << "  Expected: " << std::setw(6) << expected << "ms"
                  << "  Actual: " << std::setw(6) << actual << "ms"
                  << "  Error: " << std::setw(4) << error << "ms"
                  << " (" << std::fixed << std::setprecision(2) << error_pct << "%)"
                  << (success ? " ✓" : " ✗") << std::endl;
    }

public:
    // 测试1: 毫秒级精度 (1-999ms)
    static void test_millisecond_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 1: Millisecond-Level Precision (1-999ms)" << std::endl;
        std::cout << "Start: " << now() << std::endl;

        try {
            TimeWheelTop tw;

            // 测试关键点：1, 10, 50, 100, 500, 999ms
            std::vector<uint> test_ms = {1, 10, 50, 100, 200, 500, 750, 999};

            for (uint ms : test_ms) {
                auto task_start = std::chrono::steady_clock::now();
                std::atomic<bool> done{false};
                uint actual_ms = 0;

                TimeWheelBasics::tv tv{0, 0, 0, ms};
                tw.add_task([&]() {
                    actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    done = true;
                }, tv);

                // 等待任务完成
                while (!done) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }

                record_result(std::to_string(ms) + "ms", ms, actual_ms);
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 测试2: 秒级精度 (1-59s)
    static void test_second_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 2: Second-Level Precision (1-59s)" << std::endl;
        std::cout << "Start: " << now() << std::endl;

        try {
            TimeWheelTop tw;

            // 测试关键点：1, 5, 10, 30, 59秒
            std::vector<uint> test_sec = {1, 5, 10, 30, 59};

            for (uint sec : test_sec) {
                auto task_start = std::chrono::steady_clock::now();
                std::atomic<bool> done{false};
                uint actual_ms = 0;

                TimeWheelBasics::tv tv{0, 0, sec, 0};
                tw.add_task([&]() {
                    actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    done = true;
                }, tv);

                // 等待任务完成（稍微多等一点）
                while (!done) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }

                record_result(std::to_string(sec) + "s", sec * 1000, actual_ms);
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 测试3: 分钟级精度 (1-59min)
    static void test_minute_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 3: Minute-Level Precision (1-59min)" << std::endl;
        std::cout << "Start: " << now() << std::endl;
        std::cout << "Note: This test takes several minutes..." << std::endl;

        try {
            TimeWheelTop tw;

            // 只测试1分钟和2分钟（缩短测试时间）
            std::vector<uint> test_min = {1, 2};

            for (uint min : test_min) {
                auto task_start = std::chrono::steady_clock::now();
                std::atomic<bool> done{false};
                uint actual_ms = 0;

                TimeWheelBasics::tv tv{0, min, 0, 0};
                tw.add_task([&]() {
                    actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    done = true;
                }, tv);

                std::cout << "  Waiting " << min << " minute(s)..." << std::endl;

                while (!done) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    if (elapsed % 30 == 0) {
                        std::cout << "    ... " << elapsed << "s passed" << std::endl;
                    }
                }

                record_result(std::to_string(min) + "min", min * 60 * 1000, actual_ms);
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 测试4: 跨级联精度 (ms + s + min组合)
    static void test_cascade_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 4: Cascade Precision (Mixed Units)" << std::endl;
        std::cout << "Start: " << now() << std::endl;

        try {
            TimeWheelTop tw;

            struct TestCase {
                std::string name;
                TimeWheelBasics::tv tv;
                uint expected_ms;
            };

            std::vector<TestCase> tests = {
                    {"1s500ms",      {0, 0, 1, 500},   1500},
                    {"5s250ms",      {0, 0, 5, 250},   5250},
                    {"1min1s",       {0, 1, 1, 0},     61000},
                    {"2min30s500ms", {0, 2, 30, 500},  150500},
            };

            for (const auto& test : tests) {
                auto task_start = std::chrono::steady_clock::now();
                std::atomic<bool> done{false};
                uint actual_ms = 0;

                tw.add_task([&]() {
                    actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    done = true;
                }, test.tv);

                std::cout << "  Testing " << test.name << "..." << std::endl;

                while (!done) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                record_result(test.name, test.expected_ms, actual_ms);
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 测试5: 边界值精度
    static void test_boundary_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 5: Boundary Value Precision" << std::endl;
        std::cout << "Start: " << now() << std::endl;

        try {
            TimeWheelTop tw;

            struct TestCase {
                std::string name;
                TimeWheelBasics::tv tv;
                uint expected_ms;
            };

            std::vector<TestCase> tests = {
                    // 零值
                    {"0ms",          {0, 0, 0, 0},     0},
                    // 毫秒边界
                    {"999ms",        {0, 0, 0, 999},   999},
                    // 秒边界
                    {"1s",           {0, 0, 1, 0},     1000},
                    {"59s",          {0, 0, 59, 0},    59000},
                    // 分钟边界
                    {"1min",         {0, 1, 0, 0},     60000},
                    {"59min",        {0, 59, 0, 0},    3540000},
                    // 小时边界（可选，测试时间太长）
                    // {"1h",           {1, 0, 0, 0},     3600000},
            };

            for (const auto& test : tests) {
                // 跳过长时间测试（59分钟）
                if (test.expected_ms > 120000) { // > 2分钟
                    std::cout << "  Skipping " << test.name << " (too long)" << std::endl;
                    continue;
                }

                auto task_start = std::chrono::steady_clock::now();
                std::atomic<bool> done{false};
                uint actual_ms = 0;

                tw.add_task([&]() {
                    actual_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - task_start).count();
                    done = true;
                }, test.tv);

                std::cout << "  Testing " << test.name << "..." << std::endl;

                // 零值特殊处理
                if (test.expected_ms == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } else {
                    while (!done) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }

                record_result(test.name, test.expected_ms, actual_ms);
            }

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 测试6: 高负载下的精度
    static void test_high_load_precision() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Test 6: High Load Precision" << std::endl;
        std::cout << "Start: " << now() << std::endl;

        try {
            TimeWheelTop tw;
            const int task_count = 50;

            std::vector<uint> expected_ms;
            std::vector<uint> actual_ms(task_count);
            std::vector<std::atomic<bool>> done_flags(task_count);

            auto batch_start = std::chrono::steady_clock::now();

            // 批量添加任务，间隔100ms
            for (int i = 0; i < task_count; ++i) {
                uint delay = 500 + i * 100; // 500, 600, 700...ms
                expected_ms.push_back(delay);

                TimeWheelBasics::tv tv{0, 0, 0, delay};
                tw.add_task([&, i]() {
                    actual_ms[i] = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - batch_start).count();
                    done_flags[i] = true;
                }, tv);
            }

            std::cout << "  Added " << task_count << " tasks (500ms ~ "
                      << (500 + (task_count-1)*100) << "ms)" << std::endl;

            // 等待所有任务完成
            bool all_done = false;
            while (!all_done) {
                all_done = true;
                for (int i = 0; i < task_count; ++i) {
                    if (!done_flags[i]) {
                        all_done = false;
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 统计结果
            int max_error = 0;
            int total_error = 0;
            int success_count = 0;

            for (int i = 0; i < task_count; ++i) {
                int error = (int)actual_ms[i] - (int)expected_ms[i];
                total_error += std::abs(error);
                max_error = std::max(max_error, std::abs(error));
                if (std::abs(error) <= 10) success_count++;
            }

            double avg_error = (double)total_error / task_count;

            std::cout << "  Results:" << std::endl;
            std::cout << "    Success rate: " << success_count << "/" << task_count
                      << " (" << (success_count * 100 / task_count) << "%)" << std::endl;
            std::cout << "    Average error: " << avg_error << "ms" << std::endl;
            std::cout << "    Max error: " << max_error << "ms" << std::endl;
            std::cout << "    " << (success_count == task_count ? "✓" : "✗") << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }

    // 打印汇总报告
    static void print_summary() {
        std::cout << "\n########################################" << std::endl;
        std::cout << "#         PRECISION TEST REPORT        #" << std::endl;
        std::cout << "########################################" << std::endl;

        int total = results.size();
        int passed = 0;
        double total_error = 0;
        int max_error = 0;

        for (const auto& r : results) {
            if (r.success) passed++;
            total_error += std::abs(r.error_ms);
            max_error = std::max(max_error, std::abs(r.error_ms));
        }

        std::cout << "\nTotal Tests: " << total << std::endl;
        std::cout << "Passed: " << passed << " (" << (passed * 100.0 / total) << "%)" << std::endl;
        std::cout << "Failed: " << (total - passed) << std::endl;
        std::cout << "Average Error: " << (total_error / total) << "ms" << std::endl;
        std::cout << "Max Error: " << max_error << "ms" << std::endl;

        std::cout << "\nDetailed Results:" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        std::cout << std::left << std::setw(20) << "Test Name"
                  << std::setw(12) << "Expected"
                  << std::setw(12) << "Actual"
                  << std::setw(10) << "Error"
                  << std::setw(10) << "Error%"
                  << "Status" << std::endl;
        std::cout << std::string(80, '-') << std::endl;

        for (const auto& r : results) {
            std::cout << std::left << std::setw(20) << r.test_name
                      << std::setw(12) << r.expected_ms
                      << std::setw(12) << r.actual_ms
                      << std::setw(10) << r.error_ms
                      << std::setw(10) << std::fixed << std::setprecision(2) << r.error_percent
                      << (r.success ? "PASS" : "FAIL") << std::endl;
        }

        std::cout << std::string(80, '-') << std::endl;
        std::cout << "\n" << (passed == total ? "✓ ALL TESTS PASSED!" : "✗ SOME TESTS FAILED!") << std::endl;
    }

    // 运行所有精度测试
    static void run_all_tests() {
        results.clear();

        std::cout << "########################################" << std::endl;
        std::cout << "#                                      #" << std::endl;
        std::cout << "#   TimeWheel Precision Test Suite     #" << std::endl;
        std::cout << "#                                      #" << std::endl;
        std::cout << "########################################" << std::endl;

        test_millisecond_precision();   // 毫秒级
        test_second_precision();        // 秒级
        // test_minute_precision();     // 分钟级（取消注释以运行长时间测试）
        test_cascade_precision();       // 跨级联
        test_boundary_precision();      // 边界值
        test_high_load_precision();     // 高负载

        print_summary();
    }

    // 快速模式（跳过长时间测试）
    static void run_quick_tests() {
        results.clear();

        std::cout << "########################################" << std::endl;
        std::cout << "#    Quick Precision Test (No Wait)    #" << std::endl;
        std::cout << "########################################" << std::endl;

        test_millisecond_precision();
        test_cascade_precision();
        test_boundary_precision();

        print_summary();
    }
};

// 静态成员定义
std::chrono::steady_clock::time_point TimeWheelPrecisionTest::start_time;
std::vector<PrecisionResult> TimeWheelPrecisionTest::results;

// 主函数
int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--quick") {
        TimeWheelPrecisionTest::run_quick_tests();
    } else {
        TimeWheelPrecisionTest::run_all_tests();
    }
    return 0;
}

#endif //TIME_WHEEL_PRECISION_TEST_H
