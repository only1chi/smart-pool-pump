/*
 * system.h
 *
 *  Created on: Jun 13, 2017
 *      Author: chiz
 */

#ifndef SRC_SMARTPOOLTMR_H_
#define SRC_SMARTPOOLTMR_H_

#include "common/cs_dbg.h"
#include "common/mbuf.h"
#include "common/json_utils.h"
#include "common/platform.h"
#include "frozen/frozen.h"
#include "fw/src/mgos_hal.h"

// include from mongoose-os/fw/include
#include "fw/include/mgos_app.h"
#include "fw/include/mgos_gpio.h"
#include "fw/include/mgos_net.h"
#include "fw/include/mgos_timers.h"
#include "fw/include/mgos_sys_config.h"

// include from libraries
#include "mgos_adc.h"
#include "mgos_i2c.h"
#include "mgos_mongoose_internal.h"     // This also includes mgos_mongoose.h
#include "mgos_shadow.h"
#include "mgos_spi.h"
#include "mgos_wifi.h"

// include from esp-idf
#include "driver/adc.h"

// include from src
#define BMP180_API			                // This definition needed by bmp180.h
#include "bmp180.h"
#include "ade7912.h"

// Definitions
#define YEAR_NOW				    2018
#define YEAR_1900				    1900
#define TIME_ZONE_EST			  (int)-4
#define ISTIMESYNCED(x)			((x+YEAR_1900) >= YEAR_NOW) ? 1 : 0
#define SSID_SIZE 				  32 					// Maximum SSID size
#define PASSWORD_SIZE 			64 					// Maximum password size

#define RELAY_DRV_GPIO			21					// output
#define RELAY_FDBK_GPIO			4					  // input
#define ADC_DRDY_GPIO			  22					// input, interrupt enabled
#define ADC_CH5_GPIO			  33					// Analog Input
#define MANUAL_OVRD_GPIO		34					// input
#define GPIO_LED_YEL_OUT		27
#define GPIO_LED_GRN_OUT		12

#define CALLBACK_PERIOD 		2000 				// Frequency a timer call back is repeated

// Define JSON formats for different data outputs
#define STATUS_FMT          "{cpu: %d, ssid: %Q, ip: %Q}"
#define JSON_SPOOLTIMER_FMT "{volt: %.2f, cur: %.2f, pow: %.2f, temp: %.1f, press: %.1f, auto: %B, relay: %d, \
                              timer: { start: %d, stop: %d }, schedule: { start: %d, stop: %d }, ip: %Q }"

#define JSON_UPOOLTIMER_FMT "{volt: %.2f, cur: %.2f, pow: %.2f, temp: %.1f, press: %.1f, auto: %B,  ip: %Q }"
#define JSON_RELAY_FMT      "{relay: %d}"
#define JSON_TIMER_FMT      "{ timer: { start: %d, stop: %d}}"
#define JSON_SCHEDULE_FMT   "{ schedule: { start: %d, stop: %d}}"

#define ANALOG_VDIV 				(1.0/0.3125)
#define NODEMCU_VREF 				3.3
#define ADC_ATTEN					  2
#define ADC_MAX_RES					(float)4095

#define HDWR_VERSION				0x3033
#define SFWR_VERSION				0x3032

#define NUM_WEEKDAYS				7
#define OUTPUTBUFSIZE				16


//static const char *getIP_url = "https://api.ipify.org?format=text";


struct device_settings {
  char ssid[SSID_SIZE];
  char pswd[PASSWORD_SIZE];
};

struct bmp180_data_t {
	float temperature;
	float pressure;
};

//
// Create dpt_system types
//

typedef enum
{
	eRELAY_OFF = 0,
	eRELAY_ON = 1,
	eRELAY_FAIL = 2
} relay_status_t;

typedef enum
{
	ePUMP_OFF = 0,
	ePUMP_ON = 1
} pump_status_t;

typedef enum
{
	eSYS_NORMAL = 0,
	eMANUAL_OVRD = 1,
	eSYS_FAULT = 2
} system_state_t;

// structure for Pool pump timer
typedef struct tmTimer {
	int tm_start;
	int tm_stop;
} tmTimer_t;

// System Data Structure
typedef struct
{
    uint8_t 	pumpCmd;
    float  		box_temperature;
    float	 	  box_pressure;
    float 		pump_voltage;
    float 		pump_current;
    float	    pump_power;
    float 		dc_input_voltage;
    float 		filter_pressure;
    double 		upTime;
    int32_t		pump_offset_v;
    int32_t		pump_offset_i;
    uint8_t 	rly1Status;
    uint8_t 	rly2Status;
    uint8_t 	pumpFdbkState;
    uint8_t 	manOverride;
    uint8_t 	pumpStatus;
    uint8_t		useSchedule;
    uint8_t 	sysState;
    uint8_t		swVersion[2];
    uint8_t		hwVersion[2];
    tmTimer_t timer;
    tmTimer_t schedule;
    char     ip[16];
} dpt_system_t;

// Function declarations
s32 bmp180_sensor_initialize(void);
void bmp180_sensor_data(struct bmp180_data_t *bmp180_sensor_data);
void delay_ms(u32 msec);
void deviceInit(void);
uint8_t cmdPumpOnOff(relay_status_t cmd);
void user_init(void);
void syncTime(void);

void becomeStation(struct mgos_config_wifi_sta *device_cfg_sta); 
void report_state(void);
void savePumpSchedule(void);
void checkPumpSchedule(void);
void getPumpSchedule(void);
void savePumpTimer(void);
void getPumpTimer(void);
void getStationIP(char *sta_ip);

#endif /* SRC_SMARTPOOLTMR_H_ */
