//
//  pmSDL.cpp
//  SDLprojectM
//
//  Created by Mischa Spiegelmock on 2017-09-18.
//

#include "pmSDL.hpp"
#include "Renderer/BeatDetect.hpp"
#include "TimeKeeper.hpp"


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
    int selected = 1;
    for (int i = 0; i < count; i++)
    {
//        if (0==strcmp("Monitor of Built-in Audio Analog Stereo",SDL_GetAudioDeviceName(i, true)))
//            selected = i;
        printf("  %i: 🎤%s\n", i, SDL_GetAudioDeviceName(i, true));
    }

    while (1)
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
    int count = SDL_GetNumAudioDevices(false);  // capture, please
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

    audioDeviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(selectedAudioDevice, true), true, &want, &have, 0);

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

void projectMSDL::pollEvent() {
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

void projectMSDL::init(SDL_Window *window, SDL_Renderer *renderer) {
    win = window;
    rend = renderer;
    // selectRandom(true);
    // projectM_resetGL(width, height);
}


std::string projectMSDL::getActivePresetName()
{
    return std::string("hey");
}


#define IMAGE_SIZE 24
#define IMAGE_SCALE ((float)IMAGE_SIZE)
#define OB_LEFT 0
#define OB_RIGHT (IMAGE_SIZE-1)
#define IB_LEFT 1
#define IB_RIGHT (IMAGE_SIZE-2)
#define LEFTMOST_PIXEL 0
#define RIGHTMOST_PIXEL (IMAGE_SIZE-1)
#define RED 0
#define BLUE 1
#define GREEN 2


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
inline float constrain(float x)
{
    return constrain(x, 0.0, 1.0);
}

void saturate(float *rgb, float i=1.0)
{
    float m = MAX(rgb[0], MAX(rgb[1],rgb[2]));
    if (m <= 0)
    {
        rgb[0] = rgb[1] = rgb[2] = i;
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
    c.rgba.r += a.rgba.r;
    c.rgba.g += a.rgba.g;
    c.rgba.b += a.rgba.b;
    c.rgba.a += a.rgba.a;
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
    Generator *r, *g, *b;
public:
     ColorGenerator(Generator *_r, Generator *_g, Generator *_b) :
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



class PointContext
{
public:
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
    PatternContext() : decay(1.0), cx(0.5), sx(1.0), dx(0.0), saturate(0), gamma(2.8),
        ob_size(0), ib_size(0),
        time(0), frame(0)
    {
    }
    PatternContext(PatternContext &from)
    {
        memcpy(this,&from,sizeof(from));
    }
    float decay;
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

    PointContext points[IMAGE_SIZE];
};

#if 0
class Image
{
public:
    float rgb[3][IMAGE_SIZE];

    Image()
    {
        memset(rgb, 0, sizeof(rgb));
    }
    Image(Image &from)
    {
        memcpy(rgb, from.rgb, sizeof(rgb));
    }
    void copyFrom(Image &from)
    {
        memcpy(rgb, from.rgb, sizeof(rgb));
    }

    float getValue(unsigned color, float f)
    {
        int i=(int)floor(f);
        f = f - i;
        if (i <= 0)
            return rgb[color][0];
        if (i+1 >= IMAGE_SIZE-1)
            return rgb[color][IMAGE_SIZE-1];
        return IN(rgb[color][i], rgb[color][i+1], f);
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
        return IN(rgb[color][i], rgb[color][j], f);
    }

    void getRGB(float f, float *rgb)
    {
        rgb[0] = getValue(RED, f);
        rgb[1] = getValue(GREEN, f);
        rgb[2] = getValue(BLUE, f);
    }

    Color getRGB(float f)
    {
        Color c(getValue(RED, f), getValue(GREEN, f), getValue(BLUE, f));
        return c;
    }

    Color getRGB(int i)
    {
        Color c(rgb[RED][i], rgb[GREEN][i], rgb[BLUE][i]);
        return c;
    }

    void getRGB_wrap(float f, float *rgb)
    {
        rgb[0] = getValue_wrap(RED, f);
        rgb[1] = getValue_wrap(GREEN, f);
        rgb[2] = getValue_wrap(BLUE, f);
    }

    void setRGB(int i, float r, float g, float b)
    {
        rgb[RED][i]   = constrain(r);
        rgb[GREEN][i] = constrain(g);
        rgb[BLUE][i]  = constrain(b);
    }

    void setRGB(int i, Color &c)
    {
        rgb[RED][i]   = constrain(c.rgba.r);
        rgb[GREEN][i] = constrain(c.rgba.g);
        rgb[BLUE][i]  = constrain(c.rgba.b);
    }

    void setRGBA(int i, float r, float g, float b, float a)
    {
        rgb[RED][i]   = IN(rgb[RED][i], r, a);
        rgb[GREEN][i] = IN(rgb[GREEN][i], g, a);
        rgb[BLUE][i]  = IN(rgb[BLUE][i], b, a);
    }

    void setRGBA(int i, Color &c)
    {
        rgb[RED][i]   = IN(rgb[RED][i],   c.rgba.r, c.rgba.a);
        rgb[GREEN][i] = IN(rgb[GREEN][i], c.rgba.g, c.rgba.a);
        rgb[BLUE][i]  = IN(rgb[BLUE][i],  c.rgba.b, c.rgba.a);
    }

    void stretch(float sx, float cx)
    {
        Image cp(*this);
        float center = IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, cx);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = (i-center)/sx + center;
            rgb[RED][i]   = cp.getValue( RED,   from );
            rgb[GREEN][i] = cp.getValue( GREEN, from );
            rgb[BLUE][i]  = cp.getValue( BLUE,  from );
        }
    }

    void stretch(PatternContext &ctx)
    {
        Image cp(*this);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float center = IN(LEFTMOST_PIXEL, RIGHTMOST_PIXEL, ctx.points[i].cx);
            float from = (i-center)/ctx.points[i].sx + center;
            rgb[RED][i]   = cp.getValue( RED,   from );
            rgb[GREEN][i] = cp.getValue( GREEN, from );
            rgb[BLUE][i]  = cp.getValue( BLUE,  from );
        }
    }

    void move(float dx)
    {
        Image cp(*this);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SIZE;
            rgb[RED][i]   = cp.getValue( RED,   from );
            rgb[GREEN][i] = cp.getValue( GREEN, from );
            rgb[BLUE][i]  = cp.getValue( BLUE,  from );
        }
    }

    void rotate(float dx)
    {
        Image cp(*this);
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            float from = i + dx*IMAGE_SIZE;
            rgb[RED][i]   = cp.getValue_wrap( RED,   from );
            rgb[GREEN][i] = cp.getValue_wrap( GREEN, from );
            rgb[BLUE][i]  = cp.getValue_wrap( BLUE,  from );
        }
    }

    void decay(float d)
    {
        for (int i=LEFTMOST_PIXEL ; i<=RIGHTMOST_PIXEL ; i++)
        {
            rgb[RED][i]   *= d;
            rgb[GREEN][i] *= d;
            rgb[BLUE][i]  *= d;
        }
    }
};

#else

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
        rgb[0] = getValue(RED, f);
        rgb[1] = getValue(GREEN, f);
        rgb[2] = getValue(BLUE, f);
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
        rgb[0] = getValue_wrap(RED, f);
        rgb[1] = getValue_wrap(GREEN, f);
        rgb[2] = getValue_wrap(BLUE, f);
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
};
#endif

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


FILE *device = stdout;

void drawPiano(PatternContext &ctx, Image &image)
{
    if (device == 0)
    {
        device = fopen("/dev/ttyUSB0", "a");
    }
    const bool DMX = 1;
    const bool ONECHAR = 0;
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
        Color c = image.getRGB(i);
        int r = (int)(pow(constrain(c.rgba.r),ctx.gamma) * 255);
        int g = (int)(pow(constrain(c.rgba.g),ctx.gamma) * 255);
        int b = (int)(pow(constrain(c.rgba.b),ctx.gamma) * 255);
        if (DMX)
            fprintf(device, "%d,%d,%d,", r, g, b);
        else if (ONECHAR)
            fprintf(device, "%01x%01x%01x", r>>4, g>>4, b>>4);
        else
            fprintf(device, "%02x%02x%02x", r, g, b);
    }
    fprintf(device, "\n");
    fflush(device);
}


PatternContext context;
Image work;
Image stash;

void loadPatterns();
std::vector<Pattern *> patterns;
Pattern *currentPattern = NULL;

void projectMSDL::renderFrame()
{
    float prev_time = timeKeeper->GetRunningTime();
    timeKeeper->UpdateTimers();
    beatDetect->detectFromSamples();

    if (patterns.size() == 0)
        loadPatterns();
    if (NULL == currentPattern ||
        timeKeeper->PresetProgressA() > 1.0 ||
        ((beatDetect->vol-beatDetect->vol_old>beatDetect->beat_sensitivity ) && timeKeeper->CanHardCut()))
    {
        if (patterns.size() == 0)
            return;
        Pattern *p = patterns.at(rand() % patterns.size());
        if (p == currentPattern && patterns.size() > 1)
            p = patterns.at(rand() % patterns.size());
        currentPattern = p;
        fprintf(stderr, "%s\n", currentPattern->name().c_str());
        currentPattern->setup(context);
        timeKeeper->StartPreset();
    }
    Pattern *pattern = currentPattern;

    PatternContext frame(context);
    frame.time = timeKeeper->GetRunningTime();
    frame.dtime = frame.time - prev_time;
    frame.frame = timeKeeper->PresetProgressA();
    frame.bass = beatDetect->bass;
    frame.mid = beatDetect->mid;
    frame.treb = beatDetect->treb;
    frame.bass_att = beatDetect->bass_att;
    frame.mid_att = beatDetect->mid_att;
    frame.treb_att = beatDetect->treb_att;
    frame.vol = beatDetect->vol; // (beatDetect->bass + beatDetect->mid + beatDetect->treb) / 3.0;
    //frame.vol_att = beatDetect->vol_attr; // (beatDetect->bass_att + beatDetect->mid_att + beatDetect->treb_att) / 3.0;

    pattern->per_frame(frame);
    for (int i=0 ; i<IMAGE_SIZE ; i++)
    {
        PointContext &pt = frame.points[i];
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
    drawPiano(frame,work);
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
        image.decay(ctx.decay);
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
    bool option;
public:
    Waterfall() : AbstractPattern((const char *)"waterfall"),
        cx(0.1, 0.9, 9)
    {
        lb_color = new ColorGenerator(
            new SimpleGenerator(0.2, 0.8, 5.5),
            new SimpleGenerator(0.2, 0.8, 6.5),
            new SimpleGenerator(0.2, 0.8, 7.5)
        );
        rb_color = new ColorGenerator(
            new SimpleGenerator(0.2, 0.8, 8),
            new SimpleGenerator(0.2, 0.8, 7),
            new SimpleGenerator(0.2, 0.8, 6)
        );
    }

    void setup(PatternContext &ctx)
    {
        ctx.sx = 0.9;
        ctx.ob_size = 1;
        option = random() % 2;
        ctx.ib_size = option;       
    }

    void per_frame(PatternContext &ctx)
    {
        ctx.ob_left = lb_color->next( ctx.time ); //Color( ctx.time, lb_r, lb_g, lb_b );
        ctx.ob_left *= 0.5 + ctx.treb / 2.0;
        ctx.ib_left = ctx.ob_left;
        ctx.ib_left.complement();

        ctx.ob_right = rb_color->next( ctx.time ); //Color( ctx.time, rb_r, rb_g, rb_b );
        ctx.ob_right *= 0.5+(ctx.bass/2.0);
        ctx.ib_right = ctx.ob_right;
        ctx.ib_right.complement();

        ctx.cx   = cx.next(ctx.time);
    }
};


class GreenEnvy : public AbstractPattern
{
    bool rotate_effect;
public:
    GreenEnvy() : AbstractPattern((const char *)"green envy")
    {}

    void setup(PatternContext &ctx)
    {
        ctx.sx = 1.05;
        ctx.decay = 0.99;
        rotate_effect = random() % 2;
    }

    void per_frame(PatternContext &ctx)
    {}

    void draw(PatternContext &ctx, Image &image)
    {
        //Color c(ctx.treb+ctx.mid,ctx.mid,0); // yellow and red
        //Color c(ctx.treb,ctx.treb,ctx.mid); // yellow and blue
        Color c(ctx.treb,ctx.mid,ctx.treb); // green and purple
        c.constrain2();
        //Color c(ctx.treb,ctx.mid,ctx.bass);
        //int v = ctx.vol;// / MAX(1,ctx.vol_att);
        //int bass = ctx.bass; // MAX(ctx.bass_att, ctx.bass);
        int bass = MAX(ctx.bass_att, ctx.bass);
        int w = MIN(IMAGE_SIZE*2/3,(int)round(bass*3));
        image.setRGB(11-w,c);
        image.setRGB(12+w,c);
        for (int i=11-w+1 ; i<=12+w-1 ; i++)
            image.setRGB(i, 0, 0, 0);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (rotate_effect)
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
    float vtime;
    bool option;
public:
    Fractal() : AbstractPattern("fractal"), x()
    {
        color = new ColorGenerator(
            new SimpleGenerator(0.2, 0.8, 0.5*5.5),
            new SimpleGenerator(0.2, 0.8, 0.5*6.5),
            new SimpleGenerator(0.2, 0.8, 0.5*7.5)
        );

        SimpleGenerator a(6, IMAGE_SIZE-1-6, 13);
        SimpleGenerator b(-2, 2, 5);
        x.a = a; x.b = b;
    }

    void setup(PatternContext &ctx)
    {
        option = random() % 2;
        vtime = ctx.time;
        ctx.decay = 0.5;
        if (option)
            ctx.decay += 0.1;
        ctx.sx = 0.5;
    }

    void per_frame(PatternContext &ctx)
    {
        vtime += ctx.dtime * ctx.bass;
        ctx.decay = constrain(ctx.decay + 0.4 * ctx.bass / (ctx.bass_att+0.3));
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        // rad -0.5 : 0.5
        if (pt.rad < 0)
            pt.cx = 0;
        else
            pt.cx = 1.0;
    };

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
        image.decay(ctx.decay);
    }
    void draw(PatternContext &ctx, Image &image)
    {
        Color color1 = color->next(ctx.time);
        // Color hsl(fmod(ctx.time/3,1.0),1.0,0.5);
        // Color color2;
        // hsl2rgb(hsl.arr,color2.arr);

	    color1.saturate(1.0);
        //Color b(a);
        //float posx = x.next(ctx.time);
        float posx = x.next(vtime);
        //i = i - (i%2);
        image.setRGB((int)floor(posx), 0,0,0);
        image.setRGB((int)ceil(posx), 0,0,0);
        image.addRGB(posx, color1);
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
        color = new ColorGenerator(
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
            option = random() % 2;
        ctx.decay = 1.0;
        ctx.sx = 1.03;
        ctx.dx_wrap = 1;
    }

    void per_frame(PatternContext &ctx)
    {
        scale = (1.2 + 2*ctx.bass) * 2*M_PI;  // waviness
        if (option)
            amplitude = 0.2 * ctx.treb_att * sin(ctx.time*1.5); // speed and dir
        else
            amplitude = 0.2 * ctx.treb_att;
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
            c2.complement(); //c.complement() + Color(mid,bass,treb) * 0.4;

        // c1 = c;
        // c1.saturate(1.0);
        // c2 = c1;
        // if (option)
        //     c2 = c1.complement();

        image.setRGB(IMAGE_SIZE/2-1, c1);
        image.setRGB(IMAGE_SIZE/2, c2);
    }

    void effects(PatternContext &ctx, Image &image)
    {
        if (option)
            image.rotate(0.5);
    }
};


class Equalizer : public AbstractPattern
{
    bool option;

    int posT, posB;
    Color c1, c2, cmix;
public:

    Equalizer() : AbstractPattern("equalizer")
    {}

    void setup(PatternContext &ctx)
    {
        ctx.decay = 0.85;
        ctx.sx = 0.90;
        option = random() % 2;

        if (option)
        {
            c1 = Color(1,0,0);
            c2 = Color(0,1,0);
            cmix = Color(c1, c2, 0.5);
        }
        else
        {
            c1 = Color(0,0,1);
            c2 = Color(1,1,0);
            cmix = Color(0,1,0);
        }
    }

    void per_frame(PatternContext &ctx)
    {
        posB = MAX(ctx.bass,ctx.bass_att) * (IMAGE_SCALE/2);
        if (posB > IMAGE_SIZE-1)
            posB = IMAGE_SIZE-1;

        posT = MAX(ctx.treb,ctx.treb_att) * (IMAGE_SCALE/2);
        if (posT > IMAGE_SIZE-1)
            posT = IMAGE_SIZE-1;
        posT = IMAGE_SIZE-1 - posT;

        ctx.cx = ((posB + posT) / 2.0) / (IMAGE_SIZE-1);
    }

    void per_point(PatternContext &ctx, PointContext &pt)
    {
        float i = (pt.rad+0.5) * (IMAGE_SIZE-1);
        if (i < posB && i < posT)
            pt.cx = 0;
        else if (i > posB && i > posT)
            pt.cx = 1;
    }

    void update(PatternContext &ctx, Image &image)
    {
        Image cp(image);
        for (int i=0 ; i<IMAGE_SIZE ; i++)
        {
            Color prev = image.getRGB(i-1<0?0:i-1);
            Color color = image.getRGB(i);
            Color next = image.getRGB(i+1>IMAGE_SIZE-1?IMAGE_SIZE-1:i+1);
            Color blur = prev*0.25 + color*0.5 + next*0.25;
            image.setRGB(i,blur);
        }
        image.decay(ctx.decay);
    }


    void draw(PatternContext &ctx, Image &image)
    {
        Color white(0.5,0.5,0.5);

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
};


class EKG : public AbstractPattern
{
    Color colorLast;
    int posLast;
    float speed;
    bool option;
    bool option_set;

public:

    EKG() : AbstractPattern("EKG"), option_set(0)
    {}

    EKG(bool _option) :  AbstractPattern("EKG"), option(_option), option_set(1)
    {}

    void setup(PatternContext &ctx)
    {
        if (!option_set)
            option = random() % 2;
        ctx.decay = 0.99;
        posLast = 0;
        speed = 1.0/4.0;
        ctx.sx = 1.0;
        ctx.dx = -0.01;
        ctx.dx_wrap = true;
    }

    void per_frame(PatternContext &ctx)
    {
        ctx.dx -= 0.1*(ctx.treb-0.5);
        if (option)
            ctx.dx  = ctx.dx * -1;
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
        Color black(0,0,0);
        Color white(1.0,1.0,1.0);

        float bass = ctx.bass-ctx.bass_att;
        float mid = ctx.mid-ctx.mid_att;
        float treb = ctx.treb-ctx.treb_att;
        Color c;
        // c = white * 0.2 + white * 0.5 * ctx.bass; // Color(mid,bass,treb) * 0.4;
        // c = white * 0.2 + Color(bass,mid,treb) * 0.4;
        c = Color(bass,mid,treb);
        c.constrain2();
        c = white * 0.3 + c * 0.7;
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


void loadPatterns()
{
    patterns.push_back(new Waterfall());
    patterns.push_back(new GreenEnvy());
    patterns.push_back(new Fractal());
    patterns.push_back(new Diffusion());
    patterns.push_back(new Equalizer());
    patterns.push_back(new EKG());
}
