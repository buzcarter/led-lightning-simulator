# Audio

The microSD card for the DFPlayer Mini, laid out exactly as it goes on the card. Everything here but this file is git-ignored.

```
audio/
├── 01/    cloud rumble — the steady state, plays whenever nothing else is
├── 02/    thunder — one per strike
└── 03/    chaos — the chaos button
```

## Naming

The folder names matter exactly: two digits, `01` `02` `03`.

Inside each folder, every file name has to **start** with three digits, numbered from `001` with no gaps. Anything after the digits is ignored, so you can keep a description:

```
02/001 close crack.mp3
02/002 rolling.mp3
02/003 far off.mp3
```

Then set `SOUND_CLOUD_FILES`, `SOUND_THUNDER_FILES` and `SOUND_CHAOS_FILES` at the top of `src/sound.h` to how many files each folder holds. Each play picks one at random, never the same one twice running.

MP3 or WAV both work. Mono is fine, since the module drives one speaker. Even out the loudness across files before copying, because the board sets one volume for all of them.

Thunder files that fade out into a low rumble make the hand-back to the cloud rumble hard to hear.

## Copying to the card

Format the card FAT32, copy the three folders to its root, then remove the hidden `._` files macOS leaves on FAT cards. Some DFPlayer clones count those as tracks and play the wrong one:

```sh
cp -R audio/01 audio/02 audio/03 /Volumes/<CARD>/
dot_clean -m /Volumes/<CARD>
```
