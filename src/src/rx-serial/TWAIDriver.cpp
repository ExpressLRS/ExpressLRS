#if defined(PLATFORM_ESP32)

#include "TWAIDriver.h"

#include <atomic>

#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "esp_attr.h"
#include "esp_intr_alloc.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "hal/twai_ll.h"
#include "soc/gpio_sig_map.h"
#if defined(PLATFORM_ESP32_C3)
#include "soc/usb_serial_jtag_reg.h"
#endif

// Everything the ISR touches is registers or DRAM, with no calls into flash. The LL helpers are
// only plain `static inline` so they may be outlined into flash, and are used for setup only.

#define STATUS_RBS  (1 << 0)    // receive buffer has a frame
#define STATUS_DOS  (1 << 1)    // data overrun
#define STATUS_TBS  (1 << 2)    // transmit buffer free
#define STATUS_TCS  (1 << 3)    // last transmission completed
#define STATUS_BS   (1 << 7)    // bus-off

#define INTR_RI     (1 << 0)
#define INTR_TI     (1 << 1)
#define INTR_EI     (1 << 2)    // error warning or bus-off status changed
#define INTR_BEI    (1 << 7)    // bus error, only enabled on the ESP32 for the errata check
#if CONFIG_IDF_TARGET_ESP32
#define ENABLED_INTRS   (INTR_RI | INTR_TI | INTR_EI | INTR_BEI)
#else
#define ENABLED_INTRS   (INTR_RI | INTR_TI | INTR_EI)
#endif

#define CMD_TR      (1 << 0)
#define CMD_AT      (1 << 1)
#define CMD_RRB     (1 << 2)
#define CMD_CDO     (1 << 3)

#define FRAME_EXTD  (1 << 7)
#define FRAME_RTR   (1 << 6)

// Rides out main loop stalls (flash writes): ~33ms of a saturated 1Mbit/s bus, around 1s of the
// filtered traffic from an ArduPilot FC
#define RX_RING_LEN 256
#define TX_RING_LEN 32
static_assert((RX_RING_LEN & (RX_RING_LEN - 1)) == 0 && (TX_RING_LEN & (TX_RING_LEN - 1)) == 0, "Ring lengths must be powers of 2");

// The ISR is allocated on the same core as all callers, so mux only has to keep the ISR out
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static intr_handle_t intrHandle;
static TWAIFrame rxRing[RX_RING_LEN];
static TWAIFrame txRing[TX_RING_LEN];
// Rings are single producer/consumer: the ISR advances rxHead, receive() rxTail, transmit() txHead
// and loadTx() txTail
static std::atomic<uint32_t> rxHead, rxTail, txHead, txTail;
static volatile TWAIState busState = TWAI_STOPPED;
static volatile bool txBusy;
static volatile bool txAborting;
static volatile uint32_t txCompleted;
static TWAIStats stats;
static TWAIFilter filter;

// Called from the ISR, or with mux held
static inline IRAM_ATTR void loadTx()
{
    if (txBusy || busState != TWAI_RUNNING)
        return;
    const uint32_t tail = txTail.load(std::memory_order_relaxed);
    if (txHead.load(std::memory_order_acquire) == tail)
        return;

    const TWAIFrame &f = txRing[tail & (TX_RING_LEN - 1)];
    // ID registers hold the 29 bit id big endian, left aligned
    TWAI.tx_rx_buffer[0].val = FRAME_EXTD | f.dlc;
    TWAI.tx_rx_buffer[1].val = (f.id >> 21) & 0xFF;
    TWAI.tx_rx_buffer[2].val = (f.id >> 13) & 0xFF;
    TWAI.tx_rx_buffer[3].val = (f.id >> 5) & 0xFF;
    TWAI.tx_rx_buffer[4].val = (f.id << 3) & 0xF8;
    for (unsigned i = 0; i < f.dlc; ++i)
        TWAI.tx_rx_buffer[5 + i].val = f.data[i];
    TWAI.command_reg.val = CMD_TR;
    txTail.store(tail + 1, std::memory_order_release);
    txBusy = true;
}

// Called from the ISR, or with mux held
static inline IRAM_ATTR void checkTxDone(uint32_t status)
{
    if (!txBusy || !(status & STATUS_TBS))
        return;
    txBusy = false;
    // TCS is also set after an abort, so it can't tell an aborted frame from an acknowledged one
    if ((status & STATUS_TCS) && !txAborting)
        txCompleted++;
    else
        stats.txFailed++;
    txAborting = false;
}

#if CONFIG_IDF_TARGET_ESP32
// ESP32 errata, same checks as IDF twai_hal_get_events(): an RX bus error in the data or CRC field
// can corrupt the next received frame, and the RX FIFO corrupts at 62+ frames. Both need a reset.
static inline IRAM_ATTR bool needsErrataReset(uint32_t intr)
{
    if (intr & INTR_BEI)
    {
        const uint32_t ecc = TWAI.error_code_capture_reg.val;  // read re-arms BEI
        const uint32_t seg = ecc & 0x1F;
        const bool rxError = ecc & (1 << 5);
        if (rxError && (seg == 8 || seg == 10 || ((ecc >> 6) == 3 && seg == 27)))  // CRC, data, ACK delim
            return true;
    }
    return (intr & INTR_RI) && (TWAI.rx_message_counter_reg.val & 0x7F) >= 62;
}
#endif

static inline IRAM_ATTR void readRxFifo(uint32_t status)
{
    const uint32_t count = TWAI.rx_message_counter_reg.val & 0x7F;
    if (status & STATUS_DOS)
    {
        // FIFO contents are unreliable after an overrun, drop them all
        for (uint32_t i = 0; i < count; ++i)
            TWAI.command_reg.val = CMD_RRB;
        TWAI.command_reg.val = CMD_CDO;
        stats.rxOverrun++;
        return;
    }

    uint32_t head = rxHead.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t info = TWAI.tx_rx_buffer[0].val;
        if ((info & (FRAME_EXTD | FRAME_RTR)) == FRAME_EXTD)
        {
            if (head - rxTail.load(std::memory_order_acquire) < RX_RING_LEN)
            {
                TWAIFrame &f = rxRing[head & (RX_RING_LEN - 1)];
                f.id = ((TWAI.tx_rx_buffer[1].val & 0xFF) << 21) | ((TWAI.tx_rx_buffer[2].val & 0xFF) << 13) |
                       ((TWAI.tx_rx_buffer[3].val & 0xFF) << 5) | ((TWAI.tx_rx_buffer[4].val & 0xFF) >> 3);
                f.dlc = info & 0x0F;
                if (f.dlc > 8)
                    f.dlc = 8;
                for (unsigned d = 0; d < f.dlc; ++d)
                    f.data[d] = TWAI.tx_rx_buffer[5 + d].val;
                head++;
            }
            else
            {
                stats.rxQueueFull++;
            }
        }
        TWAI.command_reg.val = CMD_RRB;
    }
    rxHead.store(head, std::memory_order_release);
}

static void IRAM_ATTR twaiIsr(void *)
{
    const uint32_t intr = TWAI.interrupt_reg.val;   // read clears
    const uint32_t status = TWAI.status_reg.val;
    if (busState == TWAI_RESETTING)
        return;

    if (intr & INTR_EI)
    {
        if ((status & STATUS_BS) && busState == TWAI_RUNNING)
        {
            // Hardware has entered reset mode, poll() starts recovery
            busState = TWAI_BUS_OFF;
            txBusy = false;
            stats.busOff++;
#if CONFIG_IDF_TARGET_ESP32
            // Errata: REC is not cleared on bus-off, retrigger bus-off to clear it
            TWAI.tx_error_counter_reg.val = 0;
            TWAI.tx_error_counter_reg.val = 255;
            (void)TWAI.interrupt_reg.val;
#endif
        }
        else if (!(status & STATUS_BS) && busState == TWAI_RECOVERING)
        {
            busState = TWAI_RUNNING;
        }
    }

#if CONFIG_IDF_TARGET_ESP32
    if (needsErrataReset(intr) && busState == TWAI_RUNNING)
    {
        // Stop receiving and drop the FIFO, poll() resets the peripheral
        TWAI.mode_reg.rm = 1;
        busState = TWAI_RESETTING;
        if (txBusy)
        {
            txBusy = false;
            stats.txFailed++;
        }
        txAborting = false;
        stats.errataResets++;
        return;
    }
#endif

    if (status & STATUS_RBS)
        readRxFifo(status);

    // Also checked without TI, the ESP32 can lose the TX interrupt
    checkTxDone(status);
    loadTx();
}

static void configureGpio(int8_t txPin, int8_t rxPin)
{
#if defined(PLATFORM_ESP32_C3)
    // GPIO18/19 default to the USB-JTAG pads, release them for the TWAI peripheral
    if (txPin == 18 || txPin == 19 || rxPin == 18 || rxPin == 19)
    {
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    }
#endif
    gpio_reset_pin((gpio_num_t)txPin);
    gpio_reset_pin((gpio_num_t)rxPin);

    gpio_set_pull_mode((gpio_num_t)txPin, GPIO_FLOATING);
    esp_rom_gpio_connect_out_signal(txPin, TWAI_TX_IDX, false, false);
    esp_rom_gpio_pad_select_gpio(txPin);

    gpio_set_pull_mode((gpio_num_t)rxPin, GPIO_FLOATING);
    esp_rom_gpio_connect_in_signal(rxPin, TWAI_RX_IDX, false);
    esp_rom_gpio_pad_select_gpio(rxPin);
    gpio_set_direction((gpio_num_t)rxPin, GPIO_MODE_INPUT);
}

// Leaves the controller in reset mode with interrupts disabled
static bool configureController()
{
    twai_ll_enter_reset_mode(&TWAI);
    if (!twai_ll_is_in_reset_mode(&TWAI))
        return false;
#if SOC_TWAI_SUPPORT_MULTI_ADDRESS_LAYOUT
    twai_ll_enable_extended_reg_layout(&TWAI);
#endif
    twai_ll_set_mode(&TWAI, TWAI_MODE_NORMAL);
    twai_ll_set_enabled_intrs(&TWAI, 0);
    // 1Mbit/s, matches TWAI_TIMING_CONFIG_1MBITS()
    twai_ll_set_bus_timing(&TWAI, 4, 3, 15, 4, false);
    twai_ll_set_acc_filter(&TWAI, (filter.code1 << 16) | filter.code2, (filter.mask1 << 16) | filter.mask2, false);
    twai_ll_set_clkout(&TWAI, 0);
    twai_ll_set_err_warn_lim(&TWAI, 96);
    twai_ll_set_rec(&TWAI, 0);
    twai_ll_set_tec(&TWAI, 0);
    (void)twai_ll_get_and_clear_intrs(&TWAI);
    return true;
}

bool TWAIDriver::begin(int8_t txPin, int8_t rxPin, const TWAIFilter &acceptFilter)
{
    if (intrHandle)
        return false;

    periph_module_reset(PERIPH_TWAI_MODULE);
    periph_module_enable(PERIPH_TWAI_MODULE);

    filter = acceptFilter;
    if (!configureController())
    {
        periph_module_disable(PERIPH_TWAI_MODULE);
        return false;
    }
    configureGpio(txPin, rxPin);

    // Reset before the ISR is allocated, it can't fire until interrupts are enabled below
    rxHead = rxTail = 0;
    txHead = txTail = 0;
    txBusy = false;
    txAborting = false;
    txCompleted = 0;
    stats = {};

    if (esp_intr_alloc(ETS_TWAI_INTR_SOURCE, ESP_INTR_FLAG_IRAM, twaiIsr, nullptr, &intrHandle) != ESP_OK)
    {
        intrHandle = nullptr;
        periph_module_disable(PERIPH_TWAI_MODULE);
        return false;
    }

    busState = TWAI_RUNNING;
    twai_ll_set_enabled_intrs(&TWAI, ENABLED_INTRS);
    twai_ll_exit_reset_mode(&TWAI);
    return true;
}

void TWAIDriver::end()
{
    if (!intrHandle)
        return;

    esp_intr_disable(intrHandle);
    twai_ll_enter_reset_mode(&TWAI);
    twai_ll_set_enabled_intrs(&TWAI, 0);
    (void)twai_ll_get_and_clear_intrs(&TWAI);
    busState = TWAI_STOPPED;

    esp_intr_free(intrHandle);
    intrHandle = nullptr;
    periph_module_disable(PERIPH_TWAI_MODULE);
}

void TWAIDriver::poll()
{
    if (busState == TWAI_RESETTING)
    {
        // The ISR ignores everything while resetting. The GPIO routing survives the reset.
        periph_module_reset(PERIPH_TWAI_MODULE);
        configureController();
        busState = TWAI_RUNNING;
        twai_ll_set_enabled_intrs(&TWAI, ENABLED_INTRS);
        twai_ll_exit_reset_mode(&TWAI);
    }

    // Skip the lock unless there's a bus-off to recover, a completed TX not yet serviced (lost
    // interrupt) or frames queued while idle. A racy read only means taking the lock needlessly.
    if (busState == TWAI_RUNNING)
    {
        const bool idle = txBusy ? !(TWAI.status_reg.val & STATUS_TBS)
                                 : txHead.load(std::memory_order_relaxed) == txTail.load(std::memory_order_relaxed);
        if (idle)
            return;
    }

    portENTER_CRITICAL(&mux);
    if (busState == TWAI_BUS_OFF)
    {
        // Leaving reset mode starts recovery, done after 128 x 11 recessive bits
        busState = TWAI_RECOVERING;
        TWAI.mode_reg.rm = 0;
    }
    checkTxDone(TWAI.status_reg.val);
    loadTx();
    portEXIT_CRITICAL(&mux);
}

bool TWAIDriver::receive(TWAIFrame &frame)
{
    const uint32_t tail = rxTail.load(std::memory_order_relaxed);
    if (rxHead.load(std::memory_order_acquire) == tail)
        return false;
    frame = rxRing[tail & (RX_RING_LEN - 1)];
    rxTail.store(tail + 1, std::memory_order_release);
    return true;
}

bool TWAIDriver::transmit(const TWAIFrame &frame)
{
    const uint32_t head = txHead.load(std::memory_order_relaxed);
    if (head - txTail.load(std::memory_order_acquire) >= TX_RING_LEN)
        return false;
    txRing[head & (TX_RING_LEN - 1)] = frame;
    txHead.store(head + 1, std::memory_order_release);

    // Publish before reading txBusy: while busy, the ISR loads this frame when the current one completes
    std::atomic_signal_fence(std::memory_order_seq_cst);
    if (!txBusy)
    {
        portENTER_CRITICAL(&mux);
        loadTx();
        portEXIT_CRITICAL(&mux);
    }
    return true;
}

void TWAIDriver::flushTx()
{
    portENTER_CRITICAL(&mux);
    txTail.store(txHead.load(std::memory_order_relaxed), std::memory_order_relaxed);
    if (txBusy)
    {
        txAborting = true;
        TWAI.command_reg.val = CMD_AT;
    }
    portEXIT_CRITICAL(&mux);
}

uint32_t TWAIDriver::txCompletedCount()
{
    return txCompleted;
}

bool TWAIDriver::txQueued()
{
    return txBusy || txHead.load(std::memory_order_relaxed) != txTail.load(std::memory_order_relaxed);
}

TWAIState TWAIDriver::state()
{
    return busState;
}

uint8_t TWAIDriver::txErrorCounter()
{
    return TWAI.tx_error_counter_reg.val & 0xFF;
}

uint8_t TWAIDriver::rxErrorCounter()
{
    return TWAI.rx_error_counter_reg.val & 0xFF;
}

TWAIStats TWAIDriver::takeStats()
{
    portENTER_CRITICAL(&mux);
    const TWAIStats s = stats;
    stats = {};
    portEXIT_CRITICAL(&mux);
    return s;
}

#endif
