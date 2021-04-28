# Asynchonous XModem Receiver
Build status: [![GitHub Actions](https://github.com/AndreRenaud/async_xmodem/workflows/Build%20and%20Test/badge.svg)](https://github.com/AndreRenaud/async_xmodem/actions)

This is a minimal C implementation of the 
[XModem](https://en.wikipedia.org/wiki/XMODEM) transfer protocol.
It is designed to be used in bare-metal embedded systems with no
underlying operating system. It is asynchonous (non-blocking), and
all data is either directly supplied or sent out via callbacks,
with no OS-level dependencies. It does not allocate any dynamic memory,
using only 164B (1080B if large packet sizes are used) of memory while
the transfer is in progress, with a very shallow stack (3-calls deep
maximum). It is approximately 1kB of ARM-thumb2 compiled code.

## Usage
This is a simple single .c & .h file, designed to be directly imported into
most existing code bases without issue.

For most usage, there are four functions which are of interest
* `xmodem_server_init` - initialise the state, and provide the callback for
transmitting individual response bytes
* `xmodem_server_rx_byte` - insert a byte of incoming data into the server
(typically from a UART)
* `xmodem_server_process` - check for timeouts, and extract the next packet
if available
* `xmodem_server_is_done` - indicates when the transfer is completed
* `xmodem_server_get_state` - get the specific state of the transfer
(including success/failure)

## Example
```c
struct xmodem_server xdm;

xmodem_server_init(&xdm, uart_tx_char, NULL);
while (!xmodem_server_is_done(&xdm)) {
	uint8_t resp[XMODEM_MAX_PACKET_SIZE];
	uint32_t block_nr;
	int rx_data_len;

	if (uart_has_data())
		xmodem_server_rx_byte(uart_read());
	rx_data_len = xmodem_server_process(&xdm, resp, &block_nr, ms_time());
	if (rx_data_len > 0)
		handle_incoming_packet(resp, rx_data_len, block_nr);
}
if (xmodem_server_get_state(&xdm) == XMODEM_STATE_FAILURE)
	handle_transfer_failure();
```

## License
This code is licensed using the [Unlicense](https://unlicense.org/) - do
what you want with it.
