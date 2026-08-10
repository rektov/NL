#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <cstdlib>
#include <ctime>

int main(int argc, char** argv) {
#ifdef _WIN32
    _putenv_s("TZ", "UTC0");
    _tzset();
#else
    setenv("TZ", "UTC", 1);
    tzset();
#endif
    return doctest::Context(argc, argv).run();
}
