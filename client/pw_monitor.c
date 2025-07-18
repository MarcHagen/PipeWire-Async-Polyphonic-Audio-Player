#include <stdio.h>
#include <string.h>
#include "pw_monitor.h"
#include <spa/utils/result.h>
#include <spa/utils/string.h>

struct data
{
    struct pw_main_loop* loop;
    struct pw_context* context;
    struct pw_core* core;
    struct pw_registry* registry;
    struct spa_hook core_listener;
    struct spa_hook registry_listener;
    bool done;
};

static void print_node_info(struct pw_node_info* info)
{
    const char* description = NULL;
    const char* class = NULL;
    const char* name = NULL;
    const char* object_id = NULL;
    if (info->props)
    {
        description = spa_dict_lookup(info->props, PW_KEY_NODE_DESCRIPTION);
        class = spa_dict_lookup(info->props, PW_KEY_MEDIA_CLASS);
        name = spa_dict_lookup(info->props, PW_KEY_NODE_NAME);
        object_id = spa_dict_lookup(info->props, PW_KEY_OBJECT_ID);
    }

    printf("ID: %-5u | %-20s | %-20s | %-30s | %s\n",
           info->id,
           class + 6, // Skip "Audio/" prefix
           name ? name : "Unknown",
           object_id ? object_id : "Unknown",
           description ? description : "No description");
}

static void registry_event_global(void* data,
                                  uint32_t id,
                                  uint32_t permissions,
                                  const char* type,
                                  uint32_t version,
                                  const struct spa_dict* props)
{
    if (!props || strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;

    const char* class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!class)
        return;

    struct pw_node_info info = {0};
    info.id = id;
    info.props = props;
    print_node_info(&info);
}

static const struct pw_registry_events registry_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = registry_event_global,
};

static void on_core_done(void* data, uint32_t id, int seq)
{
    struct data* d = data;
    d->done = true;
    pw_main_loop_quit(d->loop);
}

static const struct pw_core_events core_events = {
    PW_VERSION_CORE_EVENTS,
    .done = on_core_done,
};

void list_audio_devices(void)
{
    struct data data = {0};

    pw_init(NULL, NULL);

    data.loop = pw_main_loop_new(NULL);
    if (!data.loop)
    {
        fprintf(stderr, "Failed to create main loop\n");
        goto cleanup;
    }

    data.context = pw_context_new(pw_main_loop_get_loop(data.loop), NULL, 0);
    if (!data.context)
    {
        fprintf(stderr, "Failed to create context\n");
        goto cleanup;
    }

    data.core = pw_context_connect(data.context, NULL, 0);
    if (!data.core)
    {
        fprintf(stderr, "Failed to connect to PipeWire\n");
        goto cleanup;
    }

    data.registry = pw_core_get_registry(data.core, PW_VERSION_REGISTRY, 0);
    if (!data.registry)
    {
        fprintf(stderr, "Failed to get registry\n");
        goto cleanup;
    }

    pw_registry_add_listener(data.registry, &data.registry_listener, &registry_events, &data);
    pw_core_add_listener(data.core, &data.core_listener, &core_events, &data);

    printf("\nAvailable PipeWire Audio Devices:\n");
    printf("-----------------------------------\n");
    printf("ID     | Type                | Object ID                | Name                           | Description\n");
    printf("--------------------------------------------------------------------------------\n");

    pw_main_loop_run(data.loop);

cleanup:
    if (data.registry)
        pw_proxy_destroy((struct pw_proxy*)data.registry);
    if (data.core)
        pw_core_disconnect(data.core);
    if (data.context)
        pw_context_destroy(data.context);
    if (data.loop)
        pw_main_loop_destroy(data.loop);

    pw_deinit();
}
