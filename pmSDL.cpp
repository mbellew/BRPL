//
//  pmSDL.cpp
//  SDLprojectM
//
//  Created by Mischa Spiegelmock on 2017-09-18.
//

#include "pmSDL.hpp"
#include "Renderer/BeatDetect.hpp"
#include "TimeKeeper.hpp"
#include "signal.h"

void clearPiano();


unsigned randomInt(unsigned max)
{
    return ((unsigned)random()) % max;
}
bool randomBool()
{
    return ((unsigned)random()) & 0x0001 ? true : false;
}
float randomFloat()
{
    return random() / (float)RAND_MAX;
}


volatile sig_atomic_t terminate = 0;

void term(int signum)
{
    terminate = 1;
}



void projectMSDL::audioInputCallbackF32(void *userdata, unsigned char *stream, int len)
{
    projectMSDL *app = (projectMSDL *) userdata;
    app->pcm()->addPCMfloat((float *)stream, len/sizeof(float));
}


void projectMSDL::audioInputCallbackS16(void *userdata, unsigned char *stream, int len)
{
    projectMSDL *app = (projectMSDL *) userdata;
    short pcm16[2][512];

    for (int i = 0; i < 512; i++) {
        for (int j = 0; j < app->audioChannelsCount; j++) {
            pcm16[j][i] = stream[i+j];
        }
    }
    app->pcm()->addPCM16(pcm16);
}


SDL_AudioDeviceID projectMSDL::selectAudioInput(int count) {
    // ask the user which capture device to use
    // printf("Please select which audio input to use:\n");
    printf("Detected devices:\n");
    int selected = -1;
    for (int i = 0; i < count; i++)
    {
        if (0==strcmp("Monitor of Bose QuietComfort 35",SDL_GetAudioDeviceName(i, true)))
            selected = i;
        printf("  %i: 🎤%s\n", i, SDL_GetAudioDeviceName(i, true));
    }
    if (selected != -1)
        return selected;
    return 0;

    while (count > 1)
    {
        printf("select input: ");
        int ret = scanf("%d\n", &selected);
        if (ret == 1 || ret == EOF)
            return selected;
    }
}


int projectMSDL::openAudioInput()
{
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
    {
        const char* driver_name = SDL_GetAudioDriver(i);
        printf("AUDIO DRIVER: %s\n", driver_name);
    }

    // get audio driver name (static)
    const char* driver_name = SDL_GetCurrentAudioDriver();
    SDL_Log("Using audio driver: %s\n", driver_name);

    // get audio input device
    int count = SDL_GetNumAudioDevices(true);  // capture, please
    if (count == 0) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "No audio capture devices found");
        SDL_Quit();
    }
    for (int i = 0; i < count; i++) {
        SDL_Log("Found audio capture device %d: %s", i, SDL_GetAudioDeviceName(i, true));
    }

    SDL_AudioDeviceID selectedAudioDevice = 0;  // device to open
    if (count > 1) {
        // need to choose which input device to use
        selectedAudioDevice = selectAudioInput(count);
        if (selectedAudioDevice > count) {
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "No audio input device specified.");
            SDL_Quit();
        }
    }

    // params for audio input
    SDL_AudioSpec want, have;

    // requested format
    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_F32;  // float
    want.channels = 2;
    want.samples = 512;
    want.callback = projectMSDL::audioInputCallbackF32;
    want.userdata = this;

    int retry = 0;
    do
    {
        audioDeviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selectedAudioDevice, true), true, &want, &have, 0);
    } 
    while (audioDeviceID == 0 && retry++ < 3);
    if (audioDeviceID == 0) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Failed to open audio capture device: %s", SDL_GetError());
        SDL_Quit();
    }

    // read characteristics of opened capture device
    SDL_Log("Opened audio capture device %i: %s", audioDeviceID, SDL_GetAudioDeviceName(selectedAudioDevice, true));
    SDL_Log("Sample rate: %i, frequency: %i, channels: %i, format: %i", have.samples, have.freq, have.channels, have.format);
    audioChannelsCount = have.channels;
    audioSampleRate = have.freq;
    audioSampleCount = have.samples;
    audioFormat = have.format;
    audioInputDevice = audioDeviceID;
    return 1;
}

void projectMSDL::beginAudioCapture()
{
    // allocate a buffer to store PCM data for feeding in
    unsigned int maxSamples = audioChannelsCount * audioSampleCount;
    SDL_PauseAudioDevice(audioDeviceID, false);
    pcm()->initPCM(2048);
}

void projectMSDL::endAudioCapture()
{
    SDL_PauseAudioDevice(audioDeviceID, true);
}

void projectMSDL::maximize()
{
    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(0, &dm) != 0) {
        SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
        return;
    }

    SDL_SetWindowSize(win, dm.w, dm.h);
    resize(dm.w, dm.h);
}

void projectMSDL::toggleFullScreen()
{
    maximize();
    if (isFullScreen) {
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
        isFullScreen = false;
        SDL_ShowCursor(true);
    } else {
        SDL_ShowCursor(false);
        SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN);
        isFullScreen = true;
    }
}

void projectMSDL::keyHandler(SDL_Event *sdl_evt) {
    projectMEvent evt;
    projectMKeycode key;
    projectMModifier mod;
    SDL_Keymod sdl_mod = (SDL_Keymod) sdl_evt->key.keysym.mod;
    SDL_Keycode sdl_keycode = sdl_evt->key.keysym.sym;

    // handle keyboard input (for our app first, then projectM)
    switch (sdl_keycode) {
        case SDLK_f:
            if (sdl_mod & KMOD_LGUI || sdl_mod & KMOD_RGUI || sdl_mod & KMOD_LCTRL) {
                // command-f: fullscreen
                toggleFullScreen();
                return; // handled
            }
            break;
    }

    // translate into projectM codes and perform default projectM handler
    evt = sdl2pmEvent(sdl_evt);
    key = sdl2pmKeycode(sdl_keycode);
    mod = sdl2pmModifier(sdl_mod);
    key_handler(evt, key, mod);
}

void projectMSDL::addFakePCM() {
    int i;
    short pcm_data[2][512];
    /** Produce some fake PCM data to stuff into projectM */
    for ( i = 0 ; i < 512 ; i++ ) {
        if ( i % 2 == 0 ) {
            pcm_data[0][i] = (float)( rand() / ( (float)RAND_MAX ) * (pow(2,14) ) );
            pcm_data[1][i] = (float)( rand() / ( (float)RAND_MAX ) * (pow(2,14) ) );
        } else {
            pcm_data[0][i] = (float)( rand() / ( (float)RAND_MAX ) * (pow(2,14) ) );
            pcm_data[1][i] = (float)( rand() / ( (float)RAND_MAX ) * (pow(2,14) ) );
        }
        if ( i % 2 == 1 ) {
            pcm_data[0][i] = -pcm_data[0][i];
            pcm_data[1][i] = -pcm_data[1][i];
        }
    }

    /** Add the waveform data */
    pcm()->addPCM16(pcm_data);
}

void projectMSDL::resize(unsigned int width_, unsigned int height_) {
    width = width_;
    height = height_;
    settings.windowWidth = width;
    settings.windowHeight = height;
    projectM_resetGL(width, height);
}

bool projectMSDL::pollEvent() {
    SDL_Event evt;

    while (SDL_PollEvent(&evt))
    {
        switch (evt.type) {
            case SDL_WINDOWEVENT:
                switch (evt.window.event) {
                    case SDL_WINDOWEVENT_RESIZED:
                        resize(evt.window.data1, evt.window.data2);
                        break;
                }
                break;
            case SDL_KEYDOWN:
                keyHandler(&evt);
                break;
            case SDL_QUIT:
                done = true;
                break;
        }
    }
    if (terminate)
        done = true;
    if (done)
       clearPiano();
   return !done;
}


projectMSDL::projectMSDL(Settings settings, int flags) : projectM(settings, flags) {
    // width = getWindowWidth();
    // height = getWindowHeight();
    done = 0;
    isFullScreen = false;
}

projectMSDL::projectMSDL(std::string config_file, int flags) : projectM(config_file, flags) {
    // width = getWindowWidth();
    // height = getWindowHeight();
    done = 0;
    isFullScreen = false;
}



void projectMSDL::init(SDL_Window *window, SDL_Renderer *renderer) 
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);

    win = window;
    rend = renderer;
    // selectRandom(true);
    // projectM_resetGL(width, height);
}


std::string projectMSDL::getActivePresetName()
{
    return std::string("hey");
}


#define IMAGE_SIZE 20
#define IMAGE_SCALE ((float)IMAGE_SIZE)
#define OB_LEFT 0
#define OB_RIGHT (IMAGE_SIZE-1)
#define IB_LEFT 1
#define IB_RIGHT (IMAGE_SIZE-2)
#define LEFTMOST_PIXEL 0
#define RIGHTMOST_PIXEL (IMAGE_SIZE-1)
#define RED_CHANNEL 0
#define BLUE_CHANNEL 1
#define GREEN_CHANNEL 2


inline float IN(float a, float b, float r)
{
    return a*(1.0-r) + b*(r);
}
inline float MAX(float a, float b)
{
    return a>b ? a : b;
}
inline float MAX(float a, float b, float c)
{
    return MAX(a,MAX(b,c));
}
inline float MIN(float a, float b)
{
    return a<b ? a : b;
}
inline float MIN(float a, float b, float c)
{
    return MIN(a,MIN(b,c));
}
inline float constrain(float x, float mn, float mx)
{
    return x<mn ? mn : x>mx ? mx : x;
}
inline float constrainf(float x, float mn, float mx)
{
    return x<mn ? mn : x>mx ? mx : x;
}
inline float constrain(float x)
{
    return constrain(x, 0.0, 1.0);
}
inline int constrain(int x, int mn, int mx)
{
    return x<mn ? mn : x>mx ? mx : x;
}

void saturate(float *rgb, float i=1.0)
{
    float m = MAX(rgb[0], MAX(rgb[1],rgb[2]));
    if (m <= 0)
    {
        rgb[0] = rgb[1] = rgb[2] = 0;
    }
    else
    {
        rgb[0] = rgb[0] * i / m;
        rgb[1] = rgb[1] * i / m;
        rgb[2] = rgb[2] * i / m;
    }
}


class Generator
{
public:
    virtual float next(float time) const = 0;
};


void rgb2hsl(float *rgb, float *hsl)
{
    float r = rgb[0], g = rgb[1], b = rgb[2];
	float min = MIN(r, g, b);
	float max = MAX(r, g, b);
	float diff = max - min;
	float h = 0, s = 0, l = (min + max) / 2;

	if (diff != 0)
    {
		s = l < 0.5 ? diff / (max + min) : diff / (2 - max - min);
		h = r == max ? 0 + (g - b) / diff :
            g == max ? 2 + (b - r) / diff :
                       4 + (r - g) / diff;
	}
    hsl[0] = h/6.0;
    hsl[1] = s;
    hsl[2] = l;
}
float _hue2rgb(float p, float q, float t)
{
    if (t < 0) t += 1;
    if (t >= 1) t -= 1;
    if (t < 1.0/6.0) return p + (q - p) * 6 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2/3 - t) * 6;
    return p;
}
void hsl2rgb(float *hsl, float *rgb)
{
    float h = fmod(hsl[0],1.0), s = hsl[1], l = hsl[2];

	if (s == 0)
    {
        rgb[0] = rgb[1] = rgb[2] = l;
	}
    else
    {
        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;
        rgb[0] = _hue2rgb(p, q, h + 1.0/3.0);
        rgb[1] = _hue2rgb(p, q, h);
        rgb[2] = _hue2rgb(p, q, h - 1.0/3.0);
    }
}

union Color
{
    Color() : arr {0,0,0,1} {}
    Color(unsigned int rgb)
    {
        rgba.r = ((rgb>>16) & 0xff) / 255.0;
        rgba.g = ((rgb>> 8) & 0xff) / 255.0;
        rgba.b = ((rgb>> 0) & 0xff) / 255.0;
        rgba.a = 1.0;
    }
    Color(unsigned r, unsigned g, unsigned b)
    {
        rgba.r = r / 255.0;
        rgba.g = g / 255.0;
        rgba.b = b / 255.0;
        rgba.a = 1.0;
    }
    Color(float r, float g, float b) : arr {r,g,b,1}
    {
    }
    Color(float r, float g, float b, float a) : arr {r,g,b,a}
    {
    }
    // Color(float t, Generator &a, Generator &b, Generator &c) :
    //     arr {a.next(t), b.next(t), c.next(t), 1.0} {}
    Color(const Color &a, const Color &b, float ratio)
    {
        rgba.r = IN(a.rgba.r,b.rgba.r,ratio);
        rgba.g = IN(a.rgba.g,b.rgba.g,ratio);
        rgba.b = IN(a.rgba.b,b.rgba.b,ratio);
        rgba.a = 1.0;
    }

    float arr[4];
    struct
    {
        float r; float g; float b; float a;
    } rgba;
    struct
    {
        float h; float s; float l; float _;
    } hsl;

    void saturate(float intensity)
    {
        ::saturate(arr,intensity);
    }
    void constrain()
    {
        rgba.r = ::constrain(rgba.r);
        rgba.g = ::constrain(rgba.g);
        rgba.b = ::constrain(rgba.b);
    }
    void constrain2()
    {
        if (rgba.r > 1.0 || rgba.b > 1.0 || rgba.g > 1.0)
            saturate(1.0);
    }
    Color &complement()
    {
        Color hsl;
        rgb2hsl(arr, hsl.arr);
        hsl.arr[0] += 0.5;
        hsl2rgb(hsl.arr, arr);
        return *this;
    }
};

const Color WHITE(1.0f,1.0f,1.0f);
const Color BLACK;
const Color RED(1.0f, 0.0f, 0.0f);
const Color GREEN(0.0f, 1.0f, 0.0f);
const Color BLUE(0.0f, 0.0f, 1.0f);


Color & operator *=( Color &c, float f )
{
    c.rgba.r *= f;
    c.rgba.g *= f;
    c.rgba.b *= f;
    c.constrain();
    return c;
}

Color operator *( const Color &c, float f )
{
    Color tmp(c);
    tmp *= f;
    return tmp;
}

Color & operator +=( Color &c, const Color &a )
{
    if (a.rgba.a == 1.0)
    {
        c.rgba.r += a.rgba.r;
        c.rgba.g += a.rgba.g;
        c.rgba.b += a.rgba.b;
        c.rgba.a = 1.0;
        return c;
    }
    c.rgba.r = IN(c.rgba.r, a.rgba.r, a.rgba.a);
    c.rgba.g = IN(c.rgba.g, a.rgba.g, a.rgba.a);
    c.rgba.b = IN(c.rgba.b, a.rgba.b, a.rgba.a);
    c.rgba.a = 1.0;
    return c;
}

Color operator +(const Color &a, const Color &b)
{
    Color ret(
        a.rgba.r + b.rgba.r,
        a.rgba.g + b.rgba.g,
        a.rgba.b + b.rgba.b,
        1.0
    );
    return ret;
}

Color operator -(const Color &a, const Color &b)
{
    Color ret(
        a.rgba.r - b.rgba.r,
        a.rgba.g - b.rgba.g,
        a.rgba.b - b.rgba.b,
        1.0
    );
    return ret;
}


class SimpleGenerator : public Generator
{
    float offset, scale, speed, r;
public:
    SimpleGenerator() : offset(0.5), scale(0.5), speed(1.0) {}
    SimpleGenerator(float _min, float _max, float _time)
    {
        offset = (_max + _min) / 2.0;
        scale = fabs(_max - _min) / 2.0;
        speed = 2*M_PI / _time;
    }
    SimpleGenerator(const SimpleGenerator &src)
    {
        offset = src.offset;
        scale = src.scale;
        speed = src.speed;
    }
    void copy(const SimpleGenerator &src)
    {
        offset = src.offset;
        scale = src.scale;
        speed = src.speed;
    }
    float next(float t) const
    {
        return offset + scale * sin(t*speed);
    }
};
class Spirograph : public Generator
{
public:
    SimpleGenerator a;
    SimpleGenerator b;
    Spirograph() : a(), b() {}
    Spirograph(const SimpleGenerator &_a, const SimpleGenerator &_b) : a(_a), b(_b)
    {
    }

    float next(float t) const
    {
        return a.next(t) + b.next(t);
    }
};
class ColorGenerator
{
public:
    virtual Color next(float t) const = 0;
};
class ComboGenerator : public ColorGenerator
{
    Generator *r, *g, *b;
public:
     ComboGenerator(Generator *_r, Generator *_g, Generator *_b) :
        r(_r), g(_g), b(_b)
    {
    }

    Color next(float t) const
    {
        Color c(r->next(t), g->next(t), b->next(t));
        c.constrain2();
        return c;
    }
};


class PaletteGenerator : public ColorGenerator
{
    float speed;
    int count;
    Color colors[10];

public:
    PaletteGenerator(const Color &c0, const Color &c1) :
        speed(1.0)
    {
        count = 2;
        colors[0] = c0;
        colors[1] = c1;
    }
    PaletteGenerator(const Color &c0, const Color &c1, const Color &c2, const Color &c3) :
        speed(1.0)
    {
        count = 4;
        colors[0] = c0;
        colors[1] = c1;
        colors[2] = c2;
        colors[3] = c3;
    }
    PaletteGenerator(const Color &c0, const Color &c1, const Color &c2, const Color &c3, const Color &c4) :
        speed(1.0)
    {
        count = 5;
        colors[0] = c0;
        colors[1] = c1;
        colors[2] = c2;
        colors[3] = c3;
        colors[4] = c4;
    }

    Color next(float t) const
    {
        float value = fmod(t/speed, 1.0) * count;

        int i = (int)floor(value);
        int j = (i+1) % count;
        float f = fmod(value,1.0);
        Color c = (colors[i] * (1.0-f)) + (colors[j] * f); 
        return c;
    }

    Color next(int t) const
    {
        return colors[t % count];
    }

    Color get(int t) const
    {
        return colors[t % count];
    }

};


PaletteGenerator palette0(
    Color(0u, 38u, 66u),        // oxford blue
    Color(132u, 0u, 50u),       // burgandy
    Color(229u, 218u, 218u),     // gainsboro
    Color(229u, 149u, 0u),      // harvest gold
    Color(2u, 4, 15, 1)          // rich black
);

// fruit salad (strawberry, orange)
PaletteGenerator palette1(
    Color(0xfe0229),
    Color(0xc6df5f),
    Color(0xfff9d4),
    Color(0xf06c00),
    Color(0xffd000)
);

PaletteGenerator palette2(
    Color(46u, 196, 182),       // maximum blue green
    Color(231u, 29, 54),        // rose madder
    Color(255u, 159u, 28),       // bright yellow (crayola)`
    Color(253u, 255, 252),      // baby powder
    Color(1u, 22, 39)          // maastrict blue
);

PaletteGenerator greekpalette(
    Color(0x034488),
    Color(0x178fd6),
    Color(0xfffcf6),
    Color(0xedece8),
    Color(0xccdde8)
);

/*
PaletteGenerator raspberry(
    Color(0xfec6d2),
    Color(0xfa5d66),
    Color(0xa20106),
    Color(0x540e0b),
    Color(0x1d0205) 
); */

PaletteGenerator palette4(
    Color(255u, 0, 34),
    Color(65u, 234, 212),
    Color(253u, 255, 252),
    Color(185u, 19, 114),
    Color(1u, 22, 39)
);

PaletteGenerator palette5(
    Color(224u, 255, 79),
    Color(255u, 102, 99),
    Color(254u, 255, 254),
    Color(11u, 57, 84),
    Color(191u, 215, 234)
);

PaletteGenerator *palettes[] =
{
    &palette0, &palette1, &palette2, &greekpalette, &palette4,
    &palette5
};
const int paletteCount = sizeof(palettes)/sizeof(void *);


class PointContext
{
public:
    int pos;
    // per_point in
    float rad;  // distance from center
    // per_point in/out
    float sx;
    float cx;
    float dx;
    // sx and cx are converted to dx in after_per_point()
};

class PatternContext
{
public:
    PatternContext() : fade(1.0), blur(false), fade_to(), cx(0.5), sx(1.0), dx(0.0), saturate(0), gamma(2.0),
        ob_size(0), ib_size(0),
        time(0), frame(0)
    {
    }
    PatternContext(PatternContext &from)
    {
        memcpy(this,&from,sizeof(from));
    }
    Color fade_to;
    float fade;
    bool blur;
    float cx;       // center
    float sx;       // stretch
    float dx;       // slide
    bool  dx_wrap;  // rotate?

    unsigned ob_size;  // left border
    Color ob_right;
    Color ob_left;
    unsigned ib_size;  // right border
    Color ib_right;
    Color ib_left;
    // post process
    float gamma;
    bool  saturate;

    float time;
    float dtime;
    unsigned frame;

    float vol;
    float bass;
    float mid;
    float treb;
    //float vol_att;
    float bass_att;
    float mid_att;
    float treb_att;
    bool  beat;
    float lastbeat;
    float interval;

    PointContext points[IMAGE_SIZE];
};


class Image
{
public:
    Color map[IMAGE_SIZE];

    Image()
    {
        memset(map, 0, sizeof(map));
    }
    Image(Image &from)
    {
        memcpy(map, from.map, sizeof(map));
    }
    void copyFrom(Image &from)
    {
        memcpy(map, from.map, sizeof(map));
    }

    float getValue(unsigned color, float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i <= 0)
            return map[0].arr[color];
        if (i+1 >= IMAGE_SIZE-1)
            return map[IMAGE_SIZE-1].arr[color];
        return IN(map[i].arr[color], map[i+1].arr[color], f);
    }

    float getValue_wrap(unsigned color, float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i < 0)
            i += IMAGE_SIZE;
        if (i >= IMAGE_SIZE)
            i = i - IMAGE_SIZE;
        int j = (i+1) % IMAGE_SIZE;
        return IN(map[i].arr[color], map[j].arr[color], f);
    }

    void getRGB(float f, float *rgb)
    {
        rgb[0] = getValue(RED_CHANNEL, f);
        rgb[1] = getValue(GREEN_CHANNEL, f);
        rgb[2] = getValue(BLUE_CHANNEL, f);
    }

    Color getRGB(float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i <= 0)
            return map[0];
        if (i+1 >= IMAGE_SIZE-1)
            return map[IMAGE_SIZE-1];
        Color c(map[i], map[i+1], f);
        return c;
    }

    Color getRGB(int i)
    {
        return map[i];
    }

    void getRGB_wrap(float f, float *rgb)
    {
        rgb[0] = getValue_wrap(RED_CHANNEL, f);
        rgb[1] = getValue_wrap(GREEN_CHANNEL, f);
        rgb[2] = getValue_wrap(BLUE_CHANNEL, f);
    }

    Color getRGB_wrap(float f)
    {
        int i=(int)floor(f);
        f = f - i;
        i = i % IMAGE_SIZE;
        if (i < 0)
            i += IMAGE_SIZE;
        int j = (i+1) % IMAGE_SIZE;
        Color c(map[i], map[j], f);
        return c;
    }

    void setRGB(int i, float r, float g, float b)
    {
        map[i].rgba.r = constrain(r);
        map[i].rgba.g = constrain(g);
        map[i].rgba.b = constrain(b);
    }

    void setRGB(int i, Color c)
    {
        map[i] = c;
        map[i].rgba.a = 1.0;
        map[i].constrain();
    }

    void setRGBA(int i, float r, float g, float b, float a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, r, a);
        map[i].rgba.g = IN(map[i].rgba.g, g, a);
        map[i].rgba.b = IN(map[i].rgba.b, b, a);
        map[i].constrain();
    }

    void setRGBA(int i, const Color &c, float a)
    {
        map[i].rgba.r = IN(map[i].rgba.r, c.rgba.r, a);
        map[i].rgba.g = IN(map[i].rgba.g, c.rgba.g, a);
        map[i].rgba.b = IN(map[i].rgba.b, c.rgba.b, a);
        map[i].constrain();
    }

    void setRGBA(int i, const Color &c)
    {
        setRGBA(i,c,c.rgba.a);
    }

    void addRGB(int i, const Color &c)
    {
        map[i] += c;
        map[i].constrain();
    }

    void addRGB(float f, const Color &c)
    {
        int i = floor(f);
        if (i<=-1 || i>=IMAGE_SIZE)
            return;
        float r = fmodf(f,1.0);
        Color tmp;
        if (i>=0 && i<IMAGE_SIZE)
        {
            tmp = c * (1-r);
            addRGB(i, tmp);
        }
        if (i+1>=0 && i+1<IMAGE_SIZE)
        {
            tmp = c * r;
            addRGB(i+1, tmp);
        }
    }

    void setAll(const Color &c)
    {
        for (int i=0 ; i<IMAGE_SIZE ; i++)
            setRGB(i,c);
    }

    void stretch(float sx, float cx)
    {
        Image cp(*this);
        float center = IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, cx);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = (i-center)/sx + center;
            setRGB(i, cp.getRGB(from));
        }
    }

    void move(float dx)
    {
        Image cp(*this);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SCALE;
            setRGB(i, cp.getRGB(from));
        }
    }

    void rotate(float dx)
    {
        Image cp(*this);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SCALE;
            setRGB(i, cp.getRGB_wrap( from ));
        }
    }

    void decay(float d)
    {
        // scale d a little
        d = (d-1.0) * 0.5 + 1.0;
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
            map[i] *= d;
    }

    void fade(float d, const Color &to)
    {
        if (d==1.0f)
            return;
        if (to.rgba.r==0 && to.rgba.g==0 && to.rgba.b==0)
        {
            decay(d);
            return;
        }
        // scale d a little
        d = (d-1.0) * 0.5 + 1.0;
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            map[i] = to - (to - map[i]) * d;
        }
    }

    void blur()
    {
        Image cp(*this);
        for (int i=0 ; i<IMAGE_SIZE ; i++)
        {
            Color prev  = cp.getRGB(i-1<0?0:i-1);
            Color color = cp.getRGB(i);
            Color next  = cp.getRGB(i+1>IMAGE_SIZE-1?IMAGE_SIZE-1:i+1);
            Color blur = prev*0.25 + color*0.5 + next*0.25;
            setRGB(i,blur);
        }
    }
};


class Pattern
{
public:
    virtual std::string name() const = 0;
    virtual void setup(PatternContext &ctx) = 0;

    // frame methods

    // update private variables
    virtual void per_frame(PatternContext &ctx) = 0;
    virtual void per_point(PatternContext &ctx, PointContext &pt) {};

    // copy/morph previous frame
    virtual void update(PatternContext &ctx, Image &image) = 0;
    // draw into the current frame
    virtual void draw(PatternContext &ctx, Image &image) = 0;
    // effects (not-persisted)
    virtual void effects(PatternContext &ctx, Image &image) = 0;
    // update private variables
    virtual void end_frame(PatternContext &ctx) = 0;
};


FILE *device = 0;

void drawPiano(projectMSDL *pmSDL, PatternContext &ctx, Image &image)
{
    if (device == 0)
    {
        if (!pmSDL->dmx_device.empty())
            device = fopen(pmSDL->dmx_device.c_str(), "a");
        else
            device = stdout;
       clearPiano();
    }
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
        Color c = image.getRGB(i);
        int r = (int)(pow(constrain(c.rgba.r),ctx.gamma) * 255);
        int g = (int)(pow(constrain(c.rgba.g),ctx.gamma) * 255);
        int b = (int)(pow(constrain(c.rgba.b),ctx.gamma) * 255);
        fprintf(device, "%d,%d,%d,", r, g, b);
    }
    fprintf(device, "\n");
    fflush(device);
}


void clearPiano()
{
    if (device)
    {
        for (int i=0 ; i<IMAGE_SIZE+4 ; i++)
            fputs("0,0,0,", device);
        fputs("\n", device);
        fflush(device);
    }
}


PatternContext context;
Image work;
Image stash;

void loadPatterns();
std::vector<Pattern *> patterns;
Pattern *currentPattern = NULL;

class MyBeat007 
{
    float sure;
    float maxdbass;
    float pbass;
public:
    bool beat;
    float lastbeat;
    float interval;

    MyBeat007() :
        sure(0.6), 
        interval(40),
        lastbeat(0),
        maxdbass(0.012)
    {}

    void update(float frame, float fps, BeatDetect *beatDetect)
    {
        float dbass = (beatDetect->bass - pbass) / fps;
        //fprintf(stderr, "%lf %lf\n", dbass, maxdbass);
        beat = dbass > 0.6*maxdbass && frame-lastbeat > 1.0/3.0;
        if (beat && abs(frame-(lastbeat+interval))<1.0/5.0)
            sure = sure+0.095f;
        else if (beat)
            beat = sure-0.095f;
        else
            sure = sure * 0.9996;
        sure = constrain(sure, 0.5f, 1.0f);

        bool cheat = frame > lastbeat+interval + int(1.0/10.0) && sure > 0.91;
        if (cheat)
        {
            beat = 1;
            sure = sure * 0.95;
        }
        maxdbass = MAX(maxdbass*0.999,dbass);
        maxdbass = constrain(maxdbass,0.012, 0.02);
        if (beat)
        {
            interval = frame-lastbeat;
            lastbeat = frame - (cheat ? (int)(1.0/10.0) : 0);
        }
        pbass = beatDetect->bass;
    }
};

MyBeat007 mybeat;
int pattern_index = 0;

void projectMSDL::renderFrame()
{
    float prev_time = timeKeeper->GetRunningTime();
    timeKeeper->UpdateTimers();
    beatDetect->detectFromSamples();
    if (beatDetect->bass < 0 || beatDetect->mid < 0 || beatDetect->treb < 0)
    {
        //*(char *)NULL = '0';
    }
    mybeat.update(timeKeeper->GetRunningTime(), 30, beatDetect);

    if (patterns.size() == 0)
        loadPatterns();

    float beat_sensitivity = beatDetect->beat_sensitivity - timeKeeper->PresetProgressA();
    if (NULL == currentPattern ||
        timeKeeper->PresetProgressA() > 1.0 ||
        ((beatDetect->vol-beatDetect->vol_old>beat_sensitivity) && timeKeeper->CanHardCut()))
    {
        if (patterns.size() == 0)
            return;
        if (patterns.size()==1)
            pattern_index = 0;
        else if (patterns.size() == 2)
            pattern_index++;
        else
            pattern_index = pattern_index+1 + randomInt(patterns.size()-1);
        pattern_index = pattern_index % patterns.size();
        currentPattern = patterns.at(pattern_index);
        currentPattern->setup(context);
        fprintf(stdout, "%s\n", currentPattern->name().c_str());
        if (device && device != stdout)
            fprintf(device, "%s\n", currentPattern->name().c_str());
        timeKeeper->StartPreset();
    }
    Pattern *pattern = currentPattern;

    //fprintf(stderr, "%lf %lf %lf\n", beatDetect->bass, beatDetect->mid, beatDetect->treb);

    PatternContext frame(context);
    frame.time  = timeKeeper->GetRunningTime();
    frame.dtime = frame.time - prev_time;
    frame.frame = timeKeeper->PresetProgressA();
    frame.bass  = constrainf(beatDetect->bass, 0.0f, 100.0f);
    frame.mid   = constrainf(beatDetect->mid, 0.0f, 100.0f);
    frame.treb  = constrainf(beatDetect->treb, 0.0f, 100.0f);
    frame.bass_att = constrainf(beatDetect->bass_att, 0.01f, 100.0f);
    frame.mid_att  = constrainf(beatDetect->mid_att, 0.01f, 100.0f);
    frame.treb_att = constrainf(beatDetect->treb_att, 0.01f, 100.0f);
    // frame.vol = beatDetect->vol;
    frame.vol      = constrainf((beatDetect->bass + beatDetect->mid + beatDetect->treb) / 3.0, 0.1, 100.0);
    //frame.vol_att = beatDetect->vol_attr; // (beatDetect->bass_att + beatDetect->mid_att + beatDetect->treb_att) / 3.0;
    frame.beat = mybeat.beat;
    frame.lastbeat = mybeat.lastbeat;
    frame.interval = mybeat.interval;

    pattern->per_frame(frame);
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
        PointContext &pt = frame.points[i];
        pt.pos = i;
        pt.rad = ((double)i)/(IMAGE_SIZE-1) - 0.5;
        pt.sx = frame.sx;
        pt.cx = frame.cx;
        pt.dx = frame.dx;
    }
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
        pattern->per_point(frame, frame.points[i]);
    }
    // convert sx,cx to dx
    for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
    {
        float center = IN(-0.5, 0.5, frame.points[i].cx);
        frame.points[i].dx += (frame.points[i].rad-center) * (frame.points[i].sx-1.0);
    }

    pattern->update(frame, work);
    pattern->draw(frame,work);
    stash.copyFrom(work);
    pattern->effects(frame,work);
    drawPiano(this, frame, work);
    work.copyFrom(stash);
    pattern->end_frame(frame);
}



/****************************
 * PATTERNS *****************
 ***************************/


class AbstractPattern : public Pattern
{
protected:
    std::string pattern_name;

    AbstractPattern(std::string _name) : pattern_name(_name)
    {}
public:
    std::string name() const
    {
        return pattern_name;
    }
    void per_Frame(PatternContext &ctx)
    {
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    void update(PatternContext &ctx, Image &image)
    {
        Image cp(image);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i - IMAGE_SCALE * ctx.points[i].dx;
            if (ctx.dx_wrap)
                image.setRGB(i, cp.getRGB_wrap(from));
            else
                image.setRGB(i, cp.getRGB(from));
        }
        if (ctx.blur)
            image.blur();
        image.fade(ctx.fade, ctx.fade_to);
    }

    void draw(PatternContext &ctx, Image &image)
    {
        if (ctx.ob_size)
            image.setRGB(OB_LEFT, ctx.ob_left);
        if (ctx.ib_size)
            image.setRGB(IB_LEFT, ctx.ib_left);

        if (ctx.ob_size)
            image.setRGB(OB_RIGHT, ctx.ob_right);
        if (ctx.ib_size)
            image.setRGB(IB_RIGHT, ctx.ib_right);
    }

    void effects(PatternContext &ctx, Image &image)
    {}

    void end_frame(PatternContext &ctx)
    {}
};


class Waterfall : public AbstractPattern
{
    ColorGenerator *lb_color;
    ColorGenerator *rb_color;
    SimpleGenerator cx;
    PaletteGenerator *palette;
    bool option=false, option_set = false;
    
public:
    Waterfall() : AbstractPattern((const char *)"waterfall"),
        cx(0.05, 0.95, 6)
    {
        option = randomBool();
        lb_color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 1.5*5.5),
            new SimpleGenerator(0.2, 0.8, 1.5*6.5),
            new SimpleGenerator(0.2, 0.8, 1.5*7.5)
        );
        rb_color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 8),
            new SimpleGenerator(0.2, 0.8, 7),
            new SimpleGenerator(0.2, 0.8, 6)
        );
    }

    Waterfall(bool opt) : Waterfall()
    {
        option = opt;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        palette = palettes[randomInt(paletteCount)];
        ctx.sx = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        pattern_name = std::string("waterfall ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        ctx.ob_left = palette->get( 7 * (int)ctx.time );
        ctx.ob_left *= 0.5 + ctx.treb / 2.0;
        ctx.ib_left = ctx.ob_left;
        ctx.ib_left.complement();

        //ctx.ob_right = rb_color->next( ctx.time );
        ctx.ob_right = palette->get( 5 * (int)(ctx.time+2.0) );
        ctx.ob_right *= 0.5 + ctx.bass / 2.0;
        ctx.ib_right = ctx.ob_right;
        ctx.ib_right.complement();

        ctx.cx = cx.next(ctx.time);
    
        ctx.sx = 0.99 - 0.1 * ctx.bass;
        //fprintf(stderr,"%lf\n", ctx.sx);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (option)
        {
            float s = sin(ctx.time/15.0);
            image.rotate(s);
        }
    }
};


class GreenFlash : public AbstractPattern
{
    bool option1, option2, wipe, option_set;
    float vol_att = 1.0;
public:
    GreenFlash() : AbstractPattern((const char *)"green flash")
    {}

    GreenFlash(bool opt1, bool opt2) : GreenFlash()
    {
        option1 = opt1;
        option2 = opt2;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        ctx.cx = 0.5;
        ctx.sx = 1.05;
        ctx.fade = 0.99;
        if (!option_set)
        {
            option1 = randomBool();
            option2 = randomBool();
        }
        pattern_name = std::string("greenflash ") + (option1?"true":"false") + "/" + (option2?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        vol_att = IN(vol_att,ctx.vol,0.2);
        if (ctx.beat)
            wipe = !wipe; 
    }

    void draw(PatternContext &ctx, Image &image)
    {
        //Color c(ctx.treb+ctx.mid,ctx.mid,0); // yellow and red
        //Color c(ctx.treb,ctx.treb,ctx.mid); // yellow and blue
        Color c, bg;
        float d = ctx.mid-ctx.treb;
        float m = ctx.mid + d;
        float t = ctx.treb - d;
        if (t > 1.0)
            { m = m/t ; t = 1.0; }
        else if (m > 1.0)
            { t = t/m ; m = 1.0; }
        if (!option2)
        {
            //c = Color(ctx.treb_att,ctx.mid_att,ctx.treb_att);
            Color c1 = Color(0.0f, 1.0f, 0.0f); // green
            Color c2 = Color(1.0f, 0.0f, 1.0f); // purple
            c = c1*m + c2*t;
        } // green and purple
        else
        {
            // c = Color(1.0f, 1.0-t, 1.0-m); // red
            float v = MAX(ctx.vol,vol_att);
            c = Color(1.0f, 1.0-v/2, 1.0-v/2); // red
            bg = Color(1.0f, 1.0f, 1.0f);
        }
        c.constrain2();
        //Color c(ctx.treb,ctx.mid,ctx.bass);
        //int v = ctx.vol;// / MAX(1,ctx.vol_att);
        //int bass = ctx.bass; // MAX(ctx.bass_att, ctx.bass);
        float bass = MAX(ctx.bass_att, ctx.bass);
        int w = MIN(IMAGE_SIZE/2-1,(int)round(bass*3));
        image.setRGB(IMAGE_SIZE/2-1-w,c);
        image.setRGB(IMAGE_SIZE/2+w,c);

        if (ctx.beat)
            for (int i=(IMAGE_SIZE/2-1-w)+1 ; i<=(IMAGE_SIZE/2+w)-1 ; i++)
                image.setRGB(i, bg);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (option1)
            image.rotate(0.5);
    }
};

class Fractal : public AbstractPattern
{
    ColorGenerator *color;
    // SimpleGenerator r;
    // SimpleGenerator g;
    // SimpleGenerator b;
    Spirograph x;
    float ptime, ctime;
    bool option, option_set;

public:
    Fractal() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 0.5*5.5),
            new SimpleGenerator(0.2, 0.8, 0.5*6.5),
            new SimpleGenerator(0.2, 0.8, 0.5*7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE-1-6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a; x.b = b;
    }

    Fractal(bool opt) : Fractal()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ptime = ctime = ctx.time;
        ctx.fade = 0.7;
        ctx.sx = 0.5;
        pattern_name = std::string("fractal ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;
        ctx.fade = constrain(ctx.fade + 0.4 * ctx.bass / (ctx.bass_att+0.25));
    }

    // void per_point(PatternContext &ctx, PointContext &pt)
    // {
    //     // rad -0.5 : 0.5
    //     if (pt.rad < 0)
    //         pt.cx = 0;
    //     else
    //         pt.cx = 1.0;
    // };

    void update(PatternContext &ctx, Image &image)
    {
        // Image.stretch doesn't work for this
        Image cp(image);
        for (int i=0 ; i<IMAGE_SIZE/2 ; i++)
        {
            //Color a = cp.getRGB(2*i), b = cp.getRGB(2*i+1);
            //Color c = cp.getRGB(2*i+0.5f);
            Color color1 = cp.getRGB(2*i);
            Color color2 = cp.getRGB(2*i+1);
            Color c(
                MAX(color1.rgba.r, color2.rgba.r),
                MAX(color1.rgba.g, color2.rgba.g),
                MAX(color1.rgba.b, color2.rgba.b)
                , 0.2
            );
            image.setRGBA(i, c);
            image.setRGBA(i+IMAGE_SIZE/2, c);
        }
        image.decay(ctx.fade);
    }

    void draw(PatternContext &ctx, Image &image)
    {
        Color c;
        if (option)
            c = Color(1.0f, 1.0f, 1.0f);
        else
        {
            c = color->next(ctime);
    	    c.saturate(1.0);
        }
        float posx = x.next(ptime);
        image.setRGB((int)floor(posx), 0,0,0);
        image.setRGB((int)ceil(posx), 0,0,0);
        image.addRGB(posx, c);
    }

    void effects(PatternContext &ctx, Image &image)
    {
    }
};


class Fractal2 : public AbstractPattern
{
    ColorGenerator *color;
    Spirograph x;
    float ptime, ctime;
    bool option, option_set;
    float stretch;

public:
    Fractal2() : AbstractPattern("fractal"), x()
    {
        option = randomBool();
        color = new ComboGenerator(
            new SimpleGenerator(0.2, 0.8, 5.5),
            new SimpleGenerator(0.2, 0.8, 6.5),
            new SimpleGenerator(0.2, 0.8, 7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE-1-6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a; x.b = b;
    }

    Fractal2(bool opt) : Fractal2()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ptime = ctime = ctx.time;
        ctx.fade = 0.95;
        ctx.dx_wrap = true;
        ctx.cx = 0.5;
        ctx.ob_size = ctx.ib_size = option;
        ctx.ob_left = ctx.ob_right = BLACK;
        ctx.ib_left = ctx.ib_right = BLACK;
        
        pattern_name = std::string("fractal2 ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        ptime += ctx.dtime * ctx.bass_att;
        ctime += ctx.dtime;
        if (ctx.treb > 2.5 * ctx.treb_att)
            ctime += 21;

        Color c = color->next(ctime);
        c.saturate(1.0);

        float vol_att = (ctx.bass_att+ctx.mid_att+ctx.treb_att) / 3.0;
        if (option)
        {
            stretch = 2.0;
            if (ctx.time < ctx.lastbeat + 0.2)
                stretch = 1.5 + 0.5*MIN(1.0,ctx.bass/ctx.bass_att);
            ctx.cx = 0.5;
            ctx.fade = 0.85;
        }
        else
        {
            stretch = 2.0;
            ctx.cx = 0.0;
            ctx.fade = constrain(0.6 + 0.4 * ctx.bass / (ctx.bass_att+0.25));
            c = c * constrain(0.7 + 0.3 * ctx.bass);
        }

        c.constrain2();
        ctx.ob_left = ctx.ob_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        // mimic stretch/wrap using dx and dx_wrap
        //float dist = pt.rad - (ctx.cx-0.5);
        //pt.dx = -(dist * (stretch-1));
        //return;
        if (option)
        {
            pt.dx = -(pt.rad * (stretch-1));
        }
        else
        {
            float src = (pt.rad+0.5) * 2;
            pt.dx = pt.rad - src;
        }
    };

    void draw(PatternContext &ctx, Image &image)
    {
        if (option)
        {
            // draw borders
            AbstractPattern::draw(ctx, image);
            return;
        }
        float posx = x.next(ptime);
        int pos = (int)round(posx);
        // image.setRGB(constrain(pos-1,0,IMAGE_SIZE-1), BLACK);
        // image.setRGB(constrain(pos+1,0,IMAGE_SIZE-1), BLACK);
        image.setRGB(constrain(pos,0,IMAGE_SIZE-1), ctx.ob_left);

        // image.setRGB((int)floor(posx), BLACK);
        // image.setRGB((int)ceil(posx), BLACK);
        // image.addRGB(posx, ctx.ob_left);
    }

    void effects(PatternContext &ctx, Image &image)
    {
    }
};




class Diffusion : public AbstractPattern
{
public:
    ColorGenerator *color;
    float scale;
    float amplitude;

    bool option;
    bool option_set;

    Diffusion() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        );
    }

    Diffusion(bool _option) : Diffusion()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        pattern_name = std::string("diffusion ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        scale = (1.2 + 2*ctx.bass_att) * 2*M_PI;  // waviness
        if (option)
            amplitude = 0.3 * ctx.treb_att * sin(ctx.time*1.5); // speed and dir
        else
            amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float x = scale * pt.rad;
        float wave_time = 0.0;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) + wave_time*ctx.time) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image)
    {
        float bass = constrain(MAX(ctx.bass,ctx.bass_att)-1.3);
        float mid = constrain(MAX(ctx.mid,ctx.mid_att)-1.3);
        float treb = constrain(MAX(ctx.treb,ctx.treb_att)-1.3);
        Color c = color->next( ctx.time * 2 );
        Color c1, c2;

        c1 = c + Color(mid,bass,treb) * 0.4;
        c2 = c1;
        if (option)
            c2 = c.complement() + Color(mid,bass,treb) * 0.4;

        image.setRGB(IMAGE_SIZE/2-1, c1);
        image.setRGB(IMAGE_SIZE/2, c2);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (!option)
            image.rotate(0.5);
    }
};


class Diffusion2 : public AbstractPattern
{
public:
    ColorGenerator *color;
    float scale;
    float amplitude;

    bool option;
    bool option_set;

    Diffusion2() : AbstractPattern("diffusion"), option_set(0)
    {
        option = randomBool();
        color = new ComboGenerator(
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
            new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        );
    }

    Diffusion2(bool _option) : Diffusion2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
        pattern_name = std::string("diffusion2 ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        scale = (1.2 + 2*ctx.bass_att) * 2*M_PI;  // waviness
        amplitude = 0.3 * ctx.treb_att;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float x = scale * pt.rad;
        float wave_time = 0.0;
        float modifier = 1.0 + (ctx.bass-ctx.bass_att)*0.2;
//        pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
        pt.sx = pt.sx + sin(fabs(x) * modifier) * amplitude;
        // pt.sx = pt.sx + sin(x + wave_time*ctx.time) * amplitude;
    }

    void draw(PatternContext &ctx, Image &image)
    {
        float bass = constrain(MAX(ctx.bass,ctx.bass_att)-1.3);
        float mid = constrain(MAX(ctx.mid,ctx.mid_att)-1.3);
        float treb = constrain(MAX(ctx.treb,ctx.treb_att)-1.3);
        Color c = color->next( ctx.time * 2 );
        Color c1, c2;

        c1 = c + Color(mid,bass,treb) * 0.4;
        c2 = c1;
        if (false && option)
            c2 = c.complement() + Color(mid,bass,treb) * 0.4;

        image.setRGB(IMAGE_SIZE/2-1, c1);
        image.setRGB(IMAGE_SIZE/2, c2);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (!option)
            image.rotate(0.5);
    }
};



class Equalizer : public AbstractPattern
{
    bool option, option_set;

    int posT, posB;
    Color c1, c2, cmix;
public:

    Equalizer() : AbstractPattern("equalizer")
    {
        option = randomBool();
    }

    Equalizer(bool opt) : AbstractPattern("equalizer")
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        int palette_index = -1;

        ctx.blur = true;
        ctx.fade = 0.85;
        ctx.sx = 0.90;

        if (!option_set)
            option = !option;

        if (!option)
        {
            c1 = Color(1.0f, 0.0f, 0.0f); // red
            c2 = Color(0.0f, 1.0f, 0.0f); // green
            cmix = Color(1.0, 1.0, 0.0, 0.5);
            //cmix = WHITE;
        }
        else
        {
            int index = randomInt(paletteCount);
            fprintf(stderr, "palette %d\n", index);
            PaletteGenerator *p = palettes[index];
            // c1 = Color(0.0f, 0.0f, 1.0f);    /Color/ blue
            // c2 = Color(1.0f, 1.0f, 0.0f);    // yellow
            // cmix = Color(0.0f, 1.0f, 0.0f);  // green
            c1 = p->get(0);
            c1.saturate(1.0);
            c2 = p->get(1);
            c2.saturate(1.0);
            cmix = p->get(2);
            cmix.saturate(1.0);
            cmix.rgba.a = 0.5;
        }
        pattern_name = std::string("equalizer ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        posB = MAX(ctx.bass,ctx.bass_att) * (IMAGE_SCALE/2);
        posB = constrain(posB, 0, IMAGE_SIZE-1);

        posT = MAX(ctx.treb,ctx.treb_att) * (IMAGE_SCALE/2);
        posT = constrain(posT, 0, IMAGE_SIZE-1);
        posT = IMAGE_SIZE-1 - posT;

        ctx.cx = ((posB + posT) / 2.0) / (IMAGE_SIZE-1);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        if (posB <= posT)
            pt.sx = 1.1;
        else 
            pt.sx = 0.9;
        // if (pt.pos <= posB && posB <= posT)
        // {
        //     pt.cx = 0.0;
        // }
        // else if (pt.pos >= posT && posT >= posB)
        // {
        //     pt.cx = 1.0;
        // }
        // else if (posB > posT)
        // {
        //     pt.sx = 
        // }
    }


    // void update(PatternContext &ctx, Image &image)
    // {
    //     image.blur();
    //     image.fade(ctx.fade, ctx.fade_to);
    // }


    void draw2(PatternContext &ctx, Image &image)
    {
        Image draw;
        // for (int i=0 ; i<posB ; i++)
        //     draw.addRGB(i, green * 0.1);
        draw.addRGB(posB, c1);

        // for (int i=posT+1 ; i<IMAGE_SIZE ; i++)
        //     draw.addRGB(i, red);
        draw.addRGB(posT, c2);

        for (int i=posT+1 ; i<posB ; i++)
            draw.addRGB(i, cmix);

        for (int i=0 ; i<IMAGE_SIZE ; i++)
            image.addRGB(i, draw.getRGB(i));
    }
  
    void draw(PatternContext &ctx, Image &image)
    {
        image.setRGB(posB, c1);
        image.setRGB(posT, c2);

        for (int i=posT+1 ; i<posB ; i++)
            image.addRGB(i, cmix);
    }
};


class EKG : public AbstractPattern
{
    Color colorLast;
    int posLast;
    float speed;
    bool option;
    bool option_set;
    float vol_att;

public:

    EKG() : AbstractPattern("EKG"), colorLast(), option_set(0), vol_att(0)
    {
        option = randomBool();
    }

    EKG(bool _option) :  EKG()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = !option;
        ctx.fade = 0.99;
        posLast = 0;
        speed = 1.0/4.0;
        ctx.sx = 1.0;
        ctx.dx_wrap = true;
        pattern_name = std::string("ekg ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        if (option)
            // ctx.dx = 0.1*(ctx.treb-0.5);
            ctx.dx = 0.1 * (ctx.vol - vol_att);
        else
            ctx.dx = 0.08 * (ctx.treb - ctx.bass);

        vol_att = IN(vol_att, ctx.vol, 0.2);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

    void draw(PatternContext &ctx, Image &image)
    {
        int pos = ((int)round(ctx.time * speed * IMAGE_SIZE) % IMAGE_SIZE);
        Color black;
        Color white(1.0f,1.0f,1.0f);

        float bass = ctx.bass-ctx.bass_att;
        float mid = ctx.mid-ctx.mid_att;
        float treb = ctx.treb-ctx.treb_att;
        Color c;
        // c = white * 0.2 + white * 0.5 * ctx.bass; // Color(mid,bass,treb) * 0.4;
        // c = white * 0.2 + Color(bass,mid,treb) * 0.4;
        c = Color(MAX(0.2,bass),MAX(0.2,mid),MAX(0.2,treb));
        c.constrain2();
        // c = white * 0.3 + c * 0.7;
        //c.saturate(1.0);
        if (posLast == pos)
        {
            c = Color(
                MAX(c.rgba.r,colorLast.rgba.r),
                MAX(c.rgba.g,colorLast.rgba.g),
                MAX(c.rgba.b,colorLast.rgba.b)
            );
        }
        image.setRGB((pos+1)%IMAGE_SIZE, black);
        image.setRGB(pos, c);

        colorLast = c;
        posLast = pos;
    }
};


class EKG2 : public AbstractPattern
{
    Color colorLast;
    float pos;
    bool option;
    bool option_set;
    float vol_att;
    PaletteGenerator *palette;
    int beatCount = 0;

public:

    EKG2() : AbstractPattern("EKG2"), colorLast(), option_set(0), vol_att(0)
    {
        option = randomBool();
        //  color = new ComboGenerator(
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.517),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.088),
        //     new SimpleGenerator(0.1, 0.7, 2*M_PI/1.037)
        // );
    }

    EKG2(bool _option) :  EKG2()
    {
        option = _option;
        option_set = 1;
    }

    void setup(PatternContext &ctx)
    {
        int p = randomInt(paletteCount);
        palette = palettes[p];
        if (!option_set)
            option = !option;
        ctx.fade = 0.94;
        ctx.sx = 1.0;
        ctx.dx_wrap = false;
        ctx.ob_size = 1;
        ctx.ob_left = BLACK;
        char tmp[100];
        sprintf(tmp, "ekg2 %s (%d)", option?"true":"false", p);
        pattern_name = std::string(tmp);
    }

    void per_frame(PatternContext &ctx)
    {
        vol_att = IN(vol_att, ctx.vol, 0.2);
        beatCount += (ctx.beat ? 1 : 0);
        pos += 0.002 * (1.0 + ctx.treb);

        ctx.dx = -0.005; // * (ctx.treb - ctx.bass);
        //ctx.fade = 0.95;
        if (ctx.time < ctx.lastbeat+0.3)
        {
            ctx.dx -= 0.03 * ctx.bass;
            //ctx.fade = 1.0;
        }

        Color c;
        if (option)
        {
            c = WHITE;
            float v = MAX(vol_att, ctx.vol);
            float s = MIN(1.0, 0.55 + 0.3 * v);
            c.saturate(s);
        }
        {
//          c = color->next((float)ctx.time);
            c = palette->get((int)beatCount);
            float v = MAX(vol_att, ctx.vol);
            float s = MIN(1.0, 0.55 + 0.3 * v);
            c.saturate(s);
        }
        ctx.ob_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
    }

    // void update(PatternContext &ctx, Image &image)
    // {
    // }

/*    void draw(PatternContext &ctx, Image &image)
    {
        Color c(1.0f,1.0f,1.0f);
        if (!option)
        {
//             c = color->next((float)ctx.time);
            c = palette->get((int)beatCount);
        }
        float v = MAX(vol_att, ctx.vol);
        c.saturate(MIN(1.0,0.4 + v/2.0));

        int p = ((int)round(pos)) % IMAGE_SIZE;
        image.setRGB(p, c);
        image.setRGB((p+1)%IMAGE_SIZE, Color());
    }
    */

    void effects(PatternContext &ctx, Image &image)
    {
        float p = fmodf(-pos, 1.0);
        image.rotate(p);
    }
};



class Pebbles : public AbstractPattern
{
    bool option, option_set;
    float vol_mean;
    float last_cx;
    Generator *r;
    Generator *g;
    PaletteGenerator *palette;
    int beatCount = 0;
    unsigned palette_color = 0;
    
// per_frame_1=wave_r = 0.5 + 0.5*sin(1.6*time);
// per_frame_2=wave_g = 0.5 + 0.5*sin(4.1*time);
// per_frame_3=wave_b = -1 + (1-wave_r + 1-wave_g);

public:

    Pebbles() : AbstractPattern("Pebbles"), vol_mean(0.1), last_cx(0.5),
        r(new SimpleGenerator(0.0,1.0,1.6/3)),
        g(new SimpleGenerator(0.0,1.0,4.1/3))
    {
        option = randomBool();
    }

    Pebbles(bool opt) : Pebbles()
    {
        option = opt;
        option_set = true;
    }

    void setup(PatternContext &ctx)
    {
        palette = palettes[randomInt(paletteCount)];
        if (!option_set)
            option = !option;
        ctx.fade = 0.97;
        ctx.blur = !option;
        ctx.fade_to = option ? Color() : Color(1.0f,1.0f,1.0f);
        ctx.dx_wrap = true;
        pattern_name = std::string("pebbles ") + (option?"true":"false");
    }

    void per_frame(PatternContext &ctx)
    {
        vol_mean = IN(vol_mean, ctx.vol, 0.05);
        vol_mean = constrain(vol_mean, 0.1f, 10000.0f);
        if (ctx.beat)
        {
            beatCount++;
            palette_color = randomInt(RAND_MAX);
            last_cx = randomFloat();
        }
        ctx.cx = last_cx;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        float dist = pt.rad - (ctx.cx-0.5);
        float radius = 0.1 + f/2.0;

        pt.sx = 1.0;
        if (option || fabs(dist) < radius)
        {
            float speed = ctx.vol / 40;
            if (dist < 0)
                pt.dx = -1 * speed;
            else
                pt.dx = speed;
        }
    };

    void draw(PatternContext &ctx, Image &image)
    {
        float f = (ctx.time - ctx.lastbeat) / ctx.interval;
        if (f < 0.3)
        {
            Color c;
            if (0==1)
            {
                float shape_r = r->next(ctx.time);
                float shape_g = g->next(ctx.time);
                float shape_b = -1 + (1-shape_r + 1-shape_g);
                c = Color(shape_r,shape_g,shape_b);
                c = c * (1.0-f);
            }
            else
            {
                c = palette->get( palette_color );
                //c = Color(0.1f, 0.1f, 0.1f);
            }

            float pos = IN(LEFTMOST_PIXEL,RIGHTMOST_PIXEL,ctx.cx);
            image.setRGB(pos, c);
        }
    }
};





class BigWhiteLight : public AbstractPattern
{
    float vol_att = 1.0;
    bool option;
public:

    BigWhiteLight() : AbstractPattern("big white")
    {
    }

    void setup(PatternContext &ctx)
    {
        option = !option;
        ctx.cx = 0.5;
        ctx.sx = 0.8;
        ctx.blur = true;
        ctx.fade = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.ob_left = ctx.ob_right = WHITE;
        ctx.ib_left = ctx.ib_right = WHITE;
    }

    void per_frame(PatternContext &ctx)
    {
        vol_att = IN(vol_att, ctx.vol, 0.3);
        float v = vol_att / 2;
        // float t = constrain(ctx.treb*0.8 - ctx.bass);
        // float b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
        float vol = (ctx.bass*2 + ctx.mid + ctx.treb*2) / 5.0;
        float t = constrain( ctx.treb / ctx.vol - 2.1);
        float b = constrain( ctx.bass / ctx.vol - 2.1);
        Color c = Color(1.0f-b, 1.0f-t-b, 1.0f-t);
        c.constrain();
        float s = MIN(1.0, 0.3 + v);
        if (!ctx.beat)
            c = c * s;
        ctx.ib_size = ctx.beat;
        ctx.ob_left = ctx.ob_right = ctx.ib_left = ctx.ib_right = c;
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (option)
            image.rotate(0.5);
    }
};


class BigWhiteLight2 : public AbstractPattern
{
    float start_time = 0.0;
    float prev_time = 0.0;
    float vol_att = 1.0;
    float vol_level = 1.0;
    bool dir = 0;
    bool option=0;
public:

    BigWhiteLight2() : AbstractPattern("big white2")
    {
    }

    void setup(PatternContext &ctx)
    {
        option = !option;
        start_time = ctx.time;
        ctx.dx = 0.0;
        ctx.cx = 0.5;
        ctx.sx = 1.0;
        ctx.blur = true;
        ctx.fade = 0.9;
        ctx.ob_size = 1;
        ctx.ib_size = 0;
        ctx.ob_left = ctx.ob_right = WHITE;
        ctx.ib_left = ctx.ib_right = WHITE;
    }

    void per_frame(PatternContext &ctx)
    {
        // if ((int)((prev_time-start_time)/20.0) != (int)((ctx.time-start_time)/20.0))
        //     dir = !dir;
        //prev_time = ctx.time;
        vol_att = IN(vol_att, ctx.vol, 0.3);
        vol_level = IN(vol_level, ctx.vol, 0.05);

        if (ctx.time - prev_time > 15.0 &&ctx.beat)
        {
            dir = !dir;
            prev_time = ctx.time;
        }

        float v = vol_att / 2;
        // float t = constrain(ctx.treb*0.8 - ctx.bass);
        // float b = constrain(ctx.bass - ctx.treb*0.8);
        // t = constrain(t / ctx.vol - 1.5);
        // b = constrain(b / ctx.vol - 1.5);
        float vol = (ctx.bass*2 + ctx.mid + ctx.treb*2) / 5.0;
        float t = constrain( ctx.treb / ctx.vol - 2.1);
        float b = constrain( ctx.bass / ctx.vol - 2.1);
        Color c = Color(1.0f-b, 1.0f-t-b, 1.0f-t);
        c.constrain();
        float s = MIN(1.0, 0.3 + v);
        if (!ctx.beat)
            c = c * s;
        ctx.ib_size = ctx.beat;
        ctx.ob_left = ctx.ob_right = ctx.ib_left = ctx.ib_right = c;
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        pt.cx = option ? 0.5 : pt.rad < 0.0 ? 1.0 : 0.0;
        float v = cos(M_PI*pt.rad);  // 0->1->0
        //pt.sx = 0.9 + v - vol_att/10.0;
        pt.sx = 1.04 - v*0.2 - ((vol_att/vol_level)-1)*0.1;
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (dir)
            image.rotate(0.5);
    }
};



class SwayBeat : public AbstractPattern
{
    PaletteGenerator *palette;
    unsigned beatCount=0;
    bool dir = 0;
    bool option=0;
    Color left=WHITE, right=WHITE;
    float mytime=0;

    Color colors[3] = {Color(0.0f,0,0,0), Color(0.0f,0,0,0), WHITE};
    int positions[3] = {0, IMAGE_SIZE-1, IMAGE_SIZE/2};

public:

    SwayBeat() : AbstractPattern("sway beat")
    {
    }

    void setup(PatternContext &ctx)
    {
        palette = palettes[randomInt(paletteCount)];
        option = !option;
        ctx.dx = 0.0;
        ctx.cx = 0.5;
        ctx.sx = 1.0;
        ctx.blur = true;
        ctx.fade = 0.88;
        ctx.ob_size = 0;
        ctx.ib_size = 0;
    }

    void per_frame(PatternContext &ctx)
    {
        mytime += ctx.dtime;
        colors[0].rgba.a *= 0.95;
        colors[1].rgba.a *= 0.95;
        //colors[2].rgba.a *= 0.95;

        if (ctx.beat)
        {
            beatCount++;
            if (randomBool() && colors[0].rgba.a < 0.3)
            {
                Color c;
                do {
                    unsigned p = randomInt(RAND_MAX);
                    c =palette->get(p);
                } while (c.rgba.r + c.rgba.g + c.rgba.b < 1.0);
                colors[0] = colors[1];
                positions[0] = positions[1];
                colors[1] = colors[2];
                positions[1] = positions[2];
                colors[2] = c;
                do {
                    positions[2] = randomInt(IMAGE_SIZE);
                } while (positions[2]==positions[1] || positions[2] == positions[0]);
                // if (randomBool()) // (beatCount % 2)
                //     left = c;
                // else
                //     right = c;
            }
        }
        //float t = (beatCount%8) + (ctx.time-ctx.lastbeat)/ctx.interval;
        //float s = 2*M_PI*t/8.0;
        ctx.dx = sin(ctx.time*0.6) * 0.05;
        //ctx.dx *= 1.0 + constrain((ctx.treb/ctx.treb_att)-1.0) * 0.2;        
    }

/*
    void update(PatternContext ctx, Image &image)
    {
        Image cp(image);
        for (int i=0 ; i<IMAGE_SIZE ; i++)
            image.setRGB(i,image.getRGB(IMAGE_SIZE-1-i));
        AbstractPattern::update(ctx,image);
    }
*/

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float t = (ctx.time-ctx.lastbeat)/ctx.interval;
        float s = 2*M_PI*t;
        pt.dx += sin(s + 3*pt.rad) * 0.02;

        // change dx to sx
        // float dx = pt.dx;
        // pt.dx = 0;
        // pt.sx = (1.0 + dx*(pt.rad<0?-1.0:1.0));
    }

    void draw(PatternContext &ctx, Image &image)
    {
        //image.setRGB((0)+5, left);
        //image.setRGB((IMAGE_SIZE-1)-5, right);
        image.addRGB(positions[0], colors[0]);
        image.addRGB(positions[1], colors[1]);
        image.addRGB(positions[2], colors[2]);
    }


    void effects(PatternContext &ctx, Image &image)
    {
    }
};




void loadAllPatterns()
{
    patterns.push_back(new Waterfall());
    patterns.push_back(new GreenFlash());
    patterns.push_back(new Fractal2(true));
    patterns.push_back(new Fractal(false));
    patterns.push_back(new Diffusion2());
    patterns.push_back(new Equalizer());
    patterns.push_back(new EKG2());
    patterns.push_back(new Pebbles());
    patterns.push_back(new BigWhiteLight2());
    patterns.push_back(new SwayBeat());
}

void loadPatterns()
{
    loadAllPatterns();
    //patterns.push_back(new SwayBeat());
    //patterns.push_back(new BigWhiteLight2());
    //patterns.push_back(new Fractal2());
    //patterns.push_back(new Pebbles());
    //patterns.push_back(new Waterfall(false));
    //patterns.push_back(new Pebbles(true));
    //patterns.push_back(new GreenFlash());
    //patterns.push_back(new EKG2());
    //patterns.push_back(new Equalizer(true));
}
