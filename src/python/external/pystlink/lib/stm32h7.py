import time
from pystlink.lib import stm32
from pystlink.lib import stlinkex

# Stm32H7 programming
class Flash():
    FPEC1_BASE		= 0x52002000
    FPEC2_BASE		= 0x52002100
    FLASH_ACR           = FPEC1_BASE
    FLASH_OPTCR         = FPEC1_BASE + 0x18
    FLASH_KEYR_OFFSET	= 0x04
    FLASH_OPTKEYR_OFFSET= 0x08
    FLASH_CR_OFFSET	= 0x0c
    FLASH_SR_OFFSET	= 0x10
    FLASH_CCR1_OFFSET	= 0x14

    FLASH_ACR_1WS       = 0x11
    FLASH_SR_BUSY	= 1 <<  0
    FLASH_SR_WBNE	= 1 <<  1
    FLASH_SR_QW		= 1 <<  2
    FLASH_SR_CRC_BUSY	= 1 <<  3
    FLASH_SR_EOP	= 1 << 16
    FLASH_SR_WRPERR	= 1 << 17
    FLASH_SR_PGSERR	= 1 << 18
    FLASH_SR_STRBERR	= 1 << 19
    FLASH_SR_INCERR	= 1 << 21
    FLASH_SR_OPERR	= 1 << 22
    FLASH_SR_OPERR	= 1 << 22
    FLASH_SR_RDPERR	= 1 << 23
    FLASH_SR_RDSERR	= 1 << 24
    FLASH_SR_SNECCERR	= 1 << 25
    FLASH_SR_DBERRERR	= 1 << 26
    FLASH_SR_ERROR_READ	= FLASH_SR_RDPERR   | FLASH_SR_RDSERR  |\
			   FLASH_SR_SNECCERR |FLASH_SR_DBERRERR
    FLASH_SR_ERROR_MASK	= FLASH_SR_WRPERR  | FLASH_SR_PGSERR  |\
                          FLASH_SR_STRBERR | FLASH_SR_INCERR  |\
                          FLASH_SR_OPERR    | FLASH_SR_ERROR_READ
    FLASH_CR_REGS =   [FPEC1_BASE + FLASH_CR_OFFSET,
                       FPEC2_BASE + FLASH_CR_OFFSET]
    FLASH_SR_REGS =   [FPEC1_BASE + FLASH_SR_OFFSET,
                       FPEC2_BASE + FLASH_SR_OFFSET]
    FLASH_CCR1_REGS = [FPEC1_BASE + FLASH_CCR1_OFFSET,
                       FPEC2_BASE + FLASH_CCR1_OFFSET]
    FLASH_KEYR_REGS = [FPEC1_BASE + FLASH_KEYR_OFFSET,
                       FPEC2_BASE + FLASH_KEYR_OFFSET]

    FLASH_CR_LOCK         = 1 << 0
    FLASH_CR_PG           = 1 << 1
    FLASH_CR_SER          = 1 << 2
    FLASH_CR_BER          = 1 << 3
    FLASH_CR_PSIZE8       = 0 << 4
    FLASH_CR_PSIZE16      = 1 << 4
    FLASH_CR_PSIZE32      = 2 << 4
    FLASH_CR_PSIZE64      = 3 << 4
    FLASH_CR_FW           = 1 << 6
    FLASH_CR_START        = 1 << 7
    FLASH_CR_SNB_BITINDEX = 8
    FLASH_CR_CRC_EN       = 1 << 15
    FLASH_CR_EOP          = 1 << 16

    FLASH_OPTCR_MER       = 1 <<  4

    def __init__(self, driver, stlink, dbg):
        self._driver = driver
        self._stlink = stlink
        self._dbg = dbg
        self._sector_size = 128 * 1024
        self._flash_size = self._stlink.get_debugreg32(0x1ff1e880) & 0xffff
        self._flash_size *= 1024
        self._driver.core_reset_halt()
        # When running from HSI64, 1 WS is good for all VOS
        self._stlink.set_debugreg32(Flash.FLASH_ACR, Flash.FLASH_ACR_1WS)
        self.unlock(0)
        self.unlock(1)

    def clear_sr(self, bank):
        # clear errors
        sr = self._stlink.get_debugreg32(Flash.FLASH_SR_REGS[bank])
        self._stlink.set_debugreg32(Flash.FLASH_CCR1_REGS[bank], sr)

    def unlock(self, bank):
        self._dbg.debug('unlock bank %d start' % bank)
        self.clear_sr(bank)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[bank],
                                    Flash.FLASH_CR_LOCK)
        # Lock first. Unlock a previous unlocks register will fail until reset!
        cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REGS[bank])
        if cr & Flash.FLASH_CR_LOCK:
            # unlock keys
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REGS[bank],0x45670123)
            self._stlink.set_debugreg32(Flash.FLASH_KEYR_REGS[bank], 0xcdef89ab)
            cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REGS[bank])
        else :
            raise stlinkex.StlinkException(
                'Unexpected unlock behaviour bank %d! FLASH_CR 0x%08x'
                                               % (bank, cr))
        # check if programing was unlocked
        if cr & Flash.FLASH_CR_LOCK:
            raise stlinkex.StlinkException(
                'Error unlocking bank %d, FLASH_CR: 0x%08x. Reset!'
                                               % (bank, cr))

    def lock(self, bank):
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[bank],
                                    Flash.FLASH_CR_LOCK)
        cr = self._stlink.get_debugreg32(Flash.FLASH_CR_REGS[bank])
        if not cr &Flash.FLASH_CR_LOCK:
            self._dbg.info('Bank %d lock faildL cr %08x' % (bank, cr))
        self._driver.core_reset_halt()

    def erase_all(self):
        self._dbg.debug('erase_all')
        self.clear_sr(0)
        self.clear_sr(1)
        cr = Flash.FLASH_CR_PSIZE32
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[0], cr)
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[1], cr)
        self._stlink.set_debugreg32(Flash.FLASH_OPTCR, Flash.FLASH_OPTCR_MER )
        self.wait_busy(20, bargraph_msg='Erasing FLASH', bank=2, check_qw=True)

    def erase_bank(self, bank):
        self._dbg.debug('erase_bank %d' % bank)
        self.clear_sr(bank)
        cr = Flash.FLASH_CR_PSIZE32 | Flash.FLASH_CR_BER
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[bank], cr)
        cr |= Flash.FLASH_CR_START
        self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[bank], cr)
        self.wait_busy(12, bank=bank, check_qw=True)

    def erase_sector(self, sector):
        cr = Flash.FLASH_CR_SER | Flash.FLASH_CR_PSIZE32
        if (sector < 8) :
            self.clear_sr(0)
            cr |= (sector << Flash.FLASH_CR_SNB_BITINDEX)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[0], cr)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[0],
                                        cr | Flash.FLASH_CR_START)
            self.wait_busy(4, bank=0, check_qw=True)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[0], 0)
        else:
            self.clear_sr(1)
            cr |= ((sector - 8) * Flash.FLASH_CR_SNB_BITINDEX)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[1], cr)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[1],
                                        cr | Flash.FLASH_CR_START)
            self.wait_busy(4, bank=1, check_qw=True)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[1], 0)

    def erase_sectors(self, addr, size):
        if size == 0:
            return
        self._dbg.debug(
            'erase_sectors page size %d from addr %08x for %d byte' %
            (self._sector_size, addr, size))
        self._dbg.bargraph_start('Erasing FLASH', value_min=addr,
                                 value_max=addr + size)
        sector     = addr - stm32.Stm32.FLASH_START
        end_sector = addr + size - stm32.Stm32.FLASH_START
        sector     //= self._sector_size
        end_sector //= self._sector_size
        if sector == 0 and end_sector >= 8:
            self.erase_bank(0)
            addr += 8* self._sector_size
            self._dbg.bargraph_update(value=addr)
            sector = 8
        while sector <= end_sector:
            if sector == 8 and end_sector >= 16:
                self.erase_bank(1)
                addr += 8* self._sector_size
                self._dbg.bargraph_update(value=addr)
                break
            self.erase_sector(sector)
            sector += 1
            addr += self._sector_size
            self._dbg.bargraph_update(value=addr)
        self._dbg.bargraph_done()

    def wait_busy(self, wait_time, bargraph_msg=None, bank=0, check_qw=False):
        end_time = time.time() + wait_time * 1.5
        if bargraph_msg:
            self._dbg.bargraph_start(bargraph_msg, value_min=time.time(), value_max=time.time() + wait_time)
        while time.time() < end_time:
            if bargraph_msg:
                self._dbg.bargraph_update(value=time.time())
            status = 0
            if bank == 0 or bank == 2:
                status |= self._stlink.get_debugreg32(Flash.FLASH_SR_REGS[0])
            if bank & 1:
                status |= self._stlink.get_debugreg32(Flash.FLASH_SR_REGS[1])
            if not status & (Flash.FLASH_SR_BUSY | (check_qw & Flash.FLASH_SR_QW)) :
                self.end_of_operation(status)
                if bargraph_msg:
                    self._dbg.bargraph_done()
                return
            time.sleep(wait_time / 20)
        raise stlinkex.StlinkException('Operation timeout')

    def end_of_operation(self, status):
        if status & Flash.FLASH_SR_ERROR_MASK:
            raise stlinkex.StlinkException('Error writing FLASH with status (FLASH_SR) %08x' % status)

class Stm32H7(stm32.Stm32):
    def flash_erase_all(self, flash_size):
        self._dbg.debug('Stm32H7.flash_erase_all()')
        flash = Flash(self, self._stlink, self._dbg)
        flash.erase_all()
        flash.lock(0)
        flash.lock(1)

    def flash_write(self, addr, data, erase=False, erase_sizes=None):
        if addr is None:
            addr = self.FLASH_START
        self._dbg.debug(
            'Stm32h7.flash_write(%s, [data:%dBytes], erase=%s)' %
            (addr, len(data), erase))
        if addr % 8:
            raise stlinkex.StlinkException(
                'Start address is not aligned to word')
        # pad data
        if len(data) % 32:
            data.extend([0xff] * (32 - len(data) % 32))
        flash = Flash(self, self._stlink, self._dbg)
        if erase:
            full_chip = (addr == self.FLASH_START) and \
                        (len(data) == flash._flash_size)
            if not erase_sizes or full_chip:
                flash.erase_all()
            else:
                flash.erase_sectors(addr, len(data))
        self._dbg.bargraph_start('Writing FLASH', value_min=addr,
                                 value_max=addr + len(data))
        cr = Flash.FLASH_CR_PG | Flash.FLASH_CR_PSIZE32
        if addr < 0x08100000:
            flash.unlock(0)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[0], cr)
            cr =  self._stlink.get_debugreg32(Flash.FLASH_CR_REGS[0])
            if not cr & Flash.FLASH_CR_PG:
                raise stlinkex.StlinkException(
                    'Bank 0 FLASH_CR not ready for programming: %08x\n' % cr)
        if addr + len(data) >= 0x08100000:
            flash.unlock(1)
            self._stlink.set_debugreg32(Flash.FLASH_CR_REGS[1], cr)
            cr =  self._stlink.get_debugreg32(Flash.FLASH_CR_REGS[1])
            if not cr & Flash.FLASH_CR_PG:
                raise stlinkex.StlinkException(
                    'Bank 1 FLASH_CR not ready for programming: %08x\n' % cr)
        datablock = data
        data_addr = addr
        while datablock:
            block = datablock[:1024]
            datablock = datablock[1024:]
            if min(block) != 0xff:
                self._stlink.set_mem32(data_addr, block)
            data_addr += len(block)
            self._dbg.bargraph_update(value=data_addr)
        flash.wait_busy(0.001)
        flash.lock(0)
        flash.lock(1)
        self._dbg.bargraph_done()
        status = self._stlink.get_debugreg32(Flash.FLASH_SR_REGS[0])
        if status & Flash.FLASH_SR_ERROR_MASK:
            raise stlinkex.StlinkException(
                'Bank 0, Error writing FLASH with status: %08x\n' % status)
        status = self._stlink.get_debugreg32(Flash.FLASH_SR_REGS[1])
        if status & Flash.FLASH_SR_ERROR_MASK:
            raise stlinkex.StlinkException(
                'Bank 1, Error writing FLASH with status: %08x\n' % status)
