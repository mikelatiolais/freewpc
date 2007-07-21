/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of FreeWPC.
 *
 * FreeWPC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * FreeWPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with FreeWPC; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <freewpc.h>

//#define PB_DEBUG


/* Proximity switches will trigger whenever a steel ball passes over
 * them.  The powerball is detected by the lack of such closures.
 */

/* Each of these represents a possible state for the powerball
 * detector.  They are mutually exclusive, but expressed as bitmasks
 * so that ranges can be tested more easily. */
#define PB_MISSING       0x0
#define PB_IN_LOCK       0x1
#define PB_IN_TROUGH     0x2
#define PB_IN_GUMBALL    0x4
#define PB_IN_PLAY       0x8
#define PB_MAYBE_IN_PLAY 0x10

#define PB_HELD         (PB_IN_LOCK | PB_IN_TROUGH | PB_IN_GUMBALL)
#define PB_KNOWN			(PB_HELD | PB_IN_PLAY)


static const audio_track_t pb_in_play_music = {
	.prio = PRI_GAME_MODE3,
	.code = MUS_POWERBALL_IN_PLAY,
};


typedef enum {
	PF_STEEL_DETECTED = 1,
	PF_PB_DETECTED,
	TROUGH_STEEL_DETECTED,
	TROUGH_PB_DETECTED,
} pb_event_t;

/** The general location of the powerball */
U8 pb_location;

/** If in lock or trough, the number of kicks before the 
 * powerball is back in play. */
U8 pb_depth;

/** Nonzero if the powerball has been detected and needs to be announced. */
U8 pb_announce_needed;

/** For debugging, this stores the last PB event that was asserted */
U8 last_pb_event;

#define switch_poll_trough_metal() switch_poll_logical(SW_TROUGH_PROXIMITY)

void pb_detect_deff (void)
{
#ifdef PB_DEBUG
	dmd_alloc_low_clean ();
	font_render_string_center (&font_mono5, 64, 8, "POWERBALL STATUS");

	if (pb_location == 0)
		sprintf ("MISSING");
	else if (pb_location & PB_IN_LOCK)
		sprintf ("LOCK %d", pb_depth);
	else if (pb_location & PB_IN_TROUGH)
		sprintf ("TROUGH %d", pb_depth);
	else if (pb_location & PB_IN_GUMBALL)
		sprintf ("GUMBALL %d", pb_depth);
	else if (pb_location & PB_IN_PLAY)
		sprintf ("IN PLAY");
	font_render_string_center (&font_mono5, 64, 15, sprintf_buffer);

	sprintf ("LAST EVENT %d", last_pb_event);
	font_render_string_center (&font_mono5, 64, 22, sprintf_buffer);
	
	dmd_show_low ();
	task_sleep_sec (2);
#else
	dmd_alloc_low_clean ();
	font_render_string_center (&font_fixed10, 64, 7, "POWERBALL");
	dmd_sched_transition (&trans_vstripe_right2left);
	sound_send (SND_POWERBALL_QUOTE);
	dmd_show_low ();
	task_sleep (TIME_500MS);
	dmd_alloc_high ();
	dmd_copy_low_to_high ();
	dmd_invert_page (dmd_low_buffer);
	deff_swap_low_high (32, TIME_100MS);
	dmd_sched_transition (&trans_scroll_up);
#endif
	deff_exit ();
}


void pb_loop_deff (void)
{
	generic_twoline_deff ("POWERBALL LOOP", "10,000,000");
}


/** Called when the powerball is known to be in a particular location.
 *
 * PB_IN_PLAY is used when it is guaranteed to be in play, even during
 * multiball.
 *
 * PB_MAYBE_IN_PLAY is used when it might be in play, but it is uncertain.
 * It *was* in play prior to some event during multiball.
 */
void pb_set_location (U8 location, U8 depth)
{
	if (pb_location != location)
	{
		pb_location = location;
		if (pb_location & PB_HELD)
		{
			pb_depth = depth;
			pb_announce_needed = 0;
		}
		else if (pb_location & PB_IN_PLAY)
		{
			lamp_tristate_flash (LM_LEFT_POWERBALL);
			lamp_tristate_flash (LM_RIGHT_POWERBALL);
			flag_on (FLAG_POWERBALL_IN_PLAY);
			pb_announce_needed = 1;
		}
		else if (pb_location & PB_MAYBE_IN_PLAY)
		{
			lamp_tristate_on (LM_LEFT_POWERBALL);
			lamp_tristate_on (LM_RIGHT_POWERBALL);
			flag_off (FLAG_POWERBALL_IN_PLAY);
			pb_announce_needed = 0;
			/* TODO - in the 'maybe' state, pulse magnets to
			figure out the ball type */
		}
	}
}

/** Called when the powerball is known *NOT* to be in a particular 
 * location. */
void pb_clear_location (U8 location)
{
	if (pb_location == location)
	{
		pb_location = PB_MISSING;
		lamp_tristate_off (LM_LEFT_POWERBALL);
		lamp_tristate_off (LM_RIGHT_POWERBALL);
		flag_off (FLAG_POWERBALL_IN_PLAY);
		pb_announce_needed = 0;
		/* TODO : music is not being stopped correctly if Powerball
		drains during multiball and game doesn't know where it is. */
		music_stop (pb_in_play_music);
		deff_stop (DEFF_PB_DETECT);
	}
}


/** Asserts a powerball detection event.  The significance depends on
 * the current state. */
void pb_detect_event (pb_event_t event)
{
	last_pb_event = event;
	switch (event)
	{
		/* Steel ball detected on playfield, via Slot Proximity */
		case PF_STEEL_DETECTED:
			if (live_balls == 1)
				pb_clear_location (PB_IN_PLAY);
#ifdef PB_DEBUG
			else
				pb_clear_location (0);
#endif
			break;

		/* Powerball detected on playfield, via Slot Proximity */
		case PF_PB_DETECTED:
			pb_set_location (PB_IN_PLAY, 0);
			break;

		case TROUGH_STEEL_DETECTED:
			if (device_entry (DEVNO_TROUGH)->actual_count == 1)
				pb_clear_location (PB_IN_TROUGH);
#ifdef PB_DEBUG
			else
				pb_clear_location (0);
#endif
			break;

		case TROUGH_PB_DETECTED:
			pb_set_location (PB_IN_TROUGH, 1);
			break;
	}
}

void pb_announce (void)
{
	if (pb_announce_needed)
	{
		/* At ball start, the important score screen may pre-empt us.
		TODO : better deff management would eliminate the need for
		this. */
		deff_wait_for_other (DEFF_SCORES_IMPORTANT);

		deff_start (DEFF_PB_DETECT);
		music_start (pb_in_play_music);
		pb_announce_needed = 0;
	}
}

void pb_poll_trough_task (void)
{
	task_sleep (TIME_200MS);
	if (switch_poll_logical (SW_RIGHT_TROUGH))
	{
		if (switch_poll_trough_metal ())
		{
			pb_detect_event (TROUGH_STEEL_DETECTED);
		}
		else
		{
			pb_detect_event (TROUGH_PB_DETECTED);
		}
	}
	task_exit ();
}


void pb_poll_trough (void)
{
	task_recreate_gid (GID_PB_POLL_TROUGH, pb_poll_trough_task);
}


/** Called when a ball enters the trough or the lock. */
void pb_container_enter (U8 location, U8 devno)
{
	device_t *dev = device_entry (devno);

	/* If the powerball is known to be in play, then the act
	of a ball entering a device is significant. */
	if (pb_location == PB_IN_PLAY)
	{
		if (live_balls <= 1)
		{
			/* In single ball play, things are fairly deterministic.
			 * We know the powerball is not in play, and it is in
			 * the device it just entered at a specific location.
			 *
			 * It is also possible that we might kick out the 
			 * ball immediately from the same device.  Changing
			 * the state here and then back again will cause the
			 * powerball to be reannounced.  So optimize this slightly
			 * and don't change anything.
			 */
			if (dev->actual_count <= dev->max_count)
			{
				pb_clear_location (PB_IN_PLAY);
				pb_set_location (location, dev->actual_count);
			}
			else
			{
			}
		}
		else
		{
			/* In multiball, container enter might mean the
			powerball entered it or not... we just don't
			know */
			pb_set_location (PB_MAYBE_IN_PLAY, 0);
		}
	}
	else
	{
		/* If balls is not known to be in play, then a container enter
		event isn't important.  (Either it's in a device somewhere or
		we're in the "maybe" state.) */
	}
}


/** Called when a ball exits the trough or the lock. */
void pb_container_exit (U8 location)
{
	if (pb_location == location)
	{	
		if (--pb_depth == 0)
		{
			pb_set_location (PB_IN_PLAY, 0);
			pb_announce ();
		}
	}

}

CALLSET_ENTRY (pb_detect, sw_camera)
{
	event_can_follow (camera_or_piano, slot_prox, TIME_5S);
}

CALLSET_ENTRY (pb_detect, sw_piano)
{
	event_can_follow (camera_or_piano, slot_prox, TIME_5S);
}

CALLSET_ENTRY (pb_detect, sw_slot_proximity)
{
	event_did_follow (camera_or_piano, slot_prox);
	pb_detect_event (PF_STEEL_DETECTED);
}

CALLSET_ENTRY (pb_detect, sw_trough_proximity)
{
	pb_poll_trough ();
}

CALLSET_ENTRY (pb_detect, dev_slot_enter)
{
	if (event_did_follow (camera_or_piano, slot_prox))
	{
		/* Proximity sensor did not trip ; must be the powerball */
		pb_detect_event (PF_PB_DETECTED);
	}
	pb_announce ();
}

CALLSET_ENTRY (pb_detect, dev_trough_enter)
{
	/* Note: during the call to enter, live_balls has not been updated
	and reflects the value prior to the ball entering the device. */
	pb_container_enter (PB_IN_TROUGH, DEVNO_TROUGH);
}

CALLSET_ENTRY (pb_detect, dev_lock_enter)
{
	pb_container_enter (PB_IN_LOCK, DEVNO_LOCK);
}

CALLSET_ENTRY (pb_detect, dev_trough_kick_attempt)
{
	pb_poll_trough ();
}

CALLSET_ENTRY (pb_detect, dev_trough_kick_success)
{
	pb_container_exit (PB_IN_TROUGH);
	pb_poll_trough ();
}

CALLSET_ENTRY (pb_detect, dev_lock_kick_success)
{
	pb_container_exit (PB_IN_LOCK);
}


CALLSET_ENTRY (pb_detect, start_ball)
{
	pb_clear_location (PB_IN_PLAY);
}


CALLSET_ENTRY (pb_detect, init)
{
	pb_location = 0;
	last_pb_event = 0;
}

