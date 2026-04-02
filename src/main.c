#include "config/config.h"
#include "config/internal/state.h"
#include "error.h"
#include "queue/queue.h"
#include "track.h"
#include "ui/cli.h"
#include "ui/cli_args.h"
#include "util/log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

int main(int argc, const char **argv) {
	printf("This is MPL v%s\n", MPL_VERSION);

	// Parse CLI args
	if (argc < 2) {
		fprintf(stderr, "usage: mpl [-v] [-vv] {file}\n");
		return 1;
	}
	CLI_args_parse(argc, argv);
	configure_av_log(); // Configure libav logging
	LOG(Verbosity_VERBOSE, "Logging enabled: %s\n", Verbosity_name(CLI_args.verbosity));

	// Parse mpl.conf
	Config config;
	char *config_path = Config_find_path();
	Config_parse(&config, config_path);
	free(config_path);

	int ret = 0;

	// Fire up CLI user interface and the main EventQueue
	UI_CLI ui;
	if (UI_CLI_init(&ui) != 0) {
		LOG(Verbosity_NORMAL, "Failed to initialize user interface, exiting\n");
		ret = 1;
		goto deinit_config;
	}

	// Form URL from file argv
	const char *file = argv[argc-1];
	static const char LIBAV_PROTO_FILE[] = "file:";
	static const size_t LIBAV_PROTO_FILE_LEN = sizeof(LIBAV_PROTO_FILE);
	const size_t url_len = LIBAV_PROTO_FILE_LEN + strlen(file);
	char *url = malloc((url_len + 1) * sizeof(char));
	snprintf(url, url_len, "%s%s", LIBAV_PROTO_FILE, file);

	// Initialize queue w/ track from argv
	Queue queue;
	if (Queue_init(&queue, &config.settings) != 0) {
		LOG(Verbosity_NORMAL, "Failed to initialize Queue, exiting\n");
		ret = 1;
		goto deinit_ui;
	}
	enum AudioBackend_ERR ab_err = Queue_connect_audio(&queue, &config.settings, ui.evt_queue);
	if (ab_err != AudioBackend_OK) {
		LOG(Verbosity_NORMAL, "Failed to connect AudioBackend: %s\n", AudioBackend_ERR_name(ab_err));
		ret = 1;
		goto deinit_queue;
	}
	if (Queue_prepend(&queue, Track_new(url, url_len)) != 0) {
		ret = 1;
		goto deinit_queue;
	}

	// Initialize configState so keybinds work
	configState_init(&queue, ui.evt_queue);

	ret = UI_CLI_mainloop(&ui, &queue, &config);

	// Cleanup
	// (The UI must outlive everything that can send it events, including the Queue and AudioBackend)
deinit_queue: // deinitialize queue, ui, and config
	LOG(Verbosity_DEBUG, "Deinitializing Queue\n");
	Queue_deinit(&queue);
	free(url);
deinit_ui: // deinitialize ui and config
	LOG(Verbosity_DEBUG, "Deinitializing UI\n");
	UI_CLI_deinit(&ui);
deinit_config: // deinitialize config
	LOG(Verbosity_DEBUG, "Deinitializing config\n");
	Config_deinit(&config);

	return ret;
}
