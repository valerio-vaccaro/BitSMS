# BitSMS
Receive a Bitcoin transaction using SMS and TTGO T-CALL board.

## Hardware
TTGO T-call board based on ESP32 (wroom) plut SIM800L module.

## Software
- firmware for the ttgo board
- bitcoin core (downlaod it from bitcoin.org)
- PHP web pages (save on pages folder in this repository)

## How it works
Export your transaction in HEX format and send it using your favourite SMS app, the board will receive all parts, put together and try to broadcast the transaction when complete.


## Know issues 
- SIM800L can not send information to SSL protected pages (HTTPS)
- SIM800L can not setup an HTTP session during it receive multiple SMS
- SIM800L can not be configured using SMS, try to specify a valid APN