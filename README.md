# Smart Plant — Tugas Besar IoT 2026

Dashboard web untuk sistem penyiraman dan pencahayaan tanaman otomatis berbasis NodeMCU ESP8266.

![Tampilan dashboard](preview.png)

## Cara kerja

- NodeMCU mengirim data sensor tiap 2 detik ke broker MQTT publik `broker.hivemq.com` (port 1883).
- Halaman ini terhubung ke broker yang sama lewat WebSocket aman (`wss://broker.hivemq.com:8884/mqtt`).
- Topik data: `smartplant/tubes2026/data` — JSON `{"soil":41,"dark":false,"autoMode":true,"pump":false,"lamp":false}`
- Topik kendali: `smartplant/tubes2026/control` — `MODE_AUTO`, `MODE_MANUAL`, `PUMP_ON`, `PUMP_OFF`, `LAMP_ON`, `LAMP_OFF`

## Isi halaman

- Model 3D tanaman yang mengikuti sensor: daun layu saat tanah kering, air menetes saat pompa menyala, grow light dan suasana siang/malam mengikuti LDR.
- Alat ukur kelembapan tanah dengan penanda batas siram 40%.
- Kendali mode Otomatis/Manual, pompa, dan grow light.
- Grafik 5 menit terakhir, statistik sesi, dan catatan kejadian.
- Tombol **Simulasi** (atau buka `index.html#simulasi`) untuk mencoba tampilan tanpa alat. Data simulasi dibuat browser, bukan dari sensor.

## Menjalankan

Buka `index.html` langsung di browser, atau aktifkan GitHub Pages:
**Settings → Pages → Source: Deploy from a branch → `main` / `(root)`**.

Tidak ada proses build. Pustaka `mqtt.js` 4.3.7 dan `three.js` r149 sudah disertakan di folder `vendor/`.
