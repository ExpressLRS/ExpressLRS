from pystlink.lib import stlinkex

class Stlink():
    STLINK_GET_VERSION = 0xf1
    STLINK_DEBUG_COMMAND = 0xf2
    STLINK_DFU_COMMAND = 0xf3
    STLINK_SWIM_COMMAND = 0xf4
    STLINK_GET_CURRENT_MODE = 0xf5
    STLINK_GET_TARGET_VOLTAGE = 0xf7
    STLINK_APIV3_GET_VERSION_EX = 0xFB

    STLINK_MODE_DFU = 0x00
    STLINK_MODE_MASS = 0x01
    STLINK_MODE_DEBUG = 0x02
    STLINK_MODE_SWIM = 0x03
    STLINK_MODE_BOOTLOADER = 0x04

    STLINK_DFU_EXIT = 0x07

    STLINK_SWIM_ENTER = 0x00
    STLINK_SWIM_EXIT = 0x01

    STLINK_DEBUG_ENTER_JTAG = 0x00
    STLINK_DEBUG_STATUS = 0x01
    STLINK_DEBUG_FORCEDEBUG = 0x02
    STLINK_DEBUG_APIV1_RESETSYS = 0x03
    STLINK_DEBUG_APIV1_READALLREGS = 0x04
    STLINK_DEBUG_APIV1_READREG = 0x05
    STLINK_DEBUG_APIV1_WRITEREG = 0x06
    STLINK_DEBUG_READMEM_32BIT = 0x07
    STLINK_DEBUG_WRITEMEM_32BIT = 0x08
    STLINK_DEBUG_RUNCORE = 0x09
    STLINK_DEBUG_STEPCORE = 0x0a
    STLINK_DEBUG_APIV1_SETFP = 0x0b
    STLINK_DEBUG_READMEM_8BIT = 0x0c
    STLINK_DEBUG_WRITEMEM_8BIT = 0x0d
    STLINK_DEBUG_APIV1_CLEARFP = 0x0e
    STLINK_DEBUG_APIV1_WRITEDEBUGREG = 0x0f
    STLINK_DEBUG_APIV1_SETWATCHPOINT = 0x10
    STLINK_DEBUG_APIV1_ENTER = 0x20
    STLINK_DEBUG_EXIT = 0x21
    STLINK_DEBUG_READCOREID = 0x22
    STLINK_DEBUG_APIV2_ENTER = 0x30
    STLINK_DEBUG_APIV2_READ_IDCODES = 0x31
    STLINK_DEBUG_APIV2_RESETSYS = 0x32
    STLINK_DEBUG_APIV2_READREG = 0x33
    STLINK_DEBUG_APIV2_WRITEREG = 0x34
    STLINK_DEBUG_APIV2_WRITEDEBUGREG = 0x35
    STLINK_DEBUG_APIV2_READDEBUGREG = 0x36
    STLINK_DEBUG_APIV2_READALLREGS = 0x3a
    STLINK_DEBUG_APIV2_GETLASTRWSTATUS = 0x3b
    STLINK_DEBUG_APIV2_DRIVE_NRST = 0x3c
    STLINK_DEBUG_SYNC = 0x3e
    STLINK_DEBUG_APIV2_START_TRACE_RX = 0x40
    STLINK_DEBUG_APIV2_STOP_TRACE_RX = 0x41
    STLINK_DEBUG_APIV2_GET_TRACE_NB = 0x42
    STLINK_DEBUG_APIV2_SWD_SET_FREQ = 0x43
    STLINK_DEBUG_APIV2_READMEM_16BIT =   0x47
    STLINK_DEBUG_APIV2_WRITEMEM_16BIT =  0x48

    STLINK_DEBUG_ENTER_SWD = 0xa3

    STLINK_DEBUG_APIV3_SET_COM_FREQ = 0x61
    STLINK_DEBUG_APIV3_GET_COM_FREQ = 0x62

    STLINK_DEBUG_APIV2_DRIVE_NRST_LOW = 0x00
    STLINK_DEBUG_APIV2_DRIVE_NRST_HIGH = 0x01
    STLINK_DEBUG_APIV2_DRIVE_NRST_PULSE = 0x02

    STLINK_DEBUG_APIV2_SWD_SET_FREQ_MAP = {
        4000000: 0,
        1800000: 1,  # default
        1200000: 2,
        950000:  3,
        480000:  7,
        240000: 15,
        125000: 31,
        100000: 40,
        50000:  79,
        25000: 158,
        # 15000: 265,
        # 5000:  798
    }

    STLINK_MAXIMUM_TRANSFER_SIZE = 1024

    def __init__(self, connector, dbg, swd_frequency=4000000):
        self._connector = connector
        self._dbg = dbg
        self.read_version()
        self.leave_state()
        self.read_target_voltage()
        if self._ver_api == 3:
            self.set_swd_freq_v3(swd_frequency)
        if self._ver_jtag >= 22:
            self.set_swd_freq(swd_frequency)
        self.enter_debug_swd()
        self.read_coreid()

    def clean_exit(self):
        # WORKAROUND for OS/X 10.11+
        # ... read from ST-Link, must be performed even times
        # call this function after last send command
        if self._connector.xfer_counter & 1:
            self._connector.xfer([Stlink.STLINK_GET_CURRENT_MODE], rx_len=2)

    def read_version(self):
        # WORKAROUNF for OS/X 10.11+
        # ... retry XFER if first is timeout.
        # only during this command it is necessary
        rx = self._connector.xfer([Stlink.STLINK_GET_VERSION, 0x80], rx_len=6, retry=2, tout=200)
        ver = int.from_bytes(rx[:2], byteorder='big')
        dev_ver = self._connector.version
        self._ver_stlink = (ver >> 12) & 0xf
        self._ver_jtag = (ver >> 6) & 0x3f
        self._ver_swim = ver & 0x3f if dev_ver == 'V2' else None
        self._ver_mass = ver & 0x3f if dev_ver == 'V2-1' else None
        self._ver_api = 3 if dev_ver[0:2] == 'V3' else 2 if self._ver_jtag > 11 else 1
        if dev_ver[0:2] == 'V3':
            rx_v3 = self._connector.xfer([Stlink.STLINK_APIV3_GET_VERSION_EX, 0x80], rx_len=16)
            self._ver_swim = int(rx_v3[1])
            self._ver_jtag = int(rx_v3[2])
            self._ver_mass = int(rx_v3[3])
            self._ver_bridge = int(rx_v3[4])
        self._ver_str = "%s V%dJ%d" % (dev_ver, self._ver_stlink, self._ver_jtag)
        if dev_ver == 'V3':
            self._ver_str += "M%d" % self._ver_mass
            self._ver_str += "B%d" % self._ver_bridge
            self._ver_str += "S%d" % self._ver_swim
        if dev_ver == 'V3E':
            self._ver_str += "M%d" % self._ver_mass
        if dev_ver == 'V2':
            self._ver_str += "S%d" % self._ver_swim
        if dev_ver == 'V2-1':
            self._ver_str += "M%d" % self._ver_mass
        if self.ver_api == 1:
            raise self._dbg.warning("ST-Link/%s is not supported, please upgrade firmware." % self._ver_str)
        if self.ver_jtag < 32 and self._ver_api == 2:
            self._dbg.warning("ST-Link/%s is not recent firmware, please upgrade first - functionality is not guaranteed." % self._ver_str)
        if self.ver_jtag < 3 and self._ver_api == 3:
            self._dbg.warning("ST-Link/%s is not recent firmware, please upgrade first - functionality is not guaranteed." % self._ver_str)

    @property
    def ver_stlink(self):
        return self._ver_stlink

    @property
    def ver_jtag(self):
        return self._ver_jtag

    @property
    def ver_mass(self):
        return self._ver_mass

    @property
    def ver_bridge(self):
        return self._ver_bridge

    @property
    def ver_swim(self):
        return self._ver_swim

    @property
    def ver_api(self):
        return self._ver_api

    @property
    def ver_str(self):
        return self._ver_str

    def read_target_voltage(self):
        rx = self._connector.xfer([Stlink.STLINK_GET_TARGET_VOLTAGE], rx_len=8)
        a0 = int.from_bytes(rx[:4], byteorder='little')
        a1 = int.from_bytes(rx[4:8], byteorder='little')
        self._target_voltage = 2 * a1 * 1.2 / a0 if a0 != 0 else None

    @property
    def target_voltage(self):
        return self._target_voltage

    def read_coreid(self):
        rx = self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_READCOREID], rx_len=4)
        if len(rx) < 4:
            self._coreid = 0
        else:
            self._coreid = int.from_bytes(rx[:4], byteorder='little')

    @property
    def coreid(self):
        return self._coreid

    def leave_state(self):
        rx = self._connector.xfer([Stlink.STLINK_GET_CURRENT_MODE], rx_len=2)
        if rx[0] == Stlink.STLINK_MODE_DFU:
            self._connector.xfer([Stlink.STLINK_DFU_COMMAND, Stlink.STLINK_DFU_EXIT])
        if rx[0] == Stlink.STLINK_MODE_DEBUG:
            self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_EXIT])
        if rx[0] == Stlink.STLINK_MODE_SWIM:
            self._connector.xfer([Stlink.STLINK_SWIM_COMMAND, Stlink.STLINK_SWIM_EXIT])

    def set_swd_freq(self, freq=1800000):
        for f, d in Stlink.STLINK_DEBUG_APIV2_SWD_SET_FREQ_MAP.items():
            if freq >= f:
                rx = self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_SWD_SET_FREQ, d], rx_len=2)
                if rx[0] != 0x80:
                    raise stlinkex.StlinkException("Error switching SWD frequency")
                return
        raise stlinkex.StlinkException("Selected SWD frequency is too low")

    def set_swd_freq_v3(self, freq=1800000):
        rx = self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV3_GET_COM_FREQ, 0], rx_len=52)
        i = 0
        freq_khz = 0
        while i < rx[8] :
            freq_khz =  int.from_bytes(rx[12 + 4 * i: 15 + 4 * i], byteorder='little')
            if freq / 1000 >= freq_khz:
                break;
            i = i + 1
        self._dbg.verbose("Using %d khz for %d kHz requested" % (freq_khz, freq/ 1000))
        if i == rx[8]:
            raise stlinkex.StlinkException("Selected SWD frequency is too low")
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV3_SET_COM_FREQ]
        cmd.extend([0x0] * 2)
        cmd.extend(list(freq_khz.to_bytes(4, byteorder='little')))
        rx = self._connector.xfer(cmd, rx_len=2)
        if rx[0] != 0x80:
            raise stlinkex.StlinkException("Error switching SWD frequency")

    def enter_debug_swd(self):
        self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_ENTER, Stlink.STLINK_DEBUG_ENTER_SWD], rx_len=2)

    def debug_resetsys(self):
        self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_RESETSYS], rx_len=2)

    def set_debugreg32(self, addr, data):
        if addr % 4:
            raise stlinkex.StlinkException('get_mem address %08x is not in multiples of 4' % addr)
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_WRITEDEBUGREG]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(data.to_bytes(4, byteorder='little')))
        return self._connector.xfer(cmd, rx_len=2)

    def get_debugreg32(self, addr):
        if addr % 4:
            raise stlinkex.StlinkException('get_mem address %08xis not in multiples of 4' % addr)
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_READDEBUGREG]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        rx = self._connector.xfer(cmd, rx_len=8)
        return int.from_bytes(rx[4:8], byteorder='little')

    def get_debugreg16(self, addr):
        if addr % 2:
            raise stlinkex.StlinkException('get_mem_short address is not in even')
        val = self.get_debugreg32(addr & 0xfffffffc)
        if addr % 4:
            val >>= 16
        return val & 0xffff

    def get_debugreg8(self, addr):
        val = self.get_debugreg32(addr & 0xfffffffc)
        val >>= (addr % 4) << 3
        return val & 0xff

    def get_reg(self, reg):
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_READREG, reg]
        rx = self._connector.xfer(cmd, rx_len=8)
        return int.from_bytes(rx[4:8], byteorder='little')

    def set_reg(self, reg, data):
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_WRITEREG, reg]
        cmd.extend(list(data.to_bytes(4, byteorder='little')))
        self._connector.xfer(cmd, rx_len=2)

    def get_mem32(self, addr, size):
        if addr % 4:
            raise stlinkex.StlinkException('get_mem32: Address must be in multiples of 4')
        if size % 4:
            raise stlinkex.StlinkException('get_mem32: Size must be in multiples of 4')
        if size > Stlink.STLINK_MAXIMUM_TRANSFER_SIZE:
            raise stlinkex.StlinkException('get_mem32: Size for reading is %d but maximum can be %d' % (size, Stlink.STLINK_MAXIMUM_TRANSFER_SIZE))
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_READMEM_32BIT]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(size.to_bytes(4, byteorder='little')))
        return self._connector.xfer(cmd, rx_len=size)

    def set_mem32(self, addr, data):
        if addr % 4:
            raise stlinkex.StlinkException('set_mem32: Address must be in multiples of 4')
        if len(data) % 4:
            raise stlinkex.StlinkException('set_mem32: Size must be in multiples of 4')
        if len(data) > Stlink.STLINK_MAXIMUM_TRANSFER_SIZE:
            raise stlinkex.StlinkException('set_mem32: Size for writing is %d but maximum can be %d' % (len(data), Stlink.STLINK_MAXIMUM_TRANSFER_SIZE))
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_WRITEMEM_32BIT]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(len(data).to_bytes(4, byteorder='little')))
        self._connector.xfer(cmd, data=data)

    def get_mem8(self, addr, size):
        if size > 64:
            raise stlinkex.StlinkException('get_mem8: Size for reading is %d but maximum can be 64' % size)
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_READMEM_8BIT]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(size.to_bytes(4, byteorder='little')))
        return self._connector.xfer(cmd, rx_len=size)

    def set_mem8(self, addr, data):
        datablock = data
        while(datablock):
            block = datablock[:64]
            datablock = datablock[64:]
            cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_WRITEMEM_8BIT]
            cmd.extend(list(addr.to_bytes(4, byteorder='little')))
            cmd.extend(list(len(block).to_bytes(4, byteorder='little')))
            self._connector.xfer(cmd, block)
            addr = addr + 64

    def get_mem16(self, addr, size):
        if addr % 2:
            raise stlinkex.StlinkException('get_mem16: Address must be in multiples of 2')
        if len(data) % 2:
            raise stlinkex.StlinkException('get_mem16: Size must be in multiples of 2')
        if len(data) > Stlink.STLINK_MAXIMUM_TRANSFER_SIZE:
            raise stlinkex.StlinkException('get_mem16: Size for writing is %d but maximum can be %d' % (len(data), Stlink.STLINK_MAXIMUM_TRANSFER_SIZE))
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_READMEM_16BIT]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(size.to_bytes(4, byteorder='little')))
        return self._connector.xfer(cmd, rx_len=size)

    def set_mem16(self, addr, data):
        if addr % 2:
            raise stlinkex.StlinkException('set_mem16: Address must be in multiples of 2')
        if len(data) % 2:
            raise stlinkex.StlinkException('set_mem16: Size must be in multiples of 2')
        if len(data) > Stlink.STLINK_MAXIMUM_TRANSFER_SIZE:
            raise stlinkex.StlinkException('set_mem16: Size for writing is %d but maximum can be %d' % (len(data), Stlink.STLINK_MAXIMUM_TRANSFER_SIZE))
        cmd = [Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_WRITEMEM_16BIT]
        cmd.extend(list(addr.to_bytes(4, byteorder='little')))
        cmd.extend(list(len(data).to_bytes(4, byteorder='little')))
        self._connector.xfer(cmd, data=data)
    def set_nrst(self, action):
        self._connector.xfer([Stlink.STLINK_DEBUG_COMMAND, Stlink.STLINK_DEBUG_APIV2_DRIVE_NRST, action], rx_len=2)
