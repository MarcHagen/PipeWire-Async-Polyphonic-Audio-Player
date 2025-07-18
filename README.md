# PAPA - PipeWire Async Polyphonic Audio Player

A powerful daemon for playing multiple audio tracks simultaneously through different audio channels with independent control.

## Overview

PAPA (PipeWire Async Polyphonic Audio Player) is designed for scenarios where you need precise control over multiple audio sources. It's ideal for:

- Museum installations
- Interactive exhibits
- Multi-zone audio playback
- Spatial audio experiences
- Any application requiring multiple synchronized or independent audio streams

## Features

- Play multiple audio tracks simultaneously with independent control
- Map audio channels to specific outputs
- Control playback through simple socket commands
- Independent volume control for each track
- Loop mode for continuous playback
- Simple command-line interface

## Installation

### Dependencies

- PipeWire (for audio playback)
- libyaml (for configuration parsing)
- libsndfile (for audio file loading)

### Building

```bash
make
```

### Installing

```bash
sudo make install
```

## Usage

### Starting the Server

```bash
papad
```

This will start the audio player daemon. The server listens for commands on a Unix socket at `/var/run/papad.sock`.

### Client Commands

The client utility can be used to control the running server:

```bash
papa --play track1        # Play a specific track
papa --stop track1        # Stop a specific track
papa --stop-all           # Stop all playing tracks
papa --list               # List all available tracks
papa --status             # Show current playback status
papa --reload             # Reload configuration
```

## Configuration

The configuration file is in YAML format and is searched for in the following locations:

- `./config/default.yml`
- `./default.yml`
- `/etc/papa/config.yml`

### Example Configuration

```yaml
logging:
  level: INFO

tracks:
  - id: track1
    file_path: /path/to/track1.wav
    loop: true
    volume: 0.8
    output:
      device: default
      mapping:
        - FL
        - FR

  - id: track2
    file_path: /path/to/track2.wav
    loop: false
    volume: 1.0
    output:
      device: default
      mapping:
        - AUX0
        - AUX1
```

## Socket Protocol

You can control PAPA programmatically by sending commands to the Unix socket:

- `play <track_id>` - Play a track
- `stop <track_id>` - Stop a track
- `stop-all` - Stop all tracks
- `list` - List available tracks
- `status` - Get player status
- `reload` - Reload configuration

## License

MIT
