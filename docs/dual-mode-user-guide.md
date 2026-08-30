# Panduan Xiaozhi - Chronchi Dual-Mode

Firmware ini mempunyai dua jalur boot yang saling eksklusif:

- **Xiaozhi**: Wi-Fi, audio, WakeNet, WebSocket, dan layanan AI.
- **Chronchi**: OLED 128x64 dan BLE ESPBridge V1. Jalur ini tidak membuat `Board` atau `Application` Xiaozhi, sehingga Wi-Fi, I2S, WakeNet, dan WebSocket tidak dijalankan.

Pilihan mode disimpan di NVS. Kredensial Wi-Fi, wake word, dan pengaturan OLED Xiaozhi tidak dihapus ketika mode diganti.

## Flash firmware lengkap

Image gabungan hasil build berada di `build/merged-binary.bin`. Flash image tersebut ke offset `0x0`:

```powershell
python -m esptool --chip esp32c3 -p COMx -b 460800 write_flash 0x0 build/merged-binary.bin
```

Ganti `COMx` dengan port ESP32-C3. Ini adalah full flash dan dapat menimpa NVS. Untuk pengembangan dari source, gunakan `idf.py flash` agar seluruh offset pada `build/flash_args` ditulis dengan benar.

## Pindah dari Xiaozhi ke Chronchi

1. Tahan tombol GPIO3 minimal 2,5 detik sampai menu `SELECT MODE` muncul.
2. Lepaskan tombol. Pelepasan ini wajib agar hold pembuka tidak dianggap sebagai konfirmasi.
3. Klik singkat sampai `Chronchi` terpilih.
4. Tahan GPIO3 minimal 2 detik untuk menyimpan pilihan.
5. ESP32 restart dan masuk ke mode Chronchi.

Menu dibatalkan otomatis bila tidak ada input selama 10 detik. Klik singkat ketika menu tidak aktif tetap menjalankan fungsi normal mode masing-masing.

## Hubungkan aplikasi Android ESPBridge

APK debug hasil build berada di `C:/Users/frank/Downloads/ESPBridge/app/build/outputs/apk/debug/app-debug.apk`.

1. Instal APK dan selesaikan onboarding izin Notification Access, Bluetooth, Location, Phone State, dan Background Connection sesuai fitur yang ingin dipakai.
2. Pastikan ESP32 sudah berada di mode Chronchi. Perangkat mengiklankan nama BLE `Chronchi` dan service ESPBridge V1.
3. Di halaman **Setup**, pilih mode koneksi **BLE** atau **Auto**.
4. Tekan **Advanced: direct BLE scan**, pilih perangkat `Chronchi`, lalu tekan **Pair**.
5. Jangan anggap koneksi selesai hanya karena GATT terhubung. Tunggu kartu koneksi menampilkan `ESPBridge V1 handshake: READY`.
6. Setelah READY, aplikasi mengirim sinkronisasi atomik: waktu, status ponsel, jaringan, cuaca/lokasi/navigasi yang tersedia, dan notifikasi terbaru.

Indikator `ACK by ESP32` berarti frame lengkap sudah direkonstruksi dan diterima parser firmware. Status write BLE saja tidak diperlakukan sebagai keberhasilan render.

## Data yang tampil di OLED

- Home: jam/tanggal, koneksi ponsel, jaringan, cuaca, baterai, charging, dan jumlah notifikasi.
- Notifikasi: pesan, profesional, pembayaran masuk/keluar, status pesanan, navigasi, dan sistem.
- Pesan, profesional, dan status pesanan tampil 7 detik; pembayaran 8 detik; sistem 5 detik. Indikator baterai disembunyikan selama layar notifikasi aktif agar waktu terbaca tanpa bertabrakan.
- Navigasi: maneuver, jarak, nama jalan, tujuan, serta waktu tiba bila tersedia.
- Data panjang difragmentasi oleh Android dan direkonstruksi berdasarkan sequence di ESP32; batas JSON logis adalah 768 byte.

Payload yang dikirim adalah data terstruktur dan sudah dinormalisasi, bukan screenshot, bitmap, atau koordinat UI. Android OLED Preview hanya simulasi layout firmware.

## Pindah kembali ke Xiaozhi

1. Tahan GPIO3 minimal 2,5 detik.
2. Lepaskan, lalu klik singkat sampai `Xiaozhi` terpilih.
3. Tahan minimal 2 detik untuk konfirmasi.
4. Setelah restart, jalur Xiaozhi memakai kembali Wi-Fi, wake word, audio, dan pengaturan OLED yang tersimpan.

## Pemeriksaan cepat

- Tidak menemukan `Chronchi`: pastikan perangkat benar-benar berada di mode Chronchi dan Bluetooth aktif.
- Berhenti pada `Waiting for firmware READY`: gunakan firmware dan APK hasil build yang sama, lalu **Forget**, scan, dan Pair ulang.
- READY tetapi data tidak muncul: periksa Notification Access dan izin Android; lihat **LIVE DATA**, **ESP OUTPUT**, dan **View Raw Data**.
- Data terkirim tetapi tidak ACK: cek serial log ESP32 dengan tag `ChronchiBLE` atau `ChronchiProtocol`; parser akan mencatat alasan penolakan.
- Xiaozhi tidak aktif: masuk kembali ke menu mode GPIO3 dan pilih Xiaozhi.

BLE V1 saat ini belum memaksa bonding/LE Secure Connections. Hindari mengirim data sensitif pada lingkungan tak tepercaya sampai pairing terenkripsi ditambahkan dan diuji bersama Android.
