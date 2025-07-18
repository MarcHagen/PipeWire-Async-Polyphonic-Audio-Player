# Tools Directory

This directory contains supplementary tools and utilities for PAPA (PipeWire Async Polyphonic Audio Player).

## Client Tool

`client.c` - A command-line client utility for controlling the PAPA server via socket communication.

This tool is compiled into the `papa` binary.

## Usage

```
papa --play track1
papa --stop track1
papa --stop-all
papa --list
papa --status
papa --reload
```

### Device Configuration

The `device` field in track output configuration can use either:
- The PipeWire node name (e.g., "alsa_output.pci-0000_00_1f.3")
- The numeric node ID (e.g., "45")

Use `papa --list-devices` to see available devices and their names/IDs.
