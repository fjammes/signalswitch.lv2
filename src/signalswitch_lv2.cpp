
// SignalSwitchLV2.cpp
// LV2 plugin that routes one audio input to N outputs based on MIDI CC value.
// Switch is based on user-configurable MIDI CC number and channel.

#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_OUTPUTS 8
#ifndef NUM_OUTPUTS
#define NUM_OUTPUTS 4
#endif

#define PLUGIN_URI "http://github.com/fjammes/signalswitch.lv2"

typedef struct {
    const float* input;
    float* outputs[NUM_OUTPUTS];
    const LV2_Atom_Sequence* midi_in;
    const float* cc_number_param;
    const float* midi_channel_param;
    LV2_URID_Map* map;
    LV2_URID midi_event;
    int current_output;
} SignalSwitch;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor, double rate,
            const char* bundle_path, const LV2_Feature* const* features) {
    SignalSwitch* self = (SignalSwitch*)calloc(1, sizeof(SignalSwitch));
    self->map = NULL;
    for (int i = 0; features && features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            self->map = (LV2_URID_Map*)features[i]->data;
        }
    }
    if (!self->map) {
        free(self);
        return NULL;
    }
    self->midi_event = self->map->map(self->map->handle, LV2_MIDI__MidiEvent);
    self->current_output = 0;
    return (LV2_Handle)self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    SignalSwitch* self = (SignalSwitch*)instance;

    if (port == 0) {
        self->input = (const float*)data;
    } else if (port == 1) {
        self->midi_in = (const LV2_Atom_Sequence*)data;
    } else if (port == 2) {
        self->cc_number_param = (const float*)data;
    } else if (port == 3) {
        self->midi_channel_param = (const float*)data;
    } else if (port >= 4 && port < 4 + NUM_OUTPUTS) {
        self->outputs[port - 4] = (float*)data;
    }
}

static void run(LV2_Handle instance, uint32_t n_samples) {
    SignalSwitch* self = (SignalSwitch*)instance;

    int target_cc = (int)(*self->cc_number_param);
    int target_channel = (int)(*self->midi_channel_param) & 0x0F;

    for (int o = 0; o < NUM_OUTPUTS; ++o) {
        memset(self->outputs[o], 0, sizeof(float) * n_samples);
    }

    // Parse MIDI
    LV2_ATOM_SEQUENCE_FOREACH(self->midi_in, ev) {
        if (ev->body.type == self->midi_event) {
            const uint8_t* msg = (const uint8_t*)(ev + 1);
            if ((msg[0] & 0xF0) == 0xB0) {
                int channel = msg[0] & 0x0F;
                int cc_num = msg[1];
                int cc_val = msg[2];

                if (channel == target_channel && cc_num == target_cc) {
                    self->current_output = cc_val % NUM_OUTPUTS;
                }
            }
        }
    }

    const float* in = self->input;
    float* out = self->outputs[self->current_output];
    for (uint32_t i = 0; i < n_samples; ++i) {
        out[i] = in[i];
    }
}

static void cleanup(LV2_Handle instance) {
    free(instance);
}

static const LV2_Descriptor descriptor = {
    PLUGIN_URI,
    instantiate,
    connect_port,
    NULL,
    run,
    NULL,
    cleanup,
    NULL
};

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : NULL;
}
