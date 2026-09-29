# SD-card diagnostic

Build with `pio run -e sd`, then upload with `pio run -e sd -t upload`.
Open the serial monitor at 115200 baud to see mount status, card/filesystem
capacity, and a recursive directory listing (up to three nested levels and 256
entries). While mounted, the diagnostic repeats the listing every 15 seconds so
it can be captured after the monitor connects.

This diagnostic uses the verified FNK0104B four-bit SDIO mapping. It never
formats the card. If mounting fails, check that the card is seated and formatted
as FAT16 or FAT32; exFAT may not be enabled in the bundled framework.
