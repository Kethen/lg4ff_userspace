#ifndef __FORCE_FEEDBACK_H
#define __FORCE_FEEDBACK_H

#include <stdint.h>
#include <stdbool.h>

#include <hidapi/hidapi.h>

#define LG4FF_MAX_EFFECTS 16

#define FF_EFFECT_STARTED 0
#define FF_EFFECT_ALLSET 1
#define FF_EFFECT_PLAYING 2
#define FF_EFFECT_UPDATING 3

#if __linux__
#include <linux/input.h>
#else
// from the linux kernel source, include/uapi/linux/input.h
// required for dealing with linux uinput, should be easily adapted to dinput8 as well since they are pretty similar, if you really want to write a virtual driver for windows as well
struct ff_envelope{
	uint16_t attack_length;
	uint16_t attack_level;
	uint16_t fade_length;
	uint16_t fade_level;
};

struct ff_rumble_effect{
	uint16_t strong_magnitude;
	uint16_t weak_magnitude;
};

struct ff_condition_effect{
	uint16_t right_saturation;
	uint16_t left_saturation;

	int16_t right_coeff;
	int16_t left_coeff;

	uint16_t deadband;
	int16_t center;
};

struct ff_periodic_effect{
	uint16_t waveform;
	uint16_t period;
	int16_t magnitude;
	int16_t offset;
	uint16_t phase;

	struct ff_envelope envelope;

	uint32_t custom_len;
	int16_t *custom_data;
};

struct ff_ramp_effect{
	int16_t start_level;
	int16_t end_level;
	struct ff_envelope;
};

struct ff_constant_effect{
	int16_t level;
	struct ff_envelope;
};

struct ff_trigger{
	uint16_t button;
	uint16_t interval;
};

struct ff_replay{
	uint16_t length;
	uint16_t delay;
};

struct ff_effect{
	uint16_t type;
	int16_t id;
	uint16_t direction;
	struct ff_trigger trigger;
	struct ff_replay replay;

	union {
		struct ff_constant_effect constant;
		struct ff_ramp_effect ramp;
		struct ff_periodic_effect periodic;
		struct ff_condition_effect condition[2];
		struct ff_rumble_effect rumble;

	} u;
};

#endif

// ported from https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c
// effect state tracking before wheel rendering
struct lg4ff_effect_state{
	struct ff_effect effect;
	struct ff_envelope *envelope;
	uint64_t start_at;
	uint64_t play_at;
	uint64_t stop_at;
	uint32_t flags;
	uint64_t time_playing;
	uint64_t updated_at;
	uint32_t phase;
	uint32_t phase_adj;
	uint32_t count;

	// unused?
	#if 0
	uint32_t cmd;
	uint64_t cmd_start_time;
	uint32_t cmd_start_count;
	#endif

	int32_t direction_gain;
	int32_t slope;
};

struct lg4ff_effect_parameters{
	int32_t level;
	int32_t d1;
	int32_t d2;
	int32_t k1;
	int32_t k2;
	uint32_t clip;
};

struct lg4ff_slot{
	int32_t id;
	struct lg4ff_effect_parameters parameters;
	uint8_t current_cmd[7];
	uint32_t cmd_op;
	bool is_updated;
	int32_t effect_type;
};

struct lg4ff_device{
	uint16_t product_id;
	struct lg4ff_effect_state states[LG4FF_MAX_EFFECTS];
	struct lg4ff_slot slots[4];
	int32_t effects_used;

	int32_t gain;
	int32_t app_gain;

	int32_t spring_level;
	int32_t damper_level;
	int32_t friction_level;

	int32_t peak_ffb_level;

	hid_device *hid_handle;
};

void lg4ff_init_slots(struct lg4ff_device *device);
int lg4ff_upload_effect(struct lg4ff_device *device, struct ff_effect *effect, struct ff_effect *old);
int lg4ff_play_effect(struct lg4ff_device *device, int effect_id, int value);
int lg4ff_timer(struct lg4ff_device *device);

#endif
