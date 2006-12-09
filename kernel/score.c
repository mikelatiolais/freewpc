/*
 * Copyright 2006 by Brian Dominy <brian@oddchange.com>
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

/**
 * \file
 * \brief Routines for doing math on scores
 */

#include <freewpc.h>

#ifndef USE_MD
static const score_t score_table[] = {
	[SC_10]      = { 0x00, 0x00, 0x00, 0x10 },
	[SC_100]     = { 0x00, 0x00, 0x01, 0x00 },
	[SC_500]     = { 0x00, 0x00, 0x05, 0x00 },
	[SC_1K]      = { 0x00, 0x00, 0x10, 0x00 },
	[SC_2500]    = { 0x00, 0x00, 0x25, 0x00 },
	[SC_5K]      = { 0x00, 0x00, 0x50, 0x00 },
	[SC_5130]    = { 0x00, 0x00, 0x51, 0x30 },
	[SC_10K]     = { 0x00, 0x01, 0x00, 0x00 },
	[SC_15K]     = { 0x00, 0x01, 0x50, 0x00 },
	[SC_20K]     = { 0x00, 0x02, 0x00, 0x00 },
	[SC_25K]     = { 0x00, 0x02, 0x50, 0x00 },
	[SC_30K]     = { 0x00, 0x03, 0x00, 0x00 },
	[SC_40K]     = { 0x00, 0x03, 0x00, 0x00 },
	[SC_50K]     = { 0x00, 0x05, 0x00, 0x00 },
	[SC_75K]     = { 0x00, 0x07, 0x50, 0x00 },
	[SC_100K]    = { 0x00, 0x10, 0x00, 0x00 },
	[SC_200K]    = { 0x00, 0x20, 0x00, 0x00 },
	[SC_250K]    = { 0x00, 0x25, 0x00, 0x00 },
	[SC_500K]    = { 0x00, 0x50, 0x00, 0x00 },
	[SC_750K]    = { 0x00, 0x75, 0x00, 0x00 },
	[SC_1M]      = { 0x01, 0x00, 0x00, 0x00 },
	[SC_2M]      = { 0x02, 0x00, 0x00, 0x00 },
	[SC_3M]      = { 0x03, 0x00, 0x00, 0x00 },
	[SC_4M]      = { 0x04, 0x00, 0x00, 0x00 },
	[SC_5M]      = { 0x05, 0x00, 0x00, 0x00 },
	[SC_10M]     = { 0x10, 0x00, 0x00, 0x00 },
	[SC_20M]     = { 0x20, 0x00, 0x00, 0x00 },
	[SC_30M]     = { 0x30, 0x00, 0x00, 0x00 },
	[SC_40M]     = { 0x40, 0x00, 0x00, 0x00 },
	[SC_50M]     = { 0x50, 0x00, 0x00, 0x00 },
};
#else
extern const score_t score_table[];
#endif


U8 score_change;
score_t scores[MAX_PLAYERS];
U8 *current_score;
score_t bonus_score;
U8 debug_value;


void scores_draw_ball (void)
{
#if defined (CONFIG_TIMED_GAME)
	U8 time_minutes, time_seconds;
	time_minutes = 0;
	time_seconds = timed_game_timer;
	while (time_seconds >= 60)
	{
		time_minutes++;
		time_seconds -= 60;
	}
	sprintf ("TIME REMAINING: %d:%02d", time_minutes, time_seconds);
	font_render_string_center (&font_mono5, 64, 26, sprintf_buffer);
#else
#if defined(SHOW_DEBUG_VALUE)
	sprintf ("DEBUG %d", debug_value);
#else
	sprintf ("PLAYER %1i", player_up);
#endif
	font_render_string_center (&font_mono5, 96, 26, sprintf_buffer);
	sprintf ("BALL %1i", ball_up);
	font_render_string_center (&font_mono5, 32, 26, sprintf_buffer);
#endif
}


void scores_draw_credits (void)
{
	credits_render ();
	font_render_string_center (&font_mono5, 64, 27, sprintf_buffer);
}


void scores_draw_current (void)
{
	sprintf ("%8b", current_score);
	font_render_string_center (&font_lucida9, 64, 10, sprintf_buffer);
}


void scores_draw (void)
{
	if (in_game)
		scores_draw_ball ();
	else
		scores_draw_credits ();
	scores_draw_current ();
}

void scores_deff (void) __taskentry__
{
	for (;;)
	{
		/* Clear score change flag */
		score_change = 0;

		/* Stop any score effects (i.e. flashing) */

		/* Redraw the scores on the screen */
		dmd_alloc_low_high ();
		dmd_clean_page_low ();
		scores_draw_ball ();
		dmd_copy_low_to_high ();
		scores_draw_current ();
		dmd_show_low ();
		
		/* Restart score effects */

		/* Wait for a score change */
		while (score_change == 0)
		{
			task_sleep (ball_in_play ? TIME_500MS : TIME_100MS);
			dmd_show_other ();
			task_sleep (ball_in_play ? TIME_200MS : TIME_100MS);
			dmd_show_other ();
		}
	}
}


void score_zero (score_t *s)
{
	memset (s, 0, sizeof (score_t));
}


void score_add (bcd_t *s1, const bcd_t *s2, U8 _len)
{
	register bcd_t *bcd1 = s1;
	register const bcd_t *bcd2 = s2;
	register U8 len = _len;

	bcd1 += len-1;
	bcd2 += len-1;

#ifdef __m6809__
	/* TODO - move into m6809/m6809.h */
	asm volatile ("lda\t%0" :: "m" (*bcd1));
	asm volatile ("adda\t%0" :: "m" (*bcd2));
	asm volatile ("daa");
	asm volatile ("sta\t%0" :: "m" (*bcd1));
#else
	fatal (ERR_NOT_IMPLEMENTED_YET);
#endif
	bcd1--;
	bcd2--;
  	len--;

	while (len > 0)
	{
	  /* TODO : possible compiler optimization could
		* be done here. (in older implementation)
		* stb ,x; leax -1,x => stb ,-x
		*/
#ifdef __m6809__
		asm volatile ("lda\t%0" :: "m" (*bcd1));
		asm volatile ("adca\t%0" :: "m" (*bcd2));
		asm volatile ("daa");
		asm volatile ("sta\t%0" :: "m" (*bcd1));
#else
	fatal (ERR_NOT_IMPLEMENTED_YET);
#endif
		bcd1--;
		bcd2--;
		len--;
	}
}


void score_add_current (const bcd_t *s)
{
	if (!in_live_game)
	{
		nonfatal (ERR_SCORE_NOT_IN_GAME);
		return;
	}

	score_add (current_score, s, sizeof (score_t));
	score_change++;
	/* TODO : check replay */
}


void score (score_id_t id)
{
	score_add_current (score_table[id]);
}


void score_multiple (score_id_t id, U8 multiplier)
{
#if 0
	score_t mult_score;
	const bcd_t *base_score = score_table[id];
	S8 i;

	/* TODO - calculate the multiplied score */
	for (i=sizeof(score_t)-1; i >=0 ; --i)
	{
		mult_score[i] = base_score[i] * multiplier;
	}

	score_add_current (&mult_score);
#else
	if (!in_live_game)
		return;

	while (multiplier > 0)
	{
		score_add (current_score, score_table[id], sizeof (score_t));
		multiplier--;
	}
	score_change++;
#endif
}


/* score[sizeof(score_t)-1] = ones
 * score[sizeof(score_t)-2] = hundreds
 * score[sizeof(score_t)-3] = ten thousands
 * score[sizeof(score_t)-4] = millions
 */
void score_add_millions_current (U8 mils)
{
	score_add (current_score + sizeof (score_t) - 4, &mils, sizeof (U8));
}


void score_sub (score_t s1, const score_t s2)
{
}

void score_mul (score_t s1, uint8_t multiplier)
{
	register U8 off = sizeof (score_t);
	register bcd_t *bcd = s1 + off - 1;
	bcd_t overflow = 0;
}


I8 score_compare (const score_t s1, const score_t s2)
{
	register U8 len = sizeof (score_t);

	while (len > 0)
	{
		if (*s1 > *s2)
			return (1);
		else if (*s1 < *s2)
			return (-1);
		else
		{
			s1++;
			s2++;
			len--;
		}
	}
	return (0);
}


void scores_reset (void)
{
	score_change = 0;
	memset ((U8 *)scores, 0, sizeof (scores));
	current_score = &scores[0][0];
}


void score_init (void)
{
	scores_reset ();
}


