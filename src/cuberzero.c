#include "cuberzero.h"

static bool callbackEmptyEvent(void* const context, const SceneManagerEvent event) {
	UNUSED(context);
	UNUSED(event);
	return false;
}

static void callbackEmptyExit(void* const context) {
	UNUSED(context);
}

static bool callbackCustomEvent(const PCUBERZERO instance, const uint32_t event) {
	if(!instance) {
		return false;
	}

	return scene_manager_handle_custom_event(instance->manager, event);
}

static bool callbackNavigationEvent(const PCUBERZERO instance) {
	if(!instance) {
		return false;
	}

	return scene_manager_handle_back_event(instance->manager);
}

int32_t cuberzeroMain(const void* const pointer) {
	UNUSED(pointer);
	FURI_LOG_I(CUBERZERO_TAG, "Initializing");
	const char* messageError = NULL;
	const PCUBERZERO instance = malloc(sizeof(CUBERZERO));

	if(!instance) {
		messageError = "malloc() failed";
		goto functionExit;
	}

	memset(instance, 0, sizeof(CUBERZERO));
	CuberZeroSettingsLoad(&instance->settings);
	instance->scene.home.selectedItem = CUBERZERO_SCENE_TIMER;

	if(!(instance->scene.timer.mutex = furi_mutex_alloc(FuriMutexTypeNormal))) {
		messageError = "furi_mutex_alloc() failed";
		goto freeInstance;
	}

	if(!(instance->scene.timer.queue = furi_message_queue_alloc(1, sizeof(InputEvent)))) {
		messageError = "furi_message_queue_alloc() failed";
		goto freeMutex;
	}

	if(!(instance->scene.timer.viewport = view_port_alloc())) {
		messageError = "view_port_alloc() failed";
		goto freeMessageQueue;
	}

	if(!(instance->scene.timer.timer = furi_timer_alloc((FuriTimerCallback) SceneTimerTick, FuriTimerTypePeriodic, instance))) {
		messageError = "furi_timer_alloc() failed";
		goto freeViewport;
	}

	if(!(instance->scene.timer.string = furi_string_alloc())) {
		messageError = "furi_string_alloc() failed";
		goto freeTimer;
	}

	if(!(instance->interface = furi_record_open(RECORD_GUI))) {
		messageError = "furi_record_open(RECORD_GUI) failed";
		goto freeString;
	}

	if(!(instance->view.submenu = submenu_alloc())) {
		messageError = "submenu_alloc() failed";
		goto closeInterface;
	}

	if(!(instance->view.variableList = variable_item_list_alloc())) {
		messageError = "variable_item_list_alloc() failed";
		goto freeSubmenu;
	}

	if(!(instance->view.widget = widget_alloc())) {
		messageError = "widget_alloc() failed";
		goto freeVariableList;
	}

	if(!(instance->dispatcher = view_dispatcher_alloc())) {
		messageError = "view_dispatcher_alloc() failed";
		goto freeWidget;
	}

	const AppSceneOnEnterCallback onEnter[] = {(AppSceneOnEnterCallback) SceneAboutEnter, (AppSceneOnEnterCallback) SceneCubeSelectEnter, (AppSceneOnEnterCallback) SceneHomeEnter, (AppSceneOnEnterCallback) SceneSettingsEnter, (AppSceneOnEnterCallback) SceneTimerEnter, (AppSceneOnEnterCallback) SceneTimerOptionsEnter};
	const AppSceneOnEventCallback onEvent[] = {callbackEmptyEvent, (AppSceneOnEventCallback) SceneCubeSelectEvent, (AppSceneOnEventCallback) SceneHomeEvent, callbackEmptyEvent, callbackEmptyEvent, callbackEmptyEvent};
	const AppSceneOnExitCallback onExit[] = {callbackEmptyExit, callbackEmptyExit, callbackEmptyExit, (AppSceneOnExitCallback) SceneSettingsExit, callbackEmptyExit, callbackEmptyExit};
	const SceneManagerHandlers handlers = {onEnter, onEvent, onExit, COUNT_CUBERZEROSCENE};

	if(!(instance->manager = scene_manager_alloc(&handlers, instance))) {
		messageError = "scene_manager_alloc() failed";
		goto freeDispatcher;
	}

	view_port_draw_callback_set(instance->scene.timer.viewport, (ViewPortDrawCallback) SceneTimerDraw, instance);
	view_port_input_callback_set(instance->scene.timer.viewport, (ViewPortInputCallback) SceneTimerInput, instance);
	view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
	view_dispatcher_set_custom_event_callback(instance->dispatcher, (ViewDispatcherCustomEventCallback) callbackCustomEvent);
	view_dispatcher_set_navigation_event_callback(instance->dispatcher, (ViewDispatcherNavigationEventCallback) callbackNavigationEvent);
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST, variable_item_list_get_view(instance->view.variableList));
	view_dispatcher_add_view(instance->dispatcher, CUBERZERO_VIEW_WIDGET, widget_get_view(instance->view.widget));
	view_dispatcher_attach_to_gui(instance->dispatcher, instance->interface, ViewDispatcherTypeFullscreen);
	scene_manager_next_scene(instance->manager, CUBERZERO_SCENE_HOME);
	view_dispatcher_run(instance->dispatcher);
	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_SUBMENU);
	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_VARIABLE_ITEM_LIST);
	view_dispatcher_remove_view(instance->dispatcher, CUBERZERO_VIEW_WIDGET);
	scene_manager_free(instance->manager);
freeDispatcher:
	view_dispatcher_free(instance->dispatcher);
freeWidget:
	widget_free(instance->view.widget);
freeVariableList:
	variable_item_list_free(instance->view.variableList);
freeSubmenu:
	submenu_free(instance->view.submenu);
closeInterface:
	furi_record_close(RECORD_GUI);
freeString:
	furi_string_free(instance->scene.timer.string);
freeTimer:
	furi_timer_free(instance->scene.timer.timer);
freeViewport:
	view_port_free(instance->scene.timer.viewport);
freeMessageQueue:
	furi_message_queue_free(instance->scene.timer.queue);
freeMutex:
	furi_mutex_free(instance->scene.timer.mutex);
freeInstance:
	CuberZeroSettingsSave(&instance->settings);
	free(instance);
functionExit:
	FURI_LOG_I(CUBERZERO_TAG, "Exiting");

	if(!messageError) {
		return 0;
	}

	FURI_LOG_E(CUBERZERO_TAG, "Error: %s", messageError);
	__furi_crash(messageError);
	return 1;
}
