#ifndef DHT_22_H__
#define DHT_22_H__


#include "nrf_gpio.h"
#include "nrf_timer.h"

#define DHTPIN                  NRF_GPIO_PIN_MAP(0, 7)
#define MAXTIMINGS              80
#define HIGH                    1
#define LOW                     0
#define DHT22_READ_INTERVAL     APP_TIMER_TICKS(2000)

void read_dht22_dat();

uint16_t getTemp();

uint16_t getHumid();

int32_t expectPulse(bool level);

static void init_dht_timer();

static void dht22_timeout_handler(void * p_context);

static void dht22_cooldown_start(void);
#endif