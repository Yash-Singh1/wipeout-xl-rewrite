#include "spu.h"

#include <string.h>

/* Arithmetic shift right with the sign preserved: the hardware's "SAR". */
static int32_t sar(int64_t value, unsigned bits)
{
    return (int32_t)(value >> bits);
}

static int16_t clamp16(int32_t value)
{
    return (int16_t)(value > 32767 ? 32767 : value < -32768 ? -32768 : value);
}

static int16_t s16(uint16_t value)
{
    return (int16_t)value;
}

static const int16_t gaussian[512] = {
#include "spu_gauss.inc"
};

/* ---- ADPCM ------------------------------------------------------------------------ */

void SpuDecodeBlock(const uint8_t block[16], int16_t history[2], int16_t out[28])
{
    static const int16_t positive[16] = {0, 60, 115, 98, 122};
    static const int16_t negative[16] = {0, 0, -52, -55, -60};
    unsigned shift = block[0] & 15;
    unsigned filter = block[0] >> 4;
    unsigned i;

    if (shift > 12) {
        shift = 9; /* reserved shift values behave as 9 */
    }
    for (i = 0; i < 28; i++) {
        int32_t nibble = (block[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int32_t sample;

        if (nibble >= 8) {
            nibble -= 16;
        }
        sample = sar(nibble * 4096, shift);
        /* each predictor product is shifted (floored) separately */
        sample += sar(history[0] * positive[filter], 6);
        sample += sar(history[1] * negative[filter], 6);
        out[i] = clamp16(sample);
        history[1] = history[0];
        history[0] = out[i];
    }
}

int32_t SpuGaussian(const int16_t samples[4], unsigned fraction)
{
    int64_t sum = (int32_t)gaussian[255 - fraction] * samples[0];

    sum += (int32_t)gaussian[511 - fraction] * samples[1];
    sum += (int32_t)gaussian[256 + fraction] * samples[2];
    sum += (int32_t)gaussian[fraction] * samples[3];
    return sar(sum, 15);
}

/* ---- envelopes ---------------------------------------------------------------------- */

void SpuEnvelopeInit(SpuEnvelope *e, unsigned rate, unsigned mask, bool decrease, bool exponential,
                     bool negative)
{
    memset(e, 0, sizeof(*e));
    e->rate = (uint8_t)rate;
    e->decrease = decrease;
    e->exponential = exponential;
    e->negative = negative && !(decrease && exponential);
    e->step = 7 - (int32_t)(rate & 3);
    if ((decrease != negative) || (decrease && exponential)) {
        e->step = -e->step - 1;
    }
    e->increment = 0x8000;
    if (rate < 44) {
        e->step *= 1 << (11 - rate / 4);
    } else if (rate >= 48) {
        e->increment >>= rate / 4 - 11;
        if (!e->increment && (rate & mask) != mask) {
            e->increment = 1;
        }
    }
}

bool SpuEnvelopeTick(SpuEnvelope *e, int16_t *level)
{
    uint32_t increment = e->increment;
    int32_t step = e->step;
    int32_t next;

    if (e->exponential) {
        if (e->decrease) {
            step = sar((int64_t)step * *level, 15);
        } else if (*level >= 0x6000) {
            if (e->rate < 40) {
                step = sar(step, 2);
            } else if (e->rate >= 44) {
                increment >>= 2;
            } else {
                step = sar(step, 1);
                increment >>= 1;
            }
        }
    }
    e->counter += increment;
    if (!(e->counter & 0x8000)) {
        return true;
    }
    e->counter = 0;
    next = *level + step;
    if (!e->decrease) {
        *level = clamp16(next);
        return *level != (step < 0 ? -32768 : 32767);
    }
    if (e->negative) {
        *level = clamp16(next > 0 ? 0 : next);
        return *level < 0;
    }
    *level = clamp16(next < 0 ? 0 : next);
    return *level > 0;
}

/* Volume registers: bit 15 clear is a fixed volume (15 bits, doubled); set is a sweep
   (bit 14 exponential, bit 13 decrease, bit 12 negative phase, bits 0-6 rate). */
static void sweep_set(SpuSweep *sweep, uint16_t reg)
{
    if (!(reg & 0x8000)) {
        sweep->level = s16((uint16_t)(reg << 1));
        sweep->active = false;
    } else {
        SpuEnvelopeInit(&sweep->envelope, reg & 127, 127, (reg & 0x2000) != 0, (reg & 0x4000) != 0,
                        (reg & 0x1000) != 0);
        sweep->active = sweep->envelope.increment != 0;
    }
}

static void sweep_tick(SpuSweep *sweep)
{
    if (sweep->active) {
        sweep->active = SpuEnvelopeTick(&sweep->envelope, &sweep->level);
    }
}

/* Loads the envelope generator for the voice's current ADSR phase. */
static void adsr_set(SpuVoice *v)
{
    uint16_t low = v->reg[4];
    uint16_t high = v->reg[5];

    switch (v->phase) {
    case ADSR_ATTACK:
        SpuEnvelopeInit(&v->envelope, (low >> 8) & 127, 127, false, (low & 0x8000) != 0, false);
        break;
    case ADSR_DECAY:
        SpuEnvelopeInit(&v->envelope, ((low >> 4) & 15) * 4, 124, true, true, false);
        break;
    case ADSR_SUSTAIN:
        SpuEnvelopeInit(&v->envelope, (high >> 6) & 127, 127, (high & 0x4000) != 0,
                        (high & 0x8000) != 0, false);
        break;
    case ADSR_RELEASE:
        SpuEnvelopeInit(&v->envelope, (high & 31) * 4, 124, true, (high & 32) != 0, false);
        break;
    default:
        memset(&v->envelope, 0, sizeof(v->envelope));
        break;
    }
}

static void adsr_tick(SpuVoice *v)
{
    int16_t level = s16(v->reg[6]);
    int32_t target;

    SpuEnvelopeTick(&v->envelope, &level);
    v->reg[6] = (uint16_t)level;
    target = ((v->reg[4] & 15) + 1) * 0x800;
    if (target > 32767) {
        target = 32767;
    }
    if ((v->phase == ADSR_ATTACK && level == 32767) || (v->phase == ADSR_DECAY && level <= target) ||
        (v->phase == ADSR_RELEASE && level == 0)) {
        v->phase = v->phase == ADSR_RELEASE ? ADSR_OFF : v->phase + 1;
        adsr_set(v);
    }
}

/* ---- registers ---------------------------------------------------------------------- */

void SpuReset(Spu *spu)
{
    memset(spu, 0, sizeof(*spu));
    spu->noise_level = 1;
}

void SpuWriteRam(Spu *spu, uint32_t address, const void *data, size_t size)
{
    const uint8_t *bytes = data;
    size_t i;

    for (i = 0; i < size; i++) {
        spu->ram[(address + i) & (SPU_RAM_SIZE - 1)] = bytes[i];
    }
}

void SpuTransfer(Spu *spu, const void *data, size_t size)
{
    SpuWriteRam(spu, spu->transfer_address, data, size);
    spu->transfer_address = (spu->transfer_address + (uint32_t)size) & (SPU_RAM_SIZE - 1);
}

uint16_t SpuReadRegister(const Spu *spu, uint32_t offset)
{
    if ((offset & 1) || offset >= 0x280) {
        return 0xFFFF;
    }
    if (offset < 0x180) {
        return spu->voice[offset / 16].reg[offset % 16 / 2];
    }
    if (offset == SPU_ENDX || offset == SPU_ENDX + 2) {
        return (uint16_t)(spu->endx >> ((offset & 2) * 8));
    }
    if (offset == SPU_STATUS) {
        return spu->reg[SPU_CONTROL / 2] & 63;
    }
    if (offset == 0x1B8 || offset == 0x1BA) {
        return (uint16_t)spu->main[(offset - 0x1B8) / 2].level;
    }
    if (offset >= 0x200 && offset < 0x260) {
        return (uint16_t)spu->voice[(offset - 0x200) / 4].volume[offset % 4 / 2].level;
    }
    return spu->reg[offset / 2];
}

void SpuWriteRegister(Spu *spu, uint32_t offset, uint16_t value)
{
    if ((offset & 1) || offset >= 0x280) {
        return;
    }
    if (spu->trace) {
        spu->trace(spu->trace_user, spu->samples, offset, value);
    }
    if (offset < 0x180) {
        SpuVoice *v = &spu->voice[offset / 16];
        unsigned index = offset % 16 / 2;

        v->reg[index] = value;
        if (index < 2) {
            sweep_set(&v->volume[index], value);
        } else if (index == 4 || index == 5) {
            adsr_set(v);
        } else if (index == 7) {
            v->ignore_loop |= v->phase == ADSR_OFF || !v->first_block;
        }
        return;
    }
    if (offset == SPU_CONTROL && !(value & SPU_CTRL_ENABLE) &&
        (spu->reg[offset / 2] & SPU_CTRL_ENABLE)) {
        unsigned i;

        for (i = 0; i < SPU_VOICES; i++) {
            spu->voice[i].phase = ADSR_OFF;
            spu->voice[i].reg[6] = 0;
        }
    }
    spu->reg[offset / 2] = value;
    if (offset == SPU_MAIN_VOL_L || offset == SPU_MAIN_VOL_R) {
        sweep_set(&spu->main[(offset - SPU_MAIN_VOL_L) / 2], value);
    } else if (offset >= SPU_KEY_ON && offset <= SPU_KEY_OFF + 2) {
        uint32_t mask = (uint32_t)value << ((offset & 2) * 8);
        uint32_t lane = 0xFFFFu << ((offset & 2) * 8);

        if (offset < SPU_KEY_OFF) {
            spu->key_on = ((spu->key_on & ~lane) | mask) & 0xFFFFFF;
            spu->key_latch |= mask & 0xFFFFFF;
        } else {
            spu->key_off = ((spu->key_off & ~lane) | mask) & 0xFFFFFF;
            spu->key_latch &= ~mask;
        }
    } else if (offset == SPU_REVERB_BASE) {
        spu->reverb_address = (uint32_t)value * 8;
    } else if (offset == SPU_TRANSFER_ADDRESS) {
        spu->transfer_address = (uint32_t)value * 8;
    } else if (offset == SPU_TRANSFER_FIFO) {
        uint8_t bytes[2];

        bytes[0] = (uint8_t)value;
        bytes[1] = (uint8_t)(value >> 8);
        SpuTransfer(spu, bytes, 2);
    }
}

/* ---- voices ------------------------------------------------------------------------- */

static uint32_t mask_register(const Spu *spu, unsigned offset)
{
    return spu->reg[offset / 2] | ((uint32_t)(spu->reg[offset / 2 + 1] & 255) << 16);
}

static void decode_voice(Spu *spu, SpuVoice *v)
{
    uint8_t block[16];
    unsigned i;

    memmove(v->decoded, v->decoded + 28, 3 * sizeof(int16_t));
    for (i = 0; i < 16; i++) {
        block[i] = spu->ram[(v->address + i) & (SPU_RAM_SIZE - 1)];
    }
    SpuDecodeBlock(block, v->history, v->decoded + 3);
    v->flags = block[1];
    if ((v->flags & 4) && !v->ignore_loop) {
        v->reg[7] = (uint16_t)(v->address / 8);
    }
    v->has_block = true;
}

static int32_t sample_voice(Spu *spu, unsigned index, int32_t previous)
{
    SpuVoice *v = &spu->voice[index];
    bool noise;
    int32_t sample;
    uint32_t pitch;
    unsigned channel;

    if (v->phase == ADSR_OFF) {
        v->last = 0;
        return 0;
    }
    if (!v->has_block) {
        decode_voice(spu, v);
    }
    noise = (mask_register(spu, SPU_NOISE_MODE) >> index) & 1;
    sample = noise ? s16(spu->noise_level)
                   : SpuGaussian(v->decoded + (v->counter >> 12), (v->counter >> 4) & 255);
    v->last = sar((int64_t)sample * s16(v->reg[6]), 15);
    adsr_tick(v);
    pitch = v->reg[2];
    if (index && ((mask_register(spu, SPU_PITCH_MOD) >> index) & 1)) {
        pitch = (uint16_t)sar((int64_t)pitch * (clamp16(previous) + 32768), 15);
    }
    if (pitch > 0x3FFF) {
        pitch = 0x3FFF;
    }
    v->counter += pitch;
    if (v->counter >= 28 * 4096) {
        v->first_block = false;
        v->counter -= 28 * 4096;
        v->address = (v->address + 16) & (SPU_RAM_SIZE - 1);
        v->has_block = false;
        if (v->flags & 1) { /* loop end */
            spu->endx |= 1u << index;
            v->address = (uint32_t)(v->reg[7] & 0xFFFE) * 8;
            if (!(v->flags & 2) && !noise) { /* no repeat flag: the voice stops */
                v->phase = ADSR_OFF;
                v->reg[6] = 0;
            }
        }
    }
    for (channel = 0; channel < 2; channel++) {
        spu->stems[index][channel] = clamp16(sar((int64_t)v->last * v->volume[channel].level, 15));
        sweep_tick(&v->volume[channel]);
    }
    return v->last;
}

/* Key on/off take effect after the sample in which they were written. */
static void apply_keys(Spu *spu)
{
    unsigned i;

    for (i = 0; i < SPU_VOICES; i++) {
        SpuVoice *v = &spu->voice[i];

        if ((spu->key_off >> i) & 1) {
            if (v->phase != ADSR_OFF && v->phase != ADSR_RELEASE) {
                v->phase = ADSR_RELEASE;
                adsr_set(v);
            }
        }
        if ((spu->key_on >> i) & 1) {
            v->address = (uint32_t)(v->reg[3] & 0xFFFE) * 8;
            v->counter = 0;
            v->reg[6] = 0;
            memset(v->history, 0, sizeof(v->history));
            memset(v->decoded, 0, sizeof(v->decoded));
            v->has_block = false;
            v->ignore_loop = false;
            v->first_block = true;
            v->phase = ADSR_ATTACK;
            adsr_set(v);
            spu->endx &= ~(1u << i);
        }
    }
    spu->key_on = 0;
    spu->key_off = 0;
}

/* ---- reverb ------------------------------------------------------------------------- */

static uint32_t reverb_address(const Spu *spu, int32_t offset)
{
    uint32_t base = (uint32_t)spu->reg[SPU_REVERB_BASE / 2] * 8;
    uint32_t address = spu->reverb_address + ((uint32_t)offset & 0x7FFFF);

    if (address >= SPU_RAM_SIZE) {
        address += base;
    }
    return address & 0x7FFFE;
}

static int16_t reverb_read(const Spu *spu, int32_t offset)
{
    uint32_t address = reverb_address(spu, offset);

    return s16((uint16_t)(spu->ram[address] | (uint16_t)spu->ram[address + 1] << 8));
}

static void reverb_write(Spu *spu, int32_t offset, int16_t sample)
{
    uint32_t address = reverb_address(spu, offset);

    spu->ram[address] = (uint8_t)sample;
    spu->ram[address + 1] = (uint8_t)((uint16_t)sample >> 8);
}

static int32_t reverb_mul(int32_t value, uint16_t coefficient)
{
    return sar((int64_t)value * s16(coefficient), 14);
}

/* The 39-tap resampling filter: even taps, tap 19 (the centre) is 0x4000. */
static const int16_t reverb_fir[20] = {-1,   2,    -10,   35,  -103, 266,  -616, 1332, -2960, 10246,
                                       10246, -2960, 1332, -616, 266,  -103, 35,   -10,  2,     -1};

static int32_t reverb_complement(int16_t sample, int16_t coefficient)
{
    if (coefficient == -32768) { /* hardware special case */
        return sample == -32768 ? 0 : sar((int64_t)sample * -65536, 14);
    }
    return sar((int64_t)sample * (32768 - coefficient), 14);
}

static uint16_t reverb_negate(uint16_t coefficient)
{
    return coefficient == 0x8000 ? 0x7FFF : (uint16_t)-coefficient;
}

static void reverb_tick(Spu *spu, const int32_t input[2], int32_t output[2])
{
    unsigned position = spu->reverb_position;
    const uint16_t *r = spu->reg + SPU_REVERB_REGS / 2;
    bool enabled = (spu->reg[SPU_CONTROL / 2] & SPU_CTRL_REVERB_ENABLE) != 0;
    unsigned c;

    for (c = 0; c < 2; c++) {
        spu->reverb_input[c][position] = clamp16(input[c]);
    }
    if (position & 1) { /* the reverb unit runs at half the output rate */
        for (c = 0; c < 2; c++) {
            int64_t sum = (int32_t)spu->reverb_input[c][(position - 19) & 63] * 0x4000;
            int16_t downsampled;
            int32_t comb;
            int16_t delayed1, delayed2, apf1, apf2;
            unsigned tap, side;

            for (tap = 0; tap < 20; tap++) {
                sum += (int32_t)spu->reverb_input[c][(position - 38 + tap * 2) & 63] * reverb_fir[tap];
            }
            downsampled = clamp16(sar(sum, 15));
            if (enabled) {
                for (side = 0; side < 2; side++) {
                    unsigned source = side ? 24 + (c ^ 1) : 16 + c;
                    unsigned destination = side ? 18 + c : 10 + c;
                    int32_t reflected = reverb_mul(reverb_read(spu, r[source] * 8), r[7]);
                    int16_t in;
                    int32_t result;

                    reflected += reverb_mul(downsampled, r[30 + c]);
                    in = clamp16(sar(reflected, 1));
                    result = reverb_mul(in, r[2]);
                    result += reverb_complement(reverb_read(spu, r[destination] * 8 - 2), s16(r[2]));
                    reverb_write(spu, r[destination] * 8, clamp16(sar(result, 1)));
                }
            }
            comb = reverb_mul(reverb_read(spu, r[12 + c] * 8), r[3]);
            comb += reverb_mul(reverb_read(spu, r[14 + c] * 8), r[4]);
            comb += reverb_mul(reverb_read(spu, r[20 + c] * 8), r[5]);
            comb += reverb_mul(reverb_read(spu, r[22 + c] * 8), r[6]);
            delayed1 = reverb_read(spu, (int32_t)(r[26 + c] - r[0]) * 8);
            delayed2 = reverb_read(spu, (int32_t)(r[28 + c] - r[1]) * 8);
            apf1 = clamp16(sar(comb + reverb_mul(delayed1, reverb_negate(r[8])), 1));
            apf2 = clamp16(delayed1 + sar(reverb_mul(apf1, r[8]) + reverb_mul(delayed2, reverb_negate(r[9])), 1));
            spu->reverb_output[c][position / 2] =
                clamp16(delayed2 + sar((int64_t)apf2 * s16(r[9]), 15));
            if (enabled) {
                reverb_write(spu, r[26 + c] * 8, apf1);
                reverb_write(spu, r[28 + c] * 8, apf2);
            }
        }
        spu->reverb_address = (spu->reverb_address + 2) & 0x7FFFF;
        if (!spu->reverb_address) {
            spu->reverb_address = (uint32_t)spu->reg[SPU_REVERB_BASE / 2] * 8;
        }
    }
    for (c = 0; c < 2; c++) {
        int32_t sample;

        if (position & 1) {
            int64_t sum = 0;
            unsigned tap;

            for (tap = 0; tap < 20; tap++) {
                sum += (int32_t)spu->reverb_output[c][(position / 2 - 19 + tap) & 31] * reverb_fir[tap];
            }
            sample = clamp16(sar(sum, 14));
        } else {
            sample = spu->reverb_output[c][(position / 2 - 10) & 31];
        }
        output[c] = sar((int64_t)sample * s16(spu->reg[SPU_REVERB_VOL_L / 2 + c]), 15);
    }
    spu->reverb_position = (position + 1) & 63;
}

static void noise_tick(Spu *spu)
{
    uint16_t control = spu->reg[SPU_CONTROL / 2];

    spu->noise_timer -= 4 + ((control >> 8) & 3);
    if (spu->noise_timer < 0) {
        unsigned parity = ((spu->noise_level >> 15) ^ (spu->noise_level >> 12) ^
                           (spu->noise_level >> 11) ^ (spu->noise_level >> 10) ^ 1) & 1;
        int32_t period = 0x20000 >> ((control >> 10) & 15);

        spu->noise_level = (uint16_t)(spu->noise_level * 2 + parity);
        spu->noise_timer += period;
        if (spu->noise_timer < 0) {
            spu->noise_timer += period;
        }
    }
}

/* ---- the mixer ---------------------------------------------------------------------- */

void SpuSample(Spu *spu, const int16_t *cd, int16_t out[2])
{
    int32_t mixed[2] = {0, 0};
    int32_t wet_input[2] = {0, 0};
    int32_t wet[2];
    int32_t previous = 0;
    uint16_t control = spu->reg[SPU_CONTROL / 2];
    uint32_t reverb_mask = mask_register(spu, SPU_REVERB_ON);
    int16_t captured[4];
    unsigned i, c, page, capture;

    memset(spu->stems, 0, sizeof(spu->stems));
    for (i = 0; i < SPU_VOICES; i++) {
        previous = sample_voice(spu, i, previous);
        for (c = 0; c < 2; c++) {
            if (control & SPU_CTRL_UNMUTE) {
                mixed[c] += spu->stems[i][c];
                if ((reverb_mask >> i) & 1) {
                    wet_input[c] += spu->stems[i][c];
                }
            }
        }
    }
    noise_tick(spu);
    for (c = 0; c < 2; c++) {
        if (cd && (control & SPU_CTRL_CD_ENABLE)) {
            spu->stems[25][c] = clamp16(sar((int64_t)cd[c] * s16(spu->reg[SPU_CD_VOL_L / 2 + c]), 15));
            mixed[c] += spu->stems[25][c];
            if (control & SPU_CTRL_CD_REVERB) {
                wet_input[c] += spu->stems[25][c];
            }
        }
    }
    reverb_tick(spu, wet_input, wet);
    for (c = 0; c < 2; c++) {
        spu->stems[24][c] = clamp16(wet[c]);
        mixed[c] += wet[c];
        out[c] = clamp16(sar((int64_t)clamp16(mixed[c]) * spu->main[c].level, 15));
        sweep_tick(&spu->main[c]);
    }
    /* The capture buffers at the start of RAM: CD left/right, voice 1, voice 3. */
    capture = (unsigned)(spu->samples & 511) * 2;
    captured[0] = cd ? cd[0] : 0;
    captured[1] = cd ? cd[1] : 0;
    captured[2] = clamp16(spu->voice[1].last);
    captured[3] = clamp16(spu->voice[3].last);
    for (page = 0; page < 4; page++) {
        unsigned address = page * 1024 + capture;

        spu->ram[address] = (uint8_t)captured[page];
        spu->ram[address + 1] = (uint8_t)((uint16_t)captured[page] >> 8);
    }
    apply_keys(spu);
    spu->samples++;
}
