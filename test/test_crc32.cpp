/*
 * Orbital Temple - CRC32 Unit Tests
 *
 * Tests the CRC32 implementation used for EEPROM data integrity.
 *
 * Compile: g++ -std=c++11 -o test_crc32 test_crc32.cpp
 * Run: ./test_crc32
 */

#include <iostream>
#include <cstdint>
#include <cstring>
#include <cassert>

// ==================== CRC32 IMPLEMENTATION (from radiation.cpp) ====================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBBBD6, 0xACBCCB40,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFAD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t calculateCRC32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; i++) {
        uint8_t index = (crc ^ data[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }

    return crc ^ 0xFFFFFFFF;
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

#define ASSERT_EQ(expected, actual) do { \
    if ((expected) != (actual)) { \
        throw std::runtime_error("Expected " + std::to_string(expected) + " but got " + std::to_string(actual)); \
    } \
} while(0)

#define ASSERT_NEQ(expected, actual) do { \
    if ((expected) == (actual)) { \
        throw std::runtime_error("Expected values to be different"); \
    } \
} while(0)

// ==================== TESTS ====================

// Test: Empty data should produce known CRC
TEST(empty_data) {
    uint8_t data[1] = {0};
    // CRC of empty data (length 0) should be 0x00000000
    uint32_t crc = calculateCRC32(data, 0);
    ASSERT_EQ(0x00000000u, crc);
}

// Test: Single byte 0x00
TEST(single_byte_zero) {
    uint8_t data[1] = {0x00};
    uint32_t crc = calculateCRC32(data, 1);
    // Known CRC32 for single byte 0x00
    ASSERT_EQ(0xD202EF8Du, crc);
}

// Test: Known string "123456789" (standard CRC32 test vector)
TEST(standard_test_vector) {
    const char* str = "123456789";
    uint32_t crc = calculateCRC32((const uint8_t*)str, 9);
    // Standard CRC32 check value for "123456789"
    ASSERT_EQ(0xCBF43926u, crc);
}

// Test: Known string "hello"
TEST(hello_string) {
    const char* str = "hello";
    uint32_t crc = calculateCRC32((const uint8_t*)str, 5);
    // Known CRC32 for "hello"
    ASSERT_EQ(0x3610A686u, crc);
}

// Test: Same data produces same CRC (deterministic)
TEST(deterministic) {
    uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    uint32_t crc1 = calculateCRC32(data, 10);
    uint32_t crc2 = calculateCRC32(data, 10);
    ASSERT_EQ(crc1, crc2);
}

// Test: Different data produces different CRC
TEST(different_data_different_crc) {
    uint8_t data1[4] = {0, 0, 0, 0};
    uint8_t data2[4] = {0, 0, 0, 1};
    uint32_t crc1 = calculateCRC32(data1, 4);
    uint32_t crc2 = calculateCRC32(data2, 4);
    ASSERT_NEQ(crc1, crc2);
}

// Test: Single bit flip changes CRC
TEST(single_bit_flip_detected) {
    uint8_t data[10] = {0xAB, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint32_t crc1 = calculateCRC32(data, 10);

    // Flip one bit
    data[5] = 0x01;
    uint32_t crc2 = calculateCRC32(data, 10);

    ASSERT_NEQ(crc1, crc2);
}

// Test: All zeros
TEST(all_zeros) {
    uint8_t data[100];
    memset(data, 0x00, 100);
    uint32_t crc = calculateCRC32(data, 100);
    // Should produce a valid non-zero CRC
    ASSERT_NEQ(0x00000000u, crc);
}

// Test: All ones (0xFF)
TEST(all_ones) {
    uint8_t data[100];
    memset(data, 0xFF, 100);
    uint32_t crc = calculateCRC32(data, 100);
    // Should produce a valid non-zero CRC
    ASSERT_NEQ(0x00000000u, crc);
}

// Test: Simulated EEPROM state block (like real usage)
TEST(eeprom_state_block) {
    // Simulate EEPROM state: magic(1) + state(1) + bootcount(4) + deploy(1) + mission_start(4)
    uint8_t eeprom[100];
    memset(eeprom, 0x00, 100);

    eeprom[0] = 0xAB;           // Magic byte
    eeprom[1] = 0x03;           // State = OPERATIONAL
    eeprom[2] = 0x05;           // Boot count = 5 (little endian)
    eeprom[3] = 0x00;
    eeprom[4] = 0x00;
    eeprom[5] = 0x00;
    eeprom[6] = 0x01;           // Antenna deployed = true
    eeprom[7] = 0x00;           // Mission start time = 0
    eeprom[8] = 0x00;
    eeprom[9] = 0x00;
    eeprom[10] = 0x00;

    uint32_t crc = calculateCRC32(eeprom, 100);

    // CRC should be non-zero
    ASSERT_NEQ(0x00000000u, crc);

    // Verify it's reproducible
    uint32_t crc2 = calculateCRC32(eeprom, 100);
    ASSERT_EQ(crc, crc2);
}

// Test: Corruption in magic byte detected
TEST(magic_byte_corruption_detected) {
    uint8_t eeprom[100];
    memset(eeprom, 0x00, 100);
    eeprom[0] = 0xAB;

    uint32_t crc_original = calculateCRC32(eeprom, 100);

    // Corrupt magic byte
    eeprom[0] = 0xAC;
    uint32_t crc_corrupted = calculateCRC32(eeprom, 100);

    ASSERT_NEQ(crc_original, crc_corrupted);
}

// Test: Corruption in boot count detected
TEST(bootcount_corruption_detected) {
    uint8_t eeprom[100];
    memset(eeprom, 0x00, 100);
    eeprom[0] = 0xAB;
    eeprom[2] = 0x05;  // Boot count = 5

    uint32_t crc_original = calculateCRC32(eeprom, 100);

    // Corrupt boot count (simulating radiation bit flip)
    eeprom[2] = 0x07;  // Boot count = 7 (bit flip)
    uint32_t crc_corrupted = calculateCRC32(eeprom, 100);

    ASSERT_NEQ(crc_original, crc_corrupted);
}

// Test: Large block (like full EEPROM)
TEST(large_block) {
    uint8_t data[512];
    for (int i = 0; i < 512; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
    uint32_t crc = calculateCRC32(data, 512);

    // Should produce consistent result
    uint32_t crc2 = calculateCRC32(data, 512);
    ASSERT_EQ(crc, crc2);
}

// Test: Partial block CRC differs from full block
TEST(partial_vs_full_block) {
    uint8_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)i;
    }

    uint32_t crc_50 = calculateCRC32(data, 50);
    uint32_t crc_100 = calculateCRC32(data, 100);

    ASSERT_NEQ(crc_50, crc_100);
}

// ==================== MAIN ====================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ORBITAL TEMPLE CRC32 UNIT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    RUN_TEST(empty_data);
    RUN_TEST(single_byte_zero);
    RUN_TEST(standard_test_vector);
    RUN_TEST(hello_string);
    RUN_TEST(deterministic);
    RUN_TEST(different_data_different_crc);
    RUN_TEST(single_bit_flip_detected);
    RUN_TEST(all_zeros);
    RUN_TEST(all_ones);
    RUN_TEST(eeprom_state_block);
    RUN_TEST(magic_byte_corruption_detected);
    RUN_TEST(bootcount_corruption_detected);
    RUN_TEST(large_block);
    RUN_TEST(partial_vs_full_block);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  RESULTS: " << tests_passed << " passed, " << tests_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
