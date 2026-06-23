#include "winbintest/winbintest.hpp"

int main() {
    winbintest_t tester("ntoskrnl.exe");
    std::vector<uint8_t> out{};

    tester.download("https://winbindex.m417z.com/data/by_filename_compressed/ntoskrnl.exe.json.gz", out);
}