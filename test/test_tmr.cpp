/*
 * Orbital Temple - TMR (Triple Modular Redundancy) Unit Tests
 *
 * Tests the radiation protection voting logic.
 *
 * Compile: g++ -std=c++11 -o test_tmr test_tmr.cpp
 * Run: ./test_tmr
 */

#include <iostream>
#include <cstdint>
#include <cassert>

// ==================== TMR IMPLEMENTATION (from radiation.h) ====================

template<typename T>
struct TMR {
    T copy1;
    T copy2;
    T copy3;
};

template<typename T>
void tmrWrite(TMR<T>& tmr, T value) {
    tmr.copy1 = value;
    tmr.copy2 = value;
    tmr.copy3 = value;
}

// Modified for testing - doesn't restart, returns sentinel value
template<typename T>
T tmrRead(TMR<T>& tmr, bool& catastrophic) {
    catastrophic = false;

    // 2-out-of-3 voting
    if (tmr.copy1 == tmr.copy2) return tmr.copy1;
    if (tmr.copy1 == tmr.copy3) return tmr.copy1;
    if (tmr.copy2 == tmr.copy3) return tmr.copy2;

    // All three different - CATASTROPHIC FAILURE
    catastrophic = true;
    return tmr.copy1;  // Would restart in real code
}

template<typename T>
bool tmrScrub(TMR<T>& tmr) {
    bool catastrophic;
    T correct = tmrRead(tmr, catastrophic);

    if (catastrophic) {
        // In real code, this would restart
        return true;  // Indicate failure
    }

    bool corrected = false;

    if (tmr.copy1 != correct) {
        tmr.copy1 = correct;
        corrected = true;
    }
    if (tmr.copy2 != correct) {
        tmr.copy2 = correct;
        corrected = true;
    }
    if (tmr.copy3 != correct) {
        tmr.copy3 = correct;
        corrected = true;
    }

    return corrected;
}

// ==================== TEST FRAMEWORK ====================

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Test: " << #name << "... "; \
    try { \
        test_##name(); \
        std::cout << "PASS" << std::endl; \
        tests_passed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAIL: " << e.what() << std::endl; \
        tests_failed++; \
    } \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        throw std::runtime_error("Assertion failed: " #condition); \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        throw std::runtime_error("Assertion failed: " #a " != " #b); \
    } \
} while(0)

// ==================== TESTS ====================

TEST(write_sets_all_copies) {
    TMR<uint8_t> tmr;
    tmrWrite(tmr, (uint8_t)42);

    ASSERT_EQ(tmr.copy1, 42);
    ASSERT_EQ(tmr.copy2, 42);
    ASSERT_EQ(tmr.copy3, 42);
}

TEST(read_all_same) {
    TMR<uint8_t> tmr = {10, 10, 10};
    bool catastrophic;

    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 10);
    ASSERT(!catastrophic);
}

TEST(read_copy1_corrupted) {
    // SEU flipped copy1
    TMR<uint8_t> tmr = {99, 10, 10};  // copy1 is wrong
    bool catastrophic;

    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 10);  // Majority wins
    ASSERT(!catastrophic);
}

TEST(read_copy2_corrupted) {
    // SEU flipped copy2
    TMR<uint8_t> tmr = {10, 99, 10};  // copy2 is wrong
    bool catastrophic;

    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 10);  // Majority wins
    ASSERT(!catastrophic);
}

TEST(read_copy3_corrupted) {
    // SEU flipped copy3
    TMR<uint8_t> tmr = {10, 10, 99};  // copy3 is wrong
    bool catastrophic;

    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 10);  // Majority wins
    ASSERT(!catastrophic);
}

TEST(read_all_different_catastrophic) {
    // Multi-bit upset - all copies different
    TMR<uint8_t> tmr = {1, 2, 3};
    bool catastrophic;

    tmrRead(tmr, catastrophic);

    ASSERT(catastrophic);  // Should trigger restart in real code
}

TEST(scrub_fixes_copy1) {
    TMR<uint8_t> tmr = {99, 10, 10};

    bool corrected = tmrScrub(tmr);

    ASSERT(corrected);
    ASSERT_EQ(tmr.copy1, 10);
    ASSERT_EQ(tmr.copy2, 10);
    ASSERT_EQ(tmr.copy3, 10);
}

TEST(scrub_fixes_copy2) {
    TMR<uint8_t> tmr = {10, 99, 10};

    bool corrected = tmrScrub(tmr);

    ASSERT(corrected);
    ASSERT_EQ(tmr.copy1, 10);
    ASSERT_EQ(tmr.copy2, 10);
    ASSERT_EQ(tmr.copy3, 10);
}

TEST(scrub_fixes_copy3) {
    TMR<uint8_t> tmr = {10, 10, 99};

    bool corrected = tmrScrub(tmr);

    ASSERT(corrected);
    ASSERT_EQ(tmr.copy1, 10);
    ASSERT_EQ(tmr.copy2, 10);
    ASSERT_EQ(tmr.copy3, 10);
}

TEST(scrub_no_correction_needed) {
    TMR<uint8_t> tmr = {10, 10, 10};

    bool corrected = tmrScrub(tmr);

    ASSERT(!corrected);
    ASSERT_EQ(tmr.copy1, 10);
    ASSERT_EQ(tmr.copy2, 10);
    ASSERT_EQ(tmr.copy3, 10);
}

TEST(tmr_with_bool) {
    TMR<bool> tmr;
    tmrWrite(tmr, true);

    ASSERT_EQ(tmr.copy1, true);
    ASSERT_EQ(tmr.copy2, true);
    ASSERT_EQ(tmr.copy3, true);

    // Corrupt one copy
    tmr.copy2 = false;

    bool catastrophic;
    bool result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, true);  // Majority wins
    ASSERT(!catastrophic);
}

TEST(tmr_with_uint32) {
    TMR<uint32_t> tmr;
    tmrWrite(tmr, (uint32_t)0xDEADBEEF);

    ASSERT_EQ(tmr.copy1, 0xDEADBEEF);
    ASSERT_EQ(tmr.copy2, 0xDEADBEEF);
    ASSERT_EQ(tmr.copy3, 0xDEADBEEF);

    // Corrupt with single bit flip
    tmr.copy1 = 0xDEADBEEE;  // Last bit flipped

    bool catastrophic;
    uint32_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 0xDEADBEEF);  // Majority wins
    ASSERT(!catastrophic);
}

TEST(tmr_edge_zero) {
    TMR<uint8_t> tmr;
    tmrWrite(tmr, (uint8_t)0);

    bool catastrophic;
    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 0);
    ASSERT(!catastrophic);
}

TEST(tmr_edge_max) {
    TMR<uint8_t> tmr;
    tmrWrite(tmr, (uint8_t)255);

    bool catastrophic;
    uint8_t result = tmrRead(tmr, catastrophic);

    ASSERT_EQ(result, 255);
    ASSERT(!catastrophic);
}

TEST(repeated_scrub_stable) {
    TMR<uint8_t> tmr = {10, 10, 99};

    // First scrub fixes it
    tmrScrub(tmr);
    ASSERT_EQ(tmr.copy3, 10);

    // Second scrub should find nothing to fix
    bool corrected = tmrScrub(tmr);
    ASSERT(!corrected);
}

// ==================== MAIN ====================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ORBITAL TEMPLE TMR UNIT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    RUN_TEST(write_sets_all_copies);
    RUN_TEST(read_all_same);
    RUN_TEST(read_copy1_corrupted);
    RUN_TEST(read_copy2_corrupted);
    RUN_TEST(read_copy3_corrupted);
    RUN_TEST(read_all_different_catastrophic);
    RUN_TEST(scrub_fixes_copy1);
    RUN_TEST(scrub_fixes_copy2);
    RUN_TEST(scrub_fixes_copy3);
    RUN_TEST(scrub_no_correction_needed);
    RUN_TEST(tmr_with_bool);
    RUN_TEST(tmr_with_uint32);
    RUN_TEST(tmr_edge_zero);
    RUN_TEST(tmr_edge_max);
    RUN_TEST(repeated_scrub_stable);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  RESULTS: " << tests_passed << " passed, "
              << tests_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
