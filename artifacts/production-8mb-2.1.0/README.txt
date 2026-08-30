Chronchi production firmware 2.1.0
Board: esp32c3-inmp441-production
Required flash: 8 MB (do not flash this package to a 4 MB device)

Factory programming:
  esptool.py --chip esp32c3 write_flash 0x0 chronchi-factory-8mb.bin

Android OTA payload:
  xiaozhi.bin

The factory image contains the bootloader, A/B partition table, initial OTA
metadata, application, and assets. Verify every released file against
CHECKSUMS.sha256 before publication or factory use.
