#include "src/cuberzero.h"
#include <dialogs/dialogs.h>
#include <gui/elements.h>
#include <storage/storage.h>

typedef struct {
	PCUBERZERO instance;
	ViewPort* viewport;
	FuriMessageQueue* queue;
	struct {
		uint8_t action : 1;
		uint8_t screen : 1;
		uint8_t button : 2;
	};
} SESSIONSCENE, *PSESSIONSCENE;

enum ACTION {
	ACTION_EXIT,
	ACTION_SELECT
};

enum SCREEN {
	SCREEN_SESSION,
	SCREEN_TEXT
};

enum BUTTONSESSION {
	BUTTON_SESSION_SELECT,
	BUTTON_SESSION_NEW,
	BUTTON_SESSION_DELETE,
	COUNT_BUTTON_SESSION
};

enum BUTTONTEXT {
	BUTTON_TEXT_SELECT,
	BUTTON_TEXT_OK,
	COUNT_BUTTON_TEXT
};

static inline void drawButton(Canvas* const canvas, const uint8_t x, const uint8_t y, const uint8_t pressed, const char* const text) {
	const uint16_t width = canvas_string_width(canvas, text);
	canvas_set_color(canvas, ColorBlack);
	canvas_draw_line(canvas, x + 1, y, x + width + 4, y);
	canvas_draw_line(canvas, x + 1, y + 12, x + width + 4, y + 12);
	canvas_draw_line(canvas, x, y + 1, x, y + 11);
	canvas_draw_line(canvas, x + width + 5, y + 1, x + width + 5, y + 11);

	if(pressed) {
		canvas_draw_box(canvas, x + 1, y + 1, width + 4, 11);
		canvas_set_color(canvas, ColorWhite);
	}

	canvas_draw_str(canvas, x + 3, y + 10, text);
}

// [Select] [Ok]     No session file was selected.
// [Select] [Open]   The selected file does not\nappear to be a session file.\nOpen anyway?
// [Select] [Ok]     The selected file\nis not a session file.\nIt might be corrupted.
// [Delete] [Cancel] Are you sure you want to\ndelete the current session?
static void callbackRender(Canvas* const canvas, void* const context) {
	furi_check(canvas && context);
	const PSESSIONSCENE instance = context;
	canvas_clear(canvas);

	switch(instance->screen) {
	case SCREEN_SESSION:
		canvas_set_font(canvas, FontPrimary);
		canvas_draw_str(canvas, 0, 8, "Current Session:");
		elements_text_box(canvas, 0, 11, 128, 39, AlignLeft, AlignTop, "Session 8192", 1);
		drawButton(canvas, 10, 51, instance->button == BUTTON_SESSION_SELECT, "Select");
		drawButton(canvas, 52, 51, instance->button == BUTTON_SESSION_NEW, "New");
		drawButton(canvas, 86, 51, instance->button == BUTTON_SESSION_DELETE, "Delete");
		break;
	case SCREEN_TEXT:
		elements_text_box(canvas, 0, 0, 128, 51, AlignCenter, AlignCenter, "Are you sure you want to\ndelete the current session?", 1);
		drawButton(canvas, 10, 51, instance->button == BUTTON_TEXT_SELECT, "Delete");
		drawButton(canvas, 70, 51, instance->button == BUTTON_TEXT_OK, "Cancel");
		break;
	}
}

static void callbackInput(InputEvent* const event, void* const context) {
	furi_check(event && context);

	if(event->type != InputTypeShort) {
		return;
	}

	const PSESSIONSCENE instance = context;

	switch(event->key) {
	case InputKeyUp:
	case InputKeyRight:
		switch(instance->screen) {
		case SCREEN_SESSION:
			instance->button = (instance->button + 1) % COUNT_BUTTON_SESSION;
			break;
		case SCREEN_TEXT:
			instance->button = (instance->button + 1) % COUNT_BUTTON_TEXT;
			break;
		}

		view_port_update(instance->viewport);
		return;
	case InputKeyDown:
	case InputKeyLeft:
		switch(instance->screen) {
		case SCREEN_SESSION:
			instance->button = (instance->button ? instance->button : COUNT_BUTTON_SESSION) - 1;
			break;
		case SCREEN_TEXT:
			instance->button = (instance->button ? instance->button : COUNT_BUTTON_TEXT) - 1;
			break;
		}

		view_port_update(instance->viewport);
		return;
	case InputKeyBack:
		switch(instance->screen) {
		case SCREEN_SESSION:
			instance->action = ACTION_EXIT;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			break;
		case SCREEN_TEXT:
			instance->screen = SCREEN_SESSION;
			instance->button = BUTTON_SESSION_SELECT;
			view_port_update(instance->viewport);
			break;
		}

		return;
	default:
		break;
	}

	switch(instance->screen) {
	case SCREEN_SESSION:
		switch(instance->button) {
		case BUTTON_SESSION_SELECT:
			instance->action = ACTION_SELECT;
			furi_message_queue_put(instance->queue, event, FuriWaitForever);
			break;
		}

		return;
	}
}

static void actionSelect(const PSESSIONSCENE instance, FuriString* const path) {
	DialogsFileBrowserOptions options;
	furi_string_set_str(path, APP_DATA_PATH("sessions"));
	memset(&options, 0, sizeof(DialogsFileBrowserOptions));
	options.skip_assets = dialog_file_browser_show(furi_record_open(RECORD_DIALOGS), path, path, &options);
	furi_record_close(RECORD_DIALOGS);

	if(!options.skip_assets) {
		instance->screen = SCREEN_TEXT;
		return;
	}

	instance->screen = SCREEN_TEXT;
	instance->button = BUTTON_TEXT_SELECT;
	view_port_update(instance->viewport);
}

void SceneSessionEnter(void* const context) {
	furi_check(context);
	const PSESSIONSCENE instance = malloc(sizeof(SESSIONSCENE));
	instance->instance = context;
	instance->viewport = view_port_alloc();
	instance->queue = furi_message_queue_alloc(1, sizeof(InputEvent));
	instance->screen = SCREEN_SESSION;
	instance->button = BUTTON_SESSION_SELECT;
	FuriString* const path = furi_string_alloc();
	view_port_draw_callback_set(instance->viewport, callbackRender, instance);
	view_port_input_callback_set(instance->viewport, callbackInput, instance);
	gui_remove_view_port(instance->instance->interface, instance->instance->dispatcher->viewport);
	gui_add_view_port(instance->instance->interface, instance->viewport, GuiLayerFullscreen);
	const InputEvent* event;

	while(furi_message_queue_get(instance->queue, &event, FuriWaitForever) == FuriStatusOk) {
		switch(instance->action) {
		case ACTION_EXIT:
			goto functionExit;
		case ACTION_SELECT:
			actionSelect(instance, path);
			break;
		}
	}
functionExit:
	gui_remove_view_port(instance->instance->interface, instance->viewport);
	gui_add_view_port(instance->instance->interface, instance->instance->dispatcher->viewport, GuiLayerFullscreen);
	furi_string_free(path);
	furi_message_queue_free(instance->queue);
	view_port_free(instance->viewport);
	free(instance);
	scene_manager_handle_back_event(((PCUBERZERO) context)->manager);
}
