#include <yaml.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "log.h"
#include "mqtt_client.h"

static void parse_logging(yaml_document_t *doc, yaml_node_t *node, global_config_t *config) {
    if (node->type != YAML_MAPPING_NODE) return;

    yaml_node_pair_t *pair;
    for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *value = yaml_document_get_node(doc, pair->value);

        if (strcmp((char *)key->data.scalar.value, "level") == 0) {
            config->logging.level = strdup((char *)value->data.scalar.value);
        }
    }
}

static void parse_mqtt(yaml_document_t *doc, yaml_node_t *node, global_config_t *config) {
    if (node->type != YAML_MAPPING_NODE) return;

    yaml_node_pair_t *pair;
    for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *value = yaml_document_get_node(doc, pair->value);

        if (strcmp((char *)key->data.scalar.value, "enabled") == 0) {
            config->mqtt.enabled = strcmp((char *)value->data.scalar.value, "true") == 0;
        } else if (strcmp((char *)key->data.scalar.value, "broker") == 0) {
            config->mqtt.broker = strdup((char *)value->data.scalar.value);
        } else if (strcmp((char *)key->data.scalar.value, "client_id") == 0) {
            config->mqtt.client_id = strdup((char *)value->data.scalar.value);
        } else if (strcmp((char *)key->data.scalar.value, "username") == 0) {
            config->mqtt.username = strdup((char *)value->data.scalar.value);
        } else if (strcmp((char *)key->data.scalar.value, "password") == 0) {
            config->mqtt.password = strdup((char *)value->data.scalar.value);
        } else if (strcmp((char *)key->data.scalar.value, "topic_prefix") == 0) {
            config->mqtt.topic_prefix = strdup((char *)value->data.scalar.value);
        }
    }
}

static void parse_track_output(yaml_document_t *doc, yaml_node_t *node, output_config_t *output) {
    if (node->type != YAML_MAPPING_NODE) return;

    yaml_node_pair_t *pair;
    for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
        yaml_node_t *key = yaml_document_get_node(doc, pair->key);
        yaml_node_t *value = yaml_document_get_node(doc, pair->value);

        if (strcmp((char *)key->data.scalar.value, "device") == 0) {
            output->device = strdup((char *)value->data.scalar.value);
        } else if (strcmp((char *)key->data.scalar.value, "mapping") == 0) {
            // Parse mapping array
            if (value->type == YAML_SEQUENCE_NODE) {
                output->mapping_count = value->data.sequence.items.top - value->data.sequence.items.start;
                output->mapping = malloc(sizeof(char *) * output->mapping_count);

                yaml_node_item_t *item;
                int i = 0;
                for (item = value->data.sequence.items.start; item < value->data.sequence.items.top; item++) {
                    yaml_node_t *map_value = yaml_document_get_node(doc, *item);
                    output->mapping[i++] = strdup((char *)map_value->data.scalar.value);
                }
            }
        }
    }
}

static void parse_tracks(yaml_document_t *doc, yaml_node_t *node, global_config_t *config) {
    if (node->type != YAML_SEQUENCE_NODE) return;

    config->track_count = node->data.sequence.items.top - node->data.sequence.items.start;
    config->tracks = malloc(sizeof(track_config_t) * config->track_count);

    yaml_node_item_t *item;
    int track_index = 0;

    for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
        yaml_node_t *track_node = yaml_document_get_node(doc, *item);
        track_config_t *track = &config->tracks[track_index++];
        memset(track, 0, sizeof(track_config_t));

        yaml_node_pair_t *pair;
        for (pair = track_node->data.mapping.pairs.start; pair < track_node->data.mapping.pairs.top; pair++) {
            yaml_node_t *key = yaml_document_get_node(doc, pair->key);
            yaml_node_t *value = yaml_document_get_node(doc, pair->value);

            if (strcmp((char *)key->data.scalar.value, "id") == 0) {
                track->id = strdup((char *)value->data.scalar.value);
            } else if (strcmp((char *)key->data.scalar.value, "file_path") == 0) {
                track->file_path = strdup((char *)value->data.scalar.value);
            } else if (strcmp((char *)key->data.scalar.value, "loop") == 0) {
                track->loop = strcmp((char *)value->data.scalar.value, "true") == 0;
            } else if (strcmp((char *)key->data.scalar.value, "volume") == 0) {
                track->volume = atof((char *)value->data.scalar.value);
            } else if (strcmp((char *)key->data.scalar.value, "output") == 0) {
                parse_track_output(doc, value, &track->output);
            }
        }
    }
}

global_config_t* config_load(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        log_error("Failed to open config file: %s", filename);
        return NULL;
    }

    yaml_parser_t parser;
    yaml_document_t document;

    if (!yaml_parser_initialize(&parser)) {
        log_error("Failed to initialize YAML parser");
        fclose(file);
        return NULL;
    }

    yaml_parser_set_input_file(&parser, file);
    if (!yaml_parser_load(&parser, &document)) {
        log_error("Failed to parse YAML document");
        yaml_parser_delete(&parser);
        fclose(file);
        return NULL;
    }

    global_config_t *config = calloc(1, sizeof(global_config_t));
    yaml_node_t *root = yaml_document_get_root_node(&document);

    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_pair_t *pair;
        for (pair = root->data.mapping.pairs.start; pair < root->data.mapping.pairs.top; pair++) {
            yaml_node_t *key = yaml_document_get_node(&document, pair->key);
            yaml_node_t *value = yaml_document_get_node(&document, pair->value);

            if (strcmp((char *)key->data.scalar.value, "logging") == 0) {
                parse_logging(&document, value, config);
            } else if (strcmp((char *)key->data.scalar.value, "mqtt") == 0) {
                parse_mqtt(&document, value, config);
            } else if (strcmp((char *)key->data.scalar.value, "tracks") == 0) {
                parse_tracks(&document, value, config);
            }
        }
    }

    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(file);

    return config;
}

void config_free(global_config_t *config) {
    if (!config) return;

    // Free logging config
    free(config->logging.level);

    // Free MQTT config
    if (config->mqtt_ctx) {
        mqtt_client_cleanup(config->mqtt_ctx);
        config->mqtt_ctx = NULL;
    }
    free(config->mqtt.broker);
    free(config->mqtt.client_id);
    free(config->mqtt.username);
    free(config->mqtt.password);
    free(config->mqtt.topic_prefix);

    // Free tracks
    for (int i = 0; i < config->track_count; i++) {
        track_config_t *track = &config->tracks[i];
        free(track->id);
        free(track->file_path);
        free(track->output.device);

        // Free each mapping string
        for (int j = 0; j < track->output.mapping_count; j++) {
            free(track->output.mapping[j]);
        }
        free(track->output.mapping);
    }
    free(config->tracks);

    free(config);
}

global_config_t* config_reload(const char *filename) {
    global_config_t *new_config = config_load(filename);
    if (!new_config) {
        log_error("Failed to reload configuration");
        return NULL;
    }
    return new_config;
}
