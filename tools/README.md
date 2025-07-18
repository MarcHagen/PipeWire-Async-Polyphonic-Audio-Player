# Tools Directory

This directory contains supplementary tools and utilities for the async-multichannel-audio-player.

## Client Tool

`client.c` - A command-line client utility for controlling the audio player server via socket communication.

This tool is compiled into the `audio-player-ctl` binary.

## Usage

```
audio-player-ctl --play track1
audio-player-ctl --stop track1
audio-player-ctl --stop-all
audio-player-ctl --list
audio-player-ctl --status
audio-player-ctl --reload
```
