#include <stdlib.h>

#include "force_feedback.h"
#include "logging.h"

#define STR(s) #s

static void log_envelope(struct ff_envelope *e){
}

void log_effect(struct ff_effect *e){
	const char *effect_name = NULL;
	#define EFFECT_NAME(n) case FF_##n: effect_name = STR(n); break;
	switch(e->type){
		EFFECT_NAME(RUMBLE);
		EFFECT_NAME(PERIODIC);
		EFFECT_NAME(CONSTANT);
		EFFECT_NAME(SPRING);
		EFFECT_NAME(FRICTION);
		EFFECT_NAME(DAMPER);
		EFFECT_NAME(INERTIA);
		EFFECT_NAME(RAMP);
		default:
			STDERR("unknown effect type 0x%04x\n", e->type);
			exit(1);
	}
	#undef EFFECT_NAME
	STDOUT("--effect %s, id %d--\n", effect_name, e->id);
	STDOUT("direction: 0x%04x\n", e->direction);
	STDOUT("trigger: button 0x%04x, interval %d\n", e->trigger.button, e->trigger.interval);
	STDOUT("replay: length %d, delay %d\n", e->replay.length, e->replay.delay);

	#define LOG_ENVELOPE(_e) STDOUT("envelope: attack length %d, attack level %d, fade length %d, fade level %d\n", _e.attack_length, _e.attack_level, _e.fade_length, _e.fade_level);

	switch(e->type){
		case FF_RUMBLE:
			STDOUT("rumble: strong magnitude %d, weak magnitude %d\n", e->u.rumble.strong_magnitude, e->u.rumble.weak_magnitude);
			break;
		case FF_PERIODIC:
			const char *periodic_name = 0;
			#define PERIODIC_NAME(n) case FF_##n: periodic_name = STR(n); break;
			switch(e->u.periodic.waveform){
				PERIODIC_NAME(SQUARE);
				PERIODIC_NAME(TRIANGLE);
				PERIODIC_NAME(SINE);
				PERIODIC_NAME(SAW_UP);
				PERIODIC_NAME(SAW_DOWN);
				PERIODIC_NAME(CUSTOM);
				default:
					STDERR("unknown waveform 0x%04x\n", e->u.periodic.waveform)
					exit(1);
			}
			#undef PERIODIC_NAME
			STDOUT("periodic: waveform %s, period %d, magnitude %d, offset %d, phase %d, custom len %d, custom 0x%016x\n", periodic_name, e->u.periodic.period, e->u.periodic.magnitude, e->u.periodic.offset, e->u.periodic.phase, e->u.periodic.custom_len, e->u.periodic.custom_data);
			LOG_ENVELOPE(e->u.periodic.envelope);
			break;
		case FF_CONSTANT:
			STDOUT("constant: level %d\n", e->u.constant.level);
			LOG_ENVELOPE(e->u.constant.envelope);
			break;
		case FF_SPRING:
		case FF_FRICTION:
		case FF_DAMPER:
		case FF_INERTIA:
			for(int i = 0; i < 2; i++){
				STDOUT("condition #%d: right saturation %d, left saturation %d, right coeff %d, left coeff %d, deadband %d, center %d\n", i, e->u.condition[i].right_saturation, e->u.condition[i].left_saturation, e->u.condition[i].right_coeff, e->u.condition[i].left_coeff, e->u.condition[i].deadband, e->u.condition[i].center);
			}
			break;
		case FF_RAMP:
			STDOUT("ramp: start level %d, end level %d\n", e->u.ramp.start_level, e->u.ramp.end_level);
			LOG_ENVELOPE(e->u.ramp.envelope);
			break;
	}

	#undef LOG_ENVELOPE
}
