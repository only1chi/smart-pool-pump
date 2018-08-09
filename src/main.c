/*
 * main.c
 *
 *  Created on: Jun 13, 2017
 *      Author: chiz
 */

#include <stdio.h>
#include "smartpooltmr.h"
#include "mgos_http_server.h"
#include "mgos_wifi.h"

/* Global variables	*/
char *msg = "This is a message";
struct mgos_config *device_cfg;
dpt_system_t system_data;
bool gADCConfigured = false;
bool gTimeSynced = false;
bool gSaveSchedule = false;
bool gTimerEnable = false;
int gPumpScheduleStartTime = 0;
int gPumpScheduleStopTime = 0;
uint8_t gPumpState = false;

/* Request URI for getting IP as recognized from an external location */
static char *getIP_url = "https://api.ipify.org";

/* Definitions 	*/
#define MGOS_F_RELOAD_CONFIG MG_F_USER_5

static struct device_settings s_settings = {"ssid", "password"};

/*
 * Handle save routine. This puts the grabs the ssid/password from the
 * web page and calls
 *
 */
static void handle_save(struct mg_connection *nc, struct http_message *hm) {
  // struct sys_config_wifi_sta device_cfg_sta;
  struct mgos_config_wifi_sta device_cfg_sta;

  memset(&device_cfg_sta, 0, sizeof(device_cfg_sta));

  // Get form variables and store settings values
  LOG(LL_INFO, ("Getting variables from %s", (&hm->uri)->p));
  LOG(LL_DEBUG, ("mg_get_http_var hm->body->p: %s hm->body.len: %d\n", (&hm->body)->p, hm->body.len));
  mg_get_http_var(&hm->body, "ssid", s_settings.ssid, sizeof(s_settings.ssid));
  mg_get_http_var(&hm->body, "password", s_settings.pswd, sizeof(s_settings.pswd));

  // Send response... trying to reload index page
  mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));

  /*
  // Using this peice of code didn't cause a crash
  LOG(LL_DEBUG, ("Rebooting now... "));
  mg_send_head(nc, 200, 0, "Connection: close\r\nContent-Type: application/json");
  nc->flags |= (MG_F_SEND_AND_CLOSE | MGOS_F_RELOAD_CONFIG);
   */

  /*
  // Use chunked encoding in order to avoid calculating Content-Length
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  mg_send_http_chunk(nc, "", 0);
  nc->flags |= MG_F_SEND_AND_CLOSE;
  */

  LOG(LL_INFO, ("WiFi Station: ssid = %s; password = %s", s_settings.ssid, s_settings.pswd));
  device_cfg_sta.ssid = s_settings.ssid;
  device_cfg_sta.pass = s_settings.pswd;
  LOG(LL_INFO, ("Device Config WiFi Station: ssid = %s; password = %s", device_cfg_sta.ssid, device_cfg_sta.pass));

  becomeStation(&device_cfg_sta);
}

/*
 * handle_get_cpu_usage. This reports CPU usage, and also provides connection status
 *
 */
static void handle_get_cpu_usage(struct mg_connection *nc) {

	int cpu_usage = 0;
  char sta_ip[16];
	struct mbuf fb;
	struct json_out fout = JSON_OUT_MBUF(&fb);
	mbuf_init(&fb, 256);
  memset(sta_ip, 0, sizeof(sta_ip));

	getStationIP(sta_ip);
	cpu_usage = ((double)mgos_get_free_heap_size()/(double)mgos_get_heap_size()) * 100.0;
	json_printf(&fout, STATUS_FMT, cpu_usage, mgos_wifi_get_connected_ssid(), sta_ip);
    mbuf_trim(&fb);

	struct mg_str f = mg_mk_str_n(fb.buf, fb.len);	/* convert to string	*/

	// Use chunked encoding in order to avoid calculating Content-Length
	mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
	//	mg_printf_http_chunk(nc, f.p);
	mg_send_http_chunk(nc, f.p, f.len);
	mg_send_http_chunk(nc, "", 0);
	nc->flags |= MG_F_SEND_AND_CLOSE;

	LOG(LL_INFO, ("%s\n", f.p));

	mbuf_free(&fb);
}

/*
 * SSI handler for SSI events...
 * Don't need this now... May delete
 *
 */
static void handle_ssi_call(struct mg_connection *nc, const char *param) {
  if (strcmp(param, "ssid") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.ssid);
  }
  else if (strcmp(param, "password") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.pswd);
  }
}

/*
 * Event Handler for http post events "/save"
 * 
 */
static void http_post_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  printf("Event: %d, uri: %s\n", ev, hm->uri.p);
  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/save") == 0) {
        handle_save(nc, hm);
      }
      break;
    case MG_EV_SSI_CALL:
      handle_ssi_call(nc, ev_data);
      break;
    default:
      break;
  }
  (void) user_data;
}


/*
 * Event Handler for http get events
 *
 */
static void http_get_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  printf("Event: %d, uri: %s\n", ev, hm->uri.p);

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/get_cpu_usage") == 0) {
        handle_get_cpu_usage(nc);
      }
      else {
    	mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));
      }
      break;
    case MG_EV_SSI_CALL:
      handle_ssi_call(nc, ev_data);
      break;
    default:
      break;
  }
  (void) user_data;
}


/*
 * Event Handler for http client events
 * 
 * Events. Meaning of event parameter (evp) is given in the comment.
 * #define MG_EV_POLL 0    Sent to each connection on each mg_mgr_poll() call 
 * #define MG_EV_ACCEPT 1  New connection accepted. union socket_address 
 * #define MG_EV_CONNECT 2 connect() succeeded or failed. int * 
 * #define MG_EV_RECV 3    Data has been received. int *num_bytes
 * #define MG_EV_SEND 4    Data has been written to a socket. int *num_bytes 
 * #define MG_EV_CLOSE 5   Connection is closed. NULL 
 * #define MG_EV_TIMER 6 now >= conn->ev_timer_time. double * 
 * 
 */
static void http_client_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm;
	int connect_status;

  LOG(LL_INFO, ("Event: %d\n", ev));


  switch (ev) {
    case MG_EV_CONNECT:
			connect_status = *(int *) ev_data;
      if (connect_status != 0) {
			  LOG(LL_INFO, ("Error connecting to %s: %s\n", getIP_url, strerror(connect_status)));
			}
			else {
			  LOG(LL_INFO, ("Connected to %s: %s\n", getIP_url, strerror(connect_status)));
			}
      break;

    case MG_EV_HTTP_REPLY:
		 	hm = (struct http_message *) ev_data;
			LOG(LL_INFO, ("Response Code:\n %d, %s\n", (int) hm->resp_code, hm->resp_status_msg.p));
			LOG(LL_INFO, ("Got reply:\n%d, %s\n", (int) hm->body.len, hm->body.p));
			memset(system_data.ip, '\0', 16);
			if ((int) hm->body.len < 16) {
				strncpy(system_data.ip, hm->body.p, (int) hm->body.len);
			}	else {
				strncpy(system_data.ip, hm->body.p,16);
			}
			// while (1);
      // nc->flags |= MG_F_SEND_AND_CLOSE;
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      break;
		
    case MG_EV_CLOSE:
			// if (s_exit_flag == 0) {
			  LOG(LL_INFO, ("Connection closed\n"));
			// }
      break;

    default:
      break;
  }
	(void) user_data;
}

/*
 * http_client_connect_event: Configures http client connection
 * net_ev_handler
 *
 * mg_connect_http(mgr, event_handler_t event_handler, const char *url, 
 *                  const char *extra_headers, const char *post_data)
 * 
 * struct mg_connection *mgos_connect_http(const char *addr, mg_event_handler_t,
 *																					void *ud);
 */

static void http_client_connect_event(int ev, void *evd, void *arg) {

  LOG(LL_INFO, ("== Net Event: %d\n", ev));
  if (ev == MGOS_NET_EV_IP_ACQUIRED) {
    LOG(LL_INFO, ("Just got IP!"));
		LOG(LL_INFO, ("Starting http client against %s\n", getIP_url));

    /* initiate an http client connection request	*/
		// mgos_connect_http(getIP_url, http_client_ev_handler, NULL);
		// mg_connect_http(mgos_get_mgr(), http_client_ev_handler, NULL, getIP_url, NULL, NULL);
		mg_connect_http(mgos_get_mgr(), http_client_ev_handler, NULL, getIP_url, NULL, NULL);
	}
 }



/*
 * Become a station connecting to an existing access point.
 */
void becomeStation(struct mgos_config_wifi_sta *device_cfg_sta) {

	mgos_sys_config_set_wifi_sta_ssid(device_cfg_sta->ssid);
	mgos_sys_config_set_wifi_sta_pass(device_cfg_sta->pass);
	mgos_sys_config_set_wifi_sta_enable(true);
	mgos_sys_config_set_mqtt_enable(true);

	LOG(LL_DEBUG, ("Device config set SSID = %s, Password = %s \n", device_cfg_sta->ssid, device_cfg_sta->pass));

	char *err = NULL;
  save_cfg(&mgos_sys_config, &err); /* Writes conf9.json */
	LOG(LL_DEBUG, ("Saving configuration: %s\n", err ? err : "no error"));
  free(err);

	(void)device_cfg_sta;
} // becomeStation


/*
 * report_state. This reports state to the AWS shadow
 *
 */

void report_state(void) {

  mgos_shadow_updatef(0, JSON_SPOOLTIMER_FMT, system_data.pump_voltage, system_data.pump_current, system_data.pump_power,
		  																				system_data.box_temperature, system_data.box_pressure, system_data.manOverride, 
																							system_data.pumpCmd, system_data.timer.tm_start, system_data.timer.tm_stop,
	 																						system_data.schedule.tm_start, system_data.schedule.tm_stop, system_data.ip);
}


/*
 * Main AWS Device Shadow state callback handler. Will get invoked when
 * connection is established or when new versions of the state arrive via one of the topics.
 *
 * CONNECTED event comes with no state.
 *
 *
 *
 */
static void shadow_state_cb(int ev, void *ev_data, void *userdata) {
  struct mg_str data = *(struct mg_str *) ev_data;
	// int result;

  LOG(LL_DEBUG, ("== Shadow Event: %d (%s)", ev, mgos_shadow_event_name(ev)));

  if (ev == MGOS_SHADOW_CONNECTED) {
    report_state();
    return;
  }

  if (ev != MGOS_SHADOW_GET_ACCEPTED &&
      ev != MGOS_SHADOW_UPDATE_DELTA) {
    return;
  }

  if (ev_data == NULL) {
    return;
  }
  
  data = *(struct mg_str *) ev_data;

  LOG(LL_INFO, ("state : %.*s\n",  (int) data.len, data.p));

	/* Update object elements if found 	*/
  if (json_scanf(data.p, data.len, JSON_RELAY_FMT, &system_data.pumpCmd) > 0)
	{
		cmdPumpOnOff(system_data.pumpCmd);
  	LOG(LL_INFO, ("Relay: %d\n", system_data.pumpCmd));
	}

  if (json_scanf(data.p, data.len, JSON_TIMER_FMT, &system_data.timer.tm_start, &system_data.timer.tm_stop) > 0)
	{
		gTimerEnable = true;
	}
	
  if(json_scanf(data.p, data.len, JSON_SCHEDULE_FMT, &system_data.schedule.tm_start, &system_data.schedule.tm_stop) > 0)
	{
		gSaveSchedule = true;
	}

  if (ev == MGOS_SHADOW_UPDATE_DELTA) {
    report_state();
  }
  (void) userdata;                                     
}


/*
 * periodicCallBackHandler: Function called periodically which updates system
 * 							variables. Gets current state of sensors and inputs
 *
 */
static void periodicCallBackHandler(void *arg) {
	struct bmp180_data_t sensor_data;

	/* Get Current System Data	*/
	bmp180_sensor_initialize();
	bmp180_sensor_data(&sensor_data);

	system_data.dc_input_voltage = ADC_ATTEN * (float)mgos_adc_read(ADC_CH5_GPIO)/ADC_MAX_RES;
//	system_data.dc_input_voltage = ADC_ATTEN * (float)adc1_get_voltage(ADC1_CHANNEL_5)/ADC_MAX_RES;

	system_data.box_temperature = (sensor_data.temperature * 1.8) + 32;
	system_data.box_pressure = sensor_data.pressure;		// convert farenheit
	system_data.manOverride = mgos_gpio_read(MANUAL_OVRD_GPIO);
	system_data.upTime = mgos_uptime();

	LOG(LL_DEBUG, ("system_data.dc_input_voltage = %.2f", system_data.dc_input_voltage));
	LOG(LL_DEBUG, ("system_data.upTime = %lf", system_data.upTime));

	/* check pump schedule and turn on pump */
	if (!gTimeSynced) {
		syncTime();
	}

	(void)arg;
}

/*
 * measurePumpTask: Function called periodically which measures ADC voltage, current and power
 *
 */
static void measurePumpTask(void *arg) {
	bool status = false;

	if (!gADCConfigured) {
		ade7912_init(eFRQ4khz);
	}
	else {
		status = ade7912_trigger_capture();
		system_data.pump_power = 0.001* system_data.pump_voltage * system_data.pump_current;

		if (status == true) {
			LOG(LL_DEBUG, ("system_data.pump_current = %.2f", system_data.pump_current));
			LOG(LL_DEBUG, ("system_data.pump_voltage = %.2f", system_data.pump_voltage));
			LOG(LL_DEBUG, ("system_data.pump_power = %.2f\n", system_data.pump_power));
		}
	}

	checkPumpSchedule();	/* Calling this function here so that it can be executed within this task	*/

	(void)arg;
}


/*
 * syncTime: Function synchronizes timer and schedule to local time
 * 					 depends on sntp
 *
 */
void syncTime(void) {

	char dtString[100] = {'\0'};

	struct timezone tzEST;
	struct timeval timeNow;
	gettimeofday(&timeNow, &tzEST);
	struct tm *dateAndTime = localtime(&timeNow.tv_sec);

	if (ISTIMESYNCED(dateAndTime->tm_year)) {
		strftime(dtString, 100, "%a, %x - %I:%M%p", dateAndTime);
		LOG(LL_INFO, ("Local time %s, daylight_savings: %d\n", dtString, dateAndTime->tm_isdst));

		/* Restore schedule/timer information	*/
		getPumpTimer();
		getPumpSchedule();

		gTimeSynced = true;
	}

}


/*
 * checkPumpSchedule: Function determines date and time
 * 					  checking the schedule to see if the pump should be turned on
 *
 */
void checkPumpSchedule(void) {
	struct timezone tzEST;
	struct timeval timeNow;
	gettimeofday(&timeNow, &tzEST);
	struct tm *dateAndTime = localtime(&timeNow.tv_sec);
	time_t local_ts = mktime(dateAndTime);
	
	
	int currentTime = dateAndTime->tm_hour*3600 + dateAndTime->tm_min*60 + dateAndTime->tm_sec;

	/* Ensure timer is enabled */
	if(gTimerEnable) {
		if ((local_ts >= system_data.timer.tm_start) && (local_ts <= system_data.timer.tm_stop)) {
			cmdPumpOnOff(true);
		}
		else {
			cmdPumpOnOff(false);
			gTimerEnable = false;
		}
	}

	if (gSaveSchedule) {
		savePumpSchedule();		// Assumes schedule/time information is in UTC
		gSaveSchedule = false;
 	}

	/* Ensure Schedule is synced	*/
	if(gTimeSynced) {
		if ((currentTime >= gPumpScheduleStartTime) && (currentTime < gPumpScheduleStopTime)) {
			cmdPumpOnOff(true);
		}
		else {
			cmdPumpOnOff(false);
		}
	}
}

/*
 * mgos_app_init: Initializes application
 *
 */
enum mgos_app_init_result mgos_app_init(void) {

	// cs_log_set_level(LL_DEBUG);					// Set log level to debug

	printf ("mgos_app_init Device id is: %s \r\n", mgos_sys_config_get_device_id());

	if (mgos_wifi_validate_sta_cfg(mgos_sys_config_get_wifi_sta(), &msg) == false) {
		LOG(LL_DEBUG, ("%s\n", msg));
	}
	if (mgos_wifi_validate_ap_cfg(mgos_sys_config_get_wifi_ap(), &msg) == false) {
		LOG(LL_DEBUG, ("%s\n", msg));
	}

	LOG(LL_DEBUG, ("registering end points..."));
	mgos_register_http_endpoint("/save", http_post_ev_handler, NULL);
	mgos_register_http_endpoint("/get_cpu_usage", http_get_ev_handler, NULL);

	/* Initialize GPIO	*/
	deviceInit();
	system_data.pumpCmd = false;
	cmdPumpOnOff(system_data.pumpCmd);

	/* Initialize bmp180 sensor and i2c peripheral	*/
	LOG(LL_INFO, ("bmp180 sensor initialization result = %d \n", bmp180_sensor_initialize()));
	LOG(LL_DEBUG, ("setup timer call back : %d msec \n", CALLBACK_PERIOD));
	mgos_set_timer(CALLBACK_PERIOD, true, periodicCallBackHandler, NULL);

	/* Initialize ade7912 a/d converter and HSPI peripheral	*/
	gADCConfigured = ade7912_init(eFRQ4khz);
	LOG(LL_DEBUG, ("ade7912 initialization result: %d [1=SUCCESS, 0=FAIL] \n", gADCConfigured));
	LOG(LL_DEBUG, ("setup timer call back : %d msec \n", CALLBACK_PERIOD));
	mgos_set_timer(990, true, measurePumpTask, NULL);

	/* Register AWS shadow callback handler	*/
  mgos_event_add_group_handler(MGOS_SHADOW_BASE, shadow_state_cb, NULL);

	/* Register http_client call back	*/
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, http_client_connect_event, NULL);

	LOG(LL_INFO, ("MGOS_APP_INIT_SUCCESS"));
  return MGOS_APP_INIT_SUCCESS;
}

