#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jesen.h"

#define JESENRPC_METHOD_NAME_MAX_LEN 256
#define JESENRPC_JSONRPC_VERSION "2.0"
#define JESENRPC_JSONRPC_VERSION_LEN 3

typedef int32_t jesenrpc_err_t;

#define JESENRPC_ERR_NONE 0
#define JESENRPC_ERR_BASE JESEN_ERR_BASE + 0xFF
#define JESENRPC_ERR_INVALID_ARGS (JESENRPC_ERR_BASE + 1)
#define JESENRPC_ERR_VALIDATION (JESENRPC_ERR_BASE + 2)
#define JESENRPC_ERR_ALLOC (JESENRPC_ERR_BASE + 3)

#define JESENRPC_JSONRPC_ERROR_PARSE -32700
#define JESENRPC_JSONRPC_ERROR_INVALID_REQUEST -32600
#define JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND -32601
#define JESENRPC_JSONRPC_ERROR_INVALID_PARAMS -32602
#define JESENRPC_JSONRPC_ERROR_INTERNAL -32603
#define JESENRPC_JSONRPC_ERROR_SERVER_MIN -32099
#define JESENRPC_JSONRPC_ERROR_SERVER_MAX -32000

typedef enum jesenrpc_id_kind {
  JESENRPC_ID_NONE = 0, /* notification (id absent) */
  JESENRPC_ID_NUMBER,
  JESENRPC_ID_STRING,
  JESENRPC_ID_NULL
} jesenrpc_id_kind_t;

typedef struct jesenrpc_id {
  jesenrpc_id_kind_t kind;
  union {
    int64_t number;
    struct {
      char *data;
      size_t len;
    } string;
  } value;
} jesenrpc_id_t;

typedef struct jesenrpc_error_object {
  int32_t code;
  char *message;
  jesen_node_t *data;
} jesenrpc_error_object_t;

typedef struct jesenrpc_request {
  const char *jsonrpc; /* always "2.0" */
  jesenrpc_id_t id;
  char *method_name;
  jesen_node_t *params;
} jesenrpc_request_t;

typedef struct jesenrpc_response {
  const char *jsonrpc; /* always "2.0" */
  jesenrpc_id_t id;
  jesen_node_t *result;           /* present when error is NULL */
  jesenrpc_error_object_t *error; /* present when result is NULL */
} jesenrpc_response_t;

typedef struct jesenrpc_request_batch {
  jesenrpc_request_t **items;
  size_t count;
} jesenrpc_request_batch_t;

typedef struct jesenrpc_response_batch {
  jesenrpc_response_t **items;
  size_t count;
} jesenrpc_response_batch_t;

/* id helpers */
jesenrpc_err_t jesenrpc_id_set_number(jesenrpc_id_t *id, int64_t value);
jesenrpc_err_t jesenrpc_id_set_string(jesenrpc_id_t *id, const char *value,
                                      size_t value_len);
jesenrpc_err_t jesenrpc_id_set_null(jesenrpc_id_t *id);
jesenrpc_err_t jesenrpc_id_set_notification(jesenrpc_id_t *id);
jesenrpc_err_t jesenrpc_id_destroy(jesenrpc_id_t *id);

jesenrpc_err_t jesenrpc_request_create(const char *method_name,
                                       jesenrpc_request_t **request);
jesenrpc_err_t jesenrpc_request_create_with_id(const char *method_name,
                                               const jesenrpc_id_t *id,
                                               jesenrpc_request_t **request);
jesenrpc_err_t jesenrpc_request_set_id(jesenrpc_request_t *request,
                                       const jesenrpc_id_t *id);
jesenrpc_err_t jesenrpc_request_set_params(jesenrpc_request_t *request,
                                           jesen_node_t *params);
bool jesenrpc_request_is_notification(const jesenrpc_request_t *request);

jesenrpc_err_t jesenrpc_request_serialize(const jesenrpc_request_t *request,
                                          char *out_buf, size_t out_buf_len);
jesenrpc_err_t jesenrpc_request_validate(const jesenrpc_request_t *request);

jesenrpc_err_t jesenrpc_request_parse(char *buf, size_t buf_len,
                                      jesenrpc_request_t **out);
jesenrpc_err_t jesenrpc_request_destroy(jesenrpc_request_t *request);

jesenrpc_err_t
jesenrpc_response_create_for_request(const jesenrpc_request_t *request,
                                     jesenrpc_response_t **response);
jesenrpc_err_t jesenrpc_response_create_with_id(const jesenrpc_id_t *id,
                                                jesenrpc_response_t **response);
jesenrpc_err_t jesenrpc_response_create(int32_t request_id,
                                        jesenrpc_response_t **response);
jesenrpc_err_t jesenrpc_response_set_result(jesenrpc_response_t *response,
                                            jesen_node_t *result);
jesenrpc_err_t jesenrpc_response_set_error(jesenrpc_response_t *response,
                                           jesenrpc_error_object_t *error);

jesenrpc_err_t jesenrpc_response_serialize(const jesenrpc_response_t *response,
                                           char *out_buf, size_t out_buf_len);
jesenrpc_err_t jesenrpc_response_validate(const jesenrpc_response_t *response);
jesenrpc_err_t jesenrpc_response_parse(char *buf, size_t buf_len,
                                       jesenrpc_response_t **out);
jesenrpc_err_t jesenrpc_response_destroy(jesenrpc_response_t *response);

jesenrpc_err_t jesenrpc_error_object_create(int32_t code, const char *message,
                                            jesenrpc_error_object_t **error);
jesenrpc_err_t jesenrpc_error_object_set_data(jesenrpc_error_object_t *error,
                                              jesen_node_t *data);
jesenrpc_err_t
jesenrpc_error_object_validate(const jesenrpc_error_object_t *error);
jesenrpc_err_t jesenrpc_error_object_destroy(jesenrpc_error_object_t *error);

/* batch helpers */
jesenrpc_err_t
jesenrpc_request_batch_serialize(jesenrpc_request_t *const *requests,
                                 size_t request_count, char *out_buf,
                                 size_t out_buf_len);
jesenrpc_err_t
jesenrpc_response_batch_serialize(jesenrpc_response_t *const *responses,
                                  size_t response_count, char *out_buf,
                                  size_t out_buf_len);
jesenrpc_err_t jesenrpc_request_batch_parse(char *buf, size_t buf_len,
                                            jesenrpc_request_batch_t *out);
jesenrpc_err_t jesenrpc_response_batch_parse(char *buf, size_t buf_len,
                                             jesenrpc_response_batch_t *out);
jesenrpc_err_t jesenrpc_request_batch_destroy(jesenrpc_request_batch_t *batch);
jesenrpc_err_t
jesenrpc_response_batch_destroy(jesenrpc_response_batch_t *batch);
