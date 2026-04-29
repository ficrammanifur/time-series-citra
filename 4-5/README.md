# 📊 Time Series Dashboard Bawang Merah  
**Stage 4 & 5: Intelligence Layer + User Visualization**

<p align="center">
  <a href="#">
    <img src="https://img.shields.io/badge/Stage-4_&_5-blueviolet?style=flat-square">
  </a>
  <a href="https://streamlit.io">
    <img src="https://img.shields.io/badge/Built_with-Streamlit-FF4B4B?style=flat-square">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Python-3.11+-blue?style=flat-square">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Analysis-Time_Series_&_AI-green?style=flat-square">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/Platform-Raspberry_Pi_5-orange?style=flat-square">
  </a>
</p>

Sistem **Time Series Citra** untuk monitoring tanaman bawang merah berbasis foto harian (pukul 08:00 WIB).  
Fokus pada **analisis pertumbuhan**, **prediksi**, **assessment risiko**, dan **visualisasi dashboard** yang user-friendly.

---

## 🎯 Tujuan Proyek

- **Stage 4**: Intelligence Layer – Time Series Analysis, Growth Rate, Trend Prediction, Estimasi Sisa Hari Panen, dan Risk Assessment (Kekerian/Kematian).
- **Stage 5**: User Dashboard – Visualisasi foto asli + grafik pertumbuhan, countdown panen, dan laporan AI.

---

## ✨ Fitur Saat Ini (Stage 4 & 5)

- ✅ Loading data otomatis dari Google Sheets (`data_bawang.csv`)
- ✅ Filter data resmi hanya pukul **08:00 WIB**
- ✅ Time Series Analysis & Trend Growth Rate (CAM0 - Depan)
- ✅ Prediksi AI menggunakan **Gemini Flash** (Analisa + Rekomendasi Taktis)
- ✅ Galeri foto 3 sudut terbaru (Depan, Kanan, Atas)
- ✅ Countdown hari menuju panen (target 70 HST)
- ✅ Visualisasi interaktif dengan **Plotly**
- ✅ Integrasi cuaca real-time (Open-Meteo - Tangerang)
- ✅ Tampilan industrial/dark theme yang clean

---

## 🛠️ Teknologi yang Digunakan

- **Framework**: Streamlit
- **Data Processing**: Pandas, NumPy
- **Visualisasi**: Plotly, Altair
- **AI**: Google Gemini Flash (via API)
- **Environment**: Python 3.11+, `python-dotenv`
- **Hardware**: Raspberry Pi 5

---

## 📁 Struktur Direktori

```bash
timeseries-dashboard/
├── dashboard_timeseries.py      # ← Aplikasi utama Streamlit
├── data_bawang.csv              # Data dari Google Sheets (auto update)
├── requirements.txt
├── .env                         # GEMINI_API_KEY
├── venv/
└── README.md
