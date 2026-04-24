import time
from pystlink.lib import stm32
from pystlink.lib import stlinkex

# Stm32 L4 and G0 programming
class Flash():
    FLASH_REG_BASE = 0x40022000
    FLASH_KEYR_REG = FLASH_REG_BASE + 0x08
    FLASH_SR_REG   = FLASH_REG_BASE + 0x10
    FLASH_CR_REG   = FLASH_REG_BASE + 0x14

    FLASH_CR_PG_BIT         = 1 <<  0
    FLASH_CR_PER_BIT        = 1 <<  1
    FLASH_CR_MER1_BIT       = 1 <<  2
    FLASH_CR_PNB_BITINDEX   = 3
    FLASH_CR_BKER_BIT       = 1 << 11
    FLASH_CR_MER2_BIT       = 1 << 15
    FLASH_CR_STRT_BIT       = 1 << 16
    FLASH_CR_FSTPG_BIT      = 1 << 18
    FLASH_CR_OPTLOCK_BIT    = 1 << 30
    FLASH_CR_LOCK_BIT       = 1 << 31

    FLASH_SR_EOP_BIT        = 1 <<  0
    FLASH_SR_OPERR_BIT      = 1 <<  1
    FLASH_SR_PROGERR_BIT    = 1 <<  3
    FLASH_SR_WPRERR_BIT     = 1 <<  4
    FLASH_SR_PGAERR_BIT     = 1 <<  5
    FLASH_SR_SIZERR_BIT     = 1 <<  6
    FLASH_SR_PGSERR_BIT     = 1 <<  7
    FLASH_SR_FASTERR_BIT    = 1 <<  8
    FLASH_SR_MISSERR_BIT    = 1 <<  9
    FLASH_SR_BUSY_BIT       = 1 << 16
    FLASH_SR_CFGBSY_BIT     = 1 << 18

    FLASH_SR_ERROR_MASK = (
        FLASH_SR_PROGERR_BIT | FLASH_SR_WPRERR_BIT | FLASH_SR_PGAERR_BIT |
        FLASH_SR_SIZERR_BIT | FLASH_SR_PGSERR_BIT | FLASH_SR_FASTERR_BIT |
        FLASH_SR_MISSERR_BIT)

    FLASH_OPTR_REG          = FLASH_REG_BASE + 0x20
    FLASH_OPTR_DBANK_BIT    = 1 << 22

    def __init__(self, driver, stlink, dbg):
        self._driver = driver
        self._stlink = stlink
        self._dbg = dbg
        self._page_size = 2048
        dev_id = self._stlink.get_debugreg32(0xE0042000) & 0xfff
        if dev_id == 0x470:
            optr = self._stlink.get_debugreg32(Flash.FLASH_OPTR_REG)
            if not optr & Flash.FLASH_OPTR_DBANK_BIT:
                self._dbg.info('STM32L4[R|S] in single bank configuration')
                self._page_size *= 4
                self._single_bank = True
            else:
                self._dbg.info('STM32L4[R|S] in dual bank configuration')
                self._page_size *= 2
        self.unlock()

    def clear_sr(self):
        # clear errors
        sr = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
        self._stlink.set_debugreg32(Flash.FLASH_SR_REG, sr)

    def unlock(self):
        self._dbg.debug('unlock start')
        self._driver.core_reset_halt()
        self.clear_sr()
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_LOCK_BIT)
        # Lock first. Double unlock results in error!
        cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        if cr & Flash.FLASH_CR_LOCK_BIT:
            # unlock keys
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REG, 0x45670123)
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REG, 0xcdef89ab)
            cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        else :
            raise stlinkex.StlinkException(
                'Unexpected unlock behaviour! FLASH_CR 0x%08x' % cr)
        # check if programing was unlocked
        if cr & Flash.FLASH_CR_LOCK_BIT:
            raise stlinkex.StlinkException(
                'Error unlocking FLASH_CR: 0x%08x. Reset!' % cr)
        if not cr & Flash.FLASH_CR_OPTLOCK_BIT:
            raise stlinkex.StlinkException(
                'Error unlocking FLASH_CR: 0x%08x. Reset!' % cr)

    def lock(self):
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_LOCK_BIT)
        cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        self._dbg.debug('lock cr %08x' % cr)

    def enable_flash_programming(self):
        value_to_write = self._stlink.get_debugreg32(self.FLASH_CR_REG)
        value_to_write |= self.FLASH_CR_PG_BIT
        self._stlink.set_debugreg32(self.FLASH_CR_REG, value_to_write)
        cr = self._stlink.get_debugreg32(self.FLASH_CR_REG)
        if not cr & self.FLASH_CR_PG_BIT:
            raise stlinkex.StlinkException('FLASH_CR_PG_BIT not set\n')

    def disable_flash_programming(self):
        value_to_write = self._stlink.get_debugreg32(self.FLASH_CR_REG)
        value_to_write &= ~self.FLASH_CR_PG_BIT
        self._stlink.set_debugreg32(self.FLASH_CR_REG, value_to_write)
        cr = self._stlink.get_debugreg32(self.FLASH_CR_REG)
        if cr & self.FLASH_CR_PG_BIT:
            raise stlinkex.StlinkException('FLASH_CR_PG_BIT not unset\n')

    def erase_all(self):
        self._dbg.debug('erase_all')
        cr =  Flash.FLASH_CR_MER1_BIT | Flash.FLASH_CR_MER2_BIT;
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, cr)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, cr|
                                    Flash.FLASH_CR_STRT_BIT)
        # max 22.1 sec on STM32L4R (two banks)
        self.wait_busy(25, 'Erasing FLASH')

    def erase_page(self, page):
        self._dbg.debug('erase_page %d' % page)
        self.clear_sr()
        flash_cr_value = Flash.FLASH_CR_PER_BIT
        flash_cr_value |= (page << Flash.FLASH_CR_PNB_BITINDEX)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, flash_cr_value)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, flash_cr_value |
                                    Flash.FLASH_CR_STRT_BIT)
        self.wait_busy(0.05)

    def erase_bank(self, bank):
        self._dbg.debug('erase_bank %d' % bank)
        self.clear_sr()
        cr =  Flash.FLASH_CR_MER1_BIT;
        if bank == 1:
            cr =  Flash.FLASH_CR_MER2_BIT
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, cr)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, cr|
                                    Flash.FLASH_CR_STRT_BIT)
        self.wait_busy(0.05)

    def erase_pages(self, addr, size):
        self._dbg.verbose('erase_pages from addr %08x for %d byte' %
                          (addr, size))
        page = (addr - stm32.Stm32.FLASH_START) // self._page_size
        last_page = (addr - stm32.Stm32.FLASH_START + size + self._page_size - 1) // self._page_size
        self._dbg.verbose('erase_pages %d to %d' % (page, last_page))
        self._dbg.bargraph_start('Erasing FLASH', value_min=page,
                                 value_max=last_page)
        if page == 0 and last_page >= 256:
            self.erase_bank(0)
            page = 256
            self._dbg.bargraph_update(value=page)
        while page < last_page:
            if page == 256 and last_page >= 512:
                self.erase_bank(1)
                page = 512
                self._dbg.bargraph_update(value=page)
                break
            self.erase_page(page)
            page += 1
            self._dbg.bargraph_update(value=page)
        self._dbg.bargraph_done()
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, 0)

    def wait_busy(self, wait_time, bargraph_msg=None, check_eop=False):
        end_time = time.time() + wait_time * 1.5
        if bargraph_msg:
            self._dbg.bargraph_start(bargraph_msg, value_min=time.time(),
                                     value_max=time.time() + wait_time)
        while time.time() < end_time:
            if bargraph_msg:
                self._dbg.bargraph_update(value=time.time())
            status = self._stlink.get_debugreg32(Flash.FLASH_SR_REG)
            if not status & (Flash.FLASH_SR_BUSY_BIT |
                             Flash.FLASH_SR_CFGBSY_BIT |
                             (check_eop & Flash.FLASH_SR_EOP_BIT)) :
                self.end_of_operation(status)
                if bargraph_msg:
                    self._dbg.bargraph_done()
                return
            time.sleep(wait_time / 20)
        raise stlinkex.StlinkException('Operation timeout')

    def end_of_operation(self, status):
        if status & Flash.FLASH_SR_ERROR_MASK:
            raise stlinkex.StlinkException(
                'Error writing FLASH with status (FLASH_SR) %08x' % status)


# support all STM32L4 and G0 MCUs with page size access to FLASH
class Stm32L4(stm32.Stm32):

    def __init__(self, stlink, dbg):
        super().__init__(stlink, dbg)
        self.flash = Flash(self, self._stlink, self._dbg)

    def flash_erase_all(self, flash_size):
        self._dbg.debug('Stm32L4.flash_erase_all()')
        flash = Flash(self, self._stlink, self._dbg)
        flash.erase_all()
        flash.lock()

    def flash_write(self, addr, data, erase=False, erase_sizes=None):
        if addr is None:
            addr = self.FLASH_START
        self._dbg.debug(
            'Stm32l4.flash_write(%s, [data:%dBytes], erase=%s, erase_sizes=%s)'
            % (addr, len(data), erase, erase_sizes))
        if addr % 8:
            raise stlinkex.StlinkException('Start address is not aligned to word')
        # pad data
        if len(data) % 8:
            data.extend([0xff] * (8 - len(data) % 8))
        flash = Flash(self, self._stlink, self._dbg)
        flash.unlock()
        if erase:
            if erase_sizes:
                flash.erase_pages(addr, len(data))
            else:
                flash.erase_all()
            flash.unlock()
        self._dbg.bargraph_start('Writing FLASH', value_min=addr, value_max=addr + len(data))
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, 0)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REG, Flash.FLASH_CR_PG_BIT)
        cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REG)
        if not cr & Flash.FLASH_CR_PG_BIT:
            raise stlinkex.StlinkException('Flash_Cr not ready for programming: %08x\n' % cr)
        while data:
            block = data[:256]
            data = data[256:]
            self._dbg.debug('Stm32l4.flash_write len %s addr %x' % (len(block), addr))
            if min(block) != 0xff:
                self._stlink.set_mem32(addr, block)
            addr += len(block)
            self._dbg.bargraph_update(value=addr)
        flash.wait_busy(0.01)
        self._dbg.bargraph_done()
        flash.lock()
