#include "base/EintrWrapper.h"
#include "base/StringFormat.h"
#include "base/System.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#include "msvc-posix.h"
#include "dirent.h"
#else
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

#include <string.h>

using FileSize = uint64_t;

#ifdef _WIN32
// Return |path| as a Unicode string, while discarding trailing separators.
Win32UnicodeString win32Path(StringView path) {
    Win32UnicodeString wpath(path);
    // Get rid of trailing directory separators, Windows doesn't like them.
    size_t size = wpath.size();
    while (size > 0U &&
           (wpath[size - 1U] == L'\\' || wpath[size - 1U] == L'/')) {
        size--;
    }
    if (size < wpath.size()) {
        wpath.resize(size);
    }
    return wpath;
}

using PathStat = struct _stat64;

#else  // _WIN32

using PathStat = struct stat;

#endif  // _WIN32

namespace {

struct TickCountImpl {
private:
    uint64_t mStartTimeUs;
#ifdef _WIN32
    long long mFreqPerSec = 0;    // 0 means 'high perf counter isn't available'
#elif defined(__APPLE__)
    clock_serv_t mClockServ;
#endif

public:
    TickCountImpl() {
#ifdef _WIN32
        LARGE_INTEGER freq;
        if (::QueryPerformanceFrequency(&freq)) {
            mFreqPerSec = freq.QuadPart;
        }
#elif defined(__APPLE__)
        host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &mClockServ);
#endif
        mStartTimeUs = getUs();
    }

#ifdef __APPLE__
    ~TickCountImpl() {
        mach_port_deallocate(mach_task_self(), mClockServ);
    }
#endif

    uint64_t getStartTimeUs() const {
        return mStartTimeUs;
    }

    uint64_t getUs() const {
#ifdef _WIN32
    if (!mFreqPerSec) {
        return ::GetTickCount() * 1000;
    }
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    return (now.QuadPart * 1000000ull) / mFreqPerSec;
#elif defined __linux__
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000ll + ts.tv_nsec / 1000;
#else // APPLE
    mach_timespec_t mts;
    clock_get_time(mClockServ, &mts);
    return mts.tv_sec * 1000000ll + mts.tv_nsec / 1000;
#endif
    }
};

// This is, maybe, the only static variable that may not be a LazyInstance:
// it holds the actual timestamp at startup, and has to be initialized as
// soon as possible after the application launch.
static const TickCountImpl kTickCount;

}  // namespace

namespace android {
namespace base {

std::string getEnvironmentVariable(const std::string& key) { 
#ifdef _WIN32
    Win32UnicodeString varname_unicode(key);
    const wchar_t* value = _wgetenv(varname_unicode.c_str());
    if (!value) {
        return std::string();
    } else {
        return Win32UnicodeString::convertToUtf8(value);
    }
#else
    const char* value = getenv(key.c_str());
    if (!value) {
        value = "";
    }
    return std::string(value);
#endif
}

void setEnvironmentVariable(const std::string& key, const std::string& value) {
#ifdef _WIN32
    std::string envStr =
        StringFormat("%s=%s", key, value);
    // Note: this leaks the result of release().
    _wputenv(Win32UnicodeString(envStr).release());
#else
    if (value.empty()) {
        unsetenv(key.c_str());
    } else {
        setenv(key.c_str(), value.c_str(), 1);
    }
#endif
}

bool isVerboseLogging() {
    return false;
}

int fdStat(int fd, PathStat* st) {
#ifdef _WIN32
    return fstat64(fd, st);
#else   // !_WIN32
    return HANDLE_EINTR(fstat(fd, st));
#endif  // !_WIN32
}

bool getFileSize(int fd, uint64_t* outFileSize) {
    if (fd < 0) {
        return false;
    }
    PathStat st;
    int ret = fdStat(fd, &st);
    if (ret < 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    // This is off_t on POSIX and a 32/64 bit integral type on windows based on
    // the host / compiler combination. We cast everything to 64 bit unsigned to
    // play safe.
    *outFileSize = static_cast<FileSize>(st.st_size);
    return true;
}

void sleepMs(uint64_t n) {
#ifdef _WIN32
    ::Sleep(n);
#else
    usleep(n * 1000);
#endif
}

void sleepUs(uint64_t n) {
#ifdef _WIN32
    ::Sleep(n / 1000);
#else
    usleep(n);
#endif
}

uint64_t getUnixTimeUs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

uint64_t getHighResTimeUs() {
    return kTickCount.getUs();
}

uint64_t getUptimeMs() {
    return (kTickCount.getUs() - kTickCount.getStartTimeUs()) / 1000;
}

std::string getProgramDirectoryFromPlatform() {
    std::string res;
#if defined(__linux__)
    char path[1024];
    memset(path, 0, sizeof(path));  // happy valgrind!
    int len = readlink("/proc/self/exe", path, sizeof(path));
    if (len > 0 && len < (int)sizeof(path)) {
        char* x = ::strrchr(path, '/');
        if (x) {
            *x = '\0';
            res.assign(path);
        }
    }
#elif defined(__APPLE__)
    char s[PATH_MAX];
    auto pid = getpid();
    proc_pidpath(pid, s, sizeof(s));
    char* x = ::strrchr(s, '/');
    if (x) {
        // skip all slashes - there might be more than one
        while (x > s && x[-1] == '/') {
            --x;
        }
        *x = '\0';
        res.assign(s);
    } else {
        res.assign("<unknown-application-dir>");
    }
#elif defined(_WIN32)
    Win32UnicodeString appDir(PATH_MAX);
    int len = GetModuleFileNameW(0, appDir.data(), appDir.size());
    res.assign("<unknown-application-dir>");
    if (len > 0) {
        if (len > (int)appDir.size()) {
            appDir.resize(static_cast<size_t>(len));
            GetModuleFileNameW(0, appDir.data(), appDir.size());
        }
        std::string dir = appDir.toString();
        char* sep = ::strrchr(&dir[0], '\\');
        if (sep) {
            *sep = '\0';
            res.assign(dir.c_str());
        }
    }
#else
#error "Unsupported platform!"
#endif
    return res;
}
std::string getProgramDirectory() {
    static std::string progDir;
    if (progDir.empty()) {
        progDir.assign(getProgramDirectoryFromPlatform());
    }
    return progDir;
}

std::string getLauncherDirectory() {
    return getProgramDirectory();
}

CpuTime cpuTime() {
    CpuTime res;

    res.wall_time_us = kTickCount.getUs();

#ifdef __APPLE__
    cpuUsageCurrentThread_macImpl(
        &res.user_time_us,
        &res.system_time_us);
#else

#ifdef __linux__
    struct rusage usage;
    getrusage(RUSAGE_THREAD, &usage);
    res.user_time_us =
        usage.ru_utime.tv_sec * 1000000ULL +
        usage.ru_utime.tv_usec;
    res.system_time_us =
        usage.ru_stime.tv_sec * 1000000ULL +
        usage.ru_stime.tv_usec;
#else // Windows
    FILETIME creation_time_struct;
    FILETIME exit_time_struct;
    FILETIME kernel_time_struct;
    FILETIME user_time_struct;
    GetThreadTimes(
        GetCurrentThread(),
        &creation_time_struct,
        &exit_time_struct,
        &kernel_time_struct,
        &user_time_struct);
    (void)creation_time_struct;
    (void)exit_time_struct;
    uint64_t user_time_100ns =
        user_time_struct.dwLowDateTime |
        ((uint64_t)user_time_struct.dwHighDateTime << 32);
    uint64_t system_time_100ns =
        kernel_time_struct.dwLowDateTime |
        ((uint64_t)kernel_time_struct.dwHighDateTime << 32);
    res.user_time_us = user_time_100ns / 10;
    res.system_time_us = system_time_100ns / 10;
#endif

#endif
    return res;
}

} // namespace base
} // namespace android
