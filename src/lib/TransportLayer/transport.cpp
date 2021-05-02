#include "transport.h"

TransportLayer::TransportLayer() {}

void TransportLayer::begin(HardwareSerial* dev, bool halfDuplex)
{
  this->_dev = dev;
  this->halfDuplex = halfDuplex;

  // All methods use now "portEnter_CRITICAL(&FIFOmux)"
  //
  // #if defined(PLATFORM_ESP32) and defined(CRSF_TX_MODULE)
  //   mutexOutFIFO = xSemaphoreCreateMutex();
  // #endif

  // set listen mode
  if (halfDuplex) {
    disableTX();
  }
}

// CRSF::end
void TransportLayer::end()
{
#if CRSF_TX_MODULE
  uint32_t startTime = millis();

#define timeout 1000
  while (outFIFO.peek() > 0) {
    poll(nullptr);
    if (millis() - startTime > timeout) {
      break;
    }
  }
#endif  // CRSF_TX_MODULE
}

void TransportLayer::updateBaudRate(unsigned rate)
{
    flushFIFO();

    if (_dev) {
#ifdef PLATFORM_ESP32
        _dev->flush();
        _dev->updateBaudRate(rate);
#else
        _dev->begin(rate);
#endif
    }
}

// CRSF::flush_port_input
void ICACHE_RAM_ATTR TransportLayer::flushInput()
{
  // Make sure there is no garbage on the UART at the start
  while (_dev && _dev->available()) {
    _dev->read();
  }
}

void ICACHE_RAM_ATTR TransportLayer::flushOutput()
{
  // check if we have data in the output FIFO that needs to be written
  uint8_t peekVal = outFIFO.peek();
  if (peekVal > 0) {
    if (outFIFO.size() >= (peekVal + 1)) {
      if (halfDuplex) {
        enableTX();
      }

#if defined(PLATFORM_ESP32) && defined(CRSF_TX_MODULE)
      portENTER_CRITICAL(&FIFOmux);  // stops other tasks from writing to the
                                     // FIFO when we want to read it
#elif defined(CRSF_RX_MODULE)
      noInterrupts();                // disable interrupts to protect the FIFO
#endif

      uint8_t outPktLen = outFIFO.pop();
      uint8_t outData[outPktLen];

      outFIFO.popBytes(outData, outPktLen);

#if defined(PLATFORM_ESP32) && defined(CRSF_TX_MODULE)
      portEXIT_CRITICAL(&FIFOmux);  // stops other tasks from writing to the
                                    // FIFO when we want to read it
#elif defined(CRSF_RX_MODULE)
      interrupts();                 // disable interrupts to protect the FIFO
#endif

      if (_dev) {
        _dev->write(outData, outPktLen);  // write the packet out
        _dev->flush();
      }

      if (halfDuplex) {
        disableTX();
      }

      // make sure there is no garbage on the UART left over
      flushInput();
    }
  }
}

void ICACHE_RAM_ATTR TransportLayer::sendAsync(uint8_t* pkt, uint8_t pkt_len)
{
#if defined(PLATFORM_ESP32) && defined(CRSF_TX_MODULE)
  portENTER_CRITICAL(&FIFOmux);
#endif

  outFIFO.push(pkt_len);  // length
  outFIFO.pushBytes(pkt, pkt_len);

#if defined(PLATFORM_ESP32) && defined(CRSF_TX_MODULE)
  portEXIT_CRITICAL(&FIFOmux);
#endif
}

void ICACHE_RAM_ATTR TransportLayer::disableTX()
{
#ifdef PLATFORM_ESP32
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
    #ifdef UART_INVERTED
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
    gpio_pulldown_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    gpio_pullup_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    #else
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, false);
    gpio_pullup_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    gpio_pulldown_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    #endif
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED);
#endif
}

void ICACHE_RAM_ATTR TransportLayer::enableTX()
{
#ifdef PLATFORM_ESP32
    gpio_matrix_in((gpio_num_t)-1, U1RXD_IN_IDX, false);
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0));
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
    #ifdef UART_INVERTED
    gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U1TXD_OUT_IDX, true, false);
    #else
    gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U1TXD_OUT_IDX, false, false);
    #endif
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, HIGH ^ GPIO_PIN_BUFFER_OE_INVERTED);
#endif
}

