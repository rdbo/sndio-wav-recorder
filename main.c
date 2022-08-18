/*
 * SND is not a real format (at least not that I know of)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndio.h>

void
print_par_info(struct sio_par *par)
{
	printf("\tBits per Sample: %u\n"
		"\tBytes per Sample: %u\n"
		"\tRecording Channels: %u\n"
		"\tRate: %u\n",
		par->bits, par->bps, par->rchan, par->rate
	);
}

struct wav_hdr {
	char		magic[4];
	unsigned int	file_size;
	char		type[4];
	char		fmt_marker[4];
	unsigned int	fmt_size;
	unsigned short	fmt_type;
	unsigned short	nchannels;
	unsigned int	sample_rate;
	unsigned int	buffer_size; /* sample_rate * frame_size */
	unsigned short	frame_size; /* (sample_bits / 8) * channels */
	unsigned short	sample_bits; /* bits per sample */
	char		data_marker[4];
	unsigned int	data_size; /* audio data size (total size - header size) */
};

void
initwavhdr(struct wav_hdr *hdr, struct sio_par *pardat)
{
	strncpy(hdr->magic, "RIFF", sizeof(hdr->magic));
	hdr->file_size = 0;
	strncpy(hdr->type, "WAVE", sizeof(hdr->type));
	strncpy(hdr->fmt_marker, "fmt ", sizeof(hdr->fmt_marker));
	hdr->fmt_size = 0x10;
	hdr->fmt_type = 0x01;
	hdr->nchannels = pardat->rchan;
	hdr->sample_rate = pardat->rate;

	hdr->sample_bits = pardat->bits;
	hdr->frame_size = (hdr->sample_bits / 8) * hdr->nchannels;
	hdr->buffer_size = hdr->sample_rate * hdr->frame_size;

	strncpy(hdr->data_marker, "data", sizeof(hdr->data_marker));
	hdr->data_size = 0;
}

void
print_wav_hdr(struct wav_hdr *hdr)
{
	printf("\tMagic: %c%c%c%c\n", hdr->magic[0], hdr->magic[1],
		hdr->magic[2], hdr->magic[3]);
	printf("\tFile Size: %u\n", hdr->file_size);
	printf("\tFile Type: %c%c%c%c\n", hdr->type[0], hdr->type[1],
		hdr->type[2], hdr->type[3]);
	printf("\tFormat Marker: %c%c%c%c\n", hdr->fmt_marker[0], hdr->fmt_marker[1],
		hdr->fmt_marker[2], hdr->fmt_marker[3]);
	printf("\tFormat Size: %u\n", hdr->fmt_size);
	printf("\tFormat Type: %hu\n", hdr->fmt_type);
	printf("\tChannels: %hu\n", hdr->nchannels);
	printf("\tSample Rate: %u\n", hdr->sample_rate);
	printf("\tSample Bits: %hu\n", hdr->sample_bits);
	printf("\tFrame Size: %u\n", hdr->frame_size);
	printf("\tBuffer Size: %u\n", hdr->buffer_size);
	printf("\tData Marker: %c%c%c%c\n", hdr->data_marker[0], hdr->data_marker[1],
		hdr->data_marker[2], hdr->data_marker[3]);
	printf("\tData Size: %u\n", hdr->data_size);
}

int
main()
{
	int retval = 0;
	struct sio_hdl	*handle;
	struct sio_par	pardat, devpardat;
	FILE		*recfile;
	const char	*recfilepath = "test.wav";
	void		*audiobuf;
	size_t		audiobufsiz;
	size_t		reclen_secs = 5;
	size_t		recrate = 100; /* update every 10ms (same as 1s / 100) */
	size_t		i, j;
	struct wav_hdr	wav_header;

	printf("Opening handle to input device...\n");
	handle = sio_open(SIO_DEVANY, SIO_REC, 0);
	if (handle == NULL) {
		perror("Unable to open handle to input device");
		retval = -1;
		goto EXIT;
	}

	recfile = fopen(recfilepath, "w");
	if (!recfile) {
		perror("Unable to open output file");
		retval = -2;
		goto CLEAN_EXIT0;
	}

	sio_getpar(handle, &devpardat);
	printf("Input Device Info:\n");
	print_par_info(&devpardat);
	printf("\n");

	sio_initpar(&pardat);
	pardat.bits = devpardat.bits;
	pardat.bps = devpardat.bps;
	pardat.rchan = 2;
	pardat.rate = 44100;
	printf("Recording Settings:\n");
	print_par_info(&pardat);
	printf("\n");

	initwavhdr(&wav_header, &pardat);
	fwrite((void *)&wav_header, sizeof(wav_header), 1, recfile);

	audiobufsiz = pardat.bps * pardat.rchan * (pardat.rate / recrate);
	audiobuf = malloc(audiobufsiz);
	if (audiobuf == NULL) {
		perror("Unable to allocate buffer for audio recording");
		retval = -3;
		goto CLEAN_EXIT1;
	}

	sio_setpar(handle, &pardat);
	sio_start(handle);

	printf("Recording started...\n");
	for (i = 0; i < reclen_secs * recrate; ++i) {
		sio_read(handle, audiobuf, audiobufsiz);
		/* WAV files use (bits / 8) bytes per sample,
		 * not the closest power of two */
		for (j = 0; j < audiobufsiz; j += pardat.bps) {
			fwrite((void *)&((char *)audiobuf)[j], 1,
				(wav_header.sample_bits / 8), recfile);
		}
	}
	printf("Recording stopped\n");

	wav_header.data_size = i * (j / pardat.bps) * (wav_header.sample_bits / 8);
	wav_header.file_size = sizeof(wav_header) + wav_header.data_size;
	printf("WAV Header:\n");
	print_wav_hdr(&wav_header);
	printf("\n");

	/* Rewrite the file header with the new 'data_size'
	 * and 'file_size' information */
	fseek(recfile, 0, SEEK_SET);
	fwrite((void *)&wav_header, sizeof(wav_header), 1, recfile);

	sio_stop(handle);
	free(audiobuf);
CLEAN_EXIT1:
	fclose(recfile);
CLEAN_EXIT0:
	sio_close(handle);
EXIT:
	return retval;
}
