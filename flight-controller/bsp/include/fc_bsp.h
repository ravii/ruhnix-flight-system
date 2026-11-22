/*
 * Board support interface for the flight controller.
 * The application (app/) only talks to hardware through these functions;
 * each board under bsp/<board>/ provides an implementation.
 */
#ifndef FC_BSP_H
#define FC_BSP_H

#include <stddef.h>
#include <stdint.h>

#include "uav/protocol/messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Clocks, GPIO, peripherals. Called once before the scheduler starts. */
void bsp_init(void);

/* Monotonic microsecond timestamp. */
uint64_t bsp_time_us(void);

/* Reads the latest IMU sample. Returns 0 on success. */
int bsp_imu_read(uav_msg_imu_t *out);

/* Writes motor outputs, each in [0, 1]. */
void bsp_motors_write(const float *outputs, size_t count);

/* Non-blocking write to the companion-computer link. Returns bytes accepted. */
size_t bsp_link_write(const uint8_t *data, size_t len);

void bsp_led_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_BSP_H */
