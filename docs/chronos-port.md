# Catatan migrasi: Chronos lama ke Chronchi ESPBridge V1

Implementasi Chronos/Mochi lama tidak lagi menjadi protokol aktif. Firmware saat ini menggunakan modul `main/chronchi/` dan kontrak Android ESPBridge V1.

Perbedaan penting:

- Nama advertising: `Chronchi`.
- Service: `7c9e0001-6f2f-4d4d-9f25-0d7fd4f0a001`.
- Phone ke ESP32: `7c9e0002-6f2f-4d4d-9f25-0d7fd4f0a001`.
- ESP32 ke Phone: `7c9e0003-6f2f-4d4d-9f25-0d7fd4f0a001`.
- Transport: header 8 byte dengan magic `0x45`, versi `1`, type, sequence, fragment index/count, dan panjang payload big-endian.
- Payload: JSON UTF-8 terstruktur dari ESPBridge, maksimum 768 byte per pesan logis.
- Handshake: firmware mengirim READY setelah notification subscription; Android baru menandai CONNECTED setelah READY.
- Delivery: firmware mengirim ACK hanya sesudah seluruh fragment diterima dan payload lolos parsing. Payload yang ditolak menghasilkan NACK dan alasan pada serial log.

## Kompatibilitas maneuver navigasi

Paket `Navigation` tetap memakai field utama `maneuver`, tetapi firmware juga
membaca `maneuverType`, `direction`, atau `instruction` sebagai cadangan. Nilai
Google Maps seperti `turn-left`, `TURN_LEFT`, `keep right`, dan
`roundabout-exit` dinormalisasi sebelum memilih ikon. Frasa Indonesia seperti
`belok kiri`, `lurus`, dan `bundaran` juga didukung.

Ikon yang tersedia adalah lurus, kiri, kanan, serong kiri/kanan, bundaran, dan
tiba. Aplikasi sebaiknya tetap mengirim `maneuver` dalam kontrak ESPBridge V1
(`STRAIGHT`, `LEFT`, `RIGHT`, `SLIGHT_LEFT`, `SLIGHT_RIGHT`, `ROUNDABOUT`, atau
`ARRIVE`) agar arti instruksi tidak ambigu.

Source `main/chronos/` dipertahankan hanya sebagai referensi legacy dan tidak dimasukkan dalam build Chronchi. Jangan memakai UUID, nama perangkat, atau framing Chronos lama pada aplikasi ESPBridge.

Arsitektur runtime tetap eksklusif: boot Chronchi tidak membuat `Board/Application` Xiaozhi, sedangkan boot Xiaozhi tidak menginisialisasi native ESP-NimBLE Chronchi.
