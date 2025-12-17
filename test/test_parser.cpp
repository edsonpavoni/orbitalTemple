/*
 * Orbital Temple - Parser Fuzz Testing
 *
 * This file tests the message validation logic with malformed inputs.
 * Run on PC to find crashes without needing hardware.
 *
 * Compile: g++ -std=c++11 -o test_parser test_parser.cpp
 * Run: ./test_parser
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// Mock Arduino String class for PC testing
class String {
public:
    std::string data;

    String() {}
    String(const char* s) : data(s) {}
    String(const std::string& s) : data(s) {}

    int indexOf(char c) const {
        size_t pos = data.find(c);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    int indexOf(const char* s) const {
        size_t pos = data.find(s);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }

    String substring(int start, int end) const {
        if (start < 0) start = 0;
        if (end > (int)data.length()) end = data.length();
        if (start >= end) return String("");
        return String(data.substr(start, end - start));
    }

    String substring(int start) const {
        return substring(start, data.length());
    }

    size_t length() const { return data.length(); }

    bool equals(const String& other) const {
        return data == other.data;
    }

    char charAt(unsigned int i) const {
        if (i >= data.length()) return 0;
        return data[i];
    }

    const char* c_str() const { return data.c_str(); }
};

// Test satellite ID
String sat_id = "SAT001";

// Mock functions
bool verifyHMAC(const String& message, const String& hmac) {
    // For testing, accept any 16-char hex string
    return hmac.length() == 16;
}

void sendMessage(const String& msg) {
    std::cout << "  -> Response: " << msg.data << std::endl;
}

// ==================== ACTUAL PARSER CODE (from loop.cpp) ====================

bool validateMessage(const String& msg, String& satId, String& command,
                     String& path, String& data, String& hmac) {
    // Message format: SAT_ID-COMMAND&PATH@DATA#HMAC
    // Minimum valid message: "X-Y&@#Z" = 7 characters

    if (msg.length() < 7) {
        std::cout << "  [PARSE] Message too short" << std::endl;
        return false;
    }

    if (msg.length() > 500) {
        std::cout << "  [PARSE] Message too long" << std::endl;
        return false;
    }

    // Find all delimiters
    int dashIdx = msg.indexOf('-');
    int ampIdx = msg.indexOf('&');
    int atIdx = msg.indexOf('@');
    int hashIdx = msg.indexOf('#');

    // Validate all delimiters present and in correct order
    if (dashIdx == -1 || ampIdx == -1 || atIdx == -1 || hashIdx == -1) {
        std::cout << "  [PARSE] Missing delimiter(s)" << std::endl;
        return false;
    }

    if (!(dashIdx < ampIdx && ampIdx < atIdx && atIdx < hashIdx)) {
        std::cout << "  [PARSE] Delimiters in wrong order" << std::endl;
        return false;
    }

    // Extract parts
    satId = msg.substring(0, dashIdx);
    command = msg.substring(dashIdx + 1, ampIdx);
    path = msg.substring(ampIdx + 1, atIdx);
    data = msg.substring(atIdx + 1, hashIdx);
    hmac = msg.substring(hashIdx + 1);

    // Validate satellite ID
    if (!satId.equals(sat_id)) {
        std::cout << "  [PARSE] Wrong satellite ID: " << satId.data << std::endl;
        return false;
    }

    // Validate command is alphanumeric
    for (unsigned int i = 0; i < command.length(); i++) {
        char c = command.charAt(i);
        if (!isalnum(c)) {
            std::cout << "  [PARSE] Invalid command characters" << std::endl;
            return false;
        }
    }

    // Validate path (no directory traversal)
    if (path.indexOf("..") != -1) {
        std::cout << "  [PARSE] Path traversal blocked!" << std::endl;
        sendMessage("ERR:PATH_TRAVERSAL_BLOCKED");
        return false;
    }

    // Verify HMAC
    String messageToVerify = msg.substring(0, hashIdx);
    if (!verifyHMAC(messageToVerify, hmac)) {
        std::cout << "  [AUTH] HMAC verification failed!" << std::endl;
        sendMessage("ERR:AUTH_FAILED");
        return false;
    }

    return true;
}

// ==================== TEST CASES ====================

struct TestCase {
    std::string name;
    std::string input;
    bool shouldPass;
};

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ORBITAL TEMPLE PARSER FUZZ TESTING" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::vector<TestCase> tests = {
        // Valid messages
        {"Valid Ping", "SAT001-Ping&@#1234567890abcdef", true},
        {"Valid Status", "SAT001-Status&@#1234567890abcdef", true},
        {"Valid WriteFile", "SAT001-WriteFile&/names.txt@John Doe#1234567890abcdef", true},

        // Empty and short
        {"Empty string", "", false},
        {"Single char", "X", false},
        {"Too short", "A-B&@#", false},
        {"Minimum valid length", "SAT001-X&@#1234567890abcdef", true},

        // Missing delimiters
        {"No dash", "SAT001Ping&@#1234567890abcdef", false},
        {"No ampersand", "SAT001-Ping@#1234567890abcdef", false},
        {"No at sign", "SAT001-Ping&#1234567890abcdef", false},
        {"No hash", "SAT001-Ping&@1234567890abcdef", false},

        // Wrong delimiter order
        {"Hash before at", "SAT001-Ping&#@1234567890abcdef", false},
        {"At before amp", "SAT001-Ping@&data#1234567890abcdef", false},
        {"All reversed", "SAT001#1234@data&path-Ping", false},

        // Wrong satellite ID
        {"Wrong ID", "SAT002-Ping&@#1234567890abcdef", false},
        {"Empty ID", "-Ping&@#1234567890abcdef", false},

        // Invalid command characters
        {"Command with space", "SAT001-Ping Me&@#1234567890abcdef", false},
        {"Command with special", "SAT001-Ping!&@#1234567890abcdef", false},
        {"Command with unicode", "SAT001-Pingé&@#1234567890abcdef", false},

        // Path traversal attacks
        {"Path traversal 1", "SAT001-ReadFile&../etc/passwd@#1234567890abcdef", false},
        {"Path traversal 2", "SAT001-ReadFile&/names/../../../etc@#1234567890abcdef", false},
        {"Path traversal 3", "SAT001-ReadFile&..@#1234567890abcdef", false},

        // Invalid HMAC
        {"Short HMAC", "SAT001-Ping&@#123", false},
        {"Empty HMAC", "SAT001-Ping&@#", false},

        // Very long inputs
        {"Long command", "SAT001-" + std::string(100, 'A') + "&@#1234567890abcdef", true},
        {"Long path", "SAT001-Ping&" + std::string(200, '/') + "@#1234567890abcdef", true},
        {"Long data", "SAT001-WriteFile&/f@" + std::string(300, 'X') + "#1234567890abcdef", true},
        {"Too long (>500)", "SAT001-Ping&@" + std::string(500, 'X') + "#1234567890abcdef", false},

        // Edge cases with multiple delimiters
        {"Multiple dashes", "SAT-001-Ping&@#1234567890abcdef", false},  // Wrong ID
        {"Multiple hashes", "SAT001-Ping&@#abc#1234567890abcdef", false},  // HMAC includes extra #, too long
        {"Multiple ats", "SAT001-Ping&path@data@more#1234567890abcdef", true},  // Data has @

        // Null bytes and special characters
        {"Data with newline", "SAT001-Write&/f@line1\\nline2#1234567890abcdef", true},
        {"Data with tab", "SAT001-Write&/f@col1\\tcol2#1234567890abcdef", true},

        // Unicode in data (should be allowed)
        {"Unicode in data", "SAT001-Write&/names@José María#1234567890abcdef", true},
        {"Japanese in data", "SAT001-Write&/names@田中太郎#1234567890abcdef", true},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        String satId, command, path, data, hmac;

        std::cout << "Test: " << test.name << std::endl;
        std::cout << "  Input: \"" << test.input.substr(0, 60)
                  << (test.input.length() > 60 ? "..." : "") << "\"" << std::endl;

        bool result = validateMessage(String(test.input), satId, command, path, data, hmac);

        bool success = (result == test.shouldPass);

        if (success) {
            std::cout << "  Result: PASS (" << (result ? "accepted" : "rejected")
                      << " as expected)" << std::endl;
            passed++;
        } else {
            std::cout << "  Result: *** FAIL *** (expected "
                      << (test.shouldPass ? "accept" : "reject")
                      << ", got " << (result ? "accept" : "reject") << ")" << std::endl;
            failed++;
        }
        std::cout << std::endl;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  RESULTS: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
