//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//

#pragma once

#if defined(PLATFORM_ESP32)

#include "common.h"
#include "rmtallocator.h"

/**
 * @brief An encoder for an RMT item.
 */
class EncoderRMT {
public:
    /**
     * @brief Encode the next bit into an rmtItem
     * @param rmtItem the rmt item to be configured
     */
    virtual bool encode_bit(rmt_item32_t *rmtItem) = 0;
};

#define TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE 64

enum eTransponderRMTState : uint8_t {
    TRMT_STATE_UNINIT,
    TRMT_STATE_RETRY_INIT,
    TRMT_STATE_INIT,
    TRMT_STATE_READY,
};

/**
 * @brief An implementation of a transponder using the RMT peripheral.
 *
 * Generates an output signal on a GPIO, which is usually fed into a high-side switching MOSFET to drive an IR LED.
 */
class TransponderRMT {
public:
    TransponderRMT() :
          haveChannel(false),
          state(TRMT_STATE_UNINIT)
              {};

    /**
     * @brief Configure the peripheral
     *
     * @param gpio the gpio pin to use
     */
    void configurePeripheral(gpio_num_t gpio);

    /**
     * @brief Initialise the RMT peripheral
     *
     * This can fail, reasons include channel availability, system driver installation failure, etc.
     *
     * @see the RMT peripheral documentation.
     *
     * @param desired_resolution_hz The resolution of each RMT duration.
     * @param carrier_hz 0 if no carrier signal required, otherwise the frequency in Hz
     * @param carrier_duty 0 if no carrier signal, otherwise the carrier duty as a percentage (1-100)
     * @return true on success
     */
    bool init(uint32_t desired_resolution_hz, uint32_t carrier_hz, uint8_t carrier_duty);

    /**
     * @brief check to see if initialisation succeeded.
     */
    bool isInitialised() { return state >= TRMT_STATE_INIT; };

    /**
     * @brief used to start the encoding process, via an encoder
     *
     * Once a call to this has been made, then `start` can be called repeatedly.
     *
     * @param encoder the encoder to use
     */
    void encode(EncoderRMT *encoder);

    /**
     * @brief start the transmission
     *
     * The signal is only sent ONCE.
     * No effect if the system is not ready or if the RMT peripheral is still transmitting due to a prior call to `start`.
     */
    void start();

    /**
     * @brief de-initialisation the system, prior to object deletion.
     *
     * @see init
     */
    void deinit();

private:
    uint32_t resolutionHz;

    bool haveChannel;
    rmt_channel_t channel;

    gpio_num_t gpio;
    eTransponderRMTState state;

    uint8_t rmtItemCount;
    rmt_item32_t rmtItems[TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE];
};

#endif
