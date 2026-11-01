#!/bin/bash
# Necessite : Xcode (+ outils en ligne de commande), CMake et Git
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
echo "Le plugin est copie dans ~/Library/Audio/Plug-Ins/VST3/"
