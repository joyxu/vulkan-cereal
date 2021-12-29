#include "host-common/GfxstreamFatalError.h"

#include <cstdlib>
#include <ostream>

#include "base/Metrics.h"
#include "host-common/logging.h"

namespace {

using android::base::CreateMetricsLogger;
using android::base::GfxstreamVkAbort;

[[noreturn]] void die() {
    // Note: we do this in this die() function instead of just calling abort() directly so that
    // downstream users of gfxstream can easily substitute their own logic.
    abort();
}

}  // namespace

AbortMessage::AbortMessage(const char *file, const char *function, int line, FatalError reason)
    : mFile(file), mFunction(function), mLine(line), mReason(reason) {
    mOss << "FATAL in " << function << ", err code: " << reason.getAbortCode() << ": ";
}

AbortMessage::~AbortMessage() {
    OutputLog(stderr, 'F', mFile, mLine, 0, mOss.str().c_str());
    fflush(stderr);
    CreateMetricsLogger()->logMetricEvent(GfxstreamVkAbort{.file = mFile,
                                                           .function = mFunction,
                                                           .msg = mOss.str().c_str(),
                                                           .line = mLine,
                                                           .abort_reason = mReason.getAbortCode()});

    die();
}
