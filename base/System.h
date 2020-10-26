#include <string>

namespace android {
namespace base {

std::string getEnvironmentVariable(const std::string& key);
void setEnvironmentVariable(const std::string& key, const std::string& value);
bool isVerboseLogging();

uint64_t getUnixTimeUs();

std::string getProgramDirectory();
std::string getLauncherDirectory();

} // namespace base
} // namespace android
