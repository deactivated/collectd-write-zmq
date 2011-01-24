/**
 * Basically copied from write_http.c
 */

#include "collectd.h"
#include "plugin.h"
#include "common.h"
#include "utils_cache.h"
#include "utils_parse_option.h"

#include <pthread.h>
#include <zmq.h>

#include "utils_value_json.h"

typedef struct {
  void *zmq_socket;
  pthread_mutex_t send_lock;
} wz_callback_t;


static void *zmq_ctx;


static void wz_callback_free (void *data) {
  wz_callback_t *cb;
  if (data == NULL)
    return;

  cb = data;
  zmq_close(cb->zmq_socket);
  sfree (cb);
}


static int wz_write(const data_set_t *ds, const value_list_t *vl,
                    user_data_t *user_data) {
  wz_callback_t *cb;
  int status;
  zmq_msg_t msg;
  size_t json_size;

  char json_buf[4096];

  if (user_data == NULL)
    return (-EINVAL);
  cb = user_data->data;

  // Format JSON
  status = value_list_to_json(json_buf, sizeof (json_buf), ds, vl, 0);
  if (status != 0) {
    WARNING("write_zmq_plugin: failed to format JSON.");
    return -1;
  }

  // Init ZMQ message
  json_size = strlen(json_buf);
  status = zmq_msg_init_size(&msg, json_size);
  if (status != 0) {
    WARNING("write_zmq_plugin: failed to allocate zmq message: %s.",
            zmq_strerror(errno));
    return -1;
  }
  memcpy(zmq_msg_data(&msg), json_buf, json_size);

  // Need mutex to let the ZMQ socket migrate threads
  pthread_mutex_lock (&cb->send_lock);
  status = zmq_send(cb->zmq_socket, &msg, 0);
  if (status != 0) {
    WARNING("write_zmq_plugin: failed to send message: %s.",
            zmq_strerror(errno));
    return -1;
  }
  pthread_mutex_unlock (&cb->send_lock);

  // Discard message
  zmq_msg_close(&msg);

  if (status != 0) {
    WARNING("write_zmq plugin: zmq_send failed: %s.",
            zmq_strerror(errno));
    return -1;
  }
  return 0;
}


int wz_config_url(oconfig_item_t *ci) {
  wz_callback_t *cb;
  user_data_t user_data;
  int status;

  cb = malloc (sizeof (*cb));
  if (cb == NULL) {
    ERROR("write_zmq plugin: malloc failed.");
    return -1;
  }

  memset (cb, 0, sizeof (*cb));
  pthread_mutex_init (&cb->send_lock, NULL);

  if (ci->values_num != 1 ||
      ci->values[0].type != OCONFIG_TYPE_STRING) {
    ERROR("write_zmq plugin: The `%s' config option "
           "needs to be a string.", ci->key);
    return -1;
  }

  cb->zmq_socket = zmq_socket(zmq_ctx, ZMQ_PUB);
  if (cb->zmq_socket == NULL) {
    ERROR("write_zmq plugin: zmq_socket failed: %s.",
          zmq_strerror(errno));
    return -1;
  }

  status = zmq_bind(cb->zmq_socket,
                    ci->values[0].value.string);
  if (status != 0) {
    ERROR("write_zmq plugin: zmq_bind failed: %s.",
          zmq_strerror(errno));
    return -1;
  }

  memset (&user_data, 0, sizeof (user_data));
  user_data.data = cb;
  user_data.free_func = wz_callback_free;
  plugin_register_write ("write_zmq", wz_write, &user_data);

  return 0;
}


static int wz_config(oconfig_item_t *ci) {
  int i;

  zmq_ctx = zmq_init(1);
  if (zmq_ctx == NULL) {
    return -1;
  }

  for (i = 0; i < ci->children_num; i++) {
    oconfig_item_t *child = ci->children + i;

    if (strcasecmp ("URL", child->key) == 0)
      wz_config_url (child);
    else {
      ERROR("write_zmq plugin: Invalid configuration "
            "option: %s.", child->key);
    }
  }

  return 0;
}


static int wz_shutdown() {
  // zmq_term(zmq_ctx);
  return 0;
}


void module_register (void) {
  plugin_register_shutdown ("write_zmq", wz_shutdown);
  plugin_register_complex_config("write_zmq", wz_config);
}
