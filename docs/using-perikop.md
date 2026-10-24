# Use Perikop

Perikop works offline after installation. The interface is in Swedish.

## Installation

Download the file for your system from [the releases](https://github.com/jsjoberg/perikop/releases).

Releases carry the `releases` downloads named below. Nightly builds, published as prereleases, carry the same downloads named `nightly` instead. Each updates itself from its own channel; see [Updates](#updates).

### Windows
Requires Windows 10 or later, x86-64. Download `Perikop-win-x64-releases-Setup.exe` and run it.
If you installed the older `windows-x64.exe` NSIS package, uninstall it once first. Your reading profile is outside the application files and is kept.
The installer is unsigned. If Microsoft Defender SmartScreen blocks it, select *More info* → *Run anyway*.

### macOS
Requires macOS 15 or later on an Apple Silicon Mac (M-series). Intel Macs are no longer supported. **About This Mac** shows whether your Mac has an Apple chip or an Intel processor.
Download `Perikop-osx-arm64-releases-Setup.pkg` and open it. The package is unsigned and not notarized: if macOS blocks it, go to *System Settings* → *Privacy & Security* and select *Open Anyway*. The app has an ad-hoc signature, and the same applies on its first launch.
A copy installed from the older DMG cannot update itself; install the package once. See [update validation](updating.md#packaging-and-validation) for the release checks.

### Linux
Requires glibc 2.34 or later, x86-64. Download `Perikop-linux-x64-releases.AppImage`.
Make it executable (`chmod +x Perikop-linux-x64-releases.AppImage`), then run it. Keep the AppImage in a directory your user can write so it can replace itself.

### Raspberry Pi 5
Requires 64-bit Raspberry Pi OS with its desktop. Use `Perikop-linux-arm64-releases.AppImage` the same way. Older Pi models are not supported.
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
- Click a verse to highlight it. In a single Swedish pane, buttons above the verse open it in online Swedish Bibles. Drag over verses in the main pane to mark a passage, or Shift-click to extend from the verse where you started. Alt/Option-click or drag adds a separate range; Ctrl/⌘ also works. Escape clears the selection. The selection actions offer **Kopiera**, the same online lookups for Swedish text, **Lyssna** to read the marked Swedish passage aloud, and a ribbon-shaped bookmark button to save it. English and Greek selections can also be bookmarked.
- **Ctrl+C** (⌘C on macOS) copies marked verses, or the clicked verse from either pane. Right-click also offers **Kopiera med referens**. Copied text ends with the passage reference in the copied edition's numbering and its translation/edition name. Separate ranges stay separate, and changing editions across chapters gives each text its own reference.
- **Bokmärken**, in the menu bar and as a submenu of the ⋮ menu, lists saved passages in Bible book order, then chapter and verse. Click the submenu row or press Right to open it beside the main menu. Cancelling the submenu returns to the main menu. Opening a bookmark selects that passage as a reading. **Ta bort bokmärke…** lets you choose a bookmark to remove and asks for confirmation; **Avbryt** keeps it. Bookmarks are saved in the local profile and survive restarting the app. Separate selected ranges become separate bookmarks; adding the same range again keeps one copy.
- Click the address field to go to another passage, or to choose a part of a reading in several parts.
- **⋮** opens the commands, as in Chrome. **−** and **+** in the menu change the text size and the read-aloud speed without closing it.
  Windows and Linux use the ⋮ menu and keyboard shortcuts; macOS also has its native menu bar.
- The text size applies to the whole program. Ctrl++ and Ctrl+- (⌘ on macOS) change it, and Ctrl+0 resets it.
- **Inställningar** holds the colour scheme and theme, the calendar, the reading order, and the voice.
- Tab moves through the toolbar, the page, and the word study column in that order. Shift+Tab moves back.
- The macOS **Redigera** menu provides the standard Undo, Redo, Cut, Copy, Paste, and Select All commands. Their shortcuts work in editable fields on every platform. In the reader, Copy includes the marked verses and their reference; Select All applies to text fields, not a whole Bible book.
- Perikop remembers the window's normal size, position, and maximized state for each local profile. A window from a disconnected monitor returns within an available display. Diagnostic runs keep their default window layout.

## Updates

**Inställningar → Uppdateringskanal** saves your choice of **Utgåvor** (releases, the default) or **Nattbyggen** (experimental nightly builds).
Changing this setting makes no network requests.
**⋮ → Sök efter uppdateringar…**, just before **Granska svenskt uttal…**, opens an update dialog and checks the chosen channel on GitHub. Each following step waits for you: **Hämta version …** downloads it, and **Starta om och installera** installs it and restarts Perikop.
In-app updates work in the downloads listed above. Copies from the DMG, the NSIS installer or the older AppImage, which 0.1.x offered, explain that they need a manual download instead. The feed becomes available when its GitHub release is published.
The updater uses deltas when a suitable cached base exists, and otherwise downloads the full package. Updating or removing the application keeps your settings and reading progress. No check, download, or installation starts automatically.

## Bible browser

Select **Gå → Gå till bibelställe**, then a book, a chapter, and a verse. Later selections set the end of the range.
Book order and references follow the Orthodox Study Bible (OSB) in every language.
Book tooltips also show OSB's English names, including 1–4 Kingdoms and 1–2 Ezra.
Translations show their own verse numbers in parentheses where they differ. Text
without a mapped OSB reference is labelled **Utan OSB-ref** and is not read aloud.
**Lägg till** adds the range as a part, so you can read several passages as one reading. **Öppna urval** opens the selection.

## Reading plans

- **Nya testamentet**: 60 parts.
- **Gamla testamentet**: the Hebrew Bible's books in their OSB form, including Manasseh at 2 Chronicles 37 and Psalm 151, in 182 parts.
- **Septuagintas övriga böcker**: the other OSB books and named Daniel sections, including 1 Ezra and 3 Maccabees, in 36 parts. Books without Swedish text use an enabled English or Greek source.

Parts are about 3,000 Swedish words long, with some variation to finish at chapter and book boundaries. Matthew, Mark, Luke, John, Acts, and Revelation each start and end in their own parts. Short letters and books stay whole and may share a part with other complete short books; longer ones split at chapter boundaries.

Progress from the original and previous plans transfers automatically on first launch. New parts wholly covered by previously read text are marked complete; partly covered parts resume at the first unread verse. Original progress is retained as recovery data. A boundary part can contain some text you have read before, but the conversion never marks unread text complete.

Plans count parts, not days. A partly read section shows its percentage and reopens at the saved verse. Position is saved automatically when you pause, leave the reading, or close Perikop. While listening, the spoken verse is saved; while reading silently, the verse at the reading line is saved. A section is marked complete when its read-aloud finishes. Merely scrolling to its end does not mark it complete.
**Börja om** clears a plan's completed sections and saved positions.

## Read-aloud

- The toolbar has one Play/Pause button: it shows Pause during playback and Play when paused or stopped. **Lyssna** reads Swedish aloud; English and Greek primary text have no read-aloud. Read-aloud needs eSpeak NG, which you install separately; **Inställningar** links to instructions when it is missing. Choose one of the Swedish voices and variants of your installed eSpeak NG there.
- **Pausa** saves the current verse. **Fortsätt** starts that verse again from the beginning, with the full passage still visible. This also works after reopening the reading or restarting Perikop. Space in the reader or **Ctrl+Shift+P** (⌘⇧P on macOS) plays or pauses.
- **Stoppa** resets the open reading to its beginning and clears its saved position, including while paused. **Ctrl+.** (⌘. on macOS) does the same. Leaving after Stop keeps the reset until you move in the text or start playing again.
- Drag across verses to mark them. Hold Ctrl (⌘ on macOS) to add more ranges. **Läs markering** reads the marked verses.
- Scrolling stops the reader from following the voice. **Följ uppläsningen** returns to it.
- **Inställningar → Markera texten som läses upp** highlights the spoken text.
- eSpeak reads with Perikop's own Swedish pronunciations and uses very little CPU. **Snabbare** and **Långsammare** change its speaking rate; during reading, the current verse starts again at the new speed.
- Beside the voice picker, **Prova rösten** reads John 3:16 with the selected eSpeak voice. **Stoppa provet** stops the sample; choosing another voice or leaving settings also stops it. Trying a voice preserves your reading position.

## Word study and pronunciation

- **Visa → Ordstudium**: click a Swedish word to see its pronunciation and dictionary entries.
- **Uppläsning → Granska svenskt uttal…**: listen to words and correct their pronunciation. Corrections change only the speech, not the text. Write a correction as a spelling, such as `å-sna`, or as exact eSpeak NG phoneme codes in `⟦…⟧`, such as `⟦k'Os.⟧`; choosing one of a word's NST pronunciations fills that in. Corrections saved in earlier versions are converted automatically.

**Om Perikop** lists every source with links. It is in the application menu on macOS and in **Hjälp** elsewhere.

Daily readings and reading-plan parts show saved passage progress as a percentage. Completed readings show a checkmark instead. Progress uses the same saved verse position as automatic resume.

## Lägg till Reformationsbibeln 2026

Öppna Inställningar och följ länken under Lokalt bibelinnehåll till bibel.se. Hämta
SRB Hela GT och SRB NT med UN noter där och välj sedan Importera båda PDF-filerna.
Efter importen aktiverar du Reformationsbibeln 2026 i källorna och väljer dess prioritet.
Texten sparas i din profil. Fotnoter och korshänvisningar importeras inte.
