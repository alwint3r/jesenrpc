#include "jesenrpc.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static jesenrpc_err_t jrpc_strdup(const char *src, size_t len, char **out) {
  if (!src || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  char *copy = (char *)calloc(len + 1, sizeof(char));
  if (!copy) {
    return JESENRPC_ERR_ALLOC;
  }
  memcpy(copy, src, len);
  copy[len] = '\0';
  *out = copy;
  return JESENRPC_ERR_NONE;
}

static void jrpc_id_cleanup(jesenrpc_id_t *id) {
  if (!id) {
    return;
  }
  if (id->kind == JESENRPC_ID_STRING && id->value.string.data) {
    free(id->value.string.data);
  }
  memset(id, 0, sizeof(*id));
}

static jesenrpc_err_t jrpc_id_clone(const jesenrpc_id_t *src,
                                    jesenrpc_id_t *dst) {
  if (!src || !dst) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(dst);
  *dst = *src;
  if (src->kind == JESENRPC_ID_STRING && src->value.string.data) {
    return jrpc_strdup(src->value.string.data, src->value.string.len,
                       &dst->value.string.data);
  }
  return JESENRPC_ERR_NONE;
}

static bool jrpc_doubles_equal(double a, double b) {
  double diff = a - b;
  return diff < 0.0000000001 && diff > -0.0000000001;
}

static jesenrpc_err_t jrpc_copy_string_value(const jesen_node_t *node,
                                             size_t max_attempt, char **out_str,
                                             size_t *out_len) {
  if (!node || !out_str || max_attempt == 0) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  size_t buf_size = 64;
  while (buf_size <= max_attempt) {
    char *buf = (char *)calloc(buf_size, sizeof(char));
    if (!buf) {
      return JESENRPC_ERR_ALLOC;
    }
    size_t len = 0;
    jesen_err_t err = jesen_value_get_string(node, buf, buf_size, &len);
    if (err == JESEN_ERR_NONE) {
      *out_str = buf;
      if (out_len) {
        *out_len = len;
      }
      return JESENRPC_ERR_NONE;
    }
    free(buf);
    if (err == JESEN_ERR_INVALID_VALUE_TYPE) {
      return err;
    }
    if (err != JESEN_ERR_INVALID_ARGS) {
      return err;
    }
    buf_size *= 2;
  }

  return JESENRPC_ERR_INVALID_ARGS;
}

static jesenrpc_err_t
jrpc_read_object_string_with_limit(const jesen_node_t *object, const char *key,
                                   size_t max_len, char **out) {
  if (!object || !key || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  size_t buf_len = max_len + 1;
  char *tmp = (char *)calloc(buf_len, sizeof(char));
  if (!tmp) {
    return JESENRPC_ERR_ALLOC;
  }
  size_t str_len = 0;
  jesen_err_t err =
      jesen_object_get_string(object, key, tmp, buf_len, &str_len);
  if (err != JESEN_ERR_NONE) {
    free(tmp);
    return err == JESEN_ERR_INVALID_ARGS ? JESENRPC_ERR_VALIDATION : err;
  }
  *out = tmp;
  (void)str_len;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t jrpc_add_id_to_object(jesen_node_t *object,
                                            const jesenrpc_id_t *id) {
  if (!object || !id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  switch (id->kind) {
  case JESENRPC_ID_NONE:
    return JESENRPC_ERR_NONE;
  case JESENRPC_ID_NULL:
    return jesen_object_add_null(object, "id");
  case JESENRPC_ID_STRING:
    if (!id->value.string.data) {
      return JESENRPC_ERR_INVALID_ARGS;
    }
    return jesen_object_add_string(object, "id", id->value.string.data,
                                   id->value.string.len);
  case JESENRPC_ID_NUMBER: {
    int64_t num = id->value.number;
    if (num >= INT32_MIN && num <= INT32_MAX) {
      return jesen_object_add_int32(object, "id", (int32_t)num);
    }
    return jesen_object_add_double(object, "id", (double)num);
  }
  default:
    return JESENRPC_ERR_INVALID_ARGS;
  }
}

static jesenrpc_err_t jrpc_build_request_node(const jesenrpc_request_t *request,
                                              jesen_node_t **out) {
  if (!request || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_err_t err = jesenrpc_request_validate(request);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  jesen_node_t *root = NULL;
  err = jesen_object_create(&root);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  err = jesen_object_add_string(root, "jsonrpc", JESENRPC_JSONRPC_VERSION,
                                JESENRPC_JSONRPC_VERSION_LEN);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  err = jrpc_add_id_to_object(root, &request->id);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  size_t method_len = strlen(request->method_name);
  err =
      jesen_object_add_string(root, "method", request->method_name, method_len);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  bool has_params = request->params != NULL;
  if (has_params) {
    err = jesen_node_assign_to(root, "params", request->params);
    if (err != JESEN_ERR_NONE) {
      jesen_destroy(root);
      return err;
    }
  }

  *out = root;

  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t
jrpc_build_error_node(const jesenrpc_error_object_t *error,
                      jesen_node_t **out_error_node) {
  if (!error || !out_error_node) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_err_t err = jesenrpc_error_object_validate(error);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  jesen_node_t *err_obj = NULL;
  err = jesen_object_create(&err_obj);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  err = jesen_object_add_int32(err_obj, "code", error->code);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(err_obj);
    return err;
  }

  size_t msg_len = strlen(error->message);
  err = jesen_object_add_string(err_obj, "message", error->message, msg_len);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(err_obj);
    return err;
  }

  bool has_data = error->data != NULL;
  if (has_data) {
    err = jesen_node_assign_to(err_obj, "data", error->data);
    if (err != JESEN_ERR_NONE) {
      jesen_destroy(err_obj);
      return err;
    }
  }

  *out_error_node = err_obj;

  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t
jrpc_build_response_node(const jesenrpc_response_t *response,
                         jesen_node_t **out_node) {
  if (!response || !out_node) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_err_t err = jesenrpc_response_validate(response);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  jesen_node_t *root = NULL;
  err = jesen_object_create(&root);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  err = jesen_object_add_string(root, "jsonrpc", JESENRPC_JSONRPC_VERSION,
                                JESENRPC_JSONRPC_VERSION_LEN);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  err = jrpc_add_id_to_object(root, &response->id);
  if (err != JESEN_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  bool has_result = response->result != NULL;
  bool has_error = response->error != NULL;
  if (has_result) {
    err = jesen_node_assign_to(root, "result", response->result);
    if (err != JESEN_ERR_NONE) {
      jesen_node_detach(response->result);
      jesen_destroy(root);
      return err;
    }
  } else if (has_error) {
    jesen_node_t *error_node = NULL;
    err = jrpc_build_error_node(response->error, &error_node);
    if (err != JESEN_ERR_NONE) {
      jesen_destroy(root);
      return err;
    }
    err = jesen_node_assign_to(root, "error", error_node);
    if (err != JESEN_ERR_NONE) {
      if (response->error->data) {
        jesen_node_detach(response->error->data);
      }
      jesen_destroy(error_node);
      jesen_destroy(root);
      return err;
    }
  }

  *out_node = root;

  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t jrpc_serialize_node(jesen_node_t *node, char *out_buf,
                                          size_t out_buf_len) {
  if (!node || !out_buf || out_buf_len == 0) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jesen_err_t err = jesen_serialize(node, out_buf, out_buf_len);
  return err;
}

static jesenrpc_err_t jrpc_parse_id(const jesen_node_t *id_node,
                                    jesenrpc_id_t *out_id) {
  if (!id_node || !out_id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_null = false;
  bool is_string = false;
  bool is_number = false;
  jesen_err_t err = jesen_value_is_null(id_node, &is_null);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  err = jesen_value_is_string(id_node, &is_string);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  err = jesen_value_is_double(id_node, &is_number);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  if (is_null) {
    return jesenrpc_id_set_null(out_id);
  }
  if (is_string) {
    char *id_str = NULL;
    size_t len = 0;
    err = jrpc_copy_string_value(id_node, 8192, &id_str, &len);
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    jrpc_id_cleanup(out_id);
    out_id->kind = JESENRPC_ID_STRING;
    out_id->value.string.data = id_str;
    out_id->value.string.len = len;
    return JESENRPC_ERR_NONE;
  }
  if (is_number) {
    double val = 0.0;
    err = jesen_value_get_double(id_node, &val);
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    if (val > (double)INT64_MAX || val < (double)INT64_MIN) {
      return JESENRPC_ERR_VALIDATION;
    }
    int64_t as_int = (int64_t)val;
    if (!jrpc_doubles_equal((double)as_int, val)) {
      return JESENRPC_ERR_VALIDATION;
    }
    return jesenrpc_id_set_number(out_id, as_int);
  }

  return JESENRPC_ERR_VALIDATION;
}

static jesenrpc_err_t
jrpc_parse_request_node(jesen_node_t *root, jesenrpc_request_t **out_request) {
  if (!root || !out_request) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_object = false;
  jesen_err_t err = jesen_value_is_object(root, &is_object);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_object) {
    return JESENRPC_ERR_VALIDATION;
  }

  char *version = NULL;
  err = jrpc_read_object_string_with_limit(
      root, "jsonrpc", JESENRPC_JSONRPC_VERSION_LEN, &version);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  bool version_ok = strcmp(version, JESENRPC_JSONRPC_VERSION) == 0;
  free(version);
  if (!version_ok) {
    return JESENRPC_ERR_VALIDATION;
  }

  char *method = NULL;
  err = jrpc_read_object_string_with_limit(
      root, "method", JESENRPC_METHOD_NAME_MAX_LEN, &method);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  jesen_node_t *params_node = NULL;
  err = jesen_node_find(root, "params", &params_node);
  bool has_params = (err == JESEN_ERR_NONE);
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    free(method);
    return err;
  }

  jesen_node_t *id_node = NULL;
  err = jesen_node_find(root, "id", &id_node);
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    free(method);
    return err;
  }

  jesenrpc_request_t *req = (jesenrpc_request_t *)calloc(1, sizeof(*req));
  if (!req) {
    free(method);
    return JESENRPC_ERR_ALLOC;
  }

  req->jsonrpc = JESENRPC_JSONRPC_VERSION;
  req->method_name = method;
  req->params = NULL;
  req->id.kind = JESENRPC_ID_NONE;

  if (has_params) {
    bool params_is_object = false;
    bool params_is_array = false;
    err = jesen_value_is_object(params_node, &params_is_object);
    if (err == JESEN_ERR_NONE) {
      err = jesen_value_is_array(params_node, &params_is_array);
    }
    if (err != JESEN_ERR_NONE || (!params_is_object && !params_is_array)) {
      jesenrpc_request_destroy(req);
      return err != JESEN_ERR_NONE ? err : JESENRPC_ERR_VALIDATION;
    }
    err = jesen_node_detach(params_node);
    if (err != JESEN_ERR_NONE) {
      jesenrpc_request_destroy(req);
      return err;
    }
    req->params = params_node;
  }

  if (id_node) {
    err = jrpc_parse_id(id_node, &req->id);
    if (err != JESEN_ERR_NONE) {
      jesenrpc_request_destroy(req);
      return err;
    }
  }

  *out_request = req;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t jrpc_parse_error_object(jesen_node_t *error_node,
                                              jesenrpc_error_object_t **out) {
  if (!error_node || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_object = false;
  jesen_err_t err = jesen_value_is_object(error_node, &is_object);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_object) {
    return JESENRPC_ERR_VALIDATION;
  }

  int32_t code = 0;
  err = jesen_object_get_int32(error_node, "code", &code);
  if (err != JESEN_ERR_NONE) {
    return err == JESEN_ERR_NOT_FOUND ? JESENRPC_ERR_VALIDATION : err;
  }

  char *message = NULL;
  err =
      jrpc_read_object_string_with_limit(error_node, "message", 4096, &message);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  jesen_node_t *data_node = NULL;
  err = jesen_node_find(error_node, "data", &data_node);
  bool has_data = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    free(message);
    return err;
  }

  jesenrpc_error_object_t *err_obj =
      (jesenrpc_error_object_t *)calloc(1, sizeof(*err_obj));
  if (!err_obj) {
    free(message);
    return JESENRPC_ERR_ALLOC;
  }

  err_obj->code = code;
  err_obj->message = message;
  err_obj->data = NULL;

  if (has_data) {
    err = jesen_node_detach(data_node);
    if (err != JESEN_ERR_NONE) {
      jesenrpc_error_object_destroy(err_obj);
      return err;
    }
    err_obj->data = data_node;
  }

  *out = err_obj;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t jrpc_parse_response_node(jesen_node_t *root,
                                               jesenrpc_response_t **out_resp) {
  if (!root || !out_resp) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_object = false;
  jesen_err_t err = jesen_value_is_object(root, &is_object);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_object) {
    return JESENRPC_ERR_VALIDATION;
  }

  char *version = NULL;
  err = jrpc_read_object_string_with_limit(
      root, "jsonrpc", JESENRPC_JSONRPC_VERSION_LEN, &version);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  bool version_ok = strcmp(version, JESENRPC_JSONRPC_VERSION) == 0;
  free(version);
  if (!version_ok) {
    return JESENRPC_ERR_VALIDATION;
  }

  jesen_node_t *id_node = NULL;
  err = jesen_node_find(root, "id", &id_node);
  if (err != JESEN_ERR_NONE) {
    return err == JESEN_ERR_NOT_FOUND ? JESENRPC_ERR_VALIDATION : err;
  }

  jesen_node_t *result_node = NULL;
  err = jesen_node_find(root, "result", &result_node);
  bool has_result = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    return err;
  }

  jesen_node_t *error_node = NULL;
  err = jesen_node_find(root, "error", &error_node);
  bool has_error = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    return err;
  }

  if ((has_result && has_error) || (!has_result && !has_error)) {
    return JESENRPC_ERR_VALIDATION;
  }

  jesenrpc_response_t *resp = (jesenrpc_response_t *)calloc(1, sizeof(*resp));
  if (!resp) {
    return JESENRPC_ERR_ALLOC;
  }

  resp->jsonrpc = JESENRPC_JSONRPC_VERSION;
  resp->result = NULL;
  resp->error = NULL;

  err = jrpc_parse_id(id_node, &resp->id);
  if (err != JESEN_ERR_NONE) {
    jesenrpc_response_destroy(resp);
    return err;
  }

  if (has_result) {
    err = jesen_node_detach(result_node);
    if (err != JESEN_ERR_NONE) {
      jesenrpc_response_destroy(resp);
      return err;
    }
    resp->result = result_node;
  } else {
    err = jrpc_parse_error_object(error_node, &resp->error);
    if (err != JESEN_ERR_NONE) {
      jesenrpc_response_destroy(resp);
      return err;
    }
  }

  *out_resp = resp;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t jrpc_parse_buffer_as_node(char *buf, size_t buf_len,
                                                jesen_node_t **out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  return jesen_parse(buf, buf_len, out);
}

jesenrpc_err_t jesenrpc_id_set_number(jesenrpc_id_t *id, int64_t value) {
  if (!id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(id);
  id->kind = JESENRPC_ID_NUMBER;
  id->value.number = value;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_id_set_string(jesenrpc_id_t *id, const char *value,
                                      size_t value_len) {
  if (!id || !value) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(id);
  id->kind = JESENRPC_ID_STRING;
  id->value.string.len = value_len;
  return jrpc_strdup(value, value_len, &id->value.string.data);
}

jesenrpc_err_t jesenrpc_id_set_null(jesenrpc_id_t *id) {
  if (!id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(id);
  id->kind = JESENRPC_ID_NULL;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_id_set_notification(jesenrpc_id_t *id) {
  if (!id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(id);
  id->kind = JESENRPC_ID_NONE;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_id_destroy(jesenrpc_id_t *id) {
  if (!id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  jrpc_id_cleanup(id);
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_request_create(const char *method_name,
                                       jesenrpc_request_t **request) {
  if (!method_name || !request) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  size_t len = strnlen(method_name, JESENRPC_METHOD_NAME_MAX_LEN + 1);
  if (len == 0 || len > JESENRPC_METHOD_NAME_MAX_LEN) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_request_t *req = (jesenrpc_request_t *)calloc(1, sizeof(*req));
  if (!req) {
    return JESENRPC_ERR_ALLOC;
  }

  req->jsonrpc = JESENRPC_JSONRPC_VERSION;
  req->id.kind = JESENRPC_ID_NONE;
  req->params = NULL;

  jesenrpc_err_t err = jrpc_strdup(method_name, len, &req->method_name);
  if (err != JESENRPC_ERR_NONE) {
    free(req);
    return err;
  }

  *request = req;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_request_create_with_id(const char *method_name,
                                               const jesenrpc_id_t *id,
                                               jesenrpc_request_t **request) {
  jesenrpc_err_t err = jesenrpc_request_create(method_name, request);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }
  err = jesenrpc_request_set_id(*request, id);
  if (err != JESENRPC_ERR_NONE) {
    jesenrpc_request_destroy(*request);
    *request = NULL;
  }
  return err;
}

jesenrpc_err_t jesenrpc_request_set_id(jesenrpc_request_t *request,
                                       const jesenrpc_id_t *id) {
  if (!request || !id) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  return jrpc_id_clone(id, &request->id);
}

jesenrpc_err_t jesenrpc_request_set_params(jesenrpc_request_t *request,
                                           jesen_node_t *params) {
  if (!request) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_array = false;
  bool is_object = false;
  if (params) {
    jesen_err_t err = jesen_value_is_array(params, &is_array);
    if (err == JESEN_ERR_NONE) {
      err = jesen_value_is_object(params, &is_object);
    }
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    if (!is_array && !is_object) {
      return JESENRPC_ERR_VALIDATION;
    }
  }

  if (request->params) {
    jesen_destroy(request->params);
  }
  request->params = params;
  return JESENRPC_ERR_NONE;
}

bool jesenrpc_request_is_notification(const jesenrpc_request_t *request) {
  if (!request) {
    return false;
  }
  return request->id.kind == JESENRPC_ID_NONE;
}

jesenrpc_err_t jesenrpc_request_serialize(const jesenrpc_request_t *request,
                                          char *out_buf, size_t out_buf_len) {
  jesen_node_t *node = NULL;
  jesenrpc_err_t err = jrpc_build_request_node(request, &node);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }
  jesenrpc_err_t serialize_err =
      jrpc_serialize_node(node, out_buf, out_buf_len);
  if (request->params) {
    jesen_node_detach(request->params);
  }
  jesen_destroy(node);
  return serialize_err;
}

jesenrpc_err_t jesenrpc_request_validate(const jesenrpc_request_t *request) {
  if (!request || !request->method_name) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  if (!request->jsonrpc ||
      strcmp(request->jsonrpc, JESENRPC_JSONRPC_VERSION) != 0) {
    return JESENRPC_ERR_VALIDATION;
  }

  size_t len = strlen(request->method_name);
  if (len == 0 || len > JESENRPC_METHOD_NAME_MAX_LEN) {
    return JESENRPC_ERR_VALIDATION;
  }

  if (request->params) {
    bool is_array = false;
    bool is_object = false;
    jesen_err_t err = jesen_value_is_array(request->params, &is_array);
    if (err == JESEN_ERR_NONE) {
      err = jesen_value_is_object(request->params, &is_object);
    }
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    if (!is_array && !is_object) {
      return JESENRPC_ERR_VALIDATION;
    }
  }

  if (request->id.kind == JESENRPC_ID_STRING &&
      (!request->id.value.string.data || request->id.value.string.len == 0)) {
    return JESENRPC_ERR_VALIDATION;
  }

  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_request_parse(char *buf, size_t buf_len,
                                      jesenrpc_request_t **out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  err = jrpc_parse_request_node(root, out);
  jesen_destroy(root);
  return err;
}

jesenrpc_err_t jesenrpc_request_destroy(jesenrpc_request_t *request) {
  if (!request) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  if (request->params) {
    jesen_destroy(request->params);
  }
  jrpc_id_cleanup(&request->id);
  if (request->method_name) {
    free(request->method_name);
  }
  free(request);
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t
jesenrpc_response_create_for_request(const jesenrpc_request_t *request,
                                     jesenrpc_response_t **response) {
  if (!request || !response) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (request->id.kind == JESENRPC_ID_NONE) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  return jesenrpc_response_create_with_id(&request->id, response);
}

jesenrpc_err_t
jesenrpc_response_create_with_id(const jesenrpc_id_t *id,
                                 jesenrpc_response_t **response) {
  if (!id || !response) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_response_t *resp = (jesenrpc_response_t *)calloc(1, sizeof(*resp));
  if (!resp) {
    return JESENRPC_ERR_ALLOC;
  }

  resp->jsonrpc = JESENRPC_JSONRPC_VERSION;
  resp->result = NULL;
  resp->error = NULL;
  resp->id.kind = JESENRPC_ID_NONE;

  jesenrpc_err_t err = jrpc_id_clone(id, &resp->id);
  if (err != JESENRPC_ERR_NONE) {
    free(resp);
    return err;
  }

  *response = resp;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_response_create(int32_t request_id,
                                        jesenrpc_response_t **response) {
  jesenrpc_id_t id = {.kind = JESENRPC_ID_NUMBER, .value.number = request_id};
  return jesenrpc_response_create_with_id(&id, response);
}

jesenrpc_err_t jesenrpc_response_set_result(jesenrpc_response_t *response,
                                            jesen_node_t *result) {
  if (!response || !result) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (response->error || response->result) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  response->result = result;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_response_set_error(jesenrpc_response_t *response,
                                           jesenrpc_error_object_t *error) {
  if (!response || !error) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (response->result || response->error) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  response->error = error;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_response_serialize(const jesenrpc_response_t *response,
                                           char *out_buf, size_t out_buf_len) {
  jesen_node_t *node = NULL;
  jesenrpc_err_t err = jrpc_build_response_node(response, &node);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }
  jesenrpc_err_t serialize_err =
      jrpc_serialize_node(node, out_buf, out_buf_len);
  if (response->result) {
    jesen_node_detach(response->result);
  } else if (response->error && response->error->data) {
    jesen_node_detach(response->error->data);
  }
  jesen_destroy(node);
  return serialize_err;
}

jesenrpc_err_t jesenrpc_response_validate(const jesenrpc_response_t *response) {
  if (!response) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (!response->jsonrpc ||
      strcmp(response->jsonrpc, JESENRPC_JSONRPC_VERSION) != 0) {
    return JESENRPC_ERR_VALIDATION;
  }
  if (response->id.kind == JESENRPC_ID_NONE) {
    return JESENRPC_ERR_VALIDATION;
  }
  if (response->id.kind == JESENRPC_ID_STRING &&
      (!response->id.value.string.data || response->id.value.string.len == 0)) {
    return JESENRPC_ERR_VALIDATION;
  }
  bool has_result = response->result != NULL;
  bool has_error = response->error != NULL;
  if ((has_result && has_error) || (!has_result && !has_error)) {
    return JESENRPC_ERR_VALIDATION;
  }
  if (has_error) {
    return jesenrpc_error_object_validate(response->error);
  }
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_response_parse(char *buf, size_t buf_len,
                                       jesenrpc_response_t **out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  err = jrpc_parse_response_node(root, out);
  jesen_destroy(root);
  return err;
}

jesenrpc_err_t jesenrpc_response_destroy(jesenrpc_response_t *response) {
  if (!response) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (response->result) {
    jesen_destroy(response->result);
  }
  if (response->error) {
    jesenrpc_error_object_destroy(response->error);
  }
  jrpc_id_cleanup(&response->id);
  free(response);
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_error_object_create(int32_t code, const char *message,
                                            jesenrpc_error_object_t **error) {
  if (!message || !error) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  size_t len = strlen(message);
  if (len == 0) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_error_object_t *err_obj =
      (jesenrpc_error_object_t *)calloc(1, sizeof(*err_obj));
  if (!err_obj) {
    return JESENRPC_ERR_ALLOC;
  }
  err_obj->code = code;
  err_obj->data = NULL;

  jesenrpc_err_t err = jrpc_strdup(message, len, &err_obj->message);
  if (err != JESENRPC_ERR_NONE) {
    free(err_obj);
    return err;
  }

  *error = err_obj;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_error_object_set_data(jesenrpc_error_object_t *error,
                                              jesen_node_t *data) {
  if (!error) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (error->data && error->data != data) {
    jesen_destroy(error->data);
  }
  error->data = data;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t
jesenrpc_error_object_validate(const jesenrpc_error_object_t *error) {
  if (!error || !error->message) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (strlen(error->message) == 0) {
    return JESENRPC_ERR_VALIDATION;
  }
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_error_object_destroy(jesenrpc_error_object_t *error) {
  if (!error) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (error->message) {
    free(error->message);
  }
  if (error->data) {
    jesen_destroy(error->data);
  }
  free(error);
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t
jrpc_parse_request_batch_from_node(jesen_node_t *root,
                                   jesenrpc_request_batch_t *out) {
  if (!root || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  out->items = NULL;
  out->count = 0;

  bool is_array = false;
  jesen_err_t err = jesen_value_is_array(root, &is_array);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_array) {
    return JESENRPC_ERR_VALIDATION;
  }

  size_t count = 0;
  err = jesen_array_size(root, &count);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  if (count == 0) {
    return JESENRPC_ERR_NONE;
  }

  jesenrpc_request_t **items =
      (jesenrpc_request_t **)calloc(count, sizeof(*items));
  if (!items) {
    return JESENRPC_ERR_ALLOC;
  }

  for (size_t i = 0; i < count; ++i) {
    jesen_node_t *elem = NULL;
    err = jesen_array_get_value(root, (uint32_t)i, &elem);
    if (err != JESEN_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        jesenrpc_request_destroy(items[j]);
      }
      free(items);
      return err;
    }
    jesenrpc_request_t *req = NULL;
    err = jrpc_parse_request_node(elem, &req);
    if (err != JESENRPC_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        jesenrpc_request_destroy(items[j]);
      }
      free(items);
      return err;
    }
    items[i] = req;
  }

  out->items = items;
  out->count = count;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t
jrpc_parse_response_batch_from_node(jesen_node_t *root,
                                    jesenrpc_response_batch_t *out) {
  if (!root || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  out->items = NULL;
  out->count = 0;

  bool is_array = false;
  jesen_err_t err = jesen_value_is_array(root, &is_array);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_array) {
    return JESENRPC_ERR_VALIDATION;
  }

  size_t count = 0;
  err = jesen_array_size(root, &count);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  if (count == 0) {
    return JESENRPC_ERR_NONE;
  }

  jesenrpc_response_t **items =
      (jesenrpc_response_t **)calloc(count, sizeof(*items));
  if (!items) {
    return JESENRPC_ERR_ALLOC;
  }

  for (size_t i = 0; i < count; ++i) {
    jesen_node_t *elem = NULL;
    err = jesen_array_get_value(root, (uint32_t)i, &elem);
    if (err != JESEN_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        jesenrpc_response_destroy(items[j]);
      }
      free(items);
      return err;
    }
    jesenrpc_response_t *resp = NULL;
    err = jrpc_parse_response_node(elem, &resp);
    if (err != JESENRPC_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        jesenrpc_response_destroy(items[j]);
      }
      free(items);
      return err;
    }
    items[i] = resp;
  }

  out->items = items;
  out->count = count;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t
jesenrpc_request_batch_serialize(jesenrpc_request_t *const *requests,
                                 size_t request_count, char *out_buf,
                                 size_t out_buf_len) {
  if (!requests || (request_count > 0 && !out_buf)) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesen_node_t *array = NULL;
  jesen_err_t err = jesen_array_create(&array);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  for (size_t i = 0; i < request_count; ++i) {
    jesen_node_t *node = NULL;
    err = jrpc_build_request_node(requests[i], &node);
    if (err != JESENRPC_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        if (requests[j] && requests[j]->params) {
          jesen_node_detach(requests[j]->params);
        }
      }
      jesen_destroy(array);
      return err;
    }
    err = jesen_node_assign_to(array, "", node);
    if (err != JESEN_ERR_NONE) {
      if (requests[i] && requests[i]->params) {
        jesen_node_detach(requests[i]->params);
      }
      jesen_destroy(node);
      for (size_t j = 0; j < i; ++j) {
        if (requests[j] && requests[j]->params) {
          jesen_node_detach(requests[j]->params);
        }
      }
      jesen_destroy(array);
      return err;
    }
  }

  jesenrpc_err_t rpc_err = jrpc_serialize_node(array, out_buf, out_buf_len);
  for (size_t i = 0; i < request_count; ++i) {
    if (requests[i] && requests[i]->params) {
      jesen_node_detach(requests[i]->params);
    }
  }
  jesen_destroy(array);
  return rpc_err;
}

jesenrpc_err_t
jesenrpc_response_batch_serialize(jesenrpc_response_t *const *responses,
                                  size_t response_count, char *out_buf,
                                  size_t out_buf_len) {
  if (!responses || (response_count > 0 && !out_buf)) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesen_node_t *array = NULL;
  jesen_err_t err = jesen_array_create(&array);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  for (size_t i = 0; i < response_count; ++i) {
    jesen_node_t *node = NULL;
    err = jrpc_build_response_node(responses[i], &node);
    if (err != JESENRPC_ERR_NONE) {
      for (size_t j = 0; j < i; ++j) {
        if (!responses[j]) {
          continue;
        }
        if (responses[j]->result) {
          jesen_node_detach(responses[j]->result);
        } else if (responses[j]->error && responses[j]->error->data) {
          jesen_node_detach(responses[j]->error->data);
        }
      }
      jesen_destroy(array);
      return err;
    }
    err = jesen_node_assign_to(array, "", node);
    if (err != JESEN_ERR_NONE) {
      if (responses[i]) {
        if (responses[i]->result) {
          jesen_node_detach(responses[i]->result);
        } else if (responses[i]->error && responses[i]->error->data) {
          jesen_node_detach(responses[i]->error->data);
        }
      }
      jesen_destroy(node);
      for (size_t j = 0; j < i; ++j) {
        if (!responses[j]) {
          continue;
        }
        if (responses[j]->result) {
          jesen_node_detach(responses[j]->result);
        } else if (responses[j]->error && responses[j]->error->data) {
          jesen_node_detach(responses[j]->error->data);
        }
      }
      jesen_destroy(array);
      return err;
    }
  }

  jesenrpc_err_t rpc_err = jrpc_serialize_node(array, out_buf, out_buf_len);
  for (size_t i = 0; i < response_count; ++i) {
    if (!responses[i]) {
      continue;
    }
    if (responses[i]->result) {
      jesen_node_detach(responses[i]->result);
    } else if (responses[i]->error && responses[i]->error->data) {
      jesen_node_detach(responses[i]->error->data);
    }
  }
  jesen_destroy(array);
  return rpc_err;
}

jesenrpc_err_t jesenrpc_request_batch_parse(char *buf, size_t buf_len,
                                            jesenrpc_request_batch_t *out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  out->items = NULL;
  out->count = 0;

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  err = jrpc_parse_request_batch_from_node(root, out);
  jesen_destroy(root);
  return err;
}

jesenrpc_err_t jesenrpc_response_batch_parse(char *buf, size_t buf_len,
                                             jesenrpc_response_batch_t *out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  out->items = NULL;
  out->count = 0;

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  err = jrpc_parse_response_batch_from_node(root, out);
  jesen_destroy(root);
  return err;
}

jesenrpc_err_t jesenrpc_request_batch_destroy(jesenrpc_request_batch_t *batch) {
  if (!batch) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (batch->items) {
    for (size_t i = 0; i < batch->count; ++i) {
      jesenrpc_request_destroy(batch->items[i]);
    }
    free(batch->items);
  }
  batch->items = NULL;
  batch->count = 0;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t
jesenrpc_response_batch_destroy(jesenrpc_response_batch_t *batch) {
  if (!batch) {
    return JESENRPC_ERR_INVALID_ARGS;
  }
  if (batch->items) {
    for (size_t i = 0; i < batch->count; ++i) {
      jesenrpc_response_destroy(batch->items[i]);
    }
    free(batch->items);
  }
  batch->items = NULL;
  batch->count = 0;
  return JESENRPC_ERR_NONE;
}

static jesenrpc_err_t
jrpc_detect_message_kind_from_object(jesen_node_t *object, bool in_batch,
                                     jesenrpc_message_kind_t *out_kind) {
  if (!object || !out_kind) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_object = false;
  jesen_err_t err = jesen_value_is_object(object, &is_object);
  if (err != JESEN_ERR_NONE) {
    return err;
  }
  if (!is_object) {
    return JESENRPC_ERR_VALIDATION;
  }

  jesen_node_t *method_node = NULL;
  err = jesen_node_find(object, "method", &method_node);
  bool has_method = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    return err;
  }

  jesen_node_t *result_node = NULL;
  err = jesen_node_find(object, "result", &result_node);
  bool has_result = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    return err;
  }

  jesen_node_t *error_node = NULL;
  err = jesen_node_find(object, "error", &error_node);
  bool has_error = err == JESEN_ERR_NONE;
  if (err != JESEN_ERR_NONE && err != JESEN_ERR_NOT_FOUND) {
    return err;
  }

  if (has_method) {
    if (has_result || has_error) {
      return JESENRPC_ERR_VALIDATION;
    }
    *out_kind = in_batch ? JESENRPC_MESSAGE_REQUEST_BATCH
                         : JESENRPC_MESSAGE_REQUEST_SINGLE;
    return JESENRPC_ERR_NONE;
  }

  if (has_result || has_error) {
    *out_kind = in_batch ? JESENRPC_MESSAGE_RESPONSE_BATCH
                         : JESENRPC_MESSAGE_RESPONSE_SINGLE;
    return JESENRPC_ERR_NONE;
  }

  return JESENRPC_ERR_VALIDATION;
}

static jesenrpc_err_t
jrpc_detect_message_kind_from_node(jesen_node_t *root,
                                   jesenrpc_message_kind_t *out_kind) {
  if (!root || !out_kind) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  bool is_array = false;
  jesen_err_t err = jesen_value_is_array(root, &is_array);
  if (err != JESEN_ERR_NONE) {
    return err;
  }

  if (is_array) {
    size_t count = 0;
    err = jesen_array_size(root, &count);
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    if (count == 0) {
      return JESENRPC_ERR_VALIDATION;
    }
    jesen_node_t *first = NULL;
    err = jesen_array_get_value(root, 0, &first);
    if (err != JESEN_ERR_NONE) {
      return err;
    }
    return jrpc_detect_message_kind_from_object(first, true, out_kind);
  }

  return jrpc_detect_message_kind_from_object(root, false, out_kind);
}

jesenrpc_err_t jesenrpc_message_peek_kind(char *buf, size_t buf_len,
                                          jesenrpc_message_kind_t *kind) {
  if (!buf || !kind) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  err = jrpc_detect_message_kind_from_node(root, kind);
  jesen_destroy(root);
  return err;
}

jesenrpc_err_t jesenrpc_message_parse(char *buf, size_t buf_len,
                                      jesenrpc_message_t *out) {
  if (!buf || !out) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  memset(out, 0, sizeof(*out));
  out->kind = JESENRPC_MESSAGE_UNKNOWN;

  jesen_node_t *root = NULL;
  jesenrpc_err_t err = jrpc_parse_buffer_as_node(buf, buf_len, &root);
  if (err != JESENRPC_ERR_NONE) {
    return err;
  }

  jesenrpc_message_kind_t kind = JESENRPC_MESSAGE_UNKNOWN;
  err = jrpc_detect_message_kind_from_node(root, &kind);
  if (err != JESENRPC_ERR_NONE) {
    jesen_destroy(root);
    return err;
  }

  switch (kind) {
  case JESENRPC_MESSAGE_REQUEST_SINGLE:
    err = jrpc_parse_request_node(root, &out->as.request);
    break;
  case JESENRPC_MESSAGE_REQUEST_BATCH:
    err = jrpc_parse_request_batch_from_node(root, &out->as.request_batch);
    break;
  case JESENRPC_MESSAGE_RESPONSE_SINGLE:
    err = jrpc_parse_response_node(root, &out->as.response);
    break;
  case JESENRPC_MESSAGE_RESPONSE_BATCH:
    err = jrpc_parse_response_batch_from_node(root, &out->as.response_batch);
    break;
  default:
    err = JESENRPC_ERR_VALIDATION;
    break;
  }

  jesen_destroy(root);
  if (err != JESENRPC_ERR_NONE) {
    memset(out, 0, sizeof(*out));
    out->kind = JESENRPC_MESSAGE_UNKNOWN;
    return err;
  }

  out->kind = kind;
  return JESENRPC_ERR_NONE;
}

jesenrpc_err_t jesenrpc_message_destroy(jesenrpc_message_t *message) {
  if (!message) {
    return JESENRPC_ERR_INVALID_ARGS;
  }

  jesenrpc_err_t err = JESENRPC_ERR_NONE;
  switch (message->kind) {
  case JESENRPC_MESSAGE_REQUEST_SINGLE:
    err = jesenrpc_request_destroy(message->as.request);
    break;
  case JESENRPC_MESSAGE_REQUEST_BATCH:
    err = jesenrpc_request_batch_destroy(&message->as.request_batch);
    break;
  case JESENRPC_MESSAGE_RESPONSE_SINGLE:
    err = jesenrpc_response_destroy(message->as.response);
    break;
  case JESENRPC_MESSAGE_RESPONSE_BATCH:
    err = jesenrpc_response_batch_destroy(&message->as.response_batch);
    break;
  default:
    return JESENRPC_ERR_INVALID_ARGS;
  }

  if (err == JESENRPC_ERR_NONE) {
    memset(message, 0, sizeof(*message));
    message->kind = JESENRPC_MESSAGE_UNKNOWN;
  }

  return err;
}
