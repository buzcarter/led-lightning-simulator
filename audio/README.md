# Audio

The microSD card for the DFPlayer Mini, laid out exactly as it goes on the card. Everything here but this file is git-ignored.

```
audio/
├── 01/    ambient — the cloud's rumble, plays whenever nothing else is
├── 02/    strikes — thunder, one per strike
└── 03/    chaos — the chaos button
```

## Naming

The folder names matter exactly: two digits, `01`, `02` and `03`. They go at the top level of the card, not inside another folder.

Inside each folder, every file name has to **start** with three digits, numbered from `001` with no gaps. Anything after the digits is ignored, so you can keep a description:

```
02/001 close crack.mp3
02/002 rolling.mp3
02/003 far off.mp3
```

Then set `SOUND_AMBIENT_FILES`, `SOUND_STRIKE_FILES` and `SOUND_CHAOS_FILES` in `src/config.h` to how many files each folder holds; files numbered past the count are never played. Each play picks one at random, never the same one twice running.

Every clip should be the same length, `SOUND_CLIP_MS` in `config.h` (5 s). The board times clips itself: when one has run that long, the ambient rumble starts again. A clip longer than that gets cut off; a shorter one leaves silence until the time is up.

MP3 or WAV both work. Mono is fine, since the module drives one speaker. Even out the loudness across files before copying, because the board sets one volume for all of them.

Thunder files that fade out into a low rumble make the hand-back to the cloud rumble hard to hear.

## Copying to the card

Format the card FAT32, copy the three folders to its root, then remove the hidden `._` files macOS leaves on FAT cards. Some DFPlayer clones count those as tracks and play the wrong one:

```sh
cp -R audio/01 audio/02 audio/03 /Volumes/<CARD>/
dot_clean -m /Volumes/<CARD>
```
