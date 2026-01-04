For commercial licensing inquiries, please contact: flyingzeroc@gmail.com

<div align="center">

# 🚀 pingmon - Advanced Ping Monitor

![Terminal Screenshot](https://img.shields.io/badge/Platform-Linux-blue?style=for-the-badge)
![License](https://img.shields.io/badge/License-Modified_MIT-green?style=for-the-badge)
![Version](https://img.shields.io/badge/Version-0.39-orange?style=for-the-badge)
![Language](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c)
![Status](https://img.shields.io/badge/Status-Production_Ready-success?style=for-the-badge)

*A feature-rich terminal-based ping monitor with real-time statistics, visual history graphs, and network quality analysis*

[✨ Features](#-features) • [📸 Screenshots](#-screenshots) • [🚀 Quick Start](#-quick-start) • [📖 Usage](#-usage) • [🛡️ Security](#️-security) • [📄 License](#-license)

</div>

## 📸 Screenshots

<div align="center">

| Normal Operation | MyIP Information |
|:---:|:---:|
| ![Normal Operation](screenshots/screenshot-normal.png) | ![MyIP Information](screenshots/screenshot-myip.png) |
| *Real-time monitoring with color-coded metrics* | *Public IP, ISP, and location details* |

| Poor Connection | History Visualization |
|:---:|:---:|
| ![Poor Connection](screenshots/screenshot-bad.png) | ![History Graph](screenshots/screenshot-history.png) |
| *Automatic warning/critical thresholds* | *Visual trend analysis over time* |

</div>

## ✨ Features

### 📊 **Real-time Monitoring**
- **Live ICMP latency tracking** with system `ping` integration
- **Configurable WARN/CRIT thresholds** (30ms/60ms default)
- **Dynamic progress bars** for quality & stability assessment
- **Color-coded metrics** for instant visual feedback

### 📈 **Advanced Visualization**
- **Sliding history graph** (40-point window, color-coded)
- **Quality scoring** based on latency measurements
- **Stability scoring** derived from packet loss percentage
- **Professional terminal UI** with ANSI escape codes

### 🎮 **Interactive Controls**
| Key | Action | Description |
|-----|--------|-------------|
| `q` | Quit | Clean exit with terminal restoration |
| `r` | Reset | Clear all statistics and history |
| `m` | MyIP | Toggle public IP information display |

### 🌐 **Network Intelligence**
- **Public IPv4 detection** with multiple fallback sources
- **ISP/Organization lookup** via ipinfo.io
- **Geolocation** (country-level, privacy-conscious)
- **Secure fetching** (no shell execution, safe `execvp` calls)

### 🛡️ **Security & Reliability**
- **No shell injection vectors** (zero `system()` or `popen()` calls)
- **Comprehensive signal handling** (SIGINT, SIGTERM, SIGSEGV, SIGPIPE, SIGABRT)
- **Graceful crash recovery** with terminal state preservation
- **IPv4 validation** with safe fallback to 8.8.8.8
- **Non-blocking I/O** for responsive user experience

### 📊 **Displayed Metrics**
| Metric | Description | Format |
|--------|-------------|--------|
| **Last** | Most recent ping latency | `45.2 ms` |
| **Avg** | Mean latency over session | `32.1 ms` |
| **Loss** | Packet loss percentage | `0.00 %` |
| **Status** | Connection state | `OK` / `TIMEOUT` |
| **Sent/Recv** | Packet counters | `15/15` |
| **Quality** | Latency-based score | `▮▮▮▮▮▮▮▮▮▮` (bar) |
| **Stability** | Loss-based score | `95%` |

## 🚀 Quick Start

### Prerequisites
```bash
# Debian/Ubuntu/Raspberry Pi OS
sudo apt update
sudo apt install gcc curl

# RHEL/CentOS/Fedora
sudo yum install gcc curl
