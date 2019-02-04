#include <mutex>

extern "C" {
#include "payloader_upm_rtsp.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

/* Payloader's library related */
/*
#include <libmediaprocspayloader/logger.h>
#include <libmediaprocspayloader/Interfaces.h>
#include <libmediaprocspayloader/TcpServer.h>
#include <libmediaprocspayloader/TcpConnection.h>
#include <libmediaprocspayloader/Packager.h>
#include <libmediaprocspayloader/Sender.h>
#include <libmediaprocspayloader/RtpFragmenter.h>
#include <libmediaprocspayloader/RtpHeaders.h>
*/
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

/**
 * [PAYLOADER UPM] RTSP de-multiplexer settings context structure.
 */

typedef struct payloader_upm_rtsp_dmux_ctx_s {
	/**
	 * Generic processor context structure.
	 * *MUST* be the first field in order to be able to cast to proc_ctx_t.
	 */
	struct proc_ctx_s proc_ctx;
	/**
	 * Live555's RTSP de-multiplexer settings.
	 * This structure extends (thus can be casted to)
	 * muxers_settings_dmux_ctx_t.
	 */
	volatile struct payloader_upm_rtsp_dmux_settings_ctx_s
	payloader_upm_rtsp_dmux_settings_ctx;
	/**
	 * Live555's TaskScheduler Class.
	 * Used for scheduling MUXER and other processing when corresponding
	 * events are signaled (e.g. de-multiplexing frame of data when a new frame
	 * is available at the input).
	 */
	//TaskScheduler *taskScheduler;
	/**
	 * Live555's UsageEnvironment Class.
	 */
	//UsageEnvironment *usageEnvironment;
	/**
	 * Live555's RTSPClient simple extension.
	 */
	//SimpleRTSPClient *simpleRTSPClient;
} payloader_upm_rtsp_dmux_ctx_t;

/* **** Prototypes **** */

/* **** Multiplexer **** */

static proc_ctx_t* payloader_upm_rtsp_mux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg);
static int payloader_upm_rtsp_mux_init_given_settings(
		payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx,
		const muxers_settings_mux_ctx_t *muxers_settings_mux_ctx,
		log_ctx_t *log_ctx);
static void payloader_upm_rtsp_mux_close(proc_ctx_t **ref_proc_ctx);
static void payloader_upm_rtsp_mux_deinit_except_settings(
		payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx, log_ctx_t *log_ctx);
static int payloader_upm_rtsp_mux_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t *iput_fifo_ctx, fifo_ctx_t *oput_fifo_ctx);
static int payloader_upm_rtsp_mux_rest_put(proc_ctx_t *proc_ctx, const char *str);
static int payloader_upm_rtsp_mux_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg);
static int payloader_upm_rtsp_mux_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);
static int payloader_upm_rtsp_mux_rest_get_es_array(procs_ctx_t *procs_ctx_es_muxers,
		cJSON **ref_cjson_es_array, log_ctx_t *log_ctx);

static int payloader_upm_rtsp_mux_settings_ctx_init(
		volatile payloader_upm_rtsp_mux_settings_ctx_t *payloader_upm_rtsp_mux_settings_ctx,
		log_ctx_t *log_ctx);
static void payloader_upm_rtsp_mux_settings_ctx_deinit(
		volatile payloader_upm_rtsp_mux_settings_ctx_t *payloader_upm_rtsp_mux_settings_ctx,
		log_ctx_t *log_ctx);
static void* taskScheduler_thr(void *t);

static proc_ctx_t* payloader_upm_rtsp_es_mux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg);
static void payloader_upm_rtsp_es_mux_close(proc_ctx_t **ref_proc_ctx);
static int payloader_upm_rtsp_es_mux_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t *iput_fifo_ctx, fifo_ctx_t *oput_fifo_ctx);
static int payloader_upm_rtsp_es_mux_rest_put(proc_ctx_t *proc_ctx, const char *str);
static int payloader_upm_rtsp_es_mux_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);

static int payloader_upm_rtsp_es_mux_settings_ctx_init(
		volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
		payloader_upm_rtsp_es_mux_settings_ctx, log_ctx_t *log_ctx);
static void payloader_upm_rtsp_es_mux_settings_ctx_deinit(
		volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
		payloader_upm_rtsp_es_mux_settings_ctx, log_ctx_t *log_ctx);

/* **** De-multiplexer **** */

static proc_ctx_t* payloader_upm_rtsp_dmux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg);
static int payloader_upm_rtsp_dmux_init_given_settings(
		payloader_upm_rtsp_dmux_ctx_t *payloader_upm_rtsp_dmux_ctx,
		const muxers_settings_dmux_ctx_t *muxers_settings_dmux_ctx,
		log_ctx_t *log_ctx);
static void payloader_upm_rtsp_dmux_close(proc_ctx_t **ref_proc_ctx);
static void payloader_upm_rtsp_dmux_deinit_except_settings(
		payloader_upm_rtsp_dmux_ctx_t *payloader_upm_rtsp_dmux_ctx, log_ctx_t *log_ctx);
static int payloader_upm_rtsp_dmux_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse);
static int payloader_upm_rtsp_dmux_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t* iput_fifo_ctx, fifo_ctx_t* oput_fifo_ctx);
static int payloader_upm_rtsp_dmux_rest_put(proc_ctx_t *proc_ctx, const char *str);

static int payloader_upm_rtsp_dmux_settings_ctx_init(
		volatile payloader_upm_rtsp_dmux_settings_ctx_t *
		payloader_upm_rtsp_dmux_settings_ctx, log_ctx_t *log_ctx);
static void payloader_upm_rtsp_dmux_settings_ctx_deinit(
		volatile payloader_upm_rtsp_dmux_settings_ctx_t *
		payloader_upm_rtsp_dmux_settings_ctx, log_ctx_t *log_ctx);

/* **** Implementaciones **** */

extern "C" {
const proc_if_t proc_if_payloader_upm_rtsp_mux=
{
	"payloader_upm_rtsp_mux", "multiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_WR,
	payloader_upm_rtsp_mux_open,//[MARIO]
	payloader_upm_rtsp_mux_close,// [MARIO]
	NULL, //live555_rtsp_mux_rest_put,
	payloader_upm_rtsp_mux_rest_get,//[MARIO]
	payloader_upm_rtsp_mux_process_frame,//[MARIO]
	payloader_upm_rtsp_mux_opt, //[MARIO]
	NULL, // input proc_frame_ctx to "private-frame-format"
	NULL, // "private-frame-format" release
	NULL, // "private-frame-format" to proc_frame_ctx
};

static const proc_if_t proc_if_payloader_upm_rtsp_es_mux=
{
	"payloader_upm_rtsp_es_mux", "multiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_WR,
	payloader_upm_rtsp_es_mux_open,//[MARIO] NULL, //live555_rtsp_mux_open,
	payloader_upm_rtsp_es_mux_close,//[MARIO] NULL, //live555_rtsp_es_mux_close,
	NULL, //live555_rtsp_es_mux_rest_put // used internally only (not in API)
	payloader_upm_rtsp_es_mux_rest_get, //NULL, //live555_rtsp_es_mux_rest_get,
	payloader_upm_rtsp_es_mux_process_frame,//[MARIO] NULL, //live555_rtsp_es_mux_process_frame,
	NULL, //live555_rtsp_es_mux_opt
	NULL,
	NULL,
	NULL,
};

const proc_if_t proc_if_payloader_upm_rtsp_dmux=
{
	"payloader_upm_rtsp_dmux", "demultiplexer", "application/octet-stream",
	(uint64_t)PROC_FEATURE_RD,
	NULL, //payloader_upm_rtsp_dmux_open, // [MARIO] NECESARIA pero aun no implementada
	NULL, //payloader_upm_rtsp_dmux_close, // [MARIO] NECESARIA  pero aun no implementada
	NULL, //payloader_upm_rtsp_dmux_rest_put, // [MARIO] NECESARIA  pero aun no implementada
	NULL, //payloader_upm_rtsp_dmux_rest_get, // [MARIO] NECESARIA  pero aun no implementada
	NULL, //payloader_upm_rtsp_dmux_process_frame, // [MARIO] NECESARIA pero aun no implementada
	NULL, //payloader_upm_rtsp_dmux_opt,
	NULL, // input proc_frame_ctx to "private-frame-format"
	NULL, // "private-frame-format" release
	NULL, // "private-frame-format" to proc_frame_ctx
};
} //extern "C"

// ###################### Agregado 100119

/* **** Multiplexer **** */

static proc_ctx_t* payloader_upm_rtsp_mux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_open \n");
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx= NULL;
	volatile payloader_upm_rtsp_mux_settings_ctx_t *payloader_upm_rtsp_mux_settings_ctx=
			NULL; // Do not release
	volatile muxers_settings_mux_ctx_t *muxers_settings_mux_ctx=
			NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Note: 'log_ctx' is allowed to be NULL

	/* Allocate context structure */
	payloader_upm_rtsp_mux_ctx= (payloader_upm_rtsp_mux_ctx_t*)calloc(1, sizeof(
			payloader_upm_rtsp_mux_ctx_t));
	CHECK_DO(payloader_upm_rtsp_mux_ctx!= NULL, goto end);

	/* Get settings structures */
	payloader_upm_rtsp_mux_settings_ctx=
			&payloader_upm_rtsp_mux_ctx->payloader_upm_rtsp_mux_settings_ctx;
	muxers_settings_mux_ctx=
			&payloader_upm_rtsp_mux_settings_ctx->muxers_settings_mux_ctx;

	/* Initialize settings to defaults */
	ret_code= payloader_upm_rtsp_mux_settings_ctx_init(payloader_upm_rtsp_mux_settings_ctx,
			LOG_CTX_GET());
printf("\n [payloader_upm_rtsp.cpp] [mux_settings_ctx_init] el ret_code [373] es: %d\n",ret_code);
printf("\n [payloader_upm_rtsp.cpp] [mux_settings_ctx_init] el ret_code [373] tiene que ser: %d\n",STAT_SUCCESS);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= payloader_upm_rtsp_mux_rest_put((proc_ctx_t*)payloader_upm_rtsp_mux_ctx,
			settings_str);
printf("\n [payloader_upm_rtsp.cpp] [mux_rest_put] el ret_code [379] es: %d\n",ret_code);
printf("\n [payloader_upm_rtsp.cpp] [mux_rest_put] el ret_code [379] tiene que ser: %d\n",STAT_SUCCESS);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

    /* **** Initialize the specific Live555 multiplexer resources ****
     * Now that all the parameters are set, we proceed with Live555 specific's.
     */
	ret_code= payloader_upm_rtsp_mux_init_given_settings(payloader_upm_rtsp_mux_ctx,
			(const muxers_settings_mux_ctx_t*)muxers_settings_mux_ctx,
			LOG_CTX_GET());
printf("\n [payloader_upm_rtsp.cpp] [init_given_settings] el ret_code [388] es: %d\n",ret_code);
printf("\n [payloader_upm_rtsp.cpp] [init_given_settings] el ret_code [388] tiene que ser: %d\n",STAT_SUCCESS);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
    if(end_code!= STAT_SUCCESS)
    	payloader_upm_rtsp_mux_close((proc_ctx_t**)&payloader_upm_rtsp_mux_ctx);
	return (proc_ctx_t*)payloader_upm_rtsp_mux_ctx;
}

static int payloader_upm_rtsp_mux_settings_ctx_init(
		volatile payloader_upm_rtsp_mux_settings_ctx_t *payloader_upm_rtsp_mux_settings_ctx,
		log_ctx_t *log_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_settings_ctx_init \n");
	int ret_code;
	volatile muxers_settings_mux_ctx_t *muxers_settings_mux_ctx= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(payloader_upm_rtsp_mux_settings_ctx!= NULL, return STAT_ERROR);

	muxers_settings_mux_ctx=
			&payloader_upm_rtsp_mux_settings_ctx->muxers_settings_mux_ctx;

	/* Initialize generic multiplexer settings */
	ret_code= muxers_settings_mux_ctx_init(muxers_settings_mux_ctx);
	if(ret_code!= STAT_SUCCESS)
		return ret_code;

	/* Initialize specific multiplexer settings */
	// Reserved for future use

	return STAT_SUCCESS;
}

static int payloader_upm_rtsp_mux_rest_put(proc_ctx_t *proc_ctx, const char *str)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_rest_put \n");
	/*
	Completar
	*/
	return STAT_SUCCESS;
}

static int payloader_upm_rtsp_mux_init_given_settings(
		payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx,
		const muxers_settings_mux_ctx_t *muxers_settings_mux_ctx,
		log_ctx_t *log_ctx)
{	
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_init_given_settings \n");
	char *stream_session_name;
	int end_code,ret_code;
	int port= 8554;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(payloader_upm_rtsp_mux_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(muxers_settings_mux_ctx!= NULL, return STAT_ERROR);
	// Note: 'log_ctx' is allowed to be NULL

	/* Initialize generic multiplexing common context structure */
	ret_code= proc_muxer_mux_ctx_init(
			(proc_muxer_mux_ctx_t*)payloader_upm_rtsp_mux_ctx, LOG_CTX_GET());

	/* [MARIO] Aqui se crea el Server RTSP en live555 */
	port= muxers_settings_mux_ctx->rtsp_port;

	/* Register ES-MUXER processor type */
	ret_code= procs_module_opt("PROCS_REGISTER_TYPE",
			&proc_if_payloader_upm_rtsp_es_mux);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_ECONFLICT, goto end);

	end_code= STAT_SUCCESS;
	/*
	Completar
	*/
end:
    if(end_code!= STAT_SUCCESS){
    	payloader_upm_rtsp_mux_deinit_except_settings(payloader_upm_rtsp_mux_ctx,
    			LOG_CTX_GET());
	}return end_code;
}


static void payloader_upm_rtsp_mux_close(proc_ctx_t **ref_proc_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_close \n");
	/*
	Completar
	*/
}

static int payloader_upm_rtsp_mux_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t* iput_fifo_ctx, fifo_ctx_t* oput_fifo_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_process_frame \n");
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx= NULL; // Do not release
	proc_muxer_mux_ctx_t *proc_muxer_mux_ctx= NULL; // DO not release
	procs_ctx_t *procs_ctx_es_muxers= NULL; // Do not release
	proc_frame_ctx_t *proc_frame_ctx_iput= NULL;
	size_t fifo_elem_size= 0;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(iput_fifo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(oput_fifo_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Get multiplexer context */
	payloader_upm_rtsp_mux_ctx= (payloader_upm_rtsp_mux_ctx_t*)proc_ctx;

	/* Get multiplexer processing common context structure */
	proc_muxer_mux_ctx= (proc_muxer_mux_ctx_t*)payloader_upm_rtsp_mux_ctx;

	/* Get elementary streams MUXERS hanlder (PROCS module) */
	procs_ctx_es_muxers= proc_muxer_mux_ctx->procs_ctx_es_muxers;
	CHECK_DO(procs_ctx_es_muxers!= NULL, goto end);

	/* Get input packet from FIFO buffer */
	ret_code= fifo_get(iput_fifo_ctx, (void**)&proc_frame_ctx_iput,
			&fifo_elem_size);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_EAGAIN, goto end);
	if(ret_code== STAT_EAGAIN) {
		/* This means FIFO was unblocked, just go out with EOF status */
		end_code= STAT_EOF;
		goto end;
	}

	/* Multiplex frame */
	ret_code= procs_send_frame(procs_ctx_es_muxers, proc_frame_ctx_iput->es_id,
			proc_frame_ctx_iput);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_EAGAIN, goto end);

	end_code= STAT_SUCCESS;
end:
	if(proc_frame_ctx_iput!= NULL)
		proc_frame_ctx_release(&proc_frame_ctx_iput);
	return end_code;
}

static int payloader_upm_rtsp_mux_opt(proc_ctx_t *proc_ctx, const char *tag,
		va_list arg)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_opt \n");
#define PROC_ID_STR_FMT "{\"elementary_stream_id\":%d}"
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx= NULL;
	proc_muxer_mux_ctx_t *proc_muxer_mux_ctx= NULL;
	procs_ctx_t *procs_ctx_es_muxers= NULL;
	char *rest_str= NULL;
	char ref_id_str[strlen(PROC_ID_STR_FMT)+ 64];
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(tag!= NULL, return STAT_ERROR);

	/*  Check that module instance critical section is locked */
	ret_code= pthread_mutex_trylock(&proc_ctx->api_mutex);
	CHECK_DO(ret_code== EBUSY, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	payloader_upm_rtsp_mux_ctx= (payloader_upm_rtsp_mux_ctx_t*)proc_ctx;
	proc_muxer_mux_ctx= &payloader_upm_rtsp_mux_ctx->proc_muxer_mux_ctx;
	procs_ctx_es_muxers= proc_muxer_mux_ctx->procs_ctx_es_muxers;

	if(TAG_IS("PROCS_ID_ES_MUX_REGISTER")) {
		char *p;
		int elementary_stream_id= -1;
		const char *settings_str= va_arg(arg, const char*);
		char **ref_rest_str= va_arg(arg, char**);
		*ref_rest_str= NULL; // Value to return in case of error
		end_code= procs_opt(procs_ctx_es_muxers, "PROCS_POST",
				"payloader_upm_rtsp_es_mux", settings_str, &rest_str
				//payloader_upm_rtsp_mux_ctx->usageEnvironment,
				//payloader_upm_rtsp_mux_ctx->serverMediaSession);
				);
		CHECK_DO(end_code== STAT_SUCCESS && rest_str!= NULL, goto end);

		/* Get processor identifier */
		p= strstr(rest_str, "\"proc_id\":");
		CHECK_DO(p!= NULL, goto end);
		p+= strlen("\"proc_id\":");
		elementary_stream_id= atoi(p);
		CHECK_DO(elementary_stream_id>= 0, goto end);

		/* Prepare JSON string to be returned */
		snprintf(ref_id_str, sizeof(ref_id_str), PROC_ID_STR_FMT,
				elementary_stream_id);
		*ref_rest_str= strdup(ref_id_str);
	} else {
		LOGE("Unknown option\n");
		end_code= STAT_ENOTFOUND;
	}

end:
	if(rest_str!= NULL)
		free(rest_str);
	return end_code;
#undef PROC_ID_STR_FMT
}

/**
 * Implements the proc_if_s::opt callback.
 * See .proc_if.h for further details.
 */

static int payloader_upm_rtsp_mux_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_rest_get \n");
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_mux_ctx_t *payloader_upm_rtsp_mux_ctx= NULL;
	procs_ctx_t *procs_ctx_es_muxers= NULL; // Do not release
	volatile payloader_upm_rtsp_mux_settings_ctx_t *
		payloader_upm_rtsp_mux_settings_ctx= NULL;
	volatile muxers_settings_mux_ctx_t *muxers_settings_mux_ctx= NULL;
	cJSON *cjson_rest= NULL, *cjson_settings= NULL, *cjson_es_array= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(rest_fmt< PROC_IF_REST_FMT_ENUM_MAX, return STAT_ERROR);
	CHECK_DO(ref_reponse!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	*ref_reponse= NULL;

	/* Create cJSON tree root object */
	cjson_rest= cJSON_CreateObject();
	CHECK_DO(cjson_rest!= NULL, goto end);

	/* JSON string to be returned:
	 * {
	 *     "settings":
	 *     {
	 *         ...
	 *     },
	 *     elementary_streams:
	 *     [
	 *         {...},
	 *         ...
	 *     ]
	 *     ... // reserved for future use
	 * }
	 */

	/* Get multiplexer settings contexts */
	payloader_upm_rtsp_mux_ctx= (payloader_upm_rtsp_mux_ctx_t*)proc_ctx;
	payloader_upm_rtsp_mux_settings_ctx=
			&payloader_upm_rtsp_mux_ctx->payloader_upm_rtsp_mux_settings_ctx;
	muxers_settings_mux_ctx=
			&payloader_upm_rtsp_mux_settings_ctx->muxers_settings_mux_ctx;

	/* GET generic multiplexer settings */
	ret_code= muxers_settings_mux_ctx_restful_get(muxers_settings_mux_ctx,
			&cjson_settings, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS && cjson_settings!= NULL, goto end);

	/* GET specific multiplexer settings */
	// Reserved for future use: attach to 'cjson_settings' (should be != NULL)

	/* Attach settings object to REST response */
	cJSON_AddItemToObject(cjson_rest, "settings", cjson_settings);
	cjson_settings= NULL; // Attached; avoid double referencing

	/* **** Attach data to REST response **** */

	/* Get ES-processors array REST and attach */
	procs_ctx_es_muxers= ((proc_muxer_mux_ctx_t*)
			payloader_upm_rtsp_mux_ctx)->procs_ctx_es_muxers;
	CHECK_DO(procs_ctx_es_muxers!= NULL, goto end);
	ret_code= payloader_upm_rtsp_mux_rest_get_es_array(procs_ctx_es_muxers,
			&cjson_es_array, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS && cjson_es_array!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "elementary_streams", cjson_es_array);
	cjson_es_array= NULL; // Attached; avoid double referencing

	// Reserved for future use
	/* Example:
	 * cjson_aux= cJSON_CreateNumber((double)payloader_upm_rtsp_mux_ctx->var1);
	 * CHECK_DO(cjson_aux!= NULL, goto end);
	 * cJSON_AddItemToObject(cjson_rest, "var1_name", cjson_aux);
	 */

	/* Format response to be returned */
	switch(rest_fmt) {
	case PROC_IF_REST_FMT_CHAR:
		/* Print cJSON structure data to char string */
		*ref_reponse= (void*)CJSON_PRINT(cjson_rest);
		CHECK_DO(*ref_reponse!= NULL && strlen((char*)*ref_reponse)> 0,
				goto end);
		break;
	case PROC_IF_REST_FMT_CJSON:
		*ref_reponse= (void*)cjson_rest;
		cjson_rest= NULL; // Avoid double referencing
		break;
	default:
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	if(cjson_settings!= NULL)
		cJSON_Delete(cjson_settings);
	if(cjson_es_array!= NULL)
		cJSON_Delete(cjson_es_array);
	return end_code;
}

static int payloader_upm_rtsp_mux_rest_get_es_array(procs_ctx_t *procs_ctx_es_muxers,
		cJSON **ref_cjson_es_array, log_ctx_t *log_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_mux_rest_get_es_array \n");
	int i, ret_code, procs_num= 0, end_code= STAT_ERROR;
	cJSON *cjson_es_array= NULL, *cjson_procs_rest= NULL,
			*cjson_procs_es_rest= NULL, *cjson_procs_es_rest_settings= NULL;
	cJSON *cjson_procs= NULL, *cjson_aux= NULL; // Do not release
	char *rest_str_aux= NULL, *es_rest_str_aux= NULL;
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(procs_ctx_es_muxers!= NULL, return STAT_ERROR);
	CHECK_DO(ref_cjson_es_array!= NULL, return STAT_ERROR);

	*ref_cjson_es_array= NULL;

	/* Create ES-processors array REST and attach*/
	cjson_es_array= cJSON_CreateArray();
	CHECK_DO(cjson_es_array!= NULL, goto end);

	/* JSON string to be returned:
	 * [
	 *     {...}, // ES-object JSON
	 *     ...
	 * ]
	 */

	ret_code= procs_opt(procs_ctx_es_muxers, "PROCS_GET", &rest_str_aux);
	CHECK_DO(ret_code== STAT_SUCCESS && rest_str_aux!= NULL, goto end);

	/* Parse to cJSON structure */
	cjson_procs_rest= cJSON_Parse(rest_str_aux);
	CHECK_DO(cjson_procs_rest!= NULL, goto end);

	/* Get ES-processors array */
	cjson_procs= cJSON_GetObjectItem(cjson_procs_rest, "procs");
	CHECK_DO(cjson_procs!= NULL, goto end);
	procs_num= cJSON_GetArraySize(cjson_procs);
	for(i= 0; i< procs_num; i++) {
		int elem_stream_id;
		cJSON *cjson_proc= cJSON_GetArrayItem(cjson_procs, i);
		CHECK_DO(cjson_proc!= NULL, continue);

		cjson_aux= cJSON_GetObjectItem(cjson_proc, "proc_id");
		CHECK_DO(cjson_aux!= NULL, continue);
		elem_stream_id= (int)cjson_aux->valuedouble;

		/* Get ES-processor REST */
		if(es_rest_str_aux!= NULL) {
			free(es_rest_str_aux);
			es_rest_str_aux= NULL;
		}
		ret_code= procs_opt(procs_ctx_es_muxers, "PROCS_ID_GET",
				elem_stream_id, &es_rest_str_aux);
		CHECK_DO(ret_code== STAT_SUCCESS && es_rest_str_aux!= NULL, continue);

		/* Parse ES-processor response to cJSON structure */
		if(cjson_procs_es_rest!= NULL) {
			cJSON_Delete(cjson_procs_es_rest);
			cjson_procs_es_rest= NULL;
		}
		cjson_procs_es_rest= cJSON_Parse(es_rest_str_aux);
		CHECK_DO(cjson_procs_es_rest!= NULL, continue);

		/* Attach elementary stream Id. (== xxx) */
		cjson_aux= cJSON_CreateNumber((double)elem_stream_id);
		CHECK_DO(cjson_aux!= NULL, continue);
		cJSON_AddItemToObject(cjson_procs_es_rest, "elementary_stream_id",
				cjson_aux);

		/* Detach settings from elementary stream REST */
		if(cjson_procs_es_rest_settings!= NULL) {
			cJSON_Delete(cjson_procs_es_rest_settings);
			cjson_procs_es_rest_settings= NULL;
		}
		cjson_procs_es_rest_settings= cJSON_DetachItemFromObject(
				cjson_procs_es_rest, "settings");

		/* Attach elementary stream data to array */
		cJSON_AddItemToArray(cjson_es_array, cjson_procs_es_rest);
		cjson_procs_es_rest= NULL; // Attached; avoid double referencing
	}

	*ref_cjson_es_array= cjson_es_array;
	cjson_es_array= NULL; // Avoid double referencing
	end_code= STAT_SUCCESS;
end:
	if(cjson_es_array!= NULL)
		cJSON_Delete(cjson_es_array);
	if(rest_str_aux!= NULL)
		free(rest_str_aux);
	if(cjson_procs_rest!= NULL)
		cJSON_Delete(cjson_procs_rest);
	if(es_rest_str_aux!= NULL)
		free(es_rest_str_aux);
	if(cjson_procs_es_rest!= NULL)
		cJSON_Delete(cjson_procs_es_rest);
	if(cjson_procs_es_rest_settings!= NULL)
		cJSON_Delete(cjson_procs_es_rest_settings);
	return end_code;
}

/* **** De-multiplexer **** */

/**
 * Implements the proc_if_s::open callback.
 * See .proc_if.h for further details.
 */


static proc_ctx_t* payloader_upm_rtsp_dmux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_dmux_open \n");
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_dmux_ctx_t *payloader_upm_rtsp_dmux_ctx= NULL;
	volatile payloader_upm_rtsp_dmux_settings_ctx_t *payloader_upm_rtsp_dmux_settings_ctx=
			NULL; // Do not release (alias)
	volatile muxers_settings_dmux_ctx_t *muxers_settings_dmux_ctx=
			NULL; // Do not release (alias)
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Note: 'log_ctx' is allowed to be NULL

	/* Allocate context structure */
	payloader_upm_rtsp_dmux_ctx= (payloader_upm_rtsp_dmux_ctx_t*)calloc(1, sizeof(
			payloader_upm_rtsp_dmux_ctx_t));
	CHECK_DO(payloader_upm_rtsp_dmux_ctx!= NULL, goto end);

	/* Get settings structures */
	payloader_upm_rtsp_dmux_settings_ctx=
			&payloader_upm_rtsp_dmux_ctx->payloader_upm_rtsp_dmux_settings_ctx;
	muxers_settings_dmux_ctx=
			&payloader_upm_rtsp_dmux_settings_ctx->muxers_settings_dmux_ctx;

	/* Initialize settings to defaults */
	ret_code= payloader_upm_rtsp_dmux_settings_ctx_init(
			payloader_upm_rtsp_dmux_settings_ctx, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= payloader_upm_rtsp_dmux_rest_put((proc_ctx_t*)payloader_upm_rtsp_dmux_ctx,
			settings_str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

    /* **** Initialize the specific Live555 de-multiplexer resources ****
     * Now that all the parameters are set, we proceed with Live555 specific's.
     */
	ret_code= payloader_upm_rtsp_dmux_init_given_settings(payloader_upm_rtsp_dmux_ctx,
			(const muxers_settings_dmux_ctx_t*)muxers_settings_dmux_ctx,
			LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	end_code= STAT_SUCCESS;
end:
    if(end_code!= STAT_SUCCESS)
    	payloader_upm_rtsp_dmux_close((proc_ctx_t**)&payloader_upm_rtsp_dmux_ctx);
	return (proc_ctx_t*)payloader_upm_rtsp_dmux_ctx;
}

static int payloader_upm_rtsp_dmux_settings_ctx_init(
		volatile payloader_upm_rtsp_dmux_settings_ctx_t *
		payloader_upm_rtsp_dmux_settings_ctx, log_ctx_t *log_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_dmux_settings_ctx_init \n");
	/*
	Completar
	*/
	return STAT_SUCCESS;
}

static int payloader_upm_rtsp_dmux_rest_put(proc_ctx_t *proc_ctx, const char *str)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_dmux_rest_put \n");
	/*
	Completar
	*/
	return STAT_SUCCESS;
}

static int payloader_upm_rtsp_dmux_init_given_settings(
		payloader_upm_rtsp_dmux_ctx_t *payloader_upm_rtsp_dmux_ctx,
		const muxers_settings_dmux_ctx_t *muxers_settings_dmux_ctx,
		log_ctx_t *log_ctx)
{	
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_dmux_init_given_settings \n");
	int end_code;
	/*
	Completar
	*/
	end_code= STAT_SUCCESS;
	return end_code;

}

static void payloader_upm_rtsp_dmux_close(proc_ctx_t **ref_proc_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_dmux_close \n");
	/*
	Completar
	*/
}

static proc_ctx_t* payloader_upm_rtsp_es_mux_open(const proc_if_t *proc_if,
		const char *settings_str, log_ctx_t *log_ctx, va_list arg)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_open \n");


	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_es_mux_ctx_t *payloader_upm_rtsp_es_mux_ctx= NULL;
	volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
			payloader_upm_rtsp_es_mux_settings_ctx= NULL; // Do not release
	//UsageEnvironment *usageEnvironment= NULL; // Do not release
	//ServerMediaSession *serverMediaSession= NULL; // Do not release
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(proc_if!= NULL, return NULL);
	CHECK_DO(settings_str!= NULL, return NULL);
	// Note: 'log_ctx' is allowed to be NULL

	/* Allocate context structure */
	payloader_upm_rtsp_es_mux_ctx= (payloader_upm_rtsp_es_mux_ctx_t*)calloc(1, sizeof(
			payloader_upm_rtsp_es_mux_ctx_t));
	CHECK_DO(payloader_upm_rtsp_es_mux_ctx!= NULL, goto end);

	/* Get settings structure */
	payloader_upm_rtsp_es_mux_settings_ctx=
			&payloader_upm_rtsp_es_mux_ctx->payloader_upm_rtsp_es_mux_settings_ctx;

	/* Initialize settings to defaults */
	ret_code= payloader_upm_rtsp_es_mux_settings_ctx_init(
			payloader_upm_rtsp_es_mux_settings_ctx, LOG_CTX_GET());
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* Parse and put given settings */
	ret_code= payloader_upm_rtsp_es_mux_rest_put((proc_ctx_t*)
			payloader_upm_rtsp_es_mux_ctx, settings_str);
	CHECK_DO(ret_code== STAT_SUCCESS, goto end);

	/* **** Initialize specific context structure members **** */

	payloader_upm_rtsp_es_mux_ctx->log_ctx= LOG_CTX_GET();

	//usageEnvironment= va_arg(arg, UsageEnvironment*);
	//CHECK_DO(usageEnvironment!= NULL, goto end);

	//serverMediaSession= va_arg(arg, ServerMediaSession*);
	//CHECK_DO(serverMediaSession!= NULL, goto end);

	//live555_rtsp_es_mux_ctx->taskScheduler=
	//		&(usageEnvironment->taskScheduler());
	//CHECK_DO(live555_rtsp_es_mux_ctx->taskScheduler!= NULL, goto end);

	//live555_rtsp_es_mux_ctx->simpleMediaSubsession=
	//		SimpleMediaSubsession::createNew(*usageEnvironment,
	//				live555_rtsp_es_mux_settings_ctx->sdp_mimetype);
	//CHECK_DO(live555_rtsp_es_mux_ctx->simpleMediaSubsession!= NULL, goto end);

	//ret_code= serverMediaSession->addSubsession(
	//		live555_rtsp_es_mux_ctx->simpleMediaSubsession);
	//CHECK_DO(ret_code== True, goto end);

    end_code= STAT_SUCCESS;
 end:
    if(end_code!= STAT_SUCCESS)
    	payloader_upm_rtsp_es_mux_close((proc_ctx_t**)&payloader_upm_rtsp_es_mux_ctx);
	return (proc_ctx_t*)payloader_upm_rtsp_es_mux_ctx;
}

/**
 * Implements the proc_if_s::close callback.
 * See .proc_if.h for further details.
 */
static void payloader_upm_rtsp_es_mux_close(proc_ctx_t **ref_proc_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_close \n");
	payloader_upm_rtsp_es_mux_ctx_t *payloader_upm_rtsp_es_mux_ctx= NULL;
	LOG_CTX_INIT(NULL);
	LOGD(">>%s\n", __FUNCTION__); //comment-me

	if(ref_proc_ctx== NULL)
		return;

	if((payloader_upm_rtsp_es_mux_ctx= (payloader_upm_rtsp_es_mux_ctx_t*)*ref_proc_ctx)!=
			NULL) {
		LOG_CTX_SET(((proc_ctx_t*)payloader_upm_rtsp_es_mux_ctx)->log_ctx);

		/* Release settings */
		payloader_upm_rtsp_es_mux_settings_ctx_deinit(
				&payloader_upm_rtsp_es_mux_ctx->payloader_upm_rtsp_es_mux_settings_ctx,
				LOG_CTX_GET());

		// Reserved for future use: release other new variables here...

		/* Release context structure */
		free(payloader_upm_rtsp_es_mux_ctx);
		*ref_proc_ctx= NULL;
	}
	LOGD("<<%s\n", __FUNCTION__); //comment-me
}

/**
 * Implements the proc_if_s::process_frame callback.
 * See .proc_if.h for further details.
 */
static int payloader_upm_rtsp_es_mux_process_frame(proc_ctx_t *proc_ctx,
		fifo_ctx_t* iput_fifo_ctx, fifo_ctx_t* oput_fifo_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_process_frame \n");
	int ret_code, end_code= STAT_ERROR;
	payloader_upm_rtsp_es_mux_ctx_t *payloader_upm_rtsp_es_mux_ctx= NULL; //Do not release
	proc_frame_ctx_t *proc_frame_ctx= NULL;
	size_t fifo_elem_size= 0;
	SimpleMediaSubsession *simpleMediaSubsession= NULL; //Do not release
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(iput_fifo_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(oput_fifo_ctx!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Get multiplexer context */
	payloader_upm_rtsp_es_mux_ctx= (payloader_upm_rtsp_es_mux_ctx_t*)proc_ctx;

	/* Get input packet from FIFO buffer */
	ret_code= fifo_get(iput_fifo_ctx, (void**)&proc_frame_ctx, &fifo_elem_size);
	CHECK_DO(ret_code== STAT_SUCCESS || ret_code== STAT_EAGAIN, goto end);
	if(ret_code== STAT_EAGAIN) {
		/* This means FIFO was unblocked, just go out with EOF status */
		end_code= STAT_EOF;
		goto end;
	}

	/* Deliver frame to payloader's "framed source" */
	//simpleMediaSubsession= (SimpleMediaSubsession*)
	//				live555_rtsp_es_mux_ctx->simpleMediaSubsession;
	//CHECK_DO(simpleMediaSubsession!= NULL, goto end);
	//simpleMediaSubsession->deliverFrame(&proc_frame_ctx);

	//end_code= STAT_SUCCESS;
end:
	/* If 'deliverFrame()' method did not consume the frame (frame pointer
	 * was not set to NULL), we must release frame and schedule to avoid
	 * CPU-consuming closed loops.
	 */
	if(proc_frame_ctx!= NULL) {
		proc_frame_ctx_release(&proc_frame_ctx);
		schedule();
	}
	return end_code;
}

/**
 * Implements the proc_if_s::rest_put callback.
 * See .proc_if.h for further details.
 */
static int payloader_upm_rtsp_es_mux_rest_put(proc_ctx_t *proc_ctx, const char *str)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_rest_put \n");
	int flag_is_query, end_code= STAT_ERROR;
	payloader_upm_rtsp_es_mux_ctx_t *payloader_upm_rtsp_es_mux_ctx= NULL;
	volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
			payloader_upm_rtsp_es_mux_settings_ctx= NULL;
	char *sdp_mimetype_str= NULL, *rtp_timestamp_freq_str= NULL,
			*bit_rate_estimated_str= NULL;
	cJSON *cjson_rest= NULL, *cjson_aux= NULL;
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(str!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	/* Get FFmpeg video encoder settings contexts */
	payloader_upm_rtsp_es_mux_ctx= (payloader_upm_rtsp_es_mux_ctx_t*)proc_ctx;
	payloader_upm_rtsp_es_mux_settings_ctx=
			&payloader_upm_rtsp_es_mux_ctx->payloader_upm_rtsp_es_mux_settings_ctx;

	/* **** PUT specific m2v video encoder settings **** */

	/* Guess string representation format (JSON-REST or Query) */
	//LOGD("'%s'\n", str); //comment-me
	flag_is_query= (str[0]=='{' && str[strlen(str)-1]=='}')? 0: 1;

	/* **** Parse RESTful string to get settings parameters **** */

	if(flag_is_query== 1) {

		/* 'sdp_mimetype' */
		sdp_mimetype_str= uri_parser_query_str_get_value("sdp_mimetype", str);
		if(sdp_mimetype_str!= NULL) {
			char *sdp_mimetype;
			CHECK_DO(strlen(sdp_mimetype_str)> 0,
					end_code= STAT_EINVAL; goto end);

			/* Allocate new session name */
			sdp_mimetype= strdup(sdp_mimetype_str);
			CHECK_DO(sdp_mimetype!= NULL, goto end);

			/* Release old session name and set new one */
			if(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype!= NULL)
				free(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype);
			payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype= sdp_mimetype;
		}

		/* 'rtp_timestamp_freq' */
		rtp_timestamp_freq_str= uri_parser_query_str_get_value(
				"rtp_timestamp_freq", str);
		if(rtp_timestamp_freq_str!= NULL)
			payloader_upm_rtsp_es_mux_settings_ctx->rtp_timestamp_freq=
					atoll(rtp_timestamp_freq_str);
	} else {

		/* In the case string format is JSON-REST, parse to cJSON structure */
		cjson_rest= cJSON_Parse(str);
		CHECK_DO(cjson_rest!= NULL, goto end);

		/* 'sdp_mimetype' */
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "sdp_mimetype");
		if(cjson_aux!= NULL) {
			char *sdp_mimetype;
			CHECK_DO(strlen(cjson_aux->valuestring)> 0,
					end_code= STAT_EINVAL; goto end);

			/* Allocate new session name */
			sdp_mimetype= strdup(cjson_aux->valuestring);
			CHECK_DO(sdp_mimetype!= NULL, goto end);

			/* Release old session name and set new one */
			if(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype!= NULL)
				free(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype);
			payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype= sdp_mimetype;
		}

		/* 'rtp_timestamp_freq' */
		cjson_aux= cJSON_GetObjectItem(cjson_rest, "rtp_timestamp_freq");
		if(cjson_aux!= NULL)
			payloader_upm_rtsp_es_mux_settings_ctx->rtp_timestamp_freq=
					cjson_aux->valuedouble;
	}

	/* Finally that we have new settings parsed, reset MUXER */
	// Reserved for future use

	end_code= STAT_SUCCESS;
end:
	if(sdp_mimetype_str!= NULL)
		free(sdp_mimetype_str);
	if(rtp_timestamp_freq_str!= NULL)
		free(rtp_timestamp_freq_str);
	if(bit_rate_estimated_str!= NULL)
		free(bit_rate_estimated_str);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	return end_code;
}

/**
 * Implements the proc_if_s::rest_get callback.
 * See .proc_if.h for further details.
 */
static int payloader_upm_rtsp_es_mux_rest_get(proc_ctx_t *proc_ctx,
		const proc_if_rest_fmt_t rest_fmt, void **ref_reponse)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_rest_get \n");
	int end_code= STAT_ERROR;
	payloader_upm_rtsp_es_mux_ctx_t *payloader_upm_rtsp_es_mux_ctx= NULL;
	volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
			payloader_upm_rtsp_es_mux_settings_ctx= NULL;
	cJSON *cjson_rest= NULL/*, *cjson_settings= NULL // Not used*/;
	cJSON *cjson_aux= NULL; // Do not release
	LOG_CTX_INIT(NULL);

	/* Check arguments */
	CHECK_DO(proc_ctx!= NULL, return STAT_ERROR);
	CHECK_DO(rest_fmt< PROC_IF_REST_FMT_ENUM_MAX, return STAT_ERROR);
	CHECK_DO(ref_reponse!= NULL, return STAT_ERROR);

	LOG_CTX_SET(proc_ctx->log_ctx);

	*ref_reponse= NULL;

	/* Create cJSON tree root object */
	cjson_rest= cJSON_CreateObject();
	CHECK_DO(cjson_rest!= NULL, goto end);

	/* JSON string to be returned:
	 * {
	 *     // "settings":{}, //RAL: Do not expose in current implementation!
	 *     "sdp_mimetype":string,
	 *     "rtp_timestamp_freq":number
	 *     ... // Reserved for future use
	 * }
	 */

	/* Get FFmpeg video encoder settings contexts */
	payloader_upm_rtsp_es_mux_ctx= (payloader_upm_rtsp_es_mux_ctx_t*)proc_ctx;
	payloader_upm_rtsp_es_mux_settings_ctx=
			&payloader_upm_rtsp_es_mux_ctx->payloader_upm_rtsp_es_mux_settings_ctx;

	/* **** GET specific ES-MUXER settings **** */

	/* Create cJSON settings object */ // Not used
	//cjson_settings= cJSON_CreateObject();
	//CHECK_DO(cjson_settings!= NULL, goto end);

	/* Attach settings object to REST response */ // Not used
	//cJSON_AddItemToObject(cjson_rest, "settings", cjson_settings);
	//cjson_settings= NULL; // Attached; avoid double referencing

	/* **** Attach data to REST response **** */

	/* 'sdp_mimetype' */
	cjson_aux= cJSON_CreateString(
			payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "sdp_mimetype", cjson_aux);

	/* 'rtp_timestamp_freq' */
	cjson_aux= cJSON_CreateNumber((double)
			payloader_upm_rtsp_es_mux_settings_ctx->rtp_timestamp_freq);
	CHECK_DO(cjson_aux!= NULL, goto end);
	cJSON_AddItemToObject(cjson_rest, "rtp_timestamp_freq", cjson_aux);

	// Reserved for future use
	/* Example:
	 * cjson_aux= cJSON_CreateNumber((double)payloader_upm_rtsp_es_mux_ctx->var1);
	 * CHECK_DO(cjson_aux!= NULL, goto end);
	 * cJSON_AddItemToObject(cjson_rest, "var1_name", cjson_aux);
	 */

	// Reserved for future use: set other data values here...

	/* Format response to be returned */
	switch(rest_fmt) {
	case PROC_IF_REST_FMT_CHAR:
		/* Print cJSON structure data to char string */
		*ref_reponse= (void*)CJSON_PRINT(cjson_rest);
		CHECK_DO(*ref_reponse!= NULL && strlen((char*)*ref_reponse)> 0,
				goto end);
		break;
	case PROC_IF_REST_FMT_CJSON:
		*ref_reponse= (void*)cjson_rest;
		cjson_rest= NULL; // Avoid double referencing
		break;
	default:
		goto end;
	}

	end_code= STAT_SUCCESS;
end:
	//if(cjson_settings!= NULL) // Not used
	//	cJSON_Delete(cjson_settings);
	if(cjson_rest!= NULL)
		cJSON_Delete(cjson_rest);
	return end_code;
}

static int payloader_upm_rtsp_es_mux_settings_ctx_init(
		volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
		payloader_upm_rtsp_es_mux_settings_ctx, log_ctx_t *log_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_settings_ctx_init \n");
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(payloader_upm_rtsp_es_mux_settings_ctx!= NULL, return STAT_ERROR);

	payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype= strdup("n/a");
	CHECK_DO(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype!= NULL,
			return STAT_ERROR);
	payloader_upm_rtsp_es_mux_settings_ctx->rtp_timestamp_freq= 9000;

	return STAT_SUCCESS;
}

static void payloader_upm_rtsp_es_mux_settings_ctx_deinit(
		volatile payloader_upm_rtsp_es_mux_settings_ctx_t *
		payloader_upm_rtsp_es_mux_settings_ctx, log_ctx_t *log_ctx)
{
printf("=====================%d\n", __LINE__); fflush(stdout); //FIXME!!
printf("[payloader_upm_rtsp.cpp] payloader_upm_rtsp_es_mux_settings_ctx_deinit \n");
	LOG_CTX_INIT(log_ctx);

	/* Check arguments */
	CHECK_DO(payloader_upm_rtsp_es_mux_settings_ctx!= NULL, return);

	/* Release ES-MUXER specific settings */
	if(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype!= NULL) {
		free(payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype);
		payloader_upm_rtsp_es_mux_settings_ctx->sdp_mimetype= NULL;
	}
}

