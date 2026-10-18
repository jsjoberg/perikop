# Use Perikop

Perikop works offline after installation. The interface is in Swedish.

## Installation

Download the file for your system from [the releases](https://github.com/jsjoberg/perikop/releases).

### Windows
Requires Windows 10 or later, x86-64. Download the EXE file and run the installer.
Choose `Perikop-win-x64-releases-Setup.exe` for in-app updates, or the corresponding `nightly` installer for experimental builds. If you installed the older `windows-x64.exe` NSIS package, uninstall it once before installing the Velopack version. Your reading profile is outside the application files and is kept.
The installer is unsigned. If Microsoft Defender SmartScreen blocks it, select *More info* → *Run anyway*.

### macOS
Requires macOS 13.4 or later. Download `macOS-arm64.dmg` for Apple Silicon (M-series), or `macOS-x86_64.dmg` for Intel.
Open the DMG and drag Perikop to Applications. **About This Mac** shows whether your Mac has an Apple chip or an Intel processor.
The app has an ad-hoc signature. If macOS blocks the app on first launch, go to *System Settings* → *Privacy & Security* and select *Open Anyway*.
The `Perikop-osx-arm64-releases-Setup.pkg` and `Perikop-osx-x64-releases-Setup.pkg` downloads install a copy that supports in-app updates. They are unsigned and not notarized. The DMGs remain available. macOS 13 update compatibility is still [awaiting baseline validation](updating.md#phases); successful checks on newer macOS do not establish it.

### Linux
Requires glibc 2.34 or later, x86-64. Download the `x86_64` AppImage file.
Make it executable (`chmod +x Perikop-*-x86_64.AppImage`), then run it.
The `Perikop-linux-x64-releases.AppImage` download supports in-app updates. Use the `linux-arm64` equivalent on the Raspberry Pi 5, or the corresponding `nightly` AppImage for experimental builds. Keep the AppImage in a directory your user can write so it can replace itself.

### Raspberry Pi 5
Requires 64-bit Raspberry Pi OS with its desktop. Use the `aarch64` AppImage the same way. Older Pi models are not supported.
Speech is only about as fast as playback, so select **Uppläsning → Förbered ljud för vald dag** before you listen.
A slow SD card makes the reader stall; an SSD or a fast card helps.

## Day page and calendar

The day page shows the selected date's readings and three reading plans. Press **Läs** to open a reading.

- The arrows beside the date change the day. **Ctrl+Shift+T** goes to today (⌘ instead of Ctrl on macOS).
- Click the date for the month calendar. Click the month or year name for a grid of months or years.
- Calendar markers: violet circle for Orthros, gold diamond for Vespers, green square for other services.
- **Inställningar** (Ctrl+, or ⌘, on macOS) selects the Greek or Slavic reading order and the Revised Julian or Julian calendar.
  With the Greek order, the day page shows Antiochian readings beside the Greek ones when they differ.

## Reader

- **Tillbaka** and **Framåt** return through the views opened in this session, preserving the reading position and Bible-picker selection. Use Alt+Left/Right on Windows and Linux, ⌘[/⌘] on macOS, or the mouse's Back/Forward side buttons.
- **Ctrl+L** opens the passage picker; **Ctrl+Shift+L** opens the selected day's readings. **Ctrl+PageUp/PageDown** changes the day, and **Ctrl+Shift+D** opens the calendar. Use ⌘ instead of Ctrl on macOS.
- The left column is Swedish. The symbols right of the address field add Greek, English, or word study. Click a symbol again to close the column.
- Click a verse to highlight it. In **Bara svenska**, buttons above the verse open it in online Swedish Bibles.
- Click the address field to go to another passage, or to choose a part of a reading in several parts.
- **⋮** opens the commands, as in Chrome. **−** and **+** in the menu change the text size and the read-aloud speed without closing it.
  On Windows and Linux, press Alt to show the menu bar.
- The text size applies to the whole program. Ctrl++ and Ctrl+- (⌘ on macOS) change it, and Ctrl+0 resets it.
- **Inställningar** holds the colour scheme and theme, the calendar, the reading order, and the voice.
- Tab moves through the toolbar, the page, and the word study column in that order. Shift+Tab moves back.

## Updates

**Inställningar → Uppdateringskanal** saves your choice of **Utgåvor** (releases, the default) or **Nattbyggen** (experimental nightly builds).
Changing this setting makes no network requests.
**Sök efter uppdateringar** checks the chosen channel on GitHub. Each step then waits for you: **Hämta version …** downloads it, and **Starta om och installera** installs it and restarts Perikop.
In-app updates require a Velopack download listed above. Plain DMG, NSIS, and older AppImage copies explain that they need a manual download instead. The feed becomes available when its GitHub release is published.
The updater uses deltas when a suitable cached base exists, and otherwise downloads the full package. Updating or removing the application keeps your settings and reading progress. No check, download, or installation starts automatically.

## Bible browser

Select **Gå → Gå till bibelställe**, then a book, a chapter, and a verse. Later selections set the end of the range.
**Lägg till** adds the range as a part, so you can read several passages as one reading. **Öppna urval** opens the selection.

## Reading plans

- **Nya testamentet**: 30 parts.
- **Gamla testamentet**: the books of the Hebrew Bible, with Septuagint chapter numbers, in 30 parts.
- **Septuagintas övriga böcker**: the other Septuagint books that Perikop has in Swedish, in 14 parts.

Plans count parts, not days. A partly read section shows its percentage and reopens at the saved verse. Position is saved automatically when you pause, leave the reading, or close Perikop. While listening, the spoken verse is saved; while reading silently, the verse at the reading line is saved. A section is marked complete when its read-aloud finishes. Merely scrolling to its end does not mark it complete.
**Börja om** clears a plan's completed sections and saved positions.

## Read-aloud

- **Lyssna** reads Swedish aloud. Select the voice, Alice or Björn, in **Inställningar**.
- **Pausa** saves the current verse. **Fortsätt** starts that verse again from the beginning, with the full passage still visible. This also works after reopening the reading or restarting Perikop. Space in the reader or **Ctrl+Shift+P** (⌘⇧P on macOS) plays or pauses.
- **Stoppa** resets the open reading to its beginning and clears its saved position, including while paused. **Ctrl+.** (⌘. on macOS) does the same. Leaving after Stop keeps the reset until you move in the text or start playing again.
- Drag across verses to mark them. Hold Ctrl (⌘ on macOS) to add more ranges. **Läs markering** reads the marked verses.
- Scrolling stops the reader from following the voice. **Följ uppläsningen** returns to it.
- **Inställningar → Markera texten som läses upp** highlights the spoken text.
- **Uppläsning → Förbered ljud för vald dag** prepares the selected day's audio in advance, for example the evening before.

## Word study and pronunciation

- **Visa → Svenska och ordstudium**: click a Swedish word to see its pronunciation and dictionary entries.
- **Uppläsning → Granska svenskt uttal…**: listen to words and correct their pronunciation. Corrections change only the speech, not the text.

**Om Perikop** lists every source with links. It is in the application menu on macOS and in **Hjälp** elsewhere.
