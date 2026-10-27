#pragma once
#ifdef __cplusplus
extern "C"
{
#endif

#include "dl_middleware.h"

extern result_t blehid_init( void );
extern result_t blehid_enable( void );
extern result_t blehid_disable( void );
extern void blehid_set_report_descriptor( const uint8_t * p_desc_report, uint16_t len );
extern bool blehid_send_report( uint8_t report_id, const uint8_t * p_key_pattern, uint8_t key_pattern_len );

#ifdef __cplusplus
}
#endif
