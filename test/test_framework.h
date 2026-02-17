/**
 * Minimal test framework for mcpd host-side tests.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <cstdio>
#include <cstring>
#include <string>

static int _tests_run = 0;
static int _tests_passed = 0;
static int _tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct _test_reg_##name { \
        _test_reg_##name() { \
            _tests_run++; \
            printf("  TEST %-50s ", #name); \
            try { \
                test_##name(); \
                _tests_passed++; \
                printf("✅ PASS\n"); \
            } catch (const char* msg) { \
                _tests_failed++; \
                printf("❌ FAIL: %s\n", msg); \
            } catch (...) { \
                _tests_failed++; \
                printf("❌ FAIL: unknown exception\n"); \
            } \
        } \
    } _test_instance_##name; \
    static void test_##name()

#define ASSERT(cond) \
    do { if (!(cond)) throw "Assertion failed: " #cond; } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) throw "Expected equal: " #a " == " #b; } while(0)

#define ASSERT_NE(a, b) \
    do { if ((a) == (b)) throw "Expected not equal: " #a " != " #b; } while(0)

#define ASSERT_STR_CONTAINS(haystack, needle) \
    do { \
        std::string _h(haystack); \
        if (_h.find(needle) == std::string::npos) \
            throw "String does not contain: " #needle; \
    } while(0)

#define TEST_SUMMARY() \
    do { \
        printf("\n  ────────────────────────────────────────\n"); \
        printf("  Results: %d/%d passed", _tests_passed, _tests_run); \
        if (_tests_failed > 0) printf(", %d FAILED", _tests_failed); \
        printf("\n\n"); \
    } while(0)

#endif // TEST_FRAMEWORK_H
