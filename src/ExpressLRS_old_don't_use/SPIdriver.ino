/*
   SPI.cpp

    Created on: Mar 3, 2017
        Author: kolban
*/

#include "SPIdriver.h"
#include <driver/spi_master.h>
#include <esp_log.h>
#include "sdkconfig.h"

//#define DEBUG 1

static const char* LOG_TAG = "iSPI";
/**
   @brief Construct an instance of the class.

   @return N/A.
*/
IRAM_ATTR iSPI::iSPI() {
  m_handle = nullptr;
  m_host   = VSPI_HOST;
}


/**
   @brief Class instance destructor.
*/
IRAM_ATTR iSPI::~iSPI() {
  ESP_LOGI(LOG_TAG, "... Removing device.");
  ESP_ERROR_CHECK(::spi_bus_remove_device(m_handle));

  ESP_LOGI(LOG_TAG, "... Freeing bus.");
  ESP_ERROR_CHECK(::spi_bus_free(m_host));
}

/**
   @brief Initialize SPI.

   @param [in] mosiPin Pin to use for MOSI %SPI function.
   @param [in] misoPin Pin to use for MISO %SPI function.
   @param [in] clkPin Pin to use for CLK %SPI function.
   @param [in] csPin Pin to use for CS %SPI function.
   @return N/A.
*/
void IRAM_ATTR iSPI::init(int mosiPin, int misoPin, int clkPin, int csPin) {
  ESP_LOGD(LOG_TAG, "init: mosi=%d, miso=%d, clk=%d, cs=%d", mosiPin, misoPin, clkPin, csPin);

  spi_bus_config_t bus_config;
  bus_config.sclk_io_num     = clkPin;  // CLK
  bus_config.mosi_io_num     = mosiPin; // MOSI
  bus_config.miso_io_num     = misoPin; // MISO
  bus_config.quadwp_io_num   = -1;      // Not used
  bus_config.quadhd_io_num   = -1;      // Not used
  bus_config.max_transfer_sz = 0;       // 0 means use default.
  bus_config.flags           = (SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO);

  ESP_LOGI(LOG_TAG, "... Initializing bus; host=%d", m_host);

  esp_err_t errRc = ::spi_bus_initialize(
                      m_host,
                      &bus_config,
                      1 // DMA Channel
                    );

  if (errRc != ESP_OK) {
    ESP_LOGE(LOG_TAG, "spi_bus_initialize(): rc=%d", errRc);
    abort();
  }

  spi_device_interface_config_t dev_config;
  dev_config.address_bits     = 0;
  dev_config.command_bits     = 0;
  dev_config.dummy_bits       = 0;
  dev_config.mode             = 0;
  dev_config.duty_cycle_pos   = 0;
  dev_config.cs_ena_posttrans = 0;
  dev_config.cs_ena_pretrans  = 0;
  dev_config.clock_speed_hz   = 5000000;
  dev_config.spics_io_num     = -1;
  dev_config.flags            = SPI_DEVICE_NO_DUMMY;
  dev_config.queue_size       = 1;
  dev_config.pre_cb           = NULL;
  dev_config.post_cb          = NULL;
  ESP_LOGI(LOG_TAG, "... Adding device bus.");
  errRc = ::spi_bus_add_device(m_host, &dev_config, &m_handle);
  if (errRc != ESP_OK) {
    ESP_LOGE(LOG_TAG, "spi_bus_add_device(): rc=%d", errRc);
    abort();
  }
} // init


/**
   @brief Set the SPI host to use.
   Call this prior to init().
   @param [in] host The SPI host to use.  Either HSPI_HOST (default) or VSPI_HOST.
*/
void IRAM_ATTR iSPI::setHost(spi_host_device_t host) {
  m_host = host;
} // setHost


/**
   @brief Send and receive data through %SPI.  This is a blocking call.

   @param [in] data A data buffer used to send and receive.
   @param [in] dataLen The number of bytes to transmit and receive.
*/
void IRAM_ATTR iSPI::transfer(uint8_t* data, size_t dataLen) {
  assert(data != nullptr);
  assert(dataLen > 0);
#ifdef DEBUG
  for (auto i = 0; i < dataLen; i++) {
    ESP_LOGD(LOG_TAG, "> %2d %.2x", i, data[i]);
  }
#endif
  spi_transaction_t trans_desc;
  //trans_desc.address   = 0;
  //trans_desc.command   = 0;
  trans_desc.flags     = 0;
  trans_desc.length    = dataLen * 8;
  trans_desc.rxlength  = 8;
  trans_desc.tx_buffer = data;
  trans_desc.rx_buffer = data;

  spi_transaction_t *recPayload;

  recPayload = &trans_desc;

  //ESP_LOGI(tag, "... Transferring");
  esp_err_t rc = ::spi_device_queue_trans(m_handle, &trans_desc, portMAX_DELAY);
  assert(rc == ESP_OK);
  rc = spi_device_get_trans_result(m_handle, &recPayload, portMAX_DELAY);
  assert(rc == ESP_OK);
  if (rc != ESP_OK) {
    ESP_LOGE(LOG_TAG, "transfer:spi_device_transmit: %d", rc);
  }
} // transmit


/**
   @brief Send and receive a single byte.
   @param [in] value The byte to send.
   @return The byte value received.
*/
uint8_t IRAM_ATTR iSPI::transferByte(uint8_t value) {
  transfer(&value, 1);
  return value;
} // transferByte
