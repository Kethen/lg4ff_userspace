// ref https://github.com/berarma/new-lg4ff/blob/master/hid-lg4ff.c, functions are mostly ported over
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <errno.h>
#include <time.h>

#include <hidapi/hidapi.h>

#include "logging.h"
#include "force_feedback.h"
#include "log_effect.h"

#define test_bit(bit, field) (*(field) & (1 << bit))
#define __set_bit(bit, field) {*(field) = *(field) | (1 << bit);}
#define __clear_bit(bit, field) {*(field) = *(field) & ~(1 << bit);}

#define sin_deg(in) (double)(sin((double)(in) * (double)M_PI / 180.0))

#define time_after_eq(a, b) (a >= b)
#define time_before(a, b) (a < b)
#define time_diff(a, b) (a - b)

#define STOP_EFFECT(state) ((state)->flags = 0)

#define CLAMP_VALUE_U16(x) ((uint16_t)((x) > 0xffff ? 0xffff : (x)))
#define SCALE_VALUE_U16(x, bits) (CLAMP_VALUE_U16(x) >> (16 - bits))
#define CLAMP_VALUE_S16(x) ((uint16_t)((x) <= -0x8000 ? -0x8000 : ((x) > 0x7fff ? 0x7fff : (x))))
#define TRANSLATE_FORCE(x) ((CLAMP_VALUE_S16(x) + 0x8000) >> 8)
#define SCALE_COEFF(x, bits) SCALE_VALUE_U16(abs(x) * 2, bits)

#if 0
#define DEBUG(...)
#else
#define DEBUG(...) STDOUT(__VA_ARGS__)
#endif
uint64_t get_time_ms(){
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return tp.tv_nsec / 1000000 + tp.tv_sec * 1000;
}

int lg4ff_upload_effect(struct lg4ff_device *device, struct ff_effect *effect, struct ff_effect *old, bool log)
{
	struct lg4ff_effect_state *state;
	uint64_t now = get_time_ms();

	if (effect->type == FF_PERIODIC && effect->u.periodic.period == 0) {
		return -EINVAL;
	}

	state = &device->states[effect->id];

	if (test_bit(FF_EFFECT_STARTED, &state->flags) && effect->type != state->effect.type) {
		return -EINVAL;
	}

	state->effect = *effect;

	if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
		__set_bit(FF_EFFECT_UPDATING, &state->flags);
		state->updated_at = now;
	}

	if(log){
		STDOUT("--upload--\n");
		log_effect(effect);
	}

	return 0;
}

int lg4ff_play_effect(struct lg4ff_device *device, int effect_id, int value, bool log)
{
	struct lg4ff_effect_state *state;
	uint64_t now = get_time_ms();

	state = &device->states[effect_id];

	if (value > 0) {
		if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
			STOP_EFFECT(state);
		} else {
			device->effects_used++;
		}
		__set_bit(FF_EFFECT_STARTED, &state->flags);
		state->start_at = now;
		state->count = value;
		if(log){
			STDOUT("--play--\n");
			log_effect(&state->effect);
		}
	} else {
		if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
			STOP_EFFECT(state);
			device->effects_used--;
		}
	}

	return 0;
}

static struct ff_envelope *lg4ff_effect_envelope(struct ff_effect *effect)
{
	switch (effect->type) {
		case FF_CONSTANT:
			return &effect->u.constant.envelope;
		case FF_RAMP:
			return &effect->u.ramp.envelope;
		case FF_PERIODIC:
			return &effect->u.periodic.envelope;
	}

	return NULL;
}

static void lg4ff_update_state(struct lg4ff_effect_state *state, const uint64_t now)
{
	struct ff_effect *effect = &state->effect;
	uint64_t phase_time;

	if (!test_bit(FF_EFFECT_ALLSET, &state->flags)) {
		state->play_at = state->start_at + effect->replay.delay;
		if (!test_bit(FF_EFFECT_UPDATING, &state->flags)) {
			state->updated_at = state->play_at;
		}
		state->direction_gain = sin_deg(effect->direction * 360 / 0x10000);
		if (effect->type == FF_PERIODIC) {
			state->phase_adj = effect->u.periodic.phase * 360 / effect->u.periodic.period;
		}
		if (effect->replay.length) {
			state->stop_at = state->play_at + effect->replay.length;
		}
	}
	__set_bit(FF_EFFECT_ALLSET, &state->flags);

	if (test_bit(FF_EFFECT_UPDATING, &state->flags)) {
		__clear_bit(FF_EFFECT_PLAYING, &state->flags);
		state->play_at = state->updated_at + effect->replay.delay;
		state->direction_gain = sin_deg(effect->direction * 360 / 0x10000);
		if (effect->replay.length) {
			state->stop_at = state->updated_at + effect->replay.length;
		}
		if (effect->type == FF_PERIODIC) {
			state->phase_adj = state->phase;
		}
	}
	__clear_bit(FF_EFFECT_UPDATING, &state->flags);

	state->envelope = lg4ff_effect_envelope(effect);

	state->slope = 0;
	if (effect->type == FF_RAMP && effect->replay.length) {
		state->slope = ((effect->u.ramp.end_level - effect->u.ramp.start_level) << 16) / (effect->replay.length - state->envelope->attack_length - state->envelope->fade_length);
	}

	if (!test_bit(FF_EFFECT_PLAYING, &state->flags) && time_after_eq(now,
				state->play_at) && (effect->replay.length == 0 ||
					time_before(now, state->stop_at))) {
		__set_bit(FF_EFFECT_PLAYING, &state->flags);
	}

	if (test_bit(FF_EFFECT_PLAYING, &state->flags)) {
		state->time_playing = time_diff(now, state->play_at);
		if (effect->type == FF_PERIODIC) {
			phase_time = time_diff(now, state->updated_at);
			state->phase = (phase_time % effect->u.periodic.period) * 360 / effect->u.periodic.period;
			state->phase += state->phase_adj % 360;
		}
	}
}

static int32_t lg4ff_calculate_constant(struct lg4ff_effect_state *state)
{
	int32_t level_sign;
	int32_t level = state->effect.u.constant.level;
	int32_t d, t;

	if (state->time_playing < state->envelope->attack_length) {
		level_sign = level < 0 ? -1 : 1;
		d = level - level_sign * state->envelope->attack_level;
		level = level_sign * state->envelope->attack_level + d * state->time_playing / state->envelope->attack_length;
	} else if (state->effect.replay.length) {
		t = state->time_playing - state->effect.replay.length + state->envelope->fade_length;
		if (t > 0) {
			level_sign = level < 0 ? -1 : 1;
			d = level - level_sign * state->envelope->fade_level;
			level = level - d * t / state->envelope->fade_length;
		}
	}

	return state->direction_gain * level;
}

static int32_t lg4ff_calculate_ramp(struct lg4ff_effect_state *state)
{
	struct ff_ramp_effect *ramp = &state->effect.u.ramp;
	int32_t level_sign;
	int32_t level;
	int32_t d, t;

	if (state->time_playing < state->envelope->attack_length) {
		level = ramp->start_level;
		level_sign =  level < 0 ? -1 : 1;
		t = state->envelope->attack_length - state->time_playing;
		d = level - level_sign * state->envelope->attack_level;
		level = level_sign * state->envelope->attack_level + d * t / state->envelope->attack_length;
	} else if (state->effect.replay.length && state->time_playing >= state->effect.replay.length - state->envelope->fade_length) {
		level = ramp->end_level;
		level_sign = level < 0 ? -1 : 1;
		t = state->time_playing - state->effect.replay.length + state->envelope->fade_length;
		d = level_sign * state->envelope->fade_level - level;
		level = level - d * t / state->envelope->fade_length;
	} else {
		t = state->time_playing - state->envelope->attack_length;
		level = ramp->start_level + ((t * state->slope) >> 16);
	}

	return state->direction_gain * level;
}

static int32_t lg4ff_calculate_periodic(struct lg4ff_effect_state *state)
{
	struct ff_periodic_effect *periodic = &state->effect.u.periodic;
	int32_t magnitude = periodic->magnitude;
	int32_t magnitude_sign = magnitude < 0 ? -1 : 1;
	int32_t level = periodic->offset;
	int32_t d, t;

	if (state->time_playing < state->envelope->attack_length) {
		d = magnitude - magnitude_sign * state->envelope->attack_level;
		magnitude = magnitude_sign * state->envelope->attack_level + d * state->time_playing / state->envelope->attack_length;
	} else if (state->effect.replay.length) {
		t = state->time_playing - state->effect.replay.length + state->envelope->fade_length;
		if (t > 0) {
			d = magnitude - magnitude_sign * state->envelope->fade_level;
			magnitude = magnitude - d * t / state->envelope->fade_length;
		}
	}

	switch (periodic->waveform) {
		case FF_SINE:
			level += sin_deg(state->phase) * magnitude;
			break;
		case FF_SQUARE:
			level += (state->phase < 180 ? 1 : -1) * magnitude;
			break;
		case FF_TRIANGLE:
			level += abs(state->phase * magnitude * 2 / 360 - magnitude) * 2 - magnitude;
			break;
		case FF_SAW_UP:
			level += state->phase * magnitude * 2 / 360 - magnitude;
			break;
		case FF_SAW_DOWN:
			level += magnitude - state->phase * magnitude * 2 / 360;
			break;
	}

	return state->direction_gain * level;
}

static void lg4ff_calculate_spring(struct lg4ff_effect_state *state, struct lg4ff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];

	parameters->d1 = ((int32_t)condition->center) - condition->deadband / 2;
	parameters->d2 = ((int32_t)condition->center) + condition->deadband / 2;
	parameters->k1 = condition->left_coeff;
	parameters->k2 = condition->right_coeff;
	parameters->clip = (uint16_t)condition->right_saturation;
}

static void lg4ff_calculate_resistance(struct lg4ff_effect_state *state, struct lg4ff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];

	parameters->k1 = condition->left_coeff;
	parameters->k2 = condition->right_coeff;
	parameters->clip = (uint16_t)condition->right_saturation;
}

static void lg4ff_update_slot(struct lg4ff_slot *slot, struct lg4ff_effect_parameters *parameters)
{
	uint8_t original_cmd[7];
	int32_t d1;
	int32_t d2;
	int32_t k1;
	int32_t k2;
	int32_t s1;
	int32_t s2;

	memcpy(original_cmd, slot->current_cmd, sizeof(original_cmd));

	if ((original_cmd[0] & 0xf) == 1) {
		original_cmd[0] = (original_cmd[0] & 0xf0) + 0xc;
	}

	if (slot->effect_type == FF_CONSTANT) {
		if (slot->cmd_op == 0) {
			slot->cmd_op = 1;
		} else {
			slot->cmd_op = 0xc;
		}
	} else {
		if (parameters->clip == 0) {
			slot->cmd_op = 3;
		} else if (slot->cmd_op == 3) {
			slot->cmd_op = 1;
		} else {
			slot->cmd_op = 0xc;
		}
	}

	slot->current_cmd[0] = (0x10 << slot->id) + slot->cmd_op;

	if (slot->cmd_op == 3) {
		slot->current_cmd[1] = 0;
		slot->current_cmd[2] = 0;
		slot->current_cmd[3] = 0;
		slot->current_cmd[4] = 0;
		slot->current_cmd[5] = 0;
		slot->current_cmd[6] = 0;
	} else {
		switch (slot->effect_type) {
			case FF_CONSTANT:
				slot->current_cmd[1] = 0x00;
				slot->current_cmd[2] = 0;
				slot->current_cmd[3] = 0;
				slot->current_cmd[4] = 0;
				slot->current_cmd[5] = 0;
				slot->current_cmd[6] = 0;
				slot->current_cmd[2 + slot->id] = TRANSLATE_FORCE(parameters->level);
				break;
			case FF_SPRING:
				d1 = SCALE_VALUE_U16(((parameters->d1) + 0x8000) & 0xffff, 11);
				d2 = SCALE_VALUE_U16(((parameters->d2) + 0x8000) & 0xffff, 11);
				s1 = parameters->k1 < 0;
				s2 = parameters->k2 < 0;
				k1 = abs(parameters->k1);
				k2 = abs(parameters->k2);
				if (k1 < 2048) {
					d1 = 0;
				} else {
					k1 -= 2048;
				}
				if (k2 < 2048) {
					d2 = 2047;
				} else {
					k2 -= 2048;
				}
				slot->current_cmd[1] = 0x0b;
				slot->current_cmd[2] = d1 >> 3;
				slot->current_cmd[3] = d2 >> 3;
				slot->current_cmd[4] = (SCALE_COEFF(k2, 4) << 4) + SCALE_COEFF(k1, 4);
				slot->current_cmd[5] = ((d2 & 7) << 5) + ((d1 & 7) << 1) + (s2 << 4) + s1;
				slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
				break;
			case FF_DAMPER:
				s1 = parameters->k1 < 0;
				s2 = parameters->k2 < 0;
				slot->current_cmd[1] = 0x0c;
				slot->current_cmd[2] = SCALE_COEFF(parameters->k1, 4);
				slot->current_cmd[3] = s1;
				slot->current_cmd[4] = SCALE_COEFF(parameters->k2, 4);
				slot->current_cmd[5] = s2;
				slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
				break;
			case FF_FRICTION:
				s1 = parameters->k1 < 0;
				s2 = parameters->k2 < 0;
				slot->current_cmd[1] = 0x0e;
				slot->current_cmd[2] = SCALE_COEFF(parameters->k1, 8);
				slot->current_cmd[3] = SCALE_COEFF(parameters->k2, 8);
				slot->current_cmd[4] = SCALE_VALUE_U16(parameters->clip, 8);
				slot->current_cmd[5] = (s2 << 4) + s1;
				slot->current_cmd[6] = 0;
				break;
		}
	}

	if (memcmp(original_cmd, slot->current_cmd, sizeof(original_cmd))) {
		slot->is_updated = 1;
	}
}

void lg4ff_init_slots(struct lg4ff_device *device)
{
	struct lg4ff_effect_parameters parameters;
	uint8_t cmd[7] = {0};
	int i;

	// Set/unset fixed loop mode
	cmd[0] = 0x0d;
	//cmd[1] = fixed_loop ? 1 : 0;
	cmd[1] = 0;
	int ret = hid_write(device->hid_handle, cmd, 7);
	if(ret == -1){
		char err_buf[128];
		wcstombs(err_buf, hid_error(device->hid_handle), sizeof(err_buf));
		STDERR("failed sending fixed loop enable/disable command, %s\n", err_buf);
		exit(1);
	}

	memset(&device->states, 0, sizeof(device->states));
	memset(&device->slots, 0, sizeof(device->slots));
	memset(&parameters, 0, sizeof(parameters));

	device->slots[0].effect_type = FF_CONSTANT;
	device->slots[1].effect_type = FF_SPRING;
	device->slots[2].effect_type = FF_DAMPER;
	device->slots[3].effect_type = FF_FRICTION;

	for (i = 0; i < 4; i++) {
		device->slots[i].id = i;
		lg4ff_update_slot(&device->slots[i], &parameters);
		ret = hid_write(device->hid_handle, cmd, 7);
		if(ret == -1){
			char err_buf[128];
			wcstombs(err_buf, hid_error(device->hid_handle), sizeof(err_buf));
			STDERR("failed sending initial effect command, %s\n", err_buf);
			exit(1);
		}
		device->slots[i].is_updated = 0;
	}
}

int lg4ff_timer(struct lg4ff_device *device)
{
	struct lg4ff_slot *slot;
	struct lg4ff_effect_state *state;
	struct lg4ff_effect_parameters parameters[4];
	uint64_t now = get_time_ms();
	uint16_t gain;
	int32_t current_period;
	int32_t count;
	int32_t effect_id;
	int i;
	int32_t ffb_level;

	// XXX how to detect stacked up effects here?

	memset(parameters, 0, sizeof(parameters));

	gain = (uint32_t)device->gain * device->app_gain / 0xffff;

	count = device->effects_used;

	for (effect_id = 0; effect_id < LG4FF_MAX_EFFECTS; effect_id++) {

		if (!count) {
			break;
		}

		state = &device->states[effect_id];

		if (!test_bit(FF_EFFECT_STARTED, &state->flags)) {
			continue;
		}

		count--;

		if (test_bit(FF_EFFECT_ALLSET, &state->flags)) {
			if (state->effect.replay.length && time_after_eq(now, state->stop_at)) {
				STOP_EFFECT(state);
				if (!--state->count) {
					device->effects_used--;
					continue;
				}
				__set_bit(FF_EFFECT_STARTED, &state->flags);
				state->start_at = state->stop_at;
			}
		}

		lg4ff_update_state(state, now);

		if (!test_bit(FF_EFFECT_PLAYING, &state->flags)) {
			continue;
		}

		switch (state->effect.type) {
			case FF_CONSTANT:
				parameters[0].level += lg4ff_calculate_constant(state);
				break;
			case FF_RAMP:
				parameters[0].level += lg4ff_calculate_ramp(state);
				break;
			case FF_PERIODIC:
				parameters[0].level += lg4ff_calculate_periodic(state);
				break;
			case FF_SPRING:
				lg4ff_calculate_spring(state, &parameters[1]);
				break;
			case FF_DAMPER:
				lg4ff_calculate_resistance(state, &parameters[2]);
				break;
			case FF_FRICTION:
				lg4ff_calculate_resistance(state, &parameters[3]);
				break;
		}
	}

	parameters[0].level = (int64_t)parameters[0].level * gain / 0xffff;
	parameters[1].clip = parameters[1].clip * device->spring_level / 100;
	parameters[2].clip = parameters[2].clip * device->damper_level / 100;
	parameters[3].clip = parameters[3].clip * device->friction_level / 100;

	ffb_level = abs(parameters[0].level);
	for (i = 1; i < 4; i++) {
		parameters[i].k1 = (int64_t)parameters[i].k1 * gain / 0xffff;
		parameters[i].k2 = (int64_t)parameters[i].k2 * gain / 0xffff;
		parameters[i].clip = parameters[i].clip * gain / 0xffff;
		ffb_level += parameters[i].clip * 0x7fff / 0xffff;
	}
	if (ffb_level > device->peak_ffb_level) {
		device->peak_ffb_level = ffb_level;
	}

	for (i = 0; i < 4; i++) {
		slot = &device->slots[i];
		lg4ff_update_slot(slot, &parameters[i]);
		if (slot->is_updated) {
			int ret = hid_write(device->hid_handle, slot->current_cmd, 7);
			if(ret == -1){
				char err_buf[128];
				wcstombs(err_buf, hid_error(device->hid_handle), sizeof(err_buf));
				STDERR("failed sending effect command, %s\n", err_buf);
				exit(1);
			}
			slot->is_updated = 0;
		}
	}

	return 0;
}
