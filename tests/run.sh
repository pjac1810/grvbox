#!/bin/sh
# Host test of GRVBOX: the factory patterns to build/render-out/*.wav, recording, TR-REC, the arpeggiator, a fuzz.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/render-out
${CC:-cc} -O1 -g -fsanitize=undefined -fno-sanitize-recover -Wall -Isrc -Iplatform -DPOOL= \
  -DRING_PUBLISH\(\)= tests/render.c src/voice.c src/tones.c src/seq.c -lm -o build/render
build/render build/render-out
