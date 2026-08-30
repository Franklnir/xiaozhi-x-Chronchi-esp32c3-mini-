# Laporan Implementasi dan Verifikasi Xiaozhi - Chronchi

Tanggal verifikasi: 28 Agustus 2026  
Target: `esp32c3-inmp441`, ESP32-C3, flash 4 MB  
Toolchain: ESP-IDF v5.5.1

## Hasil

Firmware tunggal Xiaozhi + Chronchi berhasil dikompilasi dan dipaketkan. Chronchi menggunakan native ESP-NimBLE, UUID ESPBridge Android V1, framing JSON terfragmentasi, READY/ACK/NACK, state berbatas tetap, serta renderer OLED. Jalur boot Xiaozhi tetap terpisah dan tidak menginisialisasi BLE Chronchi.

Verifikasi software: **PASS**.  
Full flash dan boot Xiaozhi pada ESP32-C3 fisik di COM3: **PASS**.  
Boot mode Chronchi, inisialisasi OLED, advertising BLE, koneksi ponsel, negosiasi MTU 247, dan subscription notifikasi protokol pada perangkat fisik: **PASS**.  
Verifikasi READY/ACK serta seluruh variasi konten OLED dengan data nyata: **BELUM DIVERIFIKASI**.

Full image ditulis pada 28 Agustus 2026 ke ESP32-C3 revision v0.4, embedded flash 4 MB, MAC `7c:e8:b1:d1:cb:0c`. Esptool memverifikasi hash hasil tulis dan melakukan hard reset. Log aplikasi kemudian mencapai Wi-Fi provisioning Xiaozhi dengan AP `Xiaozhi-CB0D`, DHCP `192.168.4.1`, web server aktif, serta audio service terinisialisasi.

Pembaruan renderer OLED kemudian ditulis sebagai app-only flash ke offset `0x10000`, sehingga NVS dan pilihan mode tidak ditimpa. Esptool memverifikasi hash, lalu log boot menunjukkan `Selected boot mode: Chronchi (1)`, SSD1306 siap di `0x3c`, ESPBridge V1 beriklan sebagai `Chronchi`, dan ponsel tersambung serta berlangganan notifikasi. Header normal kini selalu di-reset ke tepi atas/kiri-kanan setelah layar startup/system, garis horizontal dihapus, dan bidang jam digeser 2 piksel ke arah tengah. Header memakai Montserrat 14 dengan tinggi layout 16 piksel agar tepat berada pada zona kuning panel dua warna; indikator teknis `S`/`N` tidak lagi ditampilkan pada Home. Isi zona biru memakai Montserrat 12 pada grid tetap `y=17`, `33`, dan `49`, tinggi baris 15 piksel dengan sela 1 piksel. Kanvas ikon dibatasi 32 x 32 piksel agar tidak menutup baris terakhir. Label navigasi disingkat menjadi `NAVIGASI`, sedangkan nama aplikasi pada header dibatasi delapan karakter tanpa mengubah data sumber. Preview Android mengikuti batas delapan karakter serta tipografi dan spasi isi yang lebih konsisten.

## Bukti build

- `idf.py build`: PASS; seluruh `chronchi_state`, `chronchi_protocol`, `chronchi_ble`, `chronchi_mode`, board, Xiaozhi, dan bootloader terkompilasi serta ter-link.
- Ukuran `xiaozhi.bin`: 2.641.264 byte.
- Partisi aplikasi: `0x2c0000`; ruang kosong `0x3b290` atau 8,4%.
- `ninja merge-bin`: PASS; menghasilkan full image 4.123.830 byte untuk offset `0x0`.
- `python tests/test_dual_mode_contract.py`: 5/5 kelompok PASS.

## Kontrak yang diverifikasi

| Area | Hasil |
| --- | --- |
| Partisi aplikasi dan aset berada dalam flash 4 MB | PASS |
| NVS boot mode dan FSM GPIO3 | PASS |
| Xiaozhi tidak menginisialisasi Chronchi BLE | PASS |
| Chronchi tidak membuat Board/Application Xiaozhi | PASS |
| UUID service/RX/TX sama dengan Android ESPBridge V1 | PASS |
| Header `0x45`, versi `1`, sequence, fragment index/count, dan panjang big-endian | PASS |
| Reassembly maksimum 768 byte dan validasi JSON | PASS |
| READY, ACK, NACK muat pada ATT MTU default | PASS |
| Renderer OLED menerima home, notifikasi, pembayaran, pesanan, cuaca, dan navigasi | PASS |
| Pilihan WakeNet Xiaozhi tetap tersedia | PASS |

## Artefak

| Artefak | Ukuran | SHA-256 |
| --- | ---: | --- |
| `build/xiaozhi.bin` | 2.641.264 B | `EC2E059C31B4F9121081056F0EA8CB7FF0E9CF7C90B63312EB99E5174D028CF4` |
| `build/generated_assets.bin` | 1.174.710 B | `BCCBB8805B394C495C401FDF0D38AD48BAA64C72DE81B9108FE92372299653E3` |
| `build/merged-binary.bin` | 4.123.830 B | `F06DDFDE8CE5254BC4A7A0CBA1B70E50D97393D0DACC15EE776A695E7A90EC3C` |

Android ESPBridge juga diverifikasi terpisah:

- `compileDebugKotlin`, `testDebugUnitTest`, dan `assembleDebug`: PASS.
- 4 test suite, 23 test, 0 failure, 0 error, 0 skipped.
- APK: `C:/Users/frank/Downloads/ESPBridge/app/build/outputs/apk/debug/app-debug.apk`.
- Ukuran APK: 20.140.405 byte.
- SHA-256 APK: `76A03C018DABC217D120BD32BECD3B2A151A42BB25365598381A4E1A2F91EC42`.

## Acceptance test fisik yang masih wajib

1. Uji voice, koneksi Wi-Fi, wake word, audio, dan reset/provisioning lengkap pada jalur Xiaozhi. Boot serta AP provisioning sudah terverifikasi.
2. Uji timing menu GPIO3, persistensi mode, serta perpindahan Xiaozhi - Chronchi berulang.
3. Koneksi, MTU 247, dan subscription sudah terverifikasi; lanjutkan validasi payload READY/ACK/NACK dari ESPBridge.
4. Uji fragmentasi, ACK, reconnect, restart ponsel, restart ESP32, dan minimal 20 siklus reconnect.
5. Cocokkan OLED fisik dengan preview untuk home, pesan, pembayaran masuk/keluar, order, cuaca, dan seluruh maneuver navigasi.
6. Verifikasi OLED pada alamat I2C aktual board serta heap minimum setelah connect dan traffic panjang.
7. Sebelum distribusi dengan data sensitif, tambahkan dan uji BLE bonding/LE Secure Connections.

Jangan mengubah status perangkat fisik menjadi PASS sebelum seluruh acceptance test tersebut benar-benar dilakukan.
