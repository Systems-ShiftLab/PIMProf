#include <iostream>
#include <chrono>
#include <stack>
#include <assert.h>

typedef std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::nanoseconds> TimePoint;
typedef std::chrono::duration<int64_t, std::nano> Duration;

int currentPIMLevel = 0;
std::chrono::nanoseconds totalTime(0);
TimePoint start, end;

bool scopeInitialized = false;
std::stack<int> scope;

int PIMProfOffloader(int decision, int mode, int bblid, int parallel)
{
    if (!scopeInitialized) {
        scopeInitialized = true;
        scope.push(-1); // push an invalid element as the global scope
    }
    if (mode == 0) {
        if (scope.top() == 1 && decision == 0) {
            end = std::chrono::system_clock::now();
            totalTime += (end - start);
        }
        else if (scope.top() != decision && decision == 1) {
            start = std::chrono::system_clock::now();
        }
        else {
            assert(0);
        }
        scope.push(decision);

    }
    if (mode == 1) {
        scope.pop();
        if (scope.top() == 1 && decision == 0) {
            start = std::chrono::system_clock::now();
        }
        else if (scope.top() != decision && decision == 1) {
            end = std::chrono::system_clock::now();
            totalTime += (end - start);
        }
        else {
            assert(0);
        }
    }
    return 0;
}

int PIMProfOffloader2(int decision, int mode, int bblid, int parallel)
{
    if (mode == 1) {
        std::cout << "Total time (ns) = " << totalTime.count() << std::endl;
    }
    else {
        assert(0);
    }
}