# Use Perikop

Perikop works offline after installation. The interface is in Swedish.

## Installation

Download the file for your system from [the releases](https://github.com/jsjoberg/perikop/releases).

### Windows
Requires Windows 10 or later, x86-64. Download the EXE file and run the installer.
Choose `Perikop-win-x64-releases-Setup.exe` for in-app updates, or the corresponding `nightly` installer for experimental builds. If you installed the older `windows-x64.exe` NSIS package, uninstall it once before installing the Velopack version. Your reading profile is outside the application files and is kept.
The installer is unsigned. If Microsoft Defender SmartScreen blocks it, select *More info* → *Run anyway*.

### macOS
Requires macOS 15 or later on an Apple Silicon Mac (M-series). Download `macOS-arm64.dmg`. Intel Macs are no longer supported.
Open the DMG and drag Perikop to Applications. **About This Mac** shows whether your Mac has an Apple chip or an Intel processor.
The app has an ad-hoc signature. If macOS blocks the app on first launch, go to *System Settings* → *Privacy & Security* and select *Open Anyway*.
The `Perikop-osx-arm64-releases-Setup.pkg` download installs a copy that supports in-app updates. It is unsigned and not notarized. The DMG remains available. See [update validation](updating.md#packaging-and-validation) for the release checks.

### Linux
Requires glibc 2.34 or later, x86-64. Download the `x86_64` AppImage file.
Make it executable (`chmod +x Perikop-*-x86_64.AppImage`), then run it.
The `Perikop-linux-x64-releases.AppImage` download supports in-app updates. Use the `linux-arm64` equivalent on the Raspberry Pi 5, or the corresponding `nightly` AppImage for experimental builds. Keep the AppImage in a directory your user can write so it can replace itself.

### Raspberry Pi 5
Requires 64-bit Raspberry Pi OS with its desktop. Use the `aarch64` AppImage the same way. Older Pi models are not supported.
Alice and Björn are only about as fast as playback here; the eSpeak voices are much lighter.
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
- **Inställningar** has separate ordered source lists for the main text and the second pane. Enable editions with their checkboxes and move them with ↑/↓. The first enabled edition containing the chapter is used. Swedish comes first by default, then English; Greek is available but initially off for the main text. The second pane defaults to English, then Greek, with Swedish available but initially off.
- The split-view button right of the address field opens one second pane using its source preferences. Click it again to return to one text. The word-study button opens Swedish word study instead.
- Each chapter's source label is a dropdown. Choose any available edition independently for either pane, including an unchecked edition or the same edition as the other pane, or **Automatiskt enligt Inställningar** to return to the saved priority. Automatic secondary selection skips the main pane's edition. Overrides stay within the open passage and chapter; reopening the passage restores the saved preferences.
- Click a verse to highlight it. In a single Swedish pane, buttons above the verse open it in online Swedish Bibles. Drag over verses in the main pane to mark a passage; Ctrl/⌘ adds another range. The selection actions offer the same online lookups for Swedish text, **Lyssna** to read the marked Swedish passage aloud, and a ribbon-shaped bookmark button to save it. English and Greek selections can also be bookmarked.
- **Bokmärken**, in the menu bar and as a submenu of the ⋮ menu, lists saved passages in Bible book order, then chapter and verse. Click the submenu row or press Right to open it beside the main menu. Cancelling the submenu returns to the main menu. Opening a bookmark selects that passage as a reading. **Ta bort bokmärke…** lets you choose a bookmark to remove and asks for confirmation; **Avbryt** keeps it. Bookmarks are saved in the local profile and survive restarting the app. Separate selected ranges become separate bookmarks; adding the same range again keeps one copy.
- Click the address field to go to another passage, or to choose a part of a reading in several parts.
- **⋮** opens the commands, as in Chrome. **−** and **+** in the menu change the text size and the read-aloud speed without closing it.
  Windows and Linux use the ⋮ menu and keyboard shortcuts; macOS also has its native menu bar.
- The text size applies to the whole program. Ctrl++ and Ctrl+- (⌘ on macOS) change it, and Ctrl+0 resets it.
- **Inställningar** holds the colour scheme and theme, the calendar, the reading order, and the voice.
- Tab moves through the toolbar, the page, and the word study column in that order. Shift+Tab moves back.

## Updates

**Inställningar → Uppdateringskanal** saves your choice of **Utgåvor** (releases, the default) or **Nattbyggen** (experimental nightly builds).
Changing this setting makes no network requests.
**⋮ → Sök efter uppdateringar…**, just before **Granska svenskt uttal…**, opens an update dialog and checks the chosen channel on GitHub. Each following step waits for you: **Hämta version …** downloads it, and **Starta om och installera** installs it and restarts Perikop.
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

- The toolbar has one Play/Pause button: it shows Pause during playback and Play when paused or stopped. **Lyssna** reads Swedish aloud; English and Greek primary text have no read-aloud. Select the voice in **Inställningar**: Alice, Björn, or one of the Swedish voices and variants available in your installed eSpeak NG.
- **Pausa** saves the current verse. **Fortsätt** starts that verse again from the beginning, with the full passage still visible. This also works after reopening the reading or restarting Perikop. Space in the reader or **Ctrl+Shift+P** (⌘⇧P on macOS) plays or pauses.
- **Stoppa** resets the open reading to its beginning and clears its saved position, including while paused. **Ctrl+.** (⌘. on macOS) does the same. Leaving after Stop keeps the reset until you move in the text or start playing again.
- Drag across verses to mark them. Hold Ctrl (⌘ on macOS) to add more ranges. **Läs markering** reads the marked verses.
- Scrolling stops the reader from following the voice. **Följ uppläsningen** returns to it.
- **Inställningar → Markera texten som läses upp** highlights the spoken text.
- The eSpeak voices need eSpeak NG, which you install separately; **Inställningar** links to installation instructions when it is missing. They read with Perikop's own Swedish pronunciations and use very little CPU.
- Beside the voice picker, **Prova rösten** reads John 3:16 with the selected eSpeak voice. **Stoppa provet** stops the sample; choosing another voice or leaving settings also stops it. Trying a voice preserves your reading position.

## Word study and pronunciation

- **Visa → Ordstudium**: click a Swedish word to see its pronunciation and dictionary entries.
- **Uppläsning → Granska svenskt uttal…**: listen to words and correct their pronunciation. Corrections change only the speech, not the text.

**Om Perikop** lists every source with links. It is in the application menu on macOS and in **Hjälp** elsewhere.

Daily readings and reading-plan parts show saved passage progress as a percentage. Completed readings show a checkmark instead. Progress uses the same saved verse position as automatic resume.
