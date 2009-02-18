/***************************************************************************

	mac.c

	Sound handler

****************************************************************************/

#include "driver.h"
#include "includes/mac.h"
#include "streams.h"


static sound_stream *mac_stream;
static int sample_enable = 0;
static UINT16 *mac_snd_buf_ptr;


#define MAC_MAIN_SND_BUF_OFFSET	0x0300
#define MAC_ALT_SND_BUF_OFFSET	0x5F00
#define MAC_SND_BUF_SIZE		370			/* total number of scan lines */
#define MAC_SAMPLE_RATE			( MAC_SND_BUF_SIZE * 60 /*22255*/ )	/* scan line rate, should be 22254.5 Hz */


/* intermediate buffer */
#define SND_CACHE_SIZE 128

static UINT8 *snd_cache;
static int snd_cache_len;
static int snd_cache_head;
static int snd_cache_tail;


/************************************/
/* Stream updater                   */
/************************************/

static STREAM_UPDATE( mac_sound_update )
{
	INT16 last_val = 0;
	stream_sample_t *buffer = outputs[0];

	/* if we're not enabled, just fill with 0 */
	if (device->machine->sample_rate == 0)
	{
		memset(buffer, 0, samples * sizeof(*buffer));
		return;
	}

	/* fill in the sample */
	while (samples && snd_cache_len)
	{
		*buffer++ = last_val = ((snd_cache[snd_cache_head] << 8) ^ 0x8000) & 0xff00;
		snd_cache_head++;
		snd_cache_head %= SND_CACHE_SIZE;
		snd_cache_len--;
		samples--;
	}

	while (samples--)
	{	/* should never happen */
		*buffer++ = last_val;
	}
}



/************************************/
/* Sound handler start              */
/************************************/

static DEVICE_START(mac_sound)
{
	snd_cache = auto_malloc(SND_CACHE_SIZE * sizeof(*snd_cache));
	mac_stream = stream_create(device, 0, 1, MAC_SAMPLE_RATE, 0, mac_sound_update);
	snd_cache_head = snd_cache_len = snd_cache_tail = 0;
}



/*
	Set the sound enable flag (VIA port line)
*/
void mac_enable_sound(running_machine *machine, int on)
{
	sample_enable = on;
}



/*
	Set the current sound buffer (one VIA port line)
*/
void mac_set_sound_buffer(running_machine *machine, int buffer)
{
	if (buffer)
		mac_snd_buf_ptr = (UINT16 *) (mess_ram + mess_ram_size - MAC_MAIN_SND_BUF_OFFSET);
	else
		mac_snd_buf_ptr = (UINT16 *) (mess_ram + mess_ram_size - MAC_ALT_SND_BUF_OFFSET);
}



/*
	Set the current sound volume (3 VIA port line)
*/
void mac_set_volume(running_machine *machine, int volume)
{
	const device_config *device = devtag_get_device(machine, DEVICE_GET_INFO_NAME(mac_sound), "custom");

	stream_update(mac_stream);

	volume = (100 / 7) * volume;

	sound_set_output_gain(device, 0, volume / 100.0);
}



/*
	Fetch one byte from sound buffer and put it to sound output (called every scanline)
*/
void mac_sh_updatebuffer(running_machine *machine)
{
	static int indexx;
	UINT16 *base = mac_snd_buf_ptr;

	indexx++;
	indexx %= 370;

	if (snd_cache_len >= SND_CACHE_SIZE)
	{
		/* clear buffer */
		stream_update(mac_stream);
	}

	if (snd_cache_len >= SND_CACHE_SIZE)
		/* should never happen */
		return;

	snd_cache[snd_cache_tail] = sample_enable ? (base[indexx] >> 8) & 0xff : 0;
	snd_cache_tail++;
	snd_cache_tail %= SND_CACHE_SIZE;
	snd_cache_len++;
}


DEVICE_GET_INFO( mac_sound )
{
	switch (state)
	{
		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME(mac_sound);	break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case DEVINFO_STR_NAME:							strcpy(info->s, "Mac Sound Custom");				break;
		case DEVINFO_STR_SOURCE_FILE:					strcpy(info->s, __FILE__);						break;
	}
}
