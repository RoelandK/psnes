//
// Created by cpasjuste on 01/06/18.
//

#include <snes9x.h>
#include <memmap.h>
#include <apu/apu.h>
#include <controls.h>
#include <filter/blit.h>
#include <conffile.h>
#include <display.h>
#include <sys/stat.h>
#include <snapshot.h>

#include "c2dui.h"
#include "uiEmu.h"

using namespace c2d;
using namespace c2dui;

static C2DUIGuiMain *_ui;

static void S9xAudioCallback(void *userdata, Uint8 *stream, int len) {

    //printf("S9xAudioCallback\n");
    _ui->getAudio()->lock();
    S9xMixSamples(stream, len >> (Settings.SixteenBitSound ? 1 : 0));
    _ui->getAudio()->unlock();
}

static void S9xSamplesAvailable(void *data) {

    //printf("S9xSamplesAvailable\n");
    _ui->getAudio()->lock();
    S9xFinalizeSamples();
    _ui->getAudio()->unlock();
}

PSNESGuiEmu::PSNESGuiEmu(C2DUIGuiMain *ui) : C2DUIGuiEmu(ui) {

    printf("PSNESGuiEmu()\n");
    _ui = ui;
}

int PSNESGuiEmu::run(C2DUIRomList::Rom *rom) {

    memset(&Settings, 0, sizeof(Settings));
    Settings.MouseMaster = FALSE;
    Settings.SuperScopeMaster = FALSE;
    Settings.JustifierMaster = FALSE;
    Settings.MultiPlayer5Master = FALSE;
    Settings.FrameTimePAL = 20000;
    Settings.FrameTimeNTSC = 16667;
    Settings.SixteenBitSound = TRUE;
    Settings.Stereo = TRUE;
#ifdef __SWITCH__
    Settings.SoundPlaybackRate = 48000;
#else
    Settings.SoundPlaybackRate = 32000;
#endif
    Settings.SoundInputRate = 32000;
    Settings.SoundSync = TRUE;
    Settings.Transparency = TRUE;
    Settings.AutoDisplayMessages = TRUE;
    Settings.InitialInfoStringTimeout = 120;
    Settings.HDMATimingHack = 100;
    Settings.BlockInvalidVRAMAccessMaster = TRUE;
    Settings.StopEmulation = TRUE;
    Settings.WrongMovieStateProtection = TRUE;
    Settings.DumpStreamsMaxFrames = -1;
    Settings.StretchScreenshots = 1;
    Settings.SnapshotScreenshots = TRUE;
    Settings.SkipFrames = AUTO_FRAMERATE;
    Settings.TurboSkipFrames = 15;
    Settings.CartAName[0] = 0;
    Settings.CartBName[0] = 0;

    Settings.SupportHiRes = FALSE;

    CPU.Flags = 0;

    //S9xLoadConfigFiles(argv, argc);

    if (!Memory.Init()) {
        fprintf(stderr, "Could not initialize Snes9x Memory.\n");
        exit(1);
    }

    if (!S9xInitAPU()) {
        fprintf(stderr, "Could not initialize Snes9x APU.\n");
        exit(1);
    }

    if (!S9xInitSound(100, 0)) {
        fprintf(stderr, "Could not initialize Snes9x Sound.\n");
        exit(1);
    }

    // TODO
    S9xUnmapAllControls();
    S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
    S9xMapButton(0, S9xGetCommandT("Joypad1 Up"), false);
    S9xMapButton(1, S9xGetCommandT("Joypad1 Down"), false);
    S9xMapButton(2, S9xGetCommandT("Joypad1 Left"), false);
    S9xMapButton(3, S9xGetCommandT("Joypad1 Right"), false);
    S9xMapButton(4, S9xGetCommandT("Joypad1 A"), false);
    S9xMapButton(5, S9xGetCommandT("Joypad1 B"), false);
    S9xMapButton(6, S9xGetCommandT("Joypad1 X"), false);
    S9xMapButton(7, S9xGetCommandT("Joypad1 Y"), false);
    S9xMapButton(8, S9xGetCommandT("Joypad1 L"), false);
    S9xMapButton(9, S9xGetCommandT("Joypad1 R"), false);
    S9xMapButton(10, S9xGetCommandT("Joypad1 Start"), false);
    S9xMapButton(11, S9xGetCommandT("Joypad1 Select"), false);
    S9xReportControllers();
    // TODO

    uint32 saved_flags = CPU.Flags;

#ifdef GFX_MULTI_FORMAT
    S9xSetRenderPixelFormat(RGB565);
#endif

    char file[512];
    snprintf(file, 512, "%s%s.zip", getUi()->getConfig()->getRomPath(0), rom->zip);
    if (!Memory.LoadROM(file)) {
        fprintf(stderr, "Could not open ROM: %s\n", file);
        exit(1);
    }

    //Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR));
    //S9xLoadCheatFile(S9xGetFilename(".cht", CHEAT_DIR));

    CPU.Flags = saved_flags;
    Settings.StopEmulation = FALSE;

    // Initialize filters
    S9xBlitFilterInit();
    S9xBlit2xSaIFilterInit();
    S9xBlitHQ2xFilterInit();

    int w, h;
    if (!Settings.SupportHiRes) {
        w = SNES_WIDTH;
        h = SNES_HEIGHT;
    } else {
        w = IMAGE_WIDTH;
        h = IMAGE_HEIGHT;
    }

    C2DUIVideo *video = new C2DUIVideo(
            getUi(), (void **) &GFX.Screen, (int *) &GFX.Pitch, Vector2f(w, h));
    setVideo(video);
    //printf("%i x %i | %i\n", w, h, GFX.Pitch);

    S9xGraphicsInit();
    S9xSetSoundMute(FALSE);

    return C2DUIGuiEmu::run(rom);
}

void PSNESGuiEmu::stop() {

    Settings.StopEmulation = TRUE;

    Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));
    S9xResetSaveTimer(FALSE);

    S9xBlitFilterDeinit();
    S9xBlit2xSaIFilterDeinit();
    S9xBlitHQ2xFilterDeinit();

    S9xUnmapAllControls();
    Memory.Deinit();
    S9xDeinitAPU();
    S9xGraphicsDeinit();

    C2DUIGuiEmu::stop();

    delete (getUi()->getAudio());
}

void PSNESGuiEmu::updateFb() {

    // TODO
}

void PSNESGuiEmu::renderFrame(bool draw, int drawFps, float fps) {

}

void PSNESGuiEmu::updateFrame() {

    // TODO
}

int PSNESGuiEmu::update() {

    // TODO
    Input::Player *players = getUi()->getInput()->update();

    // process menu
    if ((players[0].state & Input::Key::KEY_START)
        && (players[0].state & Input::Key::KEY_COIN)) {
        pause();
        return UI_KEY_SHOW_MEMU_ROM;
    } else if ((players[0].state & Input::Key::KEY_START)
               && (players[0].state & Input::Key::KEY_FIRE5)) {
        pause();
        return UI_KEY_SHOW_MEMU_STATE;
    } else if (players[0].state & EV_RESIZE) {
        // useful for sdl resize event for example
        getVideo()->updateScaling();
    }

    if (!isPaused()) {
        S9xMainLoop();
        S9xSetSoundMute(FALSE);
    } else {
        S9xSetSoundMute(TRUE);
    }

    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Functions called by Snes9x below
///////////////////////////////////////////////////////////////////////////////

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext) {

    *drive = 0;

    const char *slash = strrchr(path, SLASH_CHAR);
    const char *dot = strrchr(path, '.');

    if (dot && slash && dot < slash) {
        dot = nullptr;
    }

    if (!slash) {
        *dir = 0;

        strcpy(fname, path);

        if (dot) {
            fname[dot - path] = 0;
            strcpy(ext, dot + 1);
        } else {
            *ext = 0;
        }
    } else {
        strcpy(dir, path);
        dir[slash - path] = 0;

        strcpy(fname, slash + 1);

        if (dot) {
            fname[dot - slash - 1] = 0;
            strcpy(ext, dot + 1);
        } else {
            *ext = 0;
        }
    }
}

void _makepath(char *path, const char *, const char *dir, const char *fname, const char *ext) {

    if (dir && *dir) {
        strcpy(path, dir);
        strcat(path, SLASH_STR);
    } else {
        *path = 0;
    }

    strcat(path, fname);

    if (ext && *ext) {
        strcat(path, ".");
        strcat(path, ext);
    }
}

/**
 * Display port-specific usage information
 */
void S9xExtraUsage() {
}

/**
 * Parse port-specific arguments
 */
void S9xParseArg(char **argv, int &i, int argc) {
}

/**
 * Parse port-specific config
 */
void S9xParsePortConfig(ConfigFile &conf, int pass) {
}

/**
 * Called when Snes9x wants to know the directory dirtype.
 */
const char *S9xGetDirectory(s9x_getdirtype dirtype) {
    static const char *s = ".";
    return s;
}

/**
 * When Snes9x needs to know the name of the cheat/IPS file and so on, this function is called.
 * Check extension and dirtype, and return the appropriate filename.
 * The current ports return the ROM file path with the given extension.
 */
const char *S9xGetFilename(const char *ex, s9x_getdirtype dirtype) {

    static char s[PATH_MAX + 1];

    char drive[_MAX_DRIVE + 1];
    char dir[_MAX_DIR + 1];
    char fname[_MAX_FNAME + 1];
    char ext[_MAX_EXT + 1];

    _splitpath(Memory.ROMFilename, drive, dir, fname, ext);
    snprintf(s, PATH_MAX + 1, "%s%s%s%s", S9xGetDirectory(dirtype), SLASH_STR, fname, ex);

    return s;
}

/**
 * Almost the same as S9xGetFilename function, but used for saving SPC files etc.
 * So you have to take care not to delete the previously saved file, by increasing
 * the number of the filename; romname.000.spc, romname.001.spc, ...
 */
const char *S9xGetFilenameInc(const char *ex, s9x_getdirtype dirtype) {

    static char s[PATH_MAX + 1];

    char drive[_MAX_DRIVE + 1];
    char dir[_MAX_DIR + 1];
    char fname[_MAX_FNAME + 1];
    char ext[_MAX_EXT + 1];

    unsigned int i = 0;
    const char *d;
    struct stat buf{};

    _splitpath(Memory.ROMFilename, drive, dir, fname, ext);
    d = S9xGetDirectory(dirtype);

    do
        snprintf(s, PATH_MAX + 1, "%s%s%s.%03d%s", d, SLASH_STR, fname, i++, ex);
    while (stat(s, &buf) == 0 && i < 1000);

    return s;
}

/**
 * Called when Snes9x wants to know the name of the ROM image.
 * Typically, extract the filename from path and return it.
 */
const char *S9xBasename(const char *f) {

    const char *p;

    if ((p = strrchr(f, '/')) != nullptr || (p = strrchr(f, '\\')) != nullptr)
        return p + 1;

    return f;
}

/**
 * If your port can match Snes9x's built-in LoadFreezeFile/SaveFreezeFile command
 * (see controls.cpp), you may choose to use this function. Otherwise return NULL.
 */
const char *S9xChooseFilename(bool8 read_only) {
    return nullptr;
}

/**
 * If your port can match Snes9x's built-in BeginRecordingMovie/LoadMovie command
 * (see controls.cpp), you may choose to use this function. Otherwise return NULL.
 */
const char *S9xChooseMovieFilename(bool8 read_only) {
    return nullptr;
}

/**
 * This function opens a freeze-game file. STREAM is defined as a gzFile if ZLIB is
 * defined else it's defined as FILE *. The read_only parameter is set to true when
 * reading a freeze-game file and false when writing a freeze-game file.
 * Open the file filepath and return its pointer file.
 */
bool8 S9xOpenSnapshotFile(const char *filename, bool8 read_only, STREAM*file) {
    return FALSE;
}

/**
 * This function closes the freeze-game file opened by S9xOpenSnapshotFile function.
 */
void S9xCloseSnapshotFile(STREAM file) {
}

/**
 * Called just before Snes9x begins to render an SNES screen.
 * Use this function if you should prepare before drawing, otherwise let it empty.
 */
bool8 S9xInitUpdate() {
    return TRUE;
}

/**
 * Called at the end of screen refresh if GFX.DoInterlace && GFX.InterlaceFrame == 0 is true (?).
 */
bool8 S9xContinueUpdate(int width, int height) {
    return TRUE;
}

/**
 * Called once a complete SNES screen has been rendered into the GFX.Screen memory buffer,
 * now is your chance to copy the SNES rendered screen to the host computer's screen memory.
 * The problem is that you have to cope with different sized SNES rendered screens:
 * 256*224, 256*239, 512*224, 512*239, 512*448 and 512*478.
 */
bool8 S9xDeinitUpdate(int width, int height) {

    static int prevWidth = 0;
    static int prevHeight = 0;

    if ((width <= SNES_WIDTH) && ((prevWidth != width) || (prevHeight != height))) {
        S9xBlitClearDelta();
        // TODO: update video
        printf("TODO: update video texture size\n");
    }

    _ui->getUiEmu()->getVideo()->unlock();
    _ui->getRenderer()->flip();

    prevWidth = width;
    prevHeight = height;

    return TRUE;
}

/**
 * If your port can match Snes9x's built-in SoundChannelXXX command (see controls.cpp),
 * you may choose to use this function. Otherwise return NULL.
 * Basically, turn on/off the sound channel c (0-7), and turn on all channels if c is 8.
 */
void S9xToggleSoundChannel(int c) {

    static uint8 sound_switch = 255;

    if (c == 8)
        sound_switch = 255;
    else
        sound_switch ^= 1 << c;
    S9xSetSoundControl(sound_switch);
}

/**
 * If Settings.AutoSaveDelay is not zero, Snes9x calls this function when the contents of
 * the S-RAM has been changed. Typically, call Memory.SaveSRAM function from this function.
 */
void S9xAutoSaveSRAM() {
    Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR));
}

/**
 * Called at the end of S9xMainLoop function, when emulating one frame has been done.
 * You should adjust the frame rate in this function
 */
void S9xSyncSpeed() {

#ifndef NOSOUND
    if (Settings.SoundSync) {
        while (!S9xSyncSound()) {
#ifdef __SWITCH__
            svcSleepThread(1);
#else
            usleep(0);
#endif
        }
    }
#endif

    if (Settings.DumpStreams)
        return;

#ifndef __SWITCH__ // TODO
    if (Settings.HighSpeedSeek > 0)
        Settings.HighSpeedSeek--;

    if (Settings.TurboMode) {
        if ((++IPPU.FrameSkip >= Settings.TurboSkipFrames) && !Settings.HighSpeedSeek) {
            IPPU.FrameSkip = 0;
            IPPU.SkippedFrames = 0;
            IPPU.RenderThisFrame = TRUE;
        } else {
            IPPU.SkippedFrames++;
            IPPU.RenderThisFrame = FALSE;
        }

        return;
    }

    static struct timeval next1 = {0, 0};
    struct timeval now;

    while (gettimeofday(&now, NULL) == -1);

    // If there is no known "next" frame, initialize it now.
    if (next1.tv_sec == 0) {
        next1 = now;
        next1.tv_usec++;
    }

    // If we're on AUTO_FRAMERATE, we'll display frames always only if there's excess time.
    // Otherwise we'll display the defined amount of frames.
    unsigned limit = (Settings.SkipFrames == AUTO_FRAMERATE) ? (timercmp(&next1, &now, <) ? 10 : 1)
                                                             : Settings.SkipFrames;

    IPPU.RenderThisFrame = (++IPPU.SkippedFrames >= limit) ? TRUE : FALSE;

    if (IPPU.RenderThisFrame)
        IPPU.SkippedFrames = 0;
    else {
        // If we were behind the schedule, check how much it is.
        if (timercmp(&next1, &now, <)) {
            unsigned lag = (now.tv_sec - next1.tv_sec) * 1000000 + now.tv_usec - next1.tv_usec;
            if (lag >= 500000) {
                // More than a half-second behind means probably pause.
                // The next line prevents the magic fast-forward effect.
                next1 = now;
            }
        }
    }

    // Delay until we're completed this frame.
    // Can't use setitimer because the sound code already could be using it. We don't actually need it either.
    while (timercmp(&next1, &now, >)) {
        // If we're ahead of time, sleep a while.
        unsigned timeleft = (next1.tv_sec - now.tv_sec) * 1000000 + next1.tv_usec - now.tv_usec;
#ifdef __SWITCH__
        svcSleepThread(timeleft * 1000);
#else
        usleep(timeleft);
#endif
        while (gettimeofday(&now, NULL) == -1);
        // Continue with a while-loop because usleep() could be interrupted by a signal.
    }

    // Calculate the timestamp of the next frame.
    next1.tv_usec += Settings.FrameTime;
    if (next1.tv_usec >= 1000000) {
        next1.tv_sec += next1.tv_usec / 1000000;
        next1.tv_usec %= 1000000;
    }
#endif
}

/**
 * Called by Snes9x to poll for buttons that should be polled.
 */
bool S9xPollButton(uint32 id, bool *pressed) {
    return false;
}

/**
 * Called by Snes9x to poll for axises that should be polled.
 */
bool S9xPollAxis(uint32 id, int16 *value) {
    return false;
}

/**
 * Called by Snes9x to poll for poiters that should be polled.
 */
bool S9xPollPointer(uint32 id, int16 *x, int16 *y) {
    return false;
}

/**
 * Handle port-specific commands (?).
 */
void S9xHandlePortCommand(s9xcommand_t cmd, int16 data1, int16 data2) {
}

/**
 * S9xInitSound function calls this function to actually open the host sound device.
 */
bool8 S9xOpenSoundDevice(void) {

#ifndef NOSOUND
    printf("S9xOpenSoundDevice\n");
    Audio *audio = new C2DAudio(Settings.SoundPlaybackRate, 60, S9xAudioCallback);
    _ui->setAudio(audio);
    S9xSetSamplesAvailableCallback(S9xSamplesAvailable, nullptr);
#endif
    return TRUE;
}

/**
 * Called when some fatal error situation arises or when the “q” debugger command is used.
 */
void S9xExit() {
    //exit(0);
    _ui->getUiEmu()->stop();
}

/**
 * When Snes9x wants to display an error, information or warning message, it calls this function.
 * Check in messages.h for the types and individual message numbers that Snes9x currently passes as parameters.
 * The idea is display the message string so the user can see it, but you choose not to display anything at all,
 * or change the message based on the message number or message type.
 * Eventually all debug output will also go via this function, trace information already does.
 */
void S9xMessage(int type, int number, const char *message) {
    printf("S9xMessage: type: %d number: %d message: %s\n", type, number, message);
}

/**
 * Used by Snes9x to ask the user for input.
 */
const char *S9xStringInput(const char *message) {
    static char buffer[256];

    printf("%s: ", message);
    fflush(stdout);

    if (fgets(buffer, sizeof(buffer) - 2, stdin)) {
        return buffer;
    }

    return nullptr;
}

/**
 * Called when the SNES color palette has changed.
 * Use this function if your system should change its color palette to match the SNES's.
 * Otherwise let it empty.
 */
void S9xSetPalette() {
}