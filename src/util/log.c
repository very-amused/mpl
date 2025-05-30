#include "log.h"

#include <libavutil/log.h>

void configure_av_log() {
#ifdef MPL_DEBUG
	// Enable logging for debug builds
	(void)0;
#else
	av_log_set_level(AV_LOG_ERROR);
#endif
}
