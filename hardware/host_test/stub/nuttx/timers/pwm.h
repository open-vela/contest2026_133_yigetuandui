#ifndef _STUB_PWM_H
#define _STUB_PWM_H
#include <stdint.h>
typedef uint16_t ub16_t;
struct pwm_info_s { uint32_t frequency; ub16_t duty; };
#define PWMIOC_SETCHARACTERISTICS 0x1a01
#define PWMIOC_START 0x1a02
#define PWMIOC_STOP  0x1a03
#endif
