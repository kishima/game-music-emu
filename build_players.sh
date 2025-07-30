#!/bin/bash

# Game Music Emu Players Build Script
# This script builds various audio players for testing

echo "Building Game Music Emu players..."

# Check if GME library exists
if [ ! -f "build/gme/libgme.a" ]; then
    echo "Error: GME library not found. Please build GME first with:"
    echo "  mkdir build && cd build && cmake .. && make"
    exit 1
fi

# Build WAV writer (outputs to file)
echo "Building WAV writer..."
g++ -o wav_writer wav_writer.cpp -Ibuild/gme -Lbuild/gme -lgme
if [ $? -eq 0 ]; then
    echo "✓ wav_writer built successfully"
else
    echo "✗ Failed to build wav_writer"
    exit 1
fi

# Build simple SDL player
echo "Building simple SDL player..."
g++ -o simple_player simple_player.cpp -Ibuild/gme -Lbuild/gme -lgme $(pkg-config --cflags --libs sdl2)
if [ $? -eq 0 ]; then
    echo "✓ simple_player built successfully"
else
    echo "✗ Failed to build simple_player"
    exit 1
fi

# Build streaming player
echo "Building streaming player..."
g++ -o streaming_player streaming_player.cpp -Ibuild/gme -Lbuild/gme -lgme
if [ $? -eq 0 ]; then
    echo "✓ streaming_player built successfully"
else
    echo "✗ Failed to build streaming_player"
    exit 1
fi

# Build PortAudio player (if available)
if pkg-config --exists portaudio-2.0; then
    echo "Building PortAudio player..."
    g++ -o portaudio_player portaudio_player.cpp -Ibuild/gme -Lbuild/gme -lgme -lportaudio
    if [ $? -eq 0 ]; then
        echo "✓ portaudio_player built successfully"
    else
        echo "✗ Failed to build portaudio_player"
    fi
else
    echo "! PortAudio not available, skipping portaudio_player"
fi

echo ""
echo "Build complete! Available players:"
echo ""
echo "1. wav_writer - Exports to WAV file"
echo "   Usage: ./wav_writer <file> [track] [seconds]"
echo "   Example: ./wav_writer test.nsf 1 30"
echo ""
echo "2. simple_player - SDL audio player"
echo "   Usage: ./simple_player <file> [track]"
echo "   Example: ./simple_player test.nsf 1"
echo ""
echo "3. streaming_player - Pipes audio to external player"
echo "   Usage: ./streaming_player <file> [track] | paplay --format=s16le --channels=2 --rate=44100 --raw"
echo "   Example: ./streaming_player test.nsf | paplay --format=s16le --channels=2 --rate=44100 --raw"
echo ""

if [ -f "portaudio_player" ]; then
echo "4. portaudio_player - PortAudio player"
echo "   Usage: ./portaudio_player <file> [track]"
echo "   Example: ./portaudio_player test.nsf 1"
echo ""
fi

echo "For WSL2 users: wav_writer is recommended due to audio latency issues."