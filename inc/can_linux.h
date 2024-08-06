#ifndef CAN_H
#define CAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stdint.h"

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "sys/uio.h"

#include <linux/can.h>
#include <linux/can/raw.h>

#include "J73.h"

int can0_init();
int can0_transmit(uint32_t id, uint8_t dlc, uint8_t* buf);
void can0_set_filter(uint32_t can_id, uint32_t can_mask);
int can0_receive(uint32_t* get_id, uint8_t* get_dlc, uint8_t* buf);
int can0_deinit();



#endif
