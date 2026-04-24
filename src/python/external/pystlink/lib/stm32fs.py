import time
from pystlink.lib import stm32
from pystlink.lib import stlinkex


class Flash():
    ITCM_BASE      = 0x00200000
    FLASH_REG_BASE = 0x40023c00
    FLASH_KEYR_REG = FLASH_REG_BASE + 0x04
    FLASH_SR_REG = FLASH_REG_BASE + 0x0c
    FLASH_CR_REG = FLASH_REG_BASE + 0x10

    FLASH_CR_LOCK_BIT = 0x80000000
    FLASH_CR_PG_BIT = 0x00000001
    FLASH_CR_SER_BIT = 0x00000002
    FLASH_CR_MER_BIT = 0x00000004
    FLASH_CR_STRT_BIT = 0x00010000
    FLASH_CR_PSIZE_X8 = 0x00000000
    FLASH_CR_PSIZE_X16 = 0x00000100
    FLASH_CR_PSIZE_X32 = 0x00000200
    FLASH_CR_PSIZE_X64 = 0x00000300
    FLASH_CR_SNB_BITINDEX = 3

    FLASH_SR_EOP        = 1 <<  0
    FLASH_SR_OPERR	= 1 <<  1
    FLASH_SR_WRPERR	= 1 <<  4
    FLASH_SR_PGAERR     = 1 <<  5
    FLASH_SR_PGPERR	= 1 <<  6
    FLASH_SR_ERSERR	= 1 <<  7
    FLASH_SR_BSY	= 1 << 16
    FLASH_SR_ERROR_MASK	= FLASH_SR_WRPERR   | FLASH_SR_PGAERR  |\
			   FLASH_SR_PGPERR |FLASH_SR_ERSERR

    VOLTAGE_DEPENDEND_PARAMS = [
        {
            'min_voltage': 2.7,
            'max_mass_erase_time': 16,
            'max_erase_time': {16: .5, 32: 1.1, 64: 1.1, 128: 2, 256: 2},
            'FLASH_CR_PSIZE': FLASH_CR_PSIZE_X32,
            'align': 4,
        }, {
            'min_voltage': 2.1,
            'max_mass_erase_time': 22,
            'max_erase_time': {16: .6, 32: 1.4, 64: 1.4, 128: 2.6, 256: 2.6},
            'FLASH_CR_PSIZE': FLASH_CR_PSIZE_X16,
            'align': 2,
       }, {
            'min_voltage': 1.8,
            'max_mass_erase_time': 32,
            'max_erase_time': {16: .8, 32: 2.4, 64: 2.4, 128: 4, 256: 4},
            'FLASH_CR_PSIZE': FLASH_CR_PSIZE_X8,
            'align': 1,
        }
    ]

    def __init__(self, driver, stlink, dbg):
        self._driver = driver
        self._stlink = stlink
        self._dbg = dbg
        self._params = self.get_voltage_dependend_params()
        self.unlock()

    def get_voltage_dependend_params(self):
        self._stlink.read_target_voltage()
        for params in Flash.VOLTAGE_DEPENDEND_PARAMS:
            if self._stlink.target_voltage > params['min_voltage']:
                return params
        raise stlinkex.StlinkException('Supply voltage is %.2fV, but minimum for FLASH program or erase is 1.8V' % self._stlink.target_voltage)

    def clear_sr(self):
        # clear errors
        sr = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
        self._stlink.set_debugreg32(Flash.FLASH_SR_REG, sr)

    def unlock(self):
        self._driver.core_reset_halt()
        self.clear_sr()
        # do dummy read of FLASH_CR_REG
        self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        # programing locked
        if self._stlink.get_debugreg32(Flash.FLASH_CR_REG) & Flash.FLASH_CR_LOCK_BIT:
            # unlock keys
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REG, 0x45670123)
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REG, 0xcdef89ab)
            # check if programing was unlocked
        if self._stlink.get_debugreg32(Flash.FLASH_CR_REG) & Flash.FLASH_CR_LOCK_BIT:
            raise stlinkex.StlinkException('Error unlocking FLASH')

    def lock(self):
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_LOCK_BIT)
        self._driver.core_reset_halt()

    def erase_all(self):
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_MER_BIT)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_MER_BIT | Flash.FLASH_CR_STRT_BIT)
        self.wait_busy(self._params['max_mass_erase_time'], 'Erasing FLASH')

    def erase_sector(self, sector, erase_size):
        flash_cr_value = Flash.FLASH_CR_SER_BIT
        flash_cr_value |= self._params['FLASH_CR_PSIZE'] | (sector << Flash.FLASH_CR_SNB_BITINDEX)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, flash_cr_value)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, flash_cr_value | Flash.FLASH_CR_STRT_BIT)
        self.wait_busy(self._params['max_erase_time'][erase_size / 1024])

    def erase_sectors(self, flash_start, erase_sizes, addr, size):
        erase_addr = flash_start
        self._dbg.bargraph_start('Erasing FLASH', value_min=flash_start, value_max=flash_start + size)
        sector = 0
        while True:
            for erase_size in erase_sizes:
                if addr < erase_addr + erase_size:
                    self._dbg.bargraph_update(value=erase_addr)
                    self.erase_sector(sector, erase_size)
                erase_addr += erase_size
                if addr + size < erase_addr:
                    self._dbg.bargraph_done()
                    return
                sector += 1

    def wait_busy(self, wait_time, bargraph_msg=None):
        end_time = time.time() + wait_time * 1.5
        if bargraph_msg:
            self._dbg.bargraph_start(bargraph_msg, value_min=time.time(), value_max=time.time() + wait_time)
        while time.time() < end_time:
            if bargraph_msg:
                self._dbg.bargraph_update(value=time.time())
            status = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
            if not status & Flash.FLASH_SR_BSY:
                self.end_of_operation(status)
                if bargraph_msg:
                    self._dbg.bargraph_done()
                return
            time.sleep(wait_time / 20)
        raise stlinkex.StlinkException('Operation timeout')

    def wait_for_breakpoint(self, wait_time):
        end_time = time.time() + wait_time
        while time.time() < end_time and not self._stlink.get_debugreg32(
                stm32.Stm32.DHCSR_REG) & stm32.Stm32.DHCSR_STATUS_HALT_BIT:
            time.sleep(wait_time / 20)
        self.end_of_operation(self._stlink.get_debugreg32(Flash.FLASH_SR_REG))

    def end_of_operation(self, status):
        if status:
            raise stlinkex.StlinkException('Error writing FLASH with status (FLASH_SR) %08x' % status)


# support all STM32F MCUs with sector access access to FLASH
# (STM32F2xx, STM32F4xx)
class Stm32FS(stm32.Stm32):
    def flash_erase_all(self, flash_size):
        self._dbg.debug('Stm32FS.flash_erase_all()')
        flash = Flash(self, self._stlink, self._dbg)
        flash.erase_all()
        flash.lock()

    def flash_write(self, addr, data, erase=False, erase_sizes=None):
        self._dbg.debug('Stm32FS.flash_write(%s, [data:%dBytes], erase=%s, erase_sizes=%s)' % (('0x%08x' % addr) if addr is not None else 'None', len(data), erase, erase_sizes))
        if addr is None:
            addr = self.FLASH_START
        if addr < self.FLASH_START and addr >= Flash.AXIM_BASE:
            addr = Flash.AXIM_BASE + addr - self.FLASH_START
        flash = Flash(self, self._stlink, self._dbg)
        if erase:
            if erase_sizes:
                flash.erase_sectors(self.FLASH_START, erase_sizes, addr, len(data))
            else:
                flash.erase_all()
        status = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
        if status & Flash.FLASH_SR_ERROR_MASK:
            # Try to clear errors
            self._stlink.set_debugreg32(Flash.FLASH_SR_REG, Flash.FLASH_SR_ERROR_MASK)
            status = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
            if status & Flash.FLASH_SR_ERROR_MASK:
                raise stlinkex.StlinkException(
                    'FLASH state error : %08x\n' % status)
        params =   Flash.get_voltage_dependend_params(self)
        print('Align %d' % params['align'])
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_PG_BIT | params['FLASH_CR_PSIZE'])
        self._dbg.bargraph_start('Writing FLASH', value_min=addr, value_max=addr + len(data))
        # align data
        if len(data) % params['align']:
            data.extend([0xff] * (params['align'] - len(data) % params['align']))
        datablock = data
        data_addr = addr
        while datablock:
            block = datablock[:1024]
            datablock = datablock[1024:]
            if min(block) != 0xff:
                if params['align'] == 4:
                    self._stlink.set_mem32(data_addr, block)
                elif params['align'] == 2:
                    self._stlink.set_mem16(data_addr, block)
                else :
                    self._stlink.set_mem8(data_addr, block)
            data_addr += len(block)
            self._dbg.bargraph_update(value=data_addr)
        flash.wait_busy(0.001)
        flash.lock()
        self._dbg.bargraph_done()
        if status & Flash.FLASH_SR_ERROR_MASK:
            raise stlinkex.StlinkException(
                'Error writing FLASH with status: %08x\n' % status)
