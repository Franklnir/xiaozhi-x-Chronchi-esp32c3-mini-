#!/usr/bin/env python3
"""
Generate Xichi Product Documentation PDF
Comprehensive product report for website & e-commerce
"""

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm, cm
from reportlab.lib.colors import HexColor, black, white, gray
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_JUSTIFY
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    PageBreak, HRFlowable, KeepTogether
)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
import os

# ─── Colors ───
BRAND_BLUE = HexColor("#1a73e8")
BRAND_DARK = HexColor("#1a1a2e")
BRAND_GREEN = HexColor("#0d9488")
BRAND_ORANGE = HexColor("#f59e0b")
BRAND_RED = HexColor("#ef4444")
LIGHT_GRAY = HexColor("#f3f4f6")
MEDIUM_GRAY = HexColor("#6b7280")
DARK_GRAY = HexColor("#374151")

OUTPUT_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "docs", "Xichi-Product-Documentation.pdf"))

def build_pdf():
    doc = SimpleDocTemplate(
        OUTPUT_PATH,
        pagesize=A4,
        rightMargin=2*cm,
        leftMargin=2*cm,
        topMargin=2*cm,
        bottomMargin=2*cm,
    )

    styles = getSampleStyleSheet()

    # ─── Custom Styles ───
    title_style = ParagraphStyle(
        'CustomTitle', parent=styles['Title'],
        fontSize=28, leading=34, textColor=BRAND_DARK,
        spaceAfter=6*mm, alignment=TA_CENTER,
        fontName='Helvetica-Bold'
    )
    subtitle_style = ParagraphStyle(
        'CustomSubtitle', parent=styles['Normal'],
        fontSize=14, leading=18, textColor=MEDIUM_GRAY,
        spaceAfter=10*mm, alignment=TA_CENTER,
        fontName='Helvetica'
    )
    h1_style = ParagraphStyle(
        'H1', parent=styles['Heading1'],
        fontSize=22, leading=28, textColor=BRAND_DARK,
        spaceBefore=12*mm, spaceAfter=6*mm,
        fontName='Helvetica-Bold',
        borderWidth=0, borderColor=BRAND_BLUE,
        borderPadding=0,
    )
    h2_style = ParagraphStyle(
        'H2', parent=styles['Heading2'],
        fontSize=16, leading=22, textColor=BRAND_BLUE,
        spaceBefore=8*mm, spaceAfter=4*mm,
        fontName='Helvetica-Bold'
    )
    h3_style = ParagraphStyle(
        'H3', parent=styles['Heading3'],
        fontSize=13, leading=18, textColor=DARK_GRAY,
        spaceBefore=5*mm, spaceAfter=3*mm,
        fontName='Helvetica-Bold'
    )
    body_style = ParagraphStyle(
        'CustomBody', parent=styles['Normal'],
        fontSize=10.5, leading=16, textColor=DARK_GRAY,
        spaceAfter=3*mm, alignment=TA_JUSTIFY,
        fontName='Helvetica'
    )
    bullet_style = ParagraphStyle(
        'CustomBullet', parent=body_style,
        leftIndent=8*mm, bulletIndent=3*mm,
        spaceBefore=1*mm, spaceAfter=1*mm,
    )
    highlight_style = ParagraphStyle(
        'Highlight', parent=body_style,
        fontSize=11, leading=16, textColor=BRAND_DARK,
        backColor=LIGHT_GRAY,
        borderWidth=1, borderColor=HexColor("#e5e7eb"),
        borderPadding=8, borderRadius=4,
        spaceBefore=3*mm, spaceAfter=3*mm,
    )
    price_style = ParagraphStyle(
        'Price', parent=styles['Normal'],
        fontSize=20, leading=26, textColor=BRAND_GREEN,
        fontName='Helvetica-Bold', alignment=TA_CENTER,
        spaceBefore=4*mm, spaceAfter=4*mm,
    )
    table_header_style = ParagraphStyle(
        'TableHeader', parent=styles['Normal'],
        fontSize=10, leading=14, textColor=white,
        fontName='Helvetica-Bold', alignment=TA_CENTER,
    )
    table_cell_style = ParagraphStyle(
        'TableCell', parent=styles['Normal'],
        fontSize=9.5, leading=13, textColor=DARK_GRAY,
        fontName='Helvetica',
    )
    table_cell_center = ParagraphStyle(
        'TableCellCenter', parent=table_cell_style,
        alignment=TA_CENTER,
    )
    footer_style = ParagraphStyle(
        'Footer', parent=styles['Normal'],
        fontSize=8, leading=10, textColor=MEDIUM_GRAY,
        alignment=TA_CENTER,
    )

    elements = []

    # ═══════════════════════════════════════════════
    # COVER PAGE
    # ═══════════════════════════════════════════════
    elements.append(Spacer(1, 40*mm))
    elements.append(Paragraph("XICHI", title_style))
    elements.append(Paragraph("Smart Voice Assistant &amp; Notification Reader", subtitle_style))
    elements.append(Spacer(1, 5*mm))
    elements.append(HRFlowable(width="60%", thickness=2, color=BRAND_BLUE, spaceAfter=5*mm))
    elements.append(Spacer(1, 10*mm))

    cover_text = """
    <b>Produk ESP32-C3 Multi-Mode</b><br/><br/>
    Perangkat IoT kompak yang menggabungkan Asisten Suara AI (Xiaozhi), 
    Smart Phone Companion (Chronchi), dan Voice Notification Reader (Xichi) 
    dalam satu board seharga kurang dari Rp 100.000.<br/><br/>
    <b>Tiga Mode dalam Satu Perangkat:</b><br/>
    &#8226; <b>Xiaozhi</b> — Asisten suara AI (ngobrol dengan AI)<br/>
    &#8226; <b>Chronchi</b> — Smart watch mini (notifikasi HP di OLED)<br/>
    &#8226; <b>Xichi</b> — Pembaca notifikasi suara (HP bicara lewat speaker)
    """
    elements.append(Paragraph(cover_text, body_style))
    elements.append(Spacer(1, 15*mm))

    elements.append(Paragraph("Dokumentasi Produk v2.0", ParagraphStyle(
        'CoverFooter', parent=footer_style, fontSize=10, textColor=MEDIUM_GRAY
    )))
    elements.append(Paragraph("September 2026", ParagraphStyle(
        'CoverDate', parent=footer_style, fontSize=10, textColor=MEDIUM_GRAY
    )))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # TABLE OF CONTENTS
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("Daftar Isi", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    toc_items = [
        "1. Ringkasan Produk",
        "2. Spesifikasi Hardware",
        "3. Mode Xiaozhi — Asisten Suara AI",
        "4. Mode Chronchi — Smart Phone Companion",
        "5. Mode Xichi — Voice Notification Reader",
        "6. Aplikasi Pendamping: ESPBridge",
        "7. Daftar Aplikasi yang Didukung",
        "8. Panduan Penggunaan",
        "9. FAQ &amp; Troubleshooting",
        "10. Informasi Produk",
    ]
    for item in toc_items:
        elements.append(Paragraph(item, ParagraphStyle(
            'TOC', parent=body_style, fontSize=12, leading=20,
            leftIndent=10*mm, textColor=BRAND_DARK
        )))
    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 1. RINGKASAN PRODUK
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("1. Ringkasan Produk", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "Xichi adalah perangkat IoT berbasis ESP32-C3 yang dirancang khusus untuk pasar Indonesia. "
        "Dengan harga kurang dari Rp 100.000, perangkat ini menawarkan tiga mode operasi yang "
        "dapat dipilih sesuai kebutuhan: sebagai asisten suara AI, smart phone companion, "
        "atau pembaca notifikasi suara hands-free.",
        body_style
    ))

    elements.append(Paragraph("Keunggulan Utama", h2_style))

    benefits = [
        "<b>Harga Terjangkau</b> — Komponen total hanya Rp 91.000, jauh lebih murah dari smart speaker komersial",
        "<b>Tiga Mode dalam Satu Perangkat</b> — Xiaozhi (AI), Chronchi (Phone), Xichi (Notification Reader)",
        "<b>Hands-Free Notification</b> — Notifikasi HP dibacakan otomatis lewat speaker, mata tetap bebas",
        "<b>Kompatibel 30+ Aplikasi Indonesia</b> — WhatsApp, GoPay, Shopee, DANA, BCA, dan banyak lagi",
        "<b>Open Source &amp; Bisa Dikustomisasi</b> — Firmware dan aplikasi bisa dimodifikasi sesuai kebutuhan",
        "<b>Setup Mudah</b> — Cukup hubungkan WiFi, pasang aplikasi ESPBridge di HP, dan mulai pakai",
        "<b>Hemat Daya</b> — Baterai 1000mAh bisa bertahan hingga 25+ jam dalam mode Xichi",
        "<b>OLED Display</b> — Layar 128x64 pixel untuk menampilkan status dan informasi",
    ]
    for b in benefits:
        elements.append(Paragraph(f"&#8226; {b}", bullet_style))

    elements.append(Spacer(1, 5*mm))
    elements.append(Paragraph(
        "<b>Tagline:</b> <i>\"HP Bicara, Kamu Dengar.\"</i>",
        highlight_style
    ))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 2. SPESIFIKASI HARDWARE
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("2. Spesifikasi Hardware", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph("Komponen Utama", h2_style))

    hw_data = [
        [Paragraph("<b>Komponen</b>", table_header_style),
         Paragraph("<b>Spesifikasi</b>", table_header_style),
         Paragraph("<b>Fungsi</b>", table_header_style)],
        [Paragraph("ESP32-C3", table_cell_style),
         Paragraph("RISC-V 160MHz, 4MB Flash, 400KB RAM", table_cell_style),
         Paragraph("Mikrokontroler utama", table_cell_style)],
        [Paragraph("INMP441", table_cell_style),
         Paragraph("I2S MEMS Microphone, 24kHz", table_cell_style),
         Paragraph("Menangkap suara user", table_cell_style)],
        [Paragraph("MAX98357A", table_cell_style),
         Paragraph("I2S Class D Amp, 3W @ 4\u03a9", table_cell_style),
         Paragraph("Output suara ke speaker", table_cell_style)],
        [Paragraph("Speaker 3W", table_cell_style),
         Paragraph("4 Ohm, 3 Watt", table_cell_style),
         Paragraph("Mengeluarkan suara", table_cell_style)],
        [Paragraph("SSD1306 OLED", table_cell_style),
         Paragraph("128x64 pixel, I2C", table_cell_style),
         Paragraph("Menampilkan status &amp; notifikasi", table_cell_style)],
        [Paragraph("BLE (NimBLE)", table_cell_style),
         Paragraph("Bluetooth 5.0 LE", table_cell_style),
         Paragraph("Koneksi ke HP Android", table_cell_style)],
        [Paragraph("Wi-Fi", table_cell_style),
         Paragraph("802.11 b/g/n, 2.4GHz", table_cell_style),
         Paragraph("Koneksi ke server AI", table_cell_style)],
    ]

    hw_table = Table(hw_data, colWidths=[3.5*cm, 5.5*cm, 6*cm])
    hw_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_BLUE),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(hw_table)

    elements.append(Paragraph("Pinout", h2_style))

    pinout_data = [
        [Paragraph("<b>GPIO</b>", table_header_style),
         Paragraph("<b>Fungsi</b>", table_header_style),
         Paragraph("<b>Koneksi</b>", table_header_style)],
        [Paragraph("GPIO 4", table_cell_center), Paragraph("Mic Data In", table_cell_style), Paragraph("INMP441 SD", table_cell_style)],
        [Paragraph("GPIO 5", table_cell_center), Paragraph("I2S BCLK (shared)", table_cell_style), Paragraph("INMP441 SCK + MAX98357A BCLK", table_cell_style)],
        [Paragraph("GPIO 6", table_cell_center), Paragraph("I2S WS/LRC (shared)", table_cell_style), Paragraph("INMP441 WS + MAX98357A LRC", table_cell_style)],
        [Paragraph("GPIO 7", table_cell_center), Paragraph("Speaker Data Out", table_cell_style), Paragraph("MAX98357A DIN", table_cell_style)],
        [Paragraph("GPIO 8", table_cell_center), Paragraph("I2C SDA", table_cell_style), Paragraph("SSD1306 OLED SDA", table_cell_style)],
        [Paragraph("GPIO 9", table_cell_center), Paragraph("I2C SCL", table_cell_style), Paragraph("SSD1306 OLED SCL", table_cell_style)],
        [Paragraph("GPIO 3", table_cell_center), Paragraph("Tombol BOOT", table_cell_style), Paragraph("Push-to-talk / Mode select", table_cell_style)],
        [Paragraph("GPIO 2", table_cell_center), Paragraph("Tombol RESET", table_cell_style), Paragraph("Reset WiFi (opsional)", table_cell_style)],
        [Paragraph("GPIO 1", table_cell_center), Paragraph("ADC Battery", table_cell_style), Paragraph("Voltage divider 100k/100k", table_cell_style)],
    ]

    pinout_table = Table(pinout_data, colWidths=[2.5*cm, 4.5*cm, 8*cm])
    pinout_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_BLUE),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(pinout_table)

    elements.append(Paragraph("Estimasi Biaya Komponen", h2_style))

    cost_data = [
        [Paragraph("<b>Komponen</b>", table_header_style),
         Paragraph("<b>Harga (IDR)</b>", table_header_style),
         Paragraph("<b>Harga (USD)</b>", table_header_style)],
        [Paragraph("ESP32-C3 DevBoard", table_cell_style), Paragraph("Rp 35.000", table_cell_center), Paragraph("$2.20", table_cell_center)],
        [Paragraph("INMP441 Mic Module", table_cell_style), Paragraph("Rp 25.000", table_cell_center), Paragraph("$1.60", table_cell_center)],
        [Paragraph("MAX98357A Amp Module", table_cell_style), Paragraph("Rp 20.000", table_cell_center), Paragraph("$1.25", table_cell_center)],
        [Paragraph("Speaker 3W 4\u03a9", table_cell_style), Paragraph("Rp 10.000", table_cell_center), Paragraph("$0.65", table_cell_center)],
        [Paragraph("Push Button", table_cell_style), Paragraph("Rp 1.000", table_cell_center), Paragraph("$0.10", table_cell_center)],
        [Paragraph("<b>TOTAL (tanpa OLED)</b>", ParagraphStyle('Bold', parent=table_cell_style, fontName='Helvetica-Bold')),
         Paragraph("<b>Rp 91.000</b>", ParagraphStyle('BoldC', parent=table_cell_center, fontName='Helvetica-Bold')),
         Paragraph("<b>$5.80</b>", ParagraphStyle('BoldC', parent=table_cell_center, fontName='Helvetica-Bold'))],
    ]

    cost_table = Table(cost_data, colWidths=[6*cm, 4.5*cm, 4.5*cm])
    cost_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_BLUE),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('BACKGROUND', (0, -1), (-1, -1), HexColor("#ecfdf5")),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(cost_table)

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 3. MODE XIAOZHI
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("3. Mode Xiaozhi \u2014 Asisten Suara AI", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "Mode Xiaozhi adalah mode utama yang mengubah ESP32-C3 menjadi asisten suara AI. "
        "User dapat berbicara dengan AI lewat mikrofon dan mendengar jawaban lewat speaker. "
        "Menggunakan server Xiaozhi cloud untuk proses speech-to-text (ASR), "
        "large language model (LLM), dan text-to-speech (TTS).",
        body_style
    ))

    elements.append(Paragraph("Fitur Voice Chat", h2_style))
    elements.append(Paragraph(
        "User dapat mengajukan pertanyaan atau memberikan perintah secara natural. "
        "AI akan memproses dan menjawab lewat speaker. Mendukung bahasa Indonesia dan Inggris.",
        body_style
    ))

    voice_features = [
        "<b>Wake Word Detection</b> \u2014 Aktivasi dengan kata \"Hi Lexin\" tanpa pencet tombol. Hands-free 100%.",
        "<b>Push-to-Talk</b> \u2014 Tekan tombol BOOT untuk bicara, lepas untuk kirim. Cocok untuk lingkungan bising.",
        "<b>Auto-Stop Listening</b> \u2014 Otomatis berhenti merekam setelah user diam beberapa detik.",
        "<b>Voice Activity Detection (VAD)</b> \u2014 Mendeteksi kapan user mulai/berhenti bicara secara otomatis.",
        "<b>Multi-bahasa</b> \u2014 Mendukung bahasa Indonesia dan Inggris. Server AI bisa memproses keduanya.",
        "<b>Interrupt TTS</b> \u2014 Bisa memotong jawaban AI yang sedang diputar dengan pencet tombol.",
    ]
    for f in voice_features:
        elements.append(Paragraph(f"\u2022 {f}", bullet_style))

    elements.append(Paragraph("MCP Device Control", h2_style))
    elements.append(Paragraph(
        "Server AI dapat mengontrol ESP32 lewat Model Context Protocol (MCP). "
        "User cukup bicara, AI akan menjalankan perintah ke perangkat.",
        body_style
    ))

    mcp_data = [
        [Paragraph("<b>Perintah Suara</b>", table_header_style),
         Paragraph("<b>MCP Tool</b>", table_header_style),
         Paragraph("<b>Fungsi</b>", table_header_style)],
        [Paragraph("\"Volume setengah\"", table_cell_style), Paragraph("audio_speaker.set_volume", table_cell_style), Paragraph("Atur volume speaker", table_cell_style)],
        [Paragraph("\"Layar lebih terang\"", table_cell_style), Paragraph("screen.set_brightness", table_cell_style), Paragraph("Atur kecerahan OLED", table_cell_style)],
        [Paragraph("\"Ganti tema gelap\"", table_cell_style), Paragraph("screen.set_theme", table_cell_style), Paragraph("Ganti tema light/dark", table_cell_style)],
        [Paragraph("\"Baterai berapa?\"", table_cell_style), Paragraph("get_device_status", table_cell_style), Paragraph("Info baterai, volume, sinyal", table_cell_style)],
        [Paragraph("\"Restart dong\"", table_cell_style), Paragraph("reboot", table_cell_style), Paragraph("Restart perangkat", table_cell_style)],
        [Paragraph("\"Putar lagu\"", table_cell_style), Paragraph("PlayLocalSong", table_cell_style), Paragraph("Putar lagu dari SPIFFS", table_cell_style)],
    ]

    mcp_table = Table(mcp_data, colWidths=[4.5*cm, 5*cm, 5.5*cm])
    mcp_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_BLUE),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ]))
    elements.append(mcp_table)

    elements.append(Paragraph("WiFi Configuration", h2_style))
    elements.append(Paragraph(
        "Setup WiFi sangat mudah: saat pertama kali dinyalakan, perangkat akan membuat hotspot "
        "dengan nama \"Xiaozhi-XXXX\". Hubungkan HP ke hotspot tersebut, buka browser, "
        "masukkan kredensial WiFi rumah, dan perangkat akan restart dan terhubung otomatis.",
        body_style
    ))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 4. MODE CHRONCHI
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("4. Mode Chronchi \u2014 Smart Phone Companion", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "Mode Chronchi mengubah ESP32-C3 menjadi \"smart watch mini\" yang terhubung ke HP Android "
        "via Bluetooth Low Energy (BLE). Perangkat menampilkan notifikasi, status baterai, "
        "cuaca, dan navigasi di OLED display.",
        body_style
    ))

    elements.append(Paragraph("Data yang Diterima dari HP", h2_style))

    chronchi_features = [
        "<b>Notifikasi Masuk</b> \u2014 WhatsApp, Telegram, email, SMS, dan 30+ aplikasi lainnya tampil di OLED",
        "<b>Status Baterai HP</b> \u2014 Level baterai dan status charging HP ditampilkan di status bar",
        "<b>Status Jaringan</b> \u2014 WiFi/4G/5G dan signal strength HP ditampilkan real-time",
        "<b>Cuaca</b> \u2014 Suhu dan lokasi dari HP ditampilkan di home screen",
        "<b>Navigasi Google Maps</b> \u2014 Arah belok, jarak, dan nama jalan tampil saat navigasi aktif",
        "<b>Sinkronisasi Jam</b> \u2014 Jam otomatis tersinkron dengan HP",
    ]
    for f in chronchi_features:
        elements.append(Paragraph(f"\u2022 {f}", bullet_style))

    elements.append(Paragraph("BLE Protocol", h2_style))
    elements.append(Paragraph(
        "Komunikasi menggunakan protokol ESPBridge V1 yang dikembangkan khusus. "
        "Data dikirim dalam format JSON yang terfragmentasi (max 768 byte per message). "
        "Koneksi terenkripsi menggunakan BLE Secure Connections dengan numeric comparison pairing.",
        body_style
    ))

    elements.append(Paragraph("OTA Update via BLE", h2_style))
    elements.append(Paragraph(
        "Firmware bisa diupdate langsung dari HP via Bluetooth, tanpa perlu kabel USB. "
        "Prosesnya aman dengan verifikasi checksum dan rollback otomatis jika gagal.",
        body_style
    ))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 5. MODE XICHI
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("5. Mode Xichi \u2014 Voice Notification Reader", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "<b>\"HP Bicara, Kamu Dengar.\"</b>",
        ParagraphStyle('Tagline', parent=body_style, fontSize=14, leading=18,
                       textColor=BRAND_BLUE, fontName='Helvetica-Bold',
                       alignment=TA_CENTER, spaceBefore=3*mm, spaceAfter=5*mm)
    ))

    elements.append(Paragraph(
        "Mode Xichi adalah fitur unggulan yang menggabungkan koneksi BLE ke HP (dari Chronchi) "
        "dengan kemampuan suara AI (dari Xiaozhi). Ketika notifikasi masuk dari HP, perangkat "
        "membacakannya secara otomatis lewat speaker. Tidak perlu melihat layar \u2014 "
        "cukup dengarkan.",
        body_style
    ))

    elements.append(Paragraph("Cara Kerja", h2_style))

    flow_steps = [
        "<b>1. Notifikasi Masuk</b> \u2014 HP menerima pesan WA, email, notifikasi pembayaran, dll",
        "<b>2. Kirim via BLE</b> \u2014 Aplikasi ESPBridge mengirim data notifikasi ke ESP32 via Bluetooth",
        "<b>3. Format &amp; Kirim ke Server</b> \u2014 ESP32 memformat notifikasi dan mengirim ke server AI via WiFi",
        "<b>4. AI Proses</b> \u2014 Server AI memproses notifikasi dan menghasilkan respons suara natural",
        "<b>5. Speaker Membacakan</b> \u2014 Suara AI keluar dari speaker MAX98357A dengan volume 100%",
        "<b>6. Standby</b> \u2014 Setelah selesai, perangkat masuk standby menunggu notifikasi berikutnya",
    ]
    for s in flow_steps:
        elements.append(Paragraph(f"\u2022 {s}", bullet_style))

    elements.append(Paragraph("Aturan Mode Xichi", h2_style))

    rules_data = [
        [Paragraph("<b>Aturan</b>", table_header_style),
         Paragraph("<b>Detail</b>", table_header_style)],
        [Paragraph("\u274c Tidak ada wake word", table_cell_style),
         Paragraph("Wake word dimatikan total untuk menghindari konflik saat speaker aktif", table_cell_style)],
        [Paragraph("\u274c Tidak masuk listening", table_cell_style),
         Paragraph("Setelah notifikasi selesai dibacakan, perangkat masuk standby (bukan listening)", table_cell_style)],
        [Paragraph("\u274c Tidak bisa ngobrol", table_cell_style),
         Paragraph("Tidak ada input mikrofon ke server (kecuali push-to-talk manual)", table_cell_style)],
        [Paragraph("\u2705 Semua notif dibacakan", table_cell_style),
         Paragraph("Tidak ada filter, semua kategori notifikasi akan dibacakan", table_cell_style)],
        [Paragraph("\u2705 Volume 100%", table_cell_style),
         Paragraph("Volume selalu penuh, tidak auto-adjust. Pastikan terdengar jelas", table_cell_style)],
        [Paragraph("\u2705 Push-to-talk aktif", table_cell_style),
         Paragraph("Tombol BOOT tetap bisa dipakai untuk ngobrol manual dengan AI", table_cell_style)],
        [Paragraph("\u2705 Notifikasi di-queue", table_cell_style),
         Paragraph("Jika notifikasi baru masuk saat sedang bicara, masuk antrian dan dibacakan berikutnya", table_cell_style)],
    ]

    rules_table = Table(rules_data, colWidths=[4.5*cm, 10.5*cm])
    rules_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_DARK),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(rules_table)

    elements.append(Paragraph("Contoh Notifikasi yang Dibacakan", h2_style))

    notif_examples = [
        ["GoPay", "Pembayaran", "\"Ada pembayaran masuk dari GoPay sebesar lima puluh ribu rupiah.\""],
        ["WhatsApp", "Pesan", "\"Pesan masuk dari Budi: otw ya, lima menit lagi sampai.\""],
        ["Shopee", "Pesanan", "\"Update pesanan dari Shopee: pesanan sudah dikirim, estimasi tiba besok.\""],
        ["Gmail", "Email", "\"Email masuk di Gmail: Undangan Meeting Project X.\""],
        ["BCA", "Banking", "\"Notifikasi dari BCA mobile: transfer berhasil sebesar satu juta rupiah.\""],
        ["Grab", "Transport", "\"Notifikasi dari Grab: driver sudah sampai di lokasi Anda.\""],
    ]

    ne_data = [[Paragraph("<b>Aplikasi</b>", table_header_style),
                Paragraph("<b>Kategori</b>", table_header_style),
                Paragraph("<b>Contoh Bacaan Suara</b>", table_header_style)]]
    for row in notif_examples:
        ne_data.append([
            Paragraph(row[0], table_cell_style),
            Paragraph(row[1], table_cell_style),
            Paragraph(row[2], table_cell_style),
        ])

    ne_table = Table(ne_data, colWidths=[2.5*cm, 2.5*cm, 10*cm])
    ne_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_GREEN),
        ('TEXTCOLOR', (0, 0), (-1, 0), white),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ]))
    elements.append(ne_table)

    elements.append(Paragraph("Latency", h2_style))
    elements.append(Paragraph(
        "Dari notifikasi masuk di HP hingga suara keluar dari speaker, "
        "total waktu yang dibutuhkan sekitar 1-5 detik, tergantung panjang teks "
        "dan kecepatan koneksi internet. Ini sangat cepat untuk ukuran notifikasi hands-free.",
        body_style
    ))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 6. ESPBRIDGE APP
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("6. Aplikasi Pendamping: ESPBridge", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "ESPBridge adalah aplikasi Android yang berjalan di HP dan mengirimkan data "
        "notifikasi, status HP, navigasi, dan informasi lainnya ke ESP32 via Bluetooth Low Energy. "
        "Aplikasi ini dikembangkan oleh irsyadlabs dan tersedia dalam bentuk source code yang bisa dimodifikasi.",
        body_style
    ))

    elements.append(Paragraph("Fitur ESPBridge", h2_style))

    espbridge_features = [
        "<b>Notification Listener</b> \u2014 Membaca semua notifikasi masuk di HP dan mengirim ke ESP32 via BLE",
        "<b>Notification Normalizer</b> \u2014 Memformat notifikasi dari berbagai aplikasi ke format standar",
        "<b>Navigation Parser</b> \u2014 Membaca instruksi navigasi dari Google Maps dan mengirim ke ESP32",
        "<b>Phone Status Monitor</b> \u2014 Mengirim status baterai, sinyal, dan jaringan HP secara berkala",
        "<b>Weather Data</b> \u2014 Mengirim data cuaca dan lokasi dari HP ke ESP32",
        "<b>BLE Connection Manager</b> \u2014 Mengelola koneksi BLE dengan auto-reconnect",
        "<b>OTA Firmware Update</b> \u2014 Mengirim firmware update ke ESP32 via BLE",
        "<b>Pairing &amp; Security</b> \u2014 Pairing aman dengan numeric comparison dan enkripsi",
    ]
    for f in espbridge_features:
        elements.append(Paragraph(f"\u2022 {f}", bullet_style))

    elements.append(Paragraph("Notification Normalizer", h2_style))
    elements.append(Paragraph(
        "ESPBridge memiliki sistem cerdas yang menormalisasi notifikasi dari berbagai aplikasi. "
        "Sistem ini mendeteksi kategori notifikasi (pesan, pembayaran, pesanan, email, dll) "
        "dan memformatnya agar ESP32 bisa membacakannya dengan benar.",
        body_style
    ))

    norm_features = [
        "<b>Deteksi Otomatis</b> \u2014 Mendeteksi apakah notifikasi adalah pesan, pembayaran, pesanan, atau email",
        "<b>Filter Promo</b> \u2014 Notifikasi promo/diskon otomatis difilter agar tidak dibacakan",
        "<b>Ekstraksi Nominal</b> \u2014 Mendeteksi nominal pembayaran (Rp XXX) dari teks notifikasi",
        "<b>Deteksi Arah Transfer</b> \u2014 Mendeteksi apakah uang masuk atau keluar",
        "<b>Ekstraksi Pengirim/Penerima</b> \u2014 Mengambil nama pengirim atau penerima dari teks",
        "<b>Sanitasi Teks</b> \u2014 Membersihkan teks dari karakter aneh dan memotong jika terlalu panjang",
    ]
    for f in norm_features:
        elements.append(Paragraph(f"\u2022 {f}", bullet_style))

    elements.append(Paragraph("Teknologi ESPBridge", h2_style))

    tech_data = [
        [Paragraph("<b>Aspek</b>", table_header_style),
         Paragraph("<b>Detail</b>", table_header_style)],
        [Paragraph("Platform", table_cell_style), Paragraph("Android (Kotlin)", table_cell_style)],
        [Paragraph("UI Framework", table_cell_style), Paragraph("Jetpack Compose", table_cell_style)],
        [Paragraph("BLE Stack", table_cell_style), Paragraph("Android BLE API + NimBLE Protocol", table_cell_style)],
        [Paragraph("Architecture", table_cell_style), Paragraph("MVVM + Repository Pattern", table_cell_style)],
        [Paragraph("Min SDK", table_cell_style), Paragraph("Android 8.0 (API 26)", table_cell_style)],
        [Paragraph("Package", table_cell_style), Paragraph("com.irsyadlabs.espbridge", table_cell_style)],
    ]

    tech_table = Table(tech_data, colWidths=[4*cm, 11*cm])
    tech_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_DARK),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(tech_table)

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 7. DAFTAR APLIKASI YANG DIDUKUNG
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("7. Daftar Aplikasi yang Didukung", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "ESPBridge mendukung 35+ aplikasi populer di Indonesia. Notifikasi dari aplikasi-aplikasi ini "
        "akan otomatis dikirim ke ESP32 dan dibacakan (di mode Xichi) atau ditampilkan di OLED (di mode Chronchi).",
        body_style
    ))

    elements.append(Paragraph("Messaging &amp; Social", h2_style))
    apps_social = [
        "WhatsApp", "WhatsApp Business", "Telegram", "Instagram", "Facebook",
        "Messenger", "TikTok", "X (Twitter)", "Threads"
    ]
    elements.append(Paragraph(", ".join(apps_social), body_style))

    elements.append(Paragraph("Professional", h2_style))
    apps_prof = ["Gmail", "LinkedIn", "JobStreet"]
    elements.append(Paragraph(", ".join(apps_prof), body_style))

    elements.append(Paragraph("Payment (Pembayaran)", h2_style))
    apps_pay = ["DANA", "OVO", "GoPay / Gojek", "ShopeePay"]
    elements.append(Paragraph(", ".join(apps_pay), body_style))

    elements.append(Paragraph("Banking", h2_style))
    apps_bank = [
        "myBCA", "BCA mobile", "BRImo", "Livin' by Mandiri", "wondr by BNI",
        "OCTO / CIMB Niaga", "SeaBank", "Bank Jago", "Superbank"
    ]
    elements.append(Paragraph(", ".join(apps_bank), body_style))

    elements.append(Paragraph("Marketplace (Belanja)", h2_style))
    apps_shop = ["Shopee", "Shopee Partner", "Tokopedia", "Lazada", "Gojek", "Grab"]
    elements.append(Paragraph(", ".join(apps_shop), body_style))

    elements.append(Paragraph("System Sources", h2_style))
    apps_sys = ["Navigation (Google Maps)", "Weather", "Location", "Phone Status", "Network Status"]
    elements.append(Paragraph(", ".join(apps_sys), body_style))

    elements.append(Spacer(1, 5*mm))
    elements.append(Paragraph(
        "<b>Catatan:</b> Aplikasi lain yang tidak ada di daftar juga tetap bisa mengirim notifikasi "
        "ke ESP32. Sistem akan memformatnya sebagai notifikasi generik.",
        highlight_style
    ))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 8. PANDUAN PENGGUNAAN
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("8. Panduan Penggunaan", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph("Pertama Kali: Setup WiFi", h2_style))

    setup_steps = [
        "<b>Langkah 1:</b> Nyalakan perangkat. Tekan tombol BOOT dalam 5 detik.",
        "<b>Langkah 2:</b> Perangkat membuat hotspot \"Xiaozhi-XXXX\". Hubungkan HP ke hotspot ini.",
        "<b>Langkah 3:</b> Buka browser di HP, akses http://192.168.4.1",
        "<b>Langkah 4:</b> Masukkan nama WiFi dan password WiFi rumah Anda.",
        "<b>Langkah 5:</b> Perangkat restart dan otomatis terhubung ke WiFi.",
    ]
    for s in setup_steps:
        elements.append(Paragraph(f"\u2022 {s}", bullet_style))

    elements.append(Paragraph("Setup Aplikasi ESPBridge (untuk Chronchi &amp; Xichi)", h2_style))

    esp_steps = [
        "<b>Langkah 1:</b> Install aplikasi ESPBridge di HP Android.",
        "<b>Langkah 2:</b> Buka aplikasi, berikan izin notifikasi dan lokasi.",
        "<b>Langkah 3:</b> Pilih sumber notifikasi yang ingin dipantau (WhatsApp, GoPay, dll).",
        "<b>Langkah 4:</b> Aplikasi akan scan dan menemukan perangkat \"Chronchi-XXYY\".",
        "<b>Langkah 5:</b> Hubungkan dan konfirmasi pairing dengan kode 6 digit.",
    ]
    for s in esp_steps:
        elements.append(Paragraph(f"\u2022 {s}", bullet_style))

    elements.append(Paragraph("Memilih Mode", h2_style))

    mode_steps = [
        "<b>Langkah 1:</b> Tahan tombol BOOT selama 2.5 detik untuk membuka menu mode.",
        "<b>Langkah 2:</b> OLED akan menampilkan 3 pilihan: Xiaozhi, Chronchi, Xichi.",
        "<b>Langkah 3:</b> Klik tombol untuk memilih mode yang diinginkan.",
        "<b>Langkah 4:</b> Tahan tombol selama 2 detik untuk konfirmasi. Perangkat restart.",
    ]
    for s in mode_steps:
        elements.append(Paragraph(f"\u2022 {s}", bullet_style))

    elements.append(Paragraph("Mode Comparison", h2_style))

    mode_data = [
        [Paragraph("<b>Fitur</b>", table_header_style),
         Paragraph("<b>Xiaozhi</b>", table_header_style),
         Paragraph("<b>Chronchi</b>", table_header_style),
         Paragraph("<b>Xichi</b>", table_header_style)],
        [Paragraph("Wi-Fi", table_cell_style),
         Paragraph("\u2705", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center)],
        [Paragraph("BLE", table_cell_style),
         Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center), Paragraph("\u2705", table_cell_center)],
        [Paragraph("Mikrofon", table_cell_style),
         Paragraph("\u2705", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("\u2705*", table_cell_center)],
        [Paragraph("Speaker", table_cell_style),
         Paragraph("\u2705", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center)],
        [Paragraph("Wake Word", table_cell_style),
         Paragraph("\u2705", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("\u274c", table_cell_center)],
        [Paragraph("Ngobrol AI", table_cell_style),
         Paragraph("\u2705", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("PTT only", table_cell_center)],
        [Paragraph("Notif OLED", table_cell_style),
         Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center), Paragraph("Minimal", table_cell_center)],
        [Paragraph("Notif Suara", table_cell_style),
         Paragraph("\u274c", table_cell_center), Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center)],
        [Paragraph("Baterai HP", table_cell_style),
         Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center), Paragraph("\u2705", table_cell_center)],
        [Paragraph("Navigasi", table_cell_style),
         Paragraph("\u274c", table_cell_center), Paragraph("\u2705", table_cell_center), Paragraph("\u2705", table_cell_center)],
    ]

    mode_table = Table(mode_data, colWidths=[3.5*cm, 3*cm, 3*cm, 3*cm])
    mode_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_DARK),
        ('BACKGROUND', (1, 0), (1, 0), HexColor("#3b82f6")),
        ('BACKGROUND', (2, 0), (2, 0), HexColor("#8b5cf6")),
        ('BACKGROUND', (3, 0), (3, 0), HexColor("#10b981")),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 5),
        ('RIGHTPADDING', (0, 0), (-1, -1), 5),
    ]))
    elements.append(mode_table)

    elements.append(Paragraph("* Push-to-talk only (tombol), bukan wake word", ParagraphStyle(
        'Note', parent=body_style, fontSize=9, textColor=MEDIUM_GRAY, fontName='Helvetica-Oblique'
    )))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 9. FAQ
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("9. FAQ &amp; Troubleshooting", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    faq_items = [
        ("Apakah butuh internet?",
         "Ya, mode Xiaozhi dan Xichi butuh internet untuk terhubung ke server AI. Mode Chronchi tidak butuh internet (BLE only)."),
        ("Berapa biaya bulanan?",
         "Server Xiaozhi (xiaozhi.me) tersedia gratis. Tidak ada biaya bulanan."),
        ("Apakah bisa tanpa server?",
         "Mode Chronchi bisa jalan tanpa server (BLE only). Mode Xichi dan Xiaozhi butuh server AI."),
        ("Berapa lama baterai tahan?",
         "Dengan baterai 1000mAh: mode Xichi ~25 jam, mode Chronchi ~66 jam, mode Xiaozhi ~12-33 jam."),
        ("HP apa yang didukung?",
         "Android 8.0 ke atas. Aplikasi ESPBridge perlu diinstall di HP."),
        ("Apakah bisa dikustomisasi?",
         "Ya! Firmware open source, bisa dimodifikasi. ESPBridge app juga bisa diubah."),
        ("Bagaimana jika WiFi putus?",
         "Perangkat otomatis reconnect. Notifikasi yang masuk saat WiFi putus tidak hilang (tersimpan di HP)."),
        ("Apakah aman?",
         "Koneksi BLE terenkripsi (Secure Connections). Data tidak disimpan di cloud pihak ketiga."),
        ("Bisa dipakai untuk apa lagi?",
         "Smart home control, monitoring baterai, navigasi suara, speaker Bluetooth, dan banyak lagi."),
        ("Berapa harganya?",
         "Komponen total sekitar Rp 91.000 (tanpa OLED) atau Rp 116.000 (dengan OLED)."),
    ]

    for q, a in faq_items:
        elements.append(Paragraph(f"<b>Q: {q}</b>", ParagraphStyle(
            'FAQ_Q', parent=body_style, fontName='Helvetica-Bold', textColor=BRAND_DARK,
            spaceBefore=4*mm, spaceAfter=1*mm
        )))
        elements.append(Paragraph(f"A: {a}", ParagraphStyle(
            'FAQ_A', parent=body_style, leftIndent=5*mm, spaceAfter=2*mm
        )))

    elements.append(PageBreak())

    # ═══════════════════════════════════════════════
    # 10. INFORMASI PRODUK
    # ═══════════════════════════════════════════════
    elements.append(Paragraph("10. Informasi Produk", h1_style))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph("Spesifikasi Perangkat", h2_style))

    spec_data = [
        [Paragraph("<b>Parameter</b>", table_header_style),
         Paragraph("<b>Nilai</b>", table_header_style)],
        [Paragraph("Mikrokontroler", table_cell_style), Paragraph("ESP32-C3 (RISC-V, 160MHz)", table_cell_style)],
        [Paragraph("Flash Memory", table_cell_style), Paragraph("4 MB", table_cell_style)],
        [Paragraph("RAM", table_cell_style), Paragraph("400 KB", table_cell_style)],
        [Paragraph("Wi-Fi", table_cell_style), Paragraph("802.11 b/g/n, 2.4 GHz", table_cell_style)],
        [Paragraph("Bluetooth", table_cell_style), Paragraph("BLE 5.0 (NimBLE)", table_cell_style)],
        [Paragraph("Display", table_cell_style), Paragraph("SSD1306 OLED 128x64 (opsional)", table_cell_style)],
        [Paragraph("Audio Input", table_cell_style), Paragraph("INMP441 I2S MEMS Mic, 24kHz", table_cell_style)],
        [Paragraph("Audio Output", table_cell_style), Paragraph("MAX98357A I2S Amp, 3W @ 4\u03a9", table_cell_style)],
        [Paragraph("Speaker", table_cell_style), Paragraph("3W, 4 Ohm", table_cell_style)],
        [Paragraph("Konektivitas HP", table_cell_style), Paragraph("Bluetooth Low Energy (BLE)", table_cell_style)],
        [Paragraph("Konektivitas Server", table_cell_style), Paragraph("WiFi (WebSocket/MQTT)", table_cell_style)],
        [Paragraph("Power Supply", table_cell_style), Paragraph("5V USB / Battery Li-ion", table_cell_style)],
        [Paragraph("Dimensi", table_cell_style), Paragraph("~5cm x 3cm x 2cm (tergantung casing)", table_cell_style)],
        [Paragraph("Firmware", table_cell_style), Paragraph("ESP-IDF v5.5, Open Source", table_cell_style)],
        [Paragraph("Aplikasi HP", table_cell_style), Paragraph("ESPBridge (Android, Open Source)", table_cell_style)],
    ]

    spec_table = Table(spec_data, colWidths=[5*cm, 10*cm])
    spec_table.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), BRAND_DARK),
        ('GRID', (0, 0), (-1, -1), 0.5, HexColor("#d1d5db")),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [white, LIGHT_GRAY]),
        ('VALIGN', (0, 0), (-1, -1), 'MIDDLE'),
        ('TOPPADDING', (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LEFTPADDING', (0, 0), (-1, -1), 6),
        ('RIGHTPADDING', (0, 0), (-1, -1), 6),
    ]))
    elements.append(spec_table)

    elements.append(Paragraph("Isi Paket", h2_style))

    package_items = [
        "1x ESP32-C3 DevBoard (pre-flashed firmware Xichi)",
        "1x INMP441 Mikrofon Module",
        "1x MAX98357A Amplifier Module",
        "1x Speaker 3W 4\u03a9",
        "1x Push Button",
        "1x SSD1306 OLED Display (opsional)",
        "1x Kabel USB untuk power &amp; flash",
        "1x Panduan penggunaan",
    ]
    for item in package_items:
        elements.append(Paragraph(f"\u2022 {item}", bullet_style))

    elements.append(Spacer(1, 10*mm))
    elements.append(HRFlowable(width="100%", thickness=1, color=BRAND_BLUE, spaceAfter=5*mm))

    elements.append(Paragraph(
        "<b>Xichi</b> \u2014 Smart Voice Assistant &amp; Notification Reader<br/>"
        "\"HP Bicara, Kamu Dengar.\"<br/><br/>"
        "Dikembangkan dengan \u2764\ufe0f untuk pasar Indonesia.<br/>"
        "Open Source | ESP32-C3 | BLE + WiFi | AI Voice",
        ParagraphStyle('FinalFooter', parent=body_style, fontSize=11, leading=16,
                       textColor=BRAND_DARK, alignment=TA_CENTER, fontName='Helvetica',
                       spaceBefore=5*mm)
    ))

    # ─── Build ───
    doc.build(elements)
    print(f"PDF generated: {OUTPUT_PATH}")
    print(f"Size: {os.path.getsize(OUTPUT_PATH) / 1024:.1f} KB")

if __name__ == "__main__":
    build_pdf()
