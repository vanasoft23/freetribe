#ifndef DSP_PROTOCOL_H
#define DSP_PROTOCOL_H

#include <stdint.h>

#define DSP_MSG_START 0xf0u
#define DSP_MSG_MAX_PAYLOAD 0xffu

#define DSP_MODULE_PARAM_KEY_PAYLOAD_SIZE   4u
#define DSP_MODULE_PARAM_VALUE_PAYLOAD_SIZE 8u
#define DSP_SYSTEM_PORT_STATE_PAYLOAD_SIZE  6u
#define DSP_SYSTEM_PROFILE_PAYLOAD_SIZE     8u

enum e_message_type {
    MSG_TYPE_MODULE,
    MSG_TYPE_SYSTEM
};

enum e_module_msg_id {
    MODULE_GET_PARAM_VALUE,
    MODULE_SET_PARAM_VALUE,
    MODULE_PARAM_VALUE,
    MODULE_GET_PARAM_NAME,
    MODULE_PARAM_NAME
};

enum e_system_msg_id {
    SYSTEM_CHECK_READY,
    SYSTEM_READY,
    SYSTEM_GET_PORT_STATE,
    SYSTEM_SET_PORT_STATE,
    SYSTEM_PORT_STATE,
    SYSTEM_GET_PROFILE,
    SYSTEM_PROFILE,
};

static inline void dsp_protocol_write_u16(uint8_t *dest, uint16_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
}

static inline void dsp_protocol_write_u32(uint8_t *dest, uint32_t value) {
    dest[0] = (uint8_t)(value & 0xffu);
    dest[1] = (uint8_t)((value >> 8) & 0xffu);
    dest[2] = (uint8_t)((value >> 16) & 0xffu);
    dest[3] = (uint8_t)((value >> 24) & 0xffu);
}

static inline void dsp_protocol_write_i32(uint8_t *dest, int32_t value) {
    dsp_protocol_write_u32(dest, (uint32_t)value);
}

static inline uint16_t dsp_protocol_read_u16(const uint8_t *src) {
    return (uint16_t)(((uint16_t)src[1] << 8) | src[0]);
}

static inline uint32_t dsp_protocol_read_u32(const uint8_t *src) {
    return ((uint32_t)src[3] << 24)
        | ((uint32_t)src[2] << 16)
        | ((uint32_t)src[1] << 8)
        | src[0];
}

static inline int32_t dsp_protocol_read_i32(const uint8_t *src) {
    return (int32_t)dsp_protocol_read_u32(src);
}

#endif
