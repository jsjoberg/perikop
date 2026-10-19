/* Perikop's Bjorn copy-synthesis experiment. Original code, MIT (see /LICENSE).
 * C11 + libm only. No model runtime, audio library, or third-party DSP code.
 * This renders fitted utterance trajectories, not arbitrary text.
 */
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265358979323846
#define FFT_SIZE 2048
#define BANDS 16
#define MAX_SECONDS 600

static void fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}
static uint32_t read_le(FILE* file, int bytes) {
    uint32_t result = 0;
    for (int i = 0; i < bytes; ++i) {
        int c = fgetc(file);
        if (c == EOF)
            fail("Truncated trajectory file.");
        result |= (uint32_t)c << (8 * i);
    }
    return result;
}
static void put_le(FILE* file, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        if (fputc((int)((value >> (8 * i)) & 255), file) == EOF)
            fail("Cannot write WAV.");
}
static double amplitude(unsigned code) {
    return code ? pow(10.0, (-96.0 + (code - 1) * (96.0 / 254)) / 20) : 0;
}
/* Iterative inverse FFT. The forward transform is only needed by the fitter. */
static void inverse_fft(double complex* values) {
    for (unsigned i = 1, j = 0; i < FFT_SIZE; ++i) {
        unsigned bit = FFT_SIZE >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double complex swap = values[i];
            values[i] = values[j];
            values[j] = swap;
        }
    }
    for (unsigned length = 2; length <= FFT_SIZE; length <<= 1) {
        double complex step = cexp(I * 2 * PI / length);
        for (unsigned i = 0; i < FFT_SIZE; i += length) {
            double complex phase = 1;
            for (unsigned j = 0; j < length / 2; ++j) {
                double complex a = values[i + j], b = phase * values[i + j + length / 2];
                values[i + j] = a + b;
                values[i + j + length / 2] = a - b;
                phase *= step;
            }
        }
    }
    for (unsigned i = 0; i < FFT_SIZE; ++i)
        values[i] /= FFT_SIZE;
}
static uint32_t random_state = 0x70657269;
static double uniform(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (random_state + 0.5) / 4294967296.0;
}
typedef struct {
    FILE* file;
    unsigned cursor, clipped;
    double peak;
    double voiced[960], noise[960], weights[960], noise_weights[960];
} Output;
static void flush_until(Output* output, unsigned end) {
    for (; output->cursor < end; ++output->cursor) {
        unsigned i = output->cursor % 960;
        double value = output->voiced[i] / fmax(output->weights[i], 1e-9) +
                       output->noise[i] / sqrt(fmax(output->noise_weights[i], 1e-9));
        if (!isfinite(value))
            fail("Non-finite synthesis output.");
        output->peak = fmax(output->peak, fabs(value));
        if (fabs(value) > 1)
            ++output->clipped;
        value = fmin(1, fmax(-1, value));
        put_le(output->file, (uint16_t)(int16_t)lrint(value * 32767), 2);
        output->voiced[i] = output->noise[i] = output->weights[i] = output->noise_weights[i] = 0;
    }
}
int main(int argc, char** argv) {
    if (argc != 3)
        fail("Usage: bjorn-render INPUT.bjp OUTPUT.wav");
    FILE* input = fopen(argv[1], "rb");
    if (!input)
        fail("Cannot open trajectory file.");
    if (read_le(input, 4) != 0x32504a42) /* BJP2 */
        fail("Unsupported trajectory format.");
    unsigned rate = read_le(input, 4), samples = read_le(input, 4), hop = read_le(input, 4);
    unsigned window = read_le(input, 4), frames = read_le(input, 4), harmonics = read_le(input, 4);
    if (rate != 24000 || !samples || samples > rate * MAX_SECONDS || hop != 240 || window != 960 ||
        frames != (samples + hop - 1) / hop + 1 || !harmonics || harmonics > 128)
        fail("Invalid trajectory dimensions.");
    size_t stride = 4 + 2 * harmonics + BANDS;
    unsigned char* data = malloc(stride * frames);
    if (!data)
        fail("Out of memory.");
    if (fread(data, stride, frames, input) != frames || fgetc(input) != EOF || ferror(input))
        fail("Wrong trajectory file length.");
    fclose(input);
    FILE* file = fopen(argv[2], "wb");
    if (!file)
        fail("Cannot open output WAV.");
    fwrite("RIFF", 1, 4, file);
    put_le(file, 36 + samples * 2, 4);
    fwrite("WAVEfmt ", 1, 8, file);
    put_le(file, 16, 4);
    put_le(file, 1, 2);
    put_le(file, 1, 2);
    put_le(file, rate, 4);
    put_le(file, rate * 2, 4);
    put_le(file, 2, 2);
    put_le(file, 16, 2);
    fwrite("data", 1, 4, file);
    put_le(file, samples * 2, 4);
    Output output = {0};
    output.file = file;
    const double edges[BANDS + 1] = {0,    100,  200,  300,  450,  650,  900,   1250, 1700,
                                     2300, 3100, 4100, 5400, 7000, 9000, 10500, 12000};
    double gains[256], taper[960];
    for (unsigned i = 0; i < 256; ++i)
        gains[i] = amplitude(i);
    for (unsigned i = 0; i < window; ++i)
        taper[i] = 0.5 - 0.5 * cos(2 * PI * i / (window - 1));
    clock_t start = clock();
    for (unsigned frame = 0; frame < frames; ++frame) {
        const unsigned char* row = data + frame * stride;
        double f0 = (row[0] + 256 * row[1]) / 64.0;
        double slope = (int16_t)(uint16_t)(row[2] + 256 * row[3]);
        if (f0 && (f0 < 60 || f0 > 400 || fabs(slope) > 10000))
            fail("Invalid fitted pitch.");
        double tone[960] = {0};
        for (unsigned h = 1; h <= harmonics && f0 && h * f0 < rate * 0.48; ++h) {
            double gain = gains[row[4 + h - 1]];
            if (!gain)
                continue;
            double time = -(double)(window / 2) / rate, dt = 1.0 / rate;
            double angle = row[4 + harmonics + h - 1] * (2 * PI / 256) +
                           2 * PI * h * (f0 * time + 0.5 * slope * time * time);
            double increment = 2 * PI * h * (f0 * dt + slope * (time * dt + 0.5 * dt * dt));
            double complex oscillator = cexp(I * angle), step = cexp(I * increment);
            double complex acceleration = cexp(I * 2 * PI * h * slope * dt * dt);
            for (unsigned n = 0; n < window; ++n) {
                tone[n] += gain * creal(oscillator);
                oscillator *= step;
                step *= acceleration;
            }
        }
        double complex spectrum[FFT_SIZE] = {0};
        for (unsigned band = 0; band < BANDS; ++band) {
            unsigned first = (unsigned)ceil(edges[band] * FFT_SIZE / rate);
            unsigned last = (unsigned)ceil(edges[band + 1] * FFT_SIZE / rate);
            if (first < 1)
                first = 1;
            if (last > FFT_SIZE / 2)
                last = FFT_SIZE / 2;
            double gain = gains[row[4 + 2 * harmonics + band]] * FFT_SIZE / sqrt(2.0 * (last - first));
            for (unsigned k = first; k < last; ++k) {
                spectrum[k] = gain * cexp(I * 2 * PI * uniform());
                spectrum[FFT_SIZE - k] = conj(spectrum[k]);
            }
        }
        inverse_fft(spectrum);
        for (unsigned n = 0; n < window; ++n) {
            int64_t at = (int64_t)frame * hop + n - window / 2;
            if (at < 0 || at >= samples)
                continue;
            double w = taper[n];
            unsigned slot = (unsigned)at % window;
            output.voiced[slot] += tone[n] * w;
            output.noise[slot] += creal(spectrum[n]) * w;
            output.weights[slot] += w;
            output.noise_weights[slot] += w * w;
        }
        int64_t complete = (int64_t)(frame + 1) * hop - window / 2;
        if (complete > 0)
            flush_until(&output, (unsigned)fmin((double)complete, samples));
    }
    flush_until(&output, samples);
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;
    if (fclose(file))
        fail("Cannot finish output WAV.");
    printf("{\"render_cpu_seconds\":%.6f,\"audio_seconds\":%.6f,\"peak\":%.6f,\"clipped_samples\":%u}\n",
           seconds, (double)samples / rate, output.peak, output.clipped);
    free(data);
    return 0;
}
