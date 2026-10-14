# Use Perikop

Perikop works offline after installation. The interface is in Swedish.

## Installation

Download the file for your system from [the releases](https://github.com/jsjoberg/perikop/releases).

### Windows
Requires Windows 10 or later, x86-64. Download the EXE file and run the installer.
The installer is unsigned. If Microsoft Defender SmartScreen blocks it, select *More info* → *Run anyway*.

### macOS
Requires macOS 13.4 or later, Intel or Apple Silicon. Download the DMG file, open it, and drag Perikop to Applications.
The app has an ad-hoc signature. If macOS blocks the app on first launch, go to *System Settings* → *Privacy & Security* and select *Open Anyway*.

### Linux
Requires glibc 2.34 or later, x86-64. Download the `x86_64` AppImage file.
Make it executable (`chmod +x Perikop-*-x86_64.AppImage`), then run it.

### Raspberry Pi 5
Requires 64-bit Raspberry Pi OS with its desktop. Use the `aarch64` AppImage the same way. Older Pi models are not supported.
Speech is only about as fast as playback, so select **Uppläsning → Förbered ljud för vald dag** before you listen.
A slow SD card makes the reader stall; an SSD or a fast card helps.

## Day page and calendar

The day page shows the selected date's readings and three reading plans. Press **Läs** to open a reading.

- The arrows beside the date change the day. **Ctrl+T** goes to today.
- Click the date for the month calendar. Click the month or year name for a grid of months or years.
- Calendar markers: violet circle for Orthros, gold diamond for Vespers, green square for other services.
- The **Kalender** menu selects the Greek or Slavic reading order and the Revised Julian or Julian calendar.
  With the Greek order, the day page shows Antiochian readings beside the Greek ones when they differ.

## Reader

- The left column is Swedish. The symbols right of the address field add Greek, English, or word study. Click a symbol again to close the column.
- Click a verse to highlight it. In **Bara svenska**, buttons above the verse open it in online Swedish Bibles.
- Click the address field to go to another passage, or to choose a part of a reading in several parts.
- **⋮** opens every menu command. On Windows and Linux, press Alt to show the menu bar.
- **Visa → Normal textstorlek** (Ctrl+0, ⌘0 on macOS) resets the text size.

## Bible browser

Select **Bibel → Gå till bibelställe**, then a book, a chapter, and a verse. Later selections set the end of the range.
**Lägg till** adds the range as a part, so you can read several passages as one reading. **Öppna urval** opens the selection.

## Reading plans

- **Nya testamentet**: 30 parts.
- **Gamla testamentet**: the books of the Hebrew Bible, with Septuagint chapter numbers, in 30 parts.
- **Septuagintas övriga böcker**: the other Septuagint books that Perikop has in Swedish, in 14 parts.

Plans count parts, not days. Perikop asks whether to mark a part as read when the voice finishes it, or when you leave it after scrolling to its end.
**Börja om** clears a plan.

## Read-aloud

- **Lyssna** reads Swedish aloud. Select the voice, Alice or Björn, in the **Uppläsning** menu.
- Drag across verses to mark them. Hold Ctrl (⌘ on macOS) to add more ranges. **Läs markering** reads the marked verses.
- Scrolling stops the reader from following the voice. **Följ uppläsningen** returns to it.
- **Visa → Markera texten som läses upp** highlights the spoken text.
- **Uppläsning → Förbered ljud för vald dag** prepares the selected day's audio in advance, for example the evening before.

## Word study and pronunciation

- **Visa → Svenska och ordstudium**: click a Swedish word to see its pronunciation and dictionary entries.
- **Uppläsning → Granska svenskt uttal…**: listen to words and correct their pronunciation. Corrections change only the speech, not the text.

**Om Perikop** lists every source with links. It is in the application menu on macOS and in **Hjälp** elsewhere.
