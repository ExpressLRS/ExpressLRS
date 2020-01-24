'''
===============================
 XMODEM file transfer protocol
===============================

.. $Id$

This is a literal implementation of XMODEM.TXT_, XMODEM1K.TXT_ and
XMODMCRC.TXT_, support for YMODEM and ZMODEM is pending. YMODEM should
be fairly easy to implement as it is a hack on top of the XMODEM
protocol using sequence bytes ``0x00`` for sending file names (and some
meta data).

.. _XMODEM.TXT: doc/XMODEM.TXT
.. _XMODEM1K.TXT: doc/XMODEM1K.TXT
.. _XMODMCRC.TXT: doc/XMODMCRC.TXT

Data flow example including error recovery
==========================================

Here is a sample of the data flow, sending a 3-block message.
It includes the two most common line hits - a garbaged block,
and an ``ACK`` reply getting garbaged. ``CRC`` or ``CSUM`` represents
the checksum bytes.

XMODEM 128 byte blocks
----------------------

::

    SENDER                                      RECEIVER

                                            <-- NAK
    SOH 01 FE Data[128] CSUM                -->
                                            <-- ACK
    SOH 02 FD Data[128] CSUM                -->
                                            <-- ACK
    SOH 03 FC Data[128] CSUM                -->
                                            <-- ACK
    SOH 04 FB Data[128] CSUM                -->
                                            <-- ACK
    SOH 05 FA Data[100] CPMEOF[28] CSUM     -->
                                            <-- ACK
    EOT                                     -->
                                            <-- ACK

XMODEM-1k blocks, CRC mode
--------------------------

::

    SENDER                                      RECEIVER

                                            <-- C
    STX 01 FE Data[1024] CRC CRC            -->
                                            <-- ACK
    STX 02 FD Data[1024] CRC CRC            -->
                                            <-- ACK
    STX 03 FC Data[1000] CPMEOF[24] CRC CRC -->
                                            <-- ACK
    EOT                                     -->
                                            <-- ACK

Mixed 1024 and 128 byte Blocks
------------------------------

::

    SENDER                                      RECEIVER

                                            <-- C
    STX 01 FE Data[1024] CRC CRC            -->
                                            <-- ACK
    STX 02 FD Data[1024] CRC CRC            -->
                                            <-- ACK
    SOH 03 FC Data[128] CRC CRC             -->
                                            <-- ACK
    SOH 04 FB Data[100] CPMEOF[28] CRC CRC  -->
                                            <-- ACK
    EOT                                     -->
                                            <-- ACK

YMODEM Batch Transmission Session (1 file)
------------------------------------------

::

    SENDER                                      RECEIVER
                                            <-- C (command:rb)
    SOH 00 FF foo.c NUL[123] CRC CRC        -->
                                            <-- ACK
                                            <-- C
    SOH 01 FE Data[128] CRC CRC             -->
                                            <-- ACK
    SOH 02 FC Data[128] CRC CRC             -->
                                            <-- ACK
    SOH 03 FB Data[100] CPMEOF[28] CRC CRC  -->
                                            <-- ACK
    EOT                                     -->
                                            <-- NAK
    EOT                                     -->
                                            <-- ACK
                                            <-- C
    SOH 00 FF NUL[128] CRC CRC              -->
                                            <-- ACK


'''
from __future__ import division, print_function

__author__ = 'Wijnand Modderman <maze@pyth0n.org>'
__copyright__ = ['Copyright (c) 2010 Wijnand Modderman',
                 'Copyright (c) 1981 Chuck Forsberg']
__license__ = 'MIT'
__version__ = '0.4.5'

import platform
import logging
import time
import sys
from functools import partial

# Protocol bytes
SOH = b'\x01'
STX = b'\x02'
EOT = b'\x04'
ACK = b'\x06'
DLE = b'\x10'
NAK = b'\x15'
CAN = b'\x18'
CRC = b'C'


class XMODEM(object):
    '''
    XMODEM Protocol handler, expects two callables which encapsulate the read
        and write operations on the underlying stream.

    Example functions for reading and writing to a serial line:

    >>> import serial
    >>> from xmodem import XMODEM
    >>> ser = serial.Serial('/dev/ttyUSB0', timeout=0) # or whatever you need
    >>> def getc(size, timeout=1):
    ...     return ser.read(size) or None
    ...
    >>> def putc(data, timeout=1):
    ...     return ser.write(data) or None
    ...
    >>> modem = XMODEM(getc, putc)


    :param getc: Function to retrieve bytes from a stream. The function takes
        the number of bytes to read from the stream and a timeout in seconds as
        parameters. It must return the bytes which were read, or ``None`` if a
        timeout occured.
    :type getc: callable
    :param putc: Function to transmit bytes to a stream. The function takes the
        bytes to be written and a timeout in seconds as parameters. It must
        return the number of bytes written to the stream, or ``None`` in case of
        a timeout.
    :type putc: callable
    :param mode: XMODEM protocol mode
    :type mode: string
    :param pad: Padding character to make the packets match the packet size
    :type pad: char

    '''

    # crctab calculated by Mark G. Mendel, Network Systems Corporation
    crctable = [
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
    ]

    def __init__(self, getc, putc, mode='xmodem', pad=b'\x1a'):
        self.getc = getc
        self.putc = putc
        self.mode = mode
        self.pad = pad
        self.log = logging.getLogger('xmodem.XMODEM')

    def abort(self, count=2, timeout=60):
        '''
        Send an abort sequence using CAN bytes.

        :param count: how many abort characters to send
        :type count: int
        :param timeout: timeout in seconds
        :type timeout: int
        '''
        for _ in range(count):
            self.putc(CAN, timeout)

    def send(self, stream, retry=16, timeout=60, quiet=False, callback=None):
        '''
        Send a stream via the XMODEM protocol.

            >>> stream = open('/etc/issue', 'rb')
            >>> print(modem.send(stream))
            True

        Returns ``True`` upon successful transmission or ``False`` in case of
        failure.

        :param stream: The stream object to send data from.
        :type stream: stream (file, etc.)
        :param retry: The maximum number of times to try to resend a failed
                      packet before failing.
        :type retry: int
        :param timeout: The number of seconds to wait for a response before
                        timing out.
        :type timeout: int
        :param quiet: If True, write transfer information to stderr.
        :type quiet: bool
        :param callback: Reference to a callback function that has the
                         following signature.  This is useful for
                         getting status updates while a xmodem
                         transfer is underway.
                         Expected callback signature:
                         def callback(total_packets, success_count, error_count)
        :type callback: callable
        '''

        # initialize protocol
        try:
            packet_size = dict(
                xmodem    = 128,
                xmodem1k  = 1024,
            )[self.mode]
        except KeyError:
            raise ValueError("Invalid mode specified: {self.mode!r}"
                             .format(self=self))

        self.log.debug('Begin start sequence, packet_size=%d', packet_size)
        error_count = 0
        crc_mode = 0
        cancel = 0
        while True:
            char = self.getc(1)
            if char:
                if char == NAK:
                    self.log.debug('standard checksum requested (NAK).')
                    crc_mode = 0
                    break
                elif char == CRC:
                    self.log.debug('16-bit CRC requested (CRC).')
                    crc_mode = 1
                    break
                elif char == CAN:
                    if not quiet:
                        print('received CAN', file=sys.stderr)
                    if cancel:
                        self.log.info('Transmission canceled: received 2xCAN '
                                      'at start-sequence')
                        return False
                    else:
                        self.log.debug('cancellation at start sequence.')
                        cancel = 1
                else:
                    self.log.error('send error: expected NAK, CRC, or CAN; '
                                   'got %r', char)

            error_count += 1
            if error_count > retry:
                self.log.info('send error: error_count reached %d, '
                              'aborting.', retry)
                self.abort(timeout=timeout)
                return False

        # send data
        error_count = 0
        success_count = 0
        total_packets = 0
        sequence = 1
        while True:
            data = stream.read(packet_size)
            if not data:
                # end of stream
                self.log.debug('send: at EOF')
                break
            total_packets += 1

            header = self._make_send_header(packet_size, sequence)
            data = data.ljust(packet_size, self.pad)
            checksum = self._make_send_checksum(crc_mode, data)

            # emit packet
            while True:
                #print("send: block" + sequence)
                self.log.debug('send: block %d', sequence)
                self.putc(header + data + checksum)
                char = self.getc(1, timeout)
                if char == ACK:
                    success_count += 1
                    if callable(callback):
                        callback(total_packets, success_count, error_count)
                    error_count = 0
                    break

                self.log.error('send error: expected ACK; got %r for block %d',
                               char, sequence)
                error_count += 1
                if callable(callback):
                    callback(total_packets, success_count, error_count)
                if error_count > retry:
                    # excessive amounts of retransmissions requested,
                    # abort transfer
                    self.log.error('send error: NAK received %d times, '
                                   'aborting.', error_count)
                    self.abort(timeout=timeout)
                    return False

            # keep track of sequence
            sequence = (sequence + 1) % 0x100

        while True:
            self.log.debug('sending EOT, awaiting ACK')
            # end of transmission
            self.putc(EOT)

            # An ACK should be returned
            char = self.getc(1, timeout)
            if char == ACK:
                break
            else:
                self.log.error('send error: expected ACK; got %r', char)
                error_count += 1
                if error_count > retry:
                    self.log.warn('EOT was not ACKd, aborting transfer')
                    self.abort(timeout=timeout)
                    return False

        self.log.info('Transmission successful (ACK received).')
        return True

    def _make_send_header(self, packet_size, sequence):
        assert packet_size in (128, 1024), packet_size
        _bytes = []
        if packet_size == 128:
            _bytes.append(ord(SOH))
        elif packet_size == 1024:
            _bytes.append(ord(STX))
        _bytes.extend([sequence, 0xff - sequence])
        return bytearray(_bytes)

    def _make_send_checksum(self, crc_mode, data):
        _bytes = []
        if crc_mode:
            crc = self.calc_crc(data)
            _bytes.extend([crc >> 8, crc & 0xff])
        else:
            crc = self.calc_checksum(data)
            _bytes.append(crc)
        return bytearray(_bytes)

    def recv(self, stream, crc_mode=1, retry=16, timeout=60, delay=1, quiet=0):
        '''
        Receive a stream via the XMODEM protocol.

            >>> stream = open('/etc/issue', 'wb')
            >>> print(modem.recv(stream))
            2342

        Returns the number of bytes received on success or ``None`` in case of
        failure.

        :param stream: The stream object to write data to.
        :type stream: stream (file, etc.)
        :param crc_mode: XMODEM CRC mode
        :type crc_mode: int
        :param retry: The maximum number of times to try to resend a failed
                      packet before failing.
        :type retry: int
        :param timeout: The number of seconds to wait for a response before
                        timing out.
        :type timeout: int
        :param delay: The number of seconds to wait between resend attempts
        :type delay: int
        :param quiet: If ``True``, write transfer information to stderr.
        :type quiet: bool

        '''

        # initiate protocol
        error_count = 0
        char = 0
        cancel = 0
        while True:
            # first try CRC mode, if this fails,
            # fall back to checksum mode
            if error_count >= retry:
                self.log.info('error_count reached %d, aborting.', retry)
                self.abort(timeout=timeout)
                return None
            elif crc_mode and error_count < (retry // 2):
                if not self.putc(CRC):
                    self.log.debug('recv error: putc failed, '
                                   'sleeping for %d', delay)
                    time.sleep(delay)
                    error_count += 1
            else:
                crc_mode = 0
                if not self.putc(NAK):
                    self.log.debug('recv error: putc failed, '
                                   'sleeping for %d', delay)
                    time.sleep(delay)
                    error_count += 1

            char = self.getc(1, timeout)
            if char is None:
                self.log.warn('recv error: getc timeout in start sequence')
                error_count += 1
                continue
            elif char == SOH:
                self.log.debug('recv: SOH')
                break
            elif char == STX:
                self.log.debug('recv: STX')
                break
            elif char == CAN:
                if cancel:
                    self.log.info('Transmission canceled: received 2xCAN '
                                  'at start-sequence')
                    return None
                else:
                    self.log.debug('cancellation at start sequence.')
                    cancel = 1
            else:
                error_count += 1

        # read data
        error_count = 0
        income_size = 0
        packet_size = 128
        sequence = 1
        cancel = 0
        while True:
            while True:
                if char == SOH:
                    if packet_size != 128:
                        self.log.debug('recv: SOH, using 128b packet_size')
                        packet_size = 128
                    break
                elif char == STX:
                    if packet_size != 1024:
                        self.log.debug('recv: SOH, using 1k packet_size')
                        packet_size = 1024
                    break
                elif char == EOT:
                    # We received an EOT, so send an ACK and return the
                    # received data length.
                    self.putc(ACK)
                    self.log.info("Transmission complete, %d bytes",
                                  income_size)
                    return income_size
                elif char == CAN:
                    # cancel at two consecutive cancels
                    if cancel:
                        self.log.info('Transmission canceled: received 2xCAN '
                                      'at block %d', sequence)
                        return None
                    else:
                        self.log.debug('cancellation at block %d', sequence)
                        cancel = 1
                else:
                    err_msg = ('recv error: expected SOH, EOT; '
                               'got {0!r}'.format(char))
                    if not quiet:
                        print(err_msg, file=sys.stderr)
                    self.log.warn(err_msg)
                    error_count += 1
                    if error_count > retry:
                        self.log.info('error_count reached %d, aborting.',
                                      retry)
                        self.abort()
                        return None

            # read sequence
            error_count = 0
            cancel = 0
            self.log.debug('recv: data block %d', sequence)
            seq1 = self.getc(1, timeout)
            if seq1 is None:
                self.log.warn('getc failed to get first sequence byte')
                seq2 = None
            else:
                seq1 = ord(seq1)
                seq2 = self.getc(1, timeout)
                if seq2 is None:
                    self.log.warn('getc failed to get second sequence byte')
                else:
                    # second byte is the same as first as 1's complement
                    seq2 = 0xff - ord(seq2)

            if not (seq1 == seq2 == sequence):
                # consume data anyway ... even though we will discard it,
                # it is not the sequence we expected!
                self.log.error('expected sequence %d, '
                               'got (seq1=%r, seq2=%r), '
                               'receiving next block, will NAK.',
                               sequence, seq1, seq2)
                self.getc(packet_size + 1 + crc_mode)
            else:
                # sequence is ok, read packet
                # packet_size + checksum
                data = self.getc(packet_size + 1 + crc_mode, timeout)
                valid, data = self._verify_recv_checksum(crc_mode, data)

                # valid data, append chunk
                if valid:
                    income_size += len(data)
                    stream.write(data)
                    self.putc(ACK)
                    sequence = (sequence + 1) % 0x100
                    # get next start-of-header byte
                    char = self.getc(1, timeout)
                    continue

            # something went wrong, request retransmission
            self.log.warn('recv error: purge, requesting retransmission (NAK)')
            while True:
                # When the receiver wishes to <nak>, it should call a "PURGE"
                # subroutine, to wait for the line to clear. Recall the sender
                # tosses any characters in its UART buffer immediately upon
                # completing sending a block, to ensure no glitches were mis-
                # interpreted.  The most common technique is for "PURGE" to
                # call the character receive subroutine, specifying a 1-second
                # timeout, and looping back to PURGE until a timeout occurs.
                # The <nak> is then sent, ensuring the other end will see it.
                data = self.getc(1, timeout=1)
                if data is None:
                    break
            self.putc(NAK)
            # get next start-of-header byte
            char = self.getc(1, timeout)
            continue

    def _verify_recv_checksum(self, crc_mode, data):
        if crc_mode:
            _checksum = bytearray(data[-2:])
            their_sum = (_checksum[0] << 8) + _checksum[1]
            data = data[:-2]

            our_sum = self.calc_crc(data)
            valid = bool(their_sum == our_sum)
            if not valid:
                self.log.warn('recv error: checksum fail '
                              '(theirs=%04x, ours=%04x), ',
                              their_sum, our_sum)
        else:
            _checksum = bytearray([data[-1]])
            their_sum = _checksum[0]
            data = data[:-1]

            our_sum = self.calc_checksum(data)
            valid = their_sum == our_sum
            if not valid:
                self.log.warn('recv error: checksum fail '
                              '(theirs=%02x, ours=%02x)',
                              their_sum, our_sum)
        return valid, data

    def calc_checksum(self, data, checksum=0):
        '''
        Calculate the checksum for a given block of data, can also be used to
        update a checksum.

            >>> csum = modem.calc_checksum('hello')
            >>> csum = modem.calc_checksum('world', csum)
            >>> hex(csum)
            '0x3c'

        '''
        if platform.python_version_tuple() >= ('3', '0', '0'):
            return (sum(data) + checksum) % 256
        else:
            return (sum(map(ord, data)) + checksum) % 256

    def calc_crc(self, data, crc=0):
        '''
        Calculate the Cyclic Redundancy Check for a given block of data, can
        also be used to update a CRC.

            >>> crc = modem.calc_crc('hello')
            >>> crc = modem.calc_crc('world', crc)
            >>> hex(crc)
            '0xd5e3'

        '''
        for char in bytearray(data):
            crctbl_idx = ((crc >> 8) ^ char) & 0xff
            crc = ((crc << 8) ^ self.crctable[crctbl_idx]) & 0xffff
        return crc & 0xffff


XMODEM1k = partial(XMODEM, mode='xmodem1k')


def run():
    import optparse
    import subprocess

    parser = optparse.OptionParser(
        usage='%prog [<options>] <send|recv> filename filename')
    parser.add_option('-m', '--mode', default='xmodem',
                      help='XMODEM mode (xmodem, xmodem1k)')

    options, args = parser.parse_args()
    if len(args) != 3:
        parser.error('invalid arguments')
        return 1

    elif args[0] not in ('send', 'recv'):
        parser.error('invalid mode')
        return 1

    def _func(so, si):
        import select

        print(('si', si))
        print(('so', so))

        def getc(size, timeout=3):
            read_ready, _, _ = select.select([so], [], [], timeout)
            if read_ready:
                data = so.read(size)
            else:
                data = None

            print(('getc(', repr(data), ')'))
            return data

        def putc(data, timeout=3):
            _, write_ready, _ = select.select([], [si], [], timeout)
            if write_ready:
                si.write(data)
                si.flush()
                size = len(data)
            else:
                size = None

            print(('putc(', repr(data), repr(size), ')'))
            return size

        return getc, putc

    def _pipe(*command):
        pipe = subprocess.Popen(command,
                                stdout=subprocess.PIPE,
                                stdin=subprocess.PIPE)
        return pipe.stdout, pipe.stdin

    if args[0] == 'recv':
        getc, putc = _func(*_pipe('sz', '--xmodem', args[2]))
        stream = open(args[1], 'wb')
        xmodem = XMODEM(getc, putc, mode=options.mode)
        status = xmodem.recv(stream, retry=8)
        assert status, ('Transfer failed, status is', False)
        stream.close()

    elif args[0] == 'send':
        getc, putc = _func(*_pipe('rz', '--xmodem', args[2]))
        stream = open(args[1], 'rb')
        xmodem = XMODEM(getc, putc, mode=options.mode)
        sent = xmodem.send(stream, retry=8)
        assert sent is not None, ('Transfer failed, sent is', sent)
        stream.close()

if __name__ == '__main__':
    sys.exit(run())
