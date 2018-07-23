
#ifdef __cplusplus
extern "C" {
#endif

/* **** Definitions **** */

/* Forward definitions */
typedef struct proc_if_s proc_if_t;

/* **** prototypes **** */

/**
 * Processor interface implementing the wrapper of the live555 RTSP
 * multiplexer.
 */
extern const proc_if_t proc_if_payloader_upm_rtsp_mux;

/**
 * Processor interface implementing the wrapper of the live555 RTSP
 * de-multiplexer.
 */
extern const proc_if_t proc_if_payloader_upm_rtsp_dmux;

#ifdef __cplusplus
} //extern "C"
#endif
