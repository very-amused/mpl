#include "config/config.h"
#include "config/internal/state.h"
#include "error.h"
#include "track_queue/queue.h"
#include "track.h"
#include "ui/cli_args.h"
#include "util/log.h"
#include "ui/interface/interface.h"
#include "ui/interface/interfaces.h"

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
	args_parse(argc, argv);
	configure_av_log(); // Configure libav logging
	LOG(Verbosity_VERBOSE, "Logging enabled: %s\n", Verbosity_name(cli_args.verbosity));

	// Parse mpl.conf
	Config config;
	char *config_path = Config_find_path();
	Config_parse(&config, config_path);
	free(config_path);

	int ret = 0;

	// Fire up user interface and main EventQueue	
	UserInterface *ui = UI_Configured(&config.settings);
	enum UserInterface_ERR ui_err = UserInterface_init(ui, &config.settings);
	if (ui_err != UserInterface_OK) {
		LOG(Verbosity_NORMAL, "Failed to initialize user interface: %s\n", UserInterface_ERR_name(ui_err));
		ret = 1;
		goto deinit_config;
	}

	// Form URL from file argv
	const char *file = argv[argc-1];
	static const char LIBAV_PROTO_FILE[] = "file:";
	static const size_t LIBAV_PROTO_FILE_LEN = sizeof(LIBAV_PROTO_FILE);
	const size_t url_len = LIBAV_PROTO_FILE_LEN + strlen(file);
	char *url = malloc((url_len + 1) * sizeof(char));
	if (!url) goto deinit_config;
	snprintf(url, url_len, "%s%s", LIBAV_PROTO_FILE, file);

	// Initialize track queue
	TrackQueue queue;
	int queue_err = TrackQueue_init(&queue, &config.settings, ui->evt_queue);
	if (queue_err != 0) {
		LOG(Verbosity_NORMAL, "Failed to initialize track queue, exiting\n");
		ret = 1;
		goto deinit_ui;
	}
	// Connect audio
	enum AudioBackend_ERR ab_err = TrackQueue_connect_audio(&queue, &config.settings, ui->evt_queue);
	if (ab_err != AudioBackend_OK) {
		LOG(Verbosity_NORMAL, "Failed to connect AudioBackend: %s\n", AudioBackend_ERR_name(ab_err));
		ret = 1;
		goto deinit_queue;
	}
	queue_err = TrackQueue_prepend(&queue, Track_new(url, url_len));
	if (queue_err != 0) {
		ret = 1;
		goto deinit_queue;
	}

	// Initialize configState so keybinds work
	configState_init(&queue, ui->evt_queue);

	ui_err = UserInterface_mainloop(ui, &queue, &config);
	if (ui_err != UserInterface_OK) {
		LOG(Verbosity_NORMAL, "Error encountered in main UI loop: %s\n", UserInterface_ERR_name(ui_err));
		ret = 1;
	}

	// Cleanup
	// (The UI must outlive everything that can send it events, including the Queue and AudioBackend)
deinit_queue:
	LOG(Verbosity_DEBUG, "Deinitializing Queue\n");
	TrackQueue_deinit(&queue);
	free(url);
deinit_ui:
	LOG(Verbosity_DEBUG, "Deinitializing UI\n");
	UserInterface_deinit(ui);
deinit_config:
	LOG(Verbosity_DEBUG, "Deinitializing config\n");
	Config_deinit(&config);

	return ret;
}
