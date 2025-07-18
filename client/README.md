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
