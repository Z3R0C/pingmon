# Changelog

All notable changes to pingmon will be documented in this file.

## [0.39] - 2026-01-01

### Added
- Modified MIT License with commercial use restriction
- Complete security overhaul
- Safe execvp() calls replacing popen()
- Enhanced signal handling (SIGINT, SIGTERM, SIGSEGV, SIGPIPE, SIGABRT)
- Improved error handling throughout

### Fixed
- Command injection vulnerabilities
- Buffer overflow risks
- Race conditions in process handling
- Memory leaks in signal handlers
- Terminal restoration on crash

### Changed
- Updated footer to show usage instructions instead of version
- Version now only in header line
- Improved history line width calculation
- Better color consistency between quality bars and history

## [0.38] - 2025-12-31

### Added
- Visual history graph with color-coded trends
- IPv4 address validation with silent fallback
- Hidden cursor for cleaner UI
- MyIP information toggle (IP, ISP, Location)
- Quality and stability scoring algorithms

### Fixed
- Terminal width calculation issues
- ANSI color code inconsistencies
- Memory management in IP fetching

## [0.37] - 2025-12-30

### Added
- Basic ping monitoring functionality
- Real-time statistics display
- Color-coded latency indicators
- Interactive keyboard controls
- Packet loss calculation

### Security
- Initial security implementation
- Basic input validation
