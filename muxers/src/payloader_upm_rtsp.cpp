#include <mutex>

extern "C" {
#include "payloader_upm_rtsp.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <libcjson/cJSON.h>
#include <libmediaprocsutils/uri_parser.h>
#include <libmediaprocsutils/log.h>
#include <libmediaprocsutils/stat_codes.h>
#include <libmediaprocsutils/check_utils.h>
#include <libmediaprocsutils/schedule.h>
#include <libmediaprocsutils/fair_lock.h>
#include <libmediaprocsutils/fifo.h>
#include <libmediaprocs/proc_if.h>
#include <libmediaprocs/procs.h>
#include <libmediaprocs/proc.h>
#include "muxers_settings.h"
#include "proc_muxer.h"
}

using namespace std;

/* **** Definitions **** */

#define SERVER_TOUT 10

#define FRAMED_SOURCE_FIFO_SLOTS 16

#define SINK_BUFFER_SIZE 200000

//#define ENABLE_DEBUG_LOGS
#ifdef ENABLE_DEBUG_LOGS
	#define LOGD_CTX_INIT(CTX) LOG_CTX_INIT(CTX)
	#define LOGD(FORMAT, ...) LOGV(FORMAT, ##__VA_ARGS__)
#else
	#define LOGD_CTX_INIT(CTX)
	#define LOGD(...)
#endif

/**
 * Returns non-zero if given 'tag' string contains 'needle' sub-string.
 */
#define TAG_HAS(NEEDLE) (strstr(tag, NEEDLE)!= NULL)

/**
 * Returns non-zero if 'tag' string is equal to given TAG string.
 */
#define TAG_IS(TAG) (strncmp(tag, TAG, strlen(TAG))== 0)

/**
 * [PAYLOADER UPM] RTSP multiplexer settings context structure.
 */
typedef struct payloader_upm_rtsp_mux_settings_ctx_s {
	/**
	 * Generic multiplexer settings.
	 * *MUST* be the first field in order to be able to cast to
	 * mux_settings_ctx_s.
	 */
	struct muxers_settings_mux_ctx_s muxers_settings_mux_ctx;
} payloader_upm_rtsp_mux_settings_ctx_t;

/**
 * [PAYLOADER UPM] RTSP multiplexer context structure.
 */
typedef struct payloader_upm_rtsp_mux_ctx_s {
	/**
	 * Generic MUXER context structure.
	 * *MUST* be the first field in order to be able to cast to both
	 * proc_muxer_mux_ctx_t or proc_ctx_t.
	 */
	struct proc_muxer_mux_ctx_s proc_muxer_mux_ctx;
	/**
	 * [PAYLOADER UPM] RTSP multiplexer settings.
	 * This structure extends (thus can be casted to) muxers_settings_mux_ctx_t.
	 */
	volatile struct payloader_upm_rtsp_mux_settings_ctx_s
	payloader_upm_rtsp_mux_settings_ctx;
	/**
	 * [PAYLOADER UPM] TaskScheduler Class.
	 * Used for scheduling MUXER and other processing when corresponding
	 * events are signaled (e.g. multiplexing frame of data when a new frame
	 * is available at the input).
	 */
//	NULL;
//	TaskScheduler *taskScheduler;
	/**
	 * Live555's UsageEnvironment Class.
	 */
//	NULL;
//	UsageEnvironment *usageEnvironment;
	/**
	 * Live555's RTSPServer Class.
	 */
//	NULL;
//	RTSPServer *rtspServer;
	/**
	 * Live555's scheduler thread.
	 */
//	NULL;
//	pthread_t taskScheduler_thread;
	/**
	 * Live555's media sessions server.
	 */
//	NULL;
//	ServerMediaSession *serverMediaSession;
	/**
	 * Reserved for future use: other parameters here ...
	 */
} payloader_upm_rtsp_mux_ctx_t;

/**
 * [PAYLOADER UPM] RTSP elementary stream (ES) multiplexer settings context structure.
 */
typedef struct payloader_upm_rtsp_es_mux_settings_ctx_s {
	/**
	 * MIME type for this ES-MUXER.
	 * (e.g. video/H264, etc.).
	 */
	char *sdp_mimetype;
	/**
	 * Time-base for this ES-MUXER.
	 * (e.g. 9000)
	 */
	unsigned int rtp_timestamp_freq;
} payloader_upm_rtsp_es_mux_settings_ctx_t;

/**
 * [PAYLOADER UPM] RTSP elementary stream (ES) multiplexer context structure.
 */
class SimpleMediaSubsession; // Forward declaration
typedef struct payloader_upm_rtsp_es_mux_ctx_s {
	/**
	 * Generic PROC context structure.
	 * *MUST* be the first field in order to be able to cast to proc_ctx_t.
	 */
	struct proc_ctx_s proc_ctx;
	/**
	 * [PAYLOADER UPM] RTSP ES-multiplexer settings.
	 */
	volatile struct payloader_upm_rtsp_es_mux_settings_ctx_s
	payloader_upm_rtsp_es_mux_settings_ctx;
	/**
	 * External LOG module context structure instance.
	 */
	log_ctx_t *log_ctx;
	/**
	 * Live555's SimpleMediaSubsession Class.
	 */
//	NULL;
//	SimpleMediaSubsession *simpleMediaSubsession;
	/**
	 * Externally defined Live555's TaskScheduler Class.
	 * Used for scheduling MUXER and other processing when corresponding
	 * events are signaled (e.g. multiplexing frame of data when a new frame
	 * is available at the input).
	 */
//	NULL;
//	TaskScheduler *taskScheduler;
	/**
	 * Reserved for future use: other parameters here ...
	 */
} payloader_upm_rtsp_es_mux_ctx_t;

/**
 * [PAYLOADER UPM] RTSP de-multiplexer settings context structure.
 */
typedef struct payloader_upm_rtsp_dmux_settings_ctx_s {
	/**
	 * Generic de-multiplexer settings.
	 * *MUST* be the first field in order to be able to cast to
	 * mux_settings_ctx_s.
	 */
	struct muxers_settings_dmux_ctx_s muxers_settings_dmux_ctx;
} payloader_upm_rtsp_dmux_settings_ctx_t;

/* **** Implementaciones **** */

extern "C" {
const proc_if_t proc_if_payloader_upm_rtsp_mux=
{
	"payloader_upm_rtsp_mux", "multiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_WR,
	NULL, //live555_rtsp_mux_open,
	NULL, //live555_rtsp_mux_close,
	NULL, //live555_rtsp_mux_rest_put,
	NULL, //live555_rtsp_mux_rest_get,
	NULL, //live555_rtsp_mux_process_frame,
	NULL, //live555_rtsp_mux_opt,
	NULL, // input proc_frame_ctx to "private-frame-format"
	NULL, // "private-frame-format" release
	NULL, // "private-frame-format" to proc_frame_ctx
};

static const proc_if_t proc_if_payloader_upm_rtsp_es_mux=
{
	"payloader_upm_rtsp_mux", "multiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_WR,
	NULL, //live555_rtsp_es_mux_open,
	NULL, //live555_rtsp_es_mux_close,
	NULL, //live555_rtsp_es_mux_rest_put // used internally only (not in API)
	NULL, //live555_rtsp_es_mux_rest_get,
	NULL, //live555_rtsp_es_mux_process_frame,
	NULL, //live555_rtsp_es_mux_opt
	NULL,
	NULL,
	NULL,
};

const proc_if_t proc_if_payloader_upm_rtsp_dmux=
{
	"payloader_upm_rtsp_dmux", "demultiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_RD,
	NULL, //live555_rtsp_dmux_open,
	NULL, //live555_rtsp_dmux_close,
	NULL, //live555_rtsp_dmux_rest_put,
	NULL, //live555_rtsp_dmux_rest_get,
	NULL, //live555_rtsp_dmux_process_frame,
	NULL, //live555_rtsp_dmux_opt,
	NULL, // input proc_frame_ctx to "private-frame-format"
	NULL, // "private-frame-format" release
	NULL, // "private-frame-format" to proc_frame_ctx
};
} //extern "C"
