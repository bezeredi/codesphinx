/**
 * Initial pocketsphinx test.
 *
 * Code sample from their website at:
 * 	http://cmusphinx.sourceforge.net/wiki/tutorialpocketsphinx
 **/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include "pocketsphinx.h"


/* Local variables */
static jmp_buf jbuf;
static cmd_ln_t *config;
static ps_decoder_t *decoder;
static const arg_t cont_args_def[] = {
	POCKETSPHINX_OPTIONS,
	/* Argument file. */
	{
		"-argfile",
		ARG_STRING,
		NULL,
		"Argument file giving extra arguments."
	},
	{
		"-adcdev",
		ARG_STRING,
		NULL,
		"Name of audio device to use for input."
	},
	{
		"-infile",
		ARG_STRING,
		NULL,
		"Audio file to transcribe."
	},
	{
		"-time",
		ARG_BOOLEAN,
		"no",
		"Print word times in file transcription."
	},
	CMDLN_EMPTY_OPTION
};


/* Local functions */
static void recognize_from_microphone(void);
static void sighandler(int signo);
static void sleep_msec(int32 ms);


/**
 * Program entrance point.
 **/
int main(int argc, char *argv[]) {
	char const *cfg;

	if(argc == 2)
		config = cmd_ln_parse_file_r(NULL, cont_args_def, argv[1], TRUE);
	else
		config = cmd_ln_parse_r(NULL, cont_args_def, argc, argv, FALSE);
	
   /* Handle argument file as -argfile. */
   if(config && (cfg = cmd_ln_str_r(config, "-argfile")) != NULL)
		config = cmd_ln_parse_file_r(config, cont_args_def, cfg, FALSE);


	/* Initialize the decoder */
	decoder = ps_init(config);

	if(!decoder){
		cmd_ln_free_r(config);
		exit(EXIT_FAILURE);
	}


	/**
	 * Begin reading from the microphone.
	 **/
	
	/* Make sure we exit cleanly (needed for profiling among other things) */
	/* Signals seem to be broken in arm-wince-pe. */
#if !defined(GNUWINCE) && !defined(_WIN32_WCE) && !defined(__SYMBIAN32__)
	signal(SIGINT, &sighandler);
#endif

	if(setjmp(jbuf) == 0)
		recognize_from_microphone();

	cmd_ln_free_r(config);
	ps_free(decoder);
	exit(EXIT_SUCCESS);
}


/**
 * Recognize words from the microphone.
 **/
static void recognize_from_microphone(void){
	ad_rec_t *ad;
	int16 adbuf[4096];
	int32 k, ts, rem;
	char const *hyp;
	char const *uttid;
	cont_ad_t *cont;
	char word[256];

	if ((ad = ad_open_dev(cmd_ln_str_r(config, "-adcdev"),
			(int)cmd_ln_float32_r(config, "-samprate"))) == NULL)
		E_FATAL("Failed to open audio device\n");

	/* Initialize continuous listening module */
	if ((cont = cont_ad_init(ad, ad_read)) == NULL)
		E_FATAL("Failed to initialize voice activity detection\n");
	if (ad_start_rec(ad) < 0)
		E_FATAL("Failed to start recording\n");
	if (cont_ad_calib(cont) < 0)
		E_FATAL("Failed to calibrate voice activity detection\n");

	for (;;) {
		/* Indicate listening for next utterance */
		printf("READY....\n");
//		fflush(stdout);
//		fflush(stderr);

		/* Wait data for next utterance */
		while ((k = cont_ad_read(cont, adbuf, 4096)) == 0)
			sleep_msec(0);

		if (k < 0)
			E_FATAL("Failed to read audio\n");

		/*
		 * Non-zero amount of data received; start recognition of new utterance.
		 * NULL argument to uttproc_begin_utt => automatic generation of utterance-id.
		 */
		if (ps_start_utt(decoder, NULL) < 0)
			E_FATAL("Failed to start utterance\n");
		ps_process_raw(decoder, adbuf, k, FALSE, FALSE);
		printf("Listening...\n");
		fflush(stdout);

		/* Note timestamp for this first block of data */
		ts = cont->read_ts;

		/* Decode utterance until end (marked by a "long" silence, >1sec) */
		for (;;) {
			/* Read non-silence audio data, if any, from continuous listening module */
			if ((k = cont_ad_read(cont, adbuf, 4096)) < 0)
				E_FATAL("Failed to read audio\n");
			if (k == 0) {
				/*
				 * No speech data available; check current timestamp with most recent
				 * speech to see if more than 1 sec elapsed.  If so, end of utterance.
				 */
				if ((cont->read_ts - ts) > DEFAULT_SAMPLES_PER_SEC)
					break;
			} else {
				/* New speech data received; note current timestamp */
				ts = cont->read_ts;
			}

			/*
			 * Decode whatever data was read above.
			 */
			rem = ps_process_raw(decoder, adbuf, k, FALSE, FALSE);

			/* If no work to be done, sleep a bit */
			if ((rem == 0) && (k == 0))
				sleep_msec(0);
		}

		/*
		 * Utterance ended; flush any accumulated, unprocessed A/D data and stop
		 * listening until current utterance completely decoded
		 */
		ad_stop_rec(ad);
		while (ad_read(ad, adbuf, 4096) >= 0);
		cont_ad_reset(cont);

		printf("Stopped listening, please wait...\n");
		fflush(stdout);
		/* Finish decoding, obtain and print result */
		ps_end_utt(decoder);
		hyp = ps_get_hyp(decoder, NULL, &uttid);
		printf("%s: %s\n", uttid, hyp);
		fflush(stdout);

		/* Exit if the first word spoken was GOODBYE */
		if (hyp) {
			sscanf(hyp, "%s", word);
			if (strcmp(word, "goodbye") == 0)
				break;
		}

		/* Resume A/D recording for next utterance */
		if (ad_start_rec(ad) < 0)
			E_FATAL("Failed to start recording\n");
	}

	cont_ad_close(cont);
	ad_close(ad);
}


/**
 * Set signals.
 **/
static void sighandler(int signo){
	longjmp(jbuf, 1);
}


/**
 * Sleep for specified msec
 *
 * @param ms - the number of mseconds to sleep.
 **/
static void sleep_msec(int32 ms){
#if (defined(WIN32) && !defined(GNUWINCE)) || defined(_WIN32_WCE)
	Sleep(ms);
#else
	/* ------------------- Unix ------------------ */
	struct timeval tmo;

	tmo.tv_sec = 0;
	tmo.tv_usec = ms * 1000;

	select(0, NULL, NULL, NULL, &tmo);
#endif
}

