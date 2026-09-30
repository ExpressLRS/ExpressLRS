#if defined(TARGET_RX) && defined(PLATFORM_ESP32)

#include "SerialDroneCAN.h"
#include "common.h"
#include "config.h"
#include "helpers.h"
#include "options.h"
#include "logging.h"

#include "TWAIDriver.h"
#include "esp_mac.h"
#include "esp_timer.h"

#include "dsdl/uavcan.protocol.NodeStatus.h"
#include "dsdl/uavcan.protocol.GetNodeInfo.h"
#include "dsdl/dronecan.sensors.rc.RCInput.h"
#include "dsdl/uavcan.tunnel.Targetted.h"

// ArduPilot's MAVLink-over-CAN needs a fixed node id (CAN_Dx_UC_S1_NOD)
#define DRONECAN_NODE_ID            68
#define DRONECAN_RC_INTERVAL_MS     10  // 100Hz, as MAVLink over UART
#define DRONECAN_RADIO_STATUS_MS    9   // 100Hz, matching RC
#define DRONECAN_TUNNEL_KEEPALIVE_MS 500
#define DRONECAN_TUNNEL_MAX_PAYLOAD 120
#define DRONECAN_MAX_RX_PER_CALL    32
#define DRONECAN_TX_POWER_REPORT_MS 2500
// With no node acknowledging, frames retry forever and RC would reach the FC late once it returns
#define DRONECAN_TX_STALL_MS        50

// Upper ID bits (28..13) compared by the hardware filter, priority is don't care
#define CAN_ID_FILTER_BITS(id)      ((id) >> 13)
#define CAN_ID_PRIORITY_DONT_CARE   0xF800

static_assert(DRONECAN_BUF_SIZE >= UAVCAN_TUNNEL_TARGETTED_MAX_SIZE, "DRONECAN_BUF_SIZE too small");
static_assert(DRONECAN_BUF_SIZE >= DRONECAN_SENSORS_RC_RCINPUT_MAX_SIZE, "DRONECAN_BUF_SIZE too small");
static_assert(DRONECAN_BUF_SIZE >= UAVCAN_PROTOCOL_NODESTATUS_MAX_SIZE, "DRONECAN_BUF_SIZE too small");

size_t DroneCANTunnelStream::write(const uint8_t *buf, size_t len)
{
    if (rxToFc.free() < len)
    {
        rxToFcDropped += len;
        return 0;
    }
    rxToFc.pushBytes(buf, len);
    return len;
}

size_t DroneCANTunnelStream::readBytes(char *buffer, size_t length)
{
    const size_t len = std::min<size_t>(length, fcToRx.size());
    fcToRx.popBytes((uint8_t *)buffer, len);
    return len;
}

static uint64_t micros64()
{
    return esp_timer_get_time();
}

static bool shouldAcceptTransfer(const CanardInstance *ins, uint64_t *outSignature, uint16_t dataTypeId,
                                 CanardTransferType transferType, uint8_t sourceNodeId)
{
    return ((SerialDroneCAN *)canardGetUserReference(ins))->shouldAcceptTransfer(outSignature, dataTypeId, transferType);
}

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
    ((SerialDroneCAN *)canardGetUserReference(ins))->onTransferReceived(ins, transfer);
}

SerialDroneCAN::SerialDroneCAN(int8_t txPin, int8_t rxPin) : SerialMavlink(tunnelStream, tunnelStream, DRONECAN_RADIO_STATUS_MS)
{
    canardInit(&canard, canardPool, sizeof(canardPool), ::onTransferReceived, ::shouldAcceptTransfer, this);
    canardSetLocalNodeID(&canard, DRONECAN_NODE_ID);

    if (txPin == UNDEF_PIN || rxPin == UNDEF_PIN)
    {
        ERRLN("DroneCAN requires both CAN TX and RX pins");
        return;
    }

    // Only accept tunnel.Targetted broadcasts and GetNodeInfo requests to us. The upper ID
    // bits cover DTID/32 and the top 2 bits of the destination, libcanard checks the rest.
    const TWAIFilter filter {
        CAN_ID_FILTER_BITS(UAVCAN_TUNNEL_TARGETTED_ID << 8), CAN_ID_PRIORITY_DONT_CARE,
        CAN_ID_FILTER_BITS((UAVCAN_PROTOCOL_GETNODEINFO_ID << 16) | (1 << 15) | (DRONECAN_NODE_ID << 8) | (1 << 7)),
        CAN_ID_PRIORITY_DONT_CARE,
    };
    if (!TWAIDriver::begin(txPin, rxPin, filter))
    {
        ERRLN("DroneCAN driver start failed");
        return;
    }
    driverStarted = true;
    DBGLN("DroneCAN started tx=%d rx=%d", txPin, rxPin);
}

SerialDroneCAN::~SerialDroneCAN()
{
    if (driverStarted)
        TWAIDriver::end();
}

uint32_t SerialDroneCAN::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    // Wait for the next OTA frame, then send at most every 20ms (frameAvailable latches in between)
    if (!driverStarted || !busActive || (!frameAvailable && !failsafe))
        return DURATION_IMMEDIATELY;
    if (failsafe && config.GetFailsafeMode() == FAILSAFE_NO_PULSES)
        return DRONECAN_RC_INTERVAL_MS;

    dronecan_sensors_rc_RCInput rcInput {};
    if (failsafe)
    {
        rcInput.status |= DRONECAN_SENSORS_RC_RCINPUT_STATUS_FAILSAFE;
    }
    else
    {
        rcInput.quality = rcInputQuality(rcInput.status);
        rcInput.status |= DRONECAN_SENSORS_RC_RCINPUT_STATUS_QUALITY_VALID;
    }
    rcInput.rcin.len = CRSF_NUM_CHANNELS;
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ++ch)
    {
        rcInput.rcin.data[ch] = CRSF_to_US(channelData[ch]);
    }

    const uint32_t len = dronecan_sensors_rc_RCInput_encode(&rcInput, encodeBuf);
    canardBroadcast(&canard, DRONECAN_SENSORS_RC_RCINPUT_SIGNATURE, DRONECAN_SENSORS_RC_RCINPUT_ID,
                    &rcInputTransferId, CANARD_TRANSFER_PRIORITY_HIGH, encodeBuf, len);
    stats.rcSent++;
    processTxQueue();

    return DRONECAN_RC_INTERVAL_MS;
}

void SerialDroneCAN::processSerialInput()
{
    if (!driverStarted)
        return;

    receiveFrames();

    const uint32_t now = millis();
    if (now - lastNodeStatusMs >= 1000)
    {
        lastNodeStatusMs = now;
        canardCleanupStaleTransfers(&canard, micros64());
        // Sent even when inactive, an acknowledgement is how another node is detected
        sendNodeStatus();
        logStats();
    }

    // Moves bytes received from the FC into the MAVLink downlink buffer
    SerialIO::processSerialInput();
    stats.inputBufferPeak = std::max(stats.inputBufferPeak, inputBufferUsed());
}

void SerialDroneCAN::sendQueuedData(uint32_t maxBytesToSend)
{
    if (!driverStarted)
        return;

    // Writes RADIO_STATUS and uplinked MAVLink into the tunnel stream
    SerialMavlink::sendQueuedData(maxBytesToSend);

    const uint32_t now = millis();
    if (serverNodeId == 0 || !busActive)
    {
        // Nowhere to send until the FC has opened the tunnel and is acknowledging
        tunnelStream.rxToFc.flush();
        lastTunnelMs = now;
    }
    else if (tunnelStream.rxToFc.size() > 0 || now - lastTunnelMs > DRONECAN_TUNNEL_KEEPALIVE_MS)
    {
        lastTunnelMs = now;
        sendTunnel();
    }

    processTxQueue();
}

uint8_t SerialDroneCAN::rcInputQuality(uint16_t &status)
{
    // Round robin of RSSI dBm, LQ + antenna, and SNR or TX power
    qualitySlot = (qualitySlot + 1) % 3;
    if (qualitySlot == 0)
    {
        status |= DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_RSSI_DBM;
        return linkStats.active_antenna ? linkStats.uplink_RSSI_2 : linkStats.uplink_RSSI_1;
    }
    if (qualitySlot == 1)
    {
        status |= DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_LQ_ACTIVE_ANTENNA;
        return linkStats.uplink_Link_quality | (linkStats.active_antenna ? 0x80 : 0);
    }

    const uint32_t now = millis();
    if (linkStats.uplink_TX_Power != lastTxPower || now - lastTxPowerMs > DRONECAN_TX_POWER_REPORT_MS)
    {
        // TX power in 5mW units, indexed by CRSF power level
        static const uint16_t crsfPowerMw[] = {0, 10, 25, 100, 500, 1000, 2000, 250, 50};
        lastTxPower = linkStats.uplink_TX_Power;
        lastTxPowerMs = now;
        status |= DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_TX_POWER;
        const uint16_t mw = lastTxPower < ARRAY_SIZE(crsfPowerMw) ? crsfPowerMw[lastTxPower] : 0;
        return std::min(mw / 5, 255);
    }
    status |= DRONECAN_SENSORS_RC_RCINPUT_QUALITY_TYPE_SNR;
    return 128 + linkStats.uplink_SNR;
}

void SerialDroneCAN::receiveFrames()
{
    TWAIFrame msg;
    if (!TWAIDriver::receive(msg))
        return;
    busActive = true;

    // One timestamp per batch, libcanard only uses it for multi-frame transfer timeouts
    const uint64_t nowUs = micros64();
    unsigned count = 0;
    do
    {
        CanardCANFrame frame {};
        frame.id = msg.id | CANARD_CAN_FRAME_EFF;
        frame.data_len = msg.dlc;
        memcpy(frame.data, msg.data, msg.dlc);
        // Reassembly failures (missed start, toggle, TID, CRC) mean frames of a transfer were lost
        const int16_t res = canardHandleRxFrame(&canard, &frame, nowUs);
        if (res <= -CANARD_ERROR_RX_MISSED_START || res == -CANARD_ERROR_OUT_OF_MEMORY)
            stats.rxTransferErrors++;
    } while (++count < DRONECAN_MAX_RX_PER_CALL && TWAIDriver::receive(msg));
    stats.rxFrames += count;
}

void SerialDroneCAN::processTxQueue()
{
    TWAIDriver::poll();

    const uint32_t now = millis();
    const uint32_t completed = TWAIDriver::txCompletedCount();
    if (completed != lastTxCompleted)
    {
        lastTxCompleted = completed;
        lastTxProgressMs = now;
        busActive = true;
    }
    else if (!TWAIDriver::txQueued())
    {
        lastTxProgressMs = now;
    }
    else if (now - lastTxProgressMs > DRONECAN_TX_STALL_MS)
    {
        // Nobody is acknowledging, drop everything queued rather than deliver it late
        lastTxProgressMs = now;
        busActive = false;
        stats.txStalls++;
        while (canardPeekTxQueue(&canard) != nullptr)
            canardPopTxQueue(&canard);
        TWAIDriver::flushTx();
        return;
    }

    const CanardCANFrame *frame;
    while ((frame = canardPeekTxQueue(&canard)) != nullptr)
    {
        TWAIFrame msg;
        msg.id = frame->id & CANARD_CAN_EXT_ID_MASK;
        msg.dlc = frame->data_len;
        memcpy(msg.data, frame->data, frame->data_len);

        if (!TWAIDriver::transmit(msg))
            break;  // driver queue full, retry on the next call
        canardPopTxQueue(&canard);
        stats.txFrames++;
    }
}

void SerialDroneCAN::logStats()
{
#if defined(DEBUG_LOG)
    // Per second; st: 1=running 2=bus-off 3=recovering 4=errata reset, drop: hw overrun/rx queue full/tx failed,
    // rst: errata resets, stall: TX flushes, lost: tunnel bytes dropped from/to FC, buf: peak downlink buffer use,
    // in: tunnel bytes from FC, xerr: transfers lost to missing frames
    const TWAIStats drv = TWAIDriver::takeStats();
    DBGLN("DC st=%u tec=%u rec=%u rx=%u tx=%u rc=%u fs=%u fc=%u drop=%u/%u/%u boff=%u rst=%u stall=%u lost=%u/%u buf=%u in=%u xerr=%u",
          TWAIDriver::state(), TWAIDriver::txErrorCounter(), TWAIDriver::rxErrorCounter(), stats.rxFrames,
          stats.txFrames, stats.rcSent, failsafe, serverNodeId, drv.rxOverrun, drv.rxQueueFull, drv.txFailed,
          drv.busOff, drv.errataResets, stats.txStalls, stats.tunnelInDropped, tunnelStream.rxToFcDropped,
          stats.inputBufferPeak, stats.tunnelInBytes, stats.rxTransferErrors);
    stats = {};
    tunnelStream.rxToFcDropped = 0;
#endif
}

void SerialDroneCAN::sendNodeStatus()
{
    uavcan_protocol_NodeStatus status {};
    status.uptime_sec = millis() / 1000;
    status.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
    status.mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;

    const uint32_t len = uavcan_protocol_NodeStatus_encode(&status, encodeBuf);
    canardBroadcast(&canard, UAVCAN_PROTOCOL_NODESTATUS_SIGNATURE, UAVCAN_PROTOCOL_NODESTATUS_ID,
                    &nodeStatusTransferId, CANARD_TRANSFER_PRIORITY_LOW, encodeBuf, len);
}

void SerialDroneCAN::handleGetNodeInfo(CanardInstance *ins, CanardRxTransfer *transfer)
{
    uavcan_protocol_GetNodeInfoResponse &info = scratch.nodeInfo;
    memset(&info, 0, sizeof(info));

    info.status.uptime_sec = millis() / 1000;
    info.status.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
    info.status.mode = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;

    // strtoul rather than sscanf, which links ~25KB of newlib scanf/strtod
    char *end;
    info.software_version.major = strtoul(version, &end, 10);
    info.software_version.minor = *end == '.' ? strtoul(end + 1, nullptr, 10) : 0;
    info.software_version.vcs_commit = strtoul(commit, nullptr, 16);
    info.software_version.optional_field_flags = UAVCAN_PROTOCOL_SOFTWAREVERSION_OPTIONAL_FIELD_FLAG_VCS_COMMIT;

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    memcpy(info.hardware_version.unique_id, mac, sizeof(mac));

    // e.g. org.expresslrs.radiomaster_xr1_dual_band_rx, node names are lowercase [a-z0-9._]
    const int nameLen = snprintf((char *)info.name.data, sizeof(info.name.data), "org.expresslrs.%s", product_name[0] ? product_name : "rx");
    info.name.len = std::min<int>(nameLen, sizeof(info.name.data) - 1);
    for (uint8_t i = sizeof("org.expresslrs.") - 1; i < info.name.len; ++i)
    {
        const char c = tolower(info.name.data[i]);
        info.name.data[i] = isalnum(c) ? c : '_';
    }

    DBGLN("DC node info to %u: %s vcs=%x", transfer->source_node_id, (char *)info.name.data, info.software_version.vcs_commit);
    const uint32_t len = uavcan_protocol_GetNodeInfoResponse_encode(&info, encodeBuf);
    canardRequestOrRespond(ins, transfer->source_node_id, UAVCAN_PROTOCOL_GETNODEINFO_SIGNATURE,
                           UAVCAN_PROTOCOL_GETNODEINFO_ID, &transfer->transfer_id, transfer->priority,
                           CanardResponse, encodeBuf, len);
}

void SerialDroneCAN::handleTunnel(CanardRxTransfer *transfer)
{
    uavcan_tunnel_Targetted &tunnel = scratch.tunnel;
    if (uavcan_tunnel_Targetted_decode(transfer, &tunnel))
        return;
    if (transfer->source_node_id == 0 || tunnel.target_node != DRONECAN_NODE_ID || tunnel.serial_id != 0)
        return;
    // ArduPilot <= 4.5 leaves protocol undefined
    if (tunnel.protocol.protocol != UAVCAN_TUNNEL_PROTOCOL_UNDEFINED &&
        tunnel.protocol.protocol != UAVCAN_TUNNEL_PROTOCOL_MAVLINK2)
        return;

    serverNodeId = transfer->source_node_id;
    if (tunnel.buffer.len == 0)
        return;
    if (tunnelStream.fcToRx.free() < tunnel.buffer.len)
    {
        stats.tunnelInDropped += tunnel.buffer.len;
        return;
    }
    tunnelStream.fcToRx.pushBytes(tunnel.buffer.data, tunnel.buffer.len);
    stats.tunnelInBytes += tunnel.buffer.len;
}

void SerialDroneCAN::sendTunnel()
{
    uavcan_tunnel_Targetted &tunnel = scratch.tunnel;
    tunnel.protocol.protocol = UAVCAN_TUNNEL_PROTOCOL_MAVLINK2;
    tunnel.target_node = serverNodeId;
    tunnel.serial_id = 0;
    tunnel.options = UAVCAN_TUNNEL_TARGETTED_OPTION_LOCK_PORT;
    tunnel.baudrate = 0;  // ignored by ArduPilot
    tunnel.buffer.len = std::min<uint16_t>(tunnelStream.rxToFc.size(), DRONECAN_TUNNEL_MAX_PAYLOAD);
    tunnelStream.rxToFc.popBytes(tunnel.buffer.data, tunnel.buffer.len);

    const uint32_t len = uavcan_tunnel_Targetted_encode(&tunnel, encodeBuf);
    canardBroadcast(&canard, UAVCAN_TUNNEL_TARGETTED_SIGNATURE, UAVCAN_TUNNEL_TARGETTED_ID,
                    &tunnelTransferId, CANARD_TRANSFER_PRIORITY_MEDIUM, encodeBuf, len);
}

bool SerialDroneCAN::shouldAcceptTransfer(uint64_t *outSignature, uint16_t dataTypeId, CanardTransferType transferType)
{
    if (transferType == CanardTransferTypeRequest && dataTypeId == UAVCAN_PROTOCOL_GETNODEINFO_ID)
    {
        *outSignature = UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_SIGNATURE;
        return true;
    }
    if (transferType == CanardTransferTypeBroadcast && dataTypeId == UAVCAN_TUNNEL_TARGETTED_ID)
    {
        *outSignature = UAVCAN_TUNNEL_TARGETTED_SIGNATURE;
        return true;
    }
    return false;
}

void SerialDroneCAN::onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
    if (transfer->transfer_type == CanardTransferTypeRequest && transfer->data_type_id == UAVCAN_PROTOCOL_GETNODEINFO_ID)
    {
        handleGetNodeInfo(ins, transfer);
    }
    else if (transfer->transfer_type == CanardTransferTypeBroadcast && transfer->data_type_id == UAVCAN_TUNNEL_TARGETTED_ID)
    {
        handleTunnel(transfer);
    }
}

#endif
