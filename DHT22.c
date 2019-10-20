#include "dht22.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "app_timer.h"
#include "sdk_common.h"
#include <string.h>

static uint8_t dht22_dat[5] = {0, 0, 0, 0, 0};
static uint16_t temperature = 0;
static uint16_t humidity = 0;
static uint8_t isCooldown = false;
static uint8_t initialized = false;

APP_TIMER_DEF(m_dht22_timer_id);

int32_t cycles[80];
void read_dht22_dat() {
  if(isCooldown == true){
    return; // if sensor is in cooldown, don't read it again.
  }
  else{
    uint8_t laststate = 1;
    uint8_t counter = 0;
    uint8_t j = 0, i;
    uint8_t lowCount = 0;

    nrf_gpio_cfg_input(DHTPIN, NRF_GPIO_PIN_PULLUP);
    laststate = nrf_gpio_pin_read(DHTPIN);
    printf("%d \n", laststate);

    dht22_dat[0] = dht22_dat[1] = dht22_dat[2] = dht22_dat[3] = dht22_dat[4] = 0;

    nrf_gpio_cfg_output(DHTPIN);
    nrf_gpio_pin_write(DHTPIN, 0);
    nrf_delay_ms(1);

    nrf_gpio_pin_write(DHTPIN, 1);
    nrf_delay_us(40);

    nrf_gpio_cfg_input(DHTPIN, NRF_GPIO_PIN_PULLUP);
    
    int32_t low80 = expectPulse(LOW);
    int32_t high80 = expectPulse(HIGH);
    
    // Timing critical portion. Grabs 80 measurements.
    // The reference low followed by the shorter 0
    // or higher 1. Analyzed after obtaining.
    for (i = 0; i < MAXTIMINGS; i += 2) {

      cycles[i] = expectPulse(LOW);
      cycles[i+1] = expectPulse(HIGH);

    }

    // Analyze the 80 measurements retrieved above.
    // high cycle < low cycle = 0 bit
    // high cycle > low cycle = 1 bit
    // Data comes in as highest bit first
    for(i = 0; i < 40; ++i){
      uint32_t lowCycles = cycles[2 * i];
      uint32_t highCycles = cycles[2 * i + 1];

      if(lowCycles == -1 || highCycles == -1) {
        break;
      }
      dht22_dat[i/8] <<= 1;
      #ifdef LOGGING
      if(i < 4){
        printf("lowCycles: %d\n", lowCycles);
        printf("highCycles: %d\n\n", highCycles);
      }
      #endif
      if(highCycles > lowCycles) {
        dht22_dat[i/8] |= 1;
      }
    }

    #ifdef LOGGING
    printf("dht22_dat[4]: %d\n", dht22_dat[4]);
    printf("dht22_dat[3]: %d\n", dht22_dat[3]);
    printf("dht22_dat[2]: %d\n", dht22_dat[2]);
    printf("dht22_dat[1]: %d\n", dht22_dat[1]);
    printf("dht22_dat[0]: %d\n", dht22_dat[0]);
    printf("checksum: %X\n", (dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF);
    printf("dht22_dat[4]: %X\n", dht22_dat[4]);
    #endif

    /*
     *  Checks to make sure that the checksum is correct so we know the data is good.
     *  Once we know it's good then we store it into the temperature and humidity variables
     *  to be retreived by the service.
     */
    if (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) {

      temperature = ((uint16_t)(dht22_dat[2] & 0x7F)) << 8 | dht22_dat[3];

      humidity = ((uint16_t)dht22_dat[0] << 8) | dht22_dat[1];
    } else {
      printf("Data not good, skip\n");
    }

    // Start a cooldown timer for 2 seconds. The sensor shouldn't be read more than every 2 seconds.
    init_dht_timer();

    dht22_cooldown_start();
  }
}

// Gets temperature. If sensor is in cooldown, it sends the stored data.
uint16_t getTemp(){
  return temperature;
}

// Gets temperature. If sensor is in cooldown, it sends the stored data.
uint16_t getHumid(){
  return humidity;
}

// returns the number of microseconds for each pulse
int32_t expectPulse(bool level) {
  uint32_t counter = 0;
  while (nrf_gpio_pin_read(DHTPIN) == level) {
    nrf_delay_us(1);
    if (counter++ == 1000) {
      return -1;
    }
  }
  return counter;
}

static void init_dht_timer() {
  if(initialized == false){
    // Initialize timer module.
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    // Create timers.
    err_code = app_timer_create(&m_dht22_timer_id,
                                APP_TIMER_MODE_SINGLE_SHOT,
                                dht22_timeout_handler);
    APP_ERROR_CHECK(err_code);
    initialized = true;
  }
}

static void dht22_timeout_handler(void * p_context){
  UNUSED_PARAMETER(p_context);
  isCooldown = false;
}

static void dht22_cooldown_start(void){
  ret_code_t err_code;
  isCooldown = true;
  err_code = app_timer_start(m_dht22_timer_id, DHT22_READ_INTERVAL, NULL);
  APP_ERROR_CHECK(err_code);
}