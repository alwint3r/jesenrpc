#include "../jesenrpc.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define EXPECT_OK(expr) assert((expr) == JESENRPC_ERR_NONE)

static void test_request_roundtrip_with_params(void) {
  jesen_node_t *params = NULL;
  EXPECT_OK(jesen_object_create(&params));
  EXPECT_OK(jesen_object_add_int32(params, "v", 5));

  jesenrpc_id_t id = {0};
  EXPECT_OK(jesenrpc_id_set_number(&id, 99));

  jesenrpc_request_t *req = NULL;
  EXPECT_OK(jesenrpc_request_create_with_id("sum", &id, &req));
  EXPECT_OK(jesenrpc_request_set_params(req, params));

  char buf[256];
  EXPECT_OK(jesenrpc_request_serialize(req, buf, sizeof buf));

  jesenrpc_request_t *parsed = NULL;
  EXPECT_OK(jesenrpc_request_parse(buf, strlen(buf), &parsed));
  assert(parsed->id.kind == JESENRPC_ID_NUMBER);
  assert(parsed->id.value.number == 99);
  assert(!jesenrpc_request_is_notification(parsed));
  int32_t val = 0;
  EXPECT_OK(jesen_object_get_int32(parsed->params, "v", &val));
  assert(val == 5);

  EXPECT_OK(jesenrpc_request_destroy(parsed));
  EXPECT_OK(jesenrpc_request_destroy(req));
}

static void test_notification_roundtrip(void) {
  jesenrpc_request_t *req = NULL;
  EXPECT_OK(jesenrpc_request_create("ping", &req));

  char buf[128];
  EXPECT_OK(jesenrpc_request_serialize(req, buf, sizeof buf));

  jesenrpc_request_t *parsed = NULL;
  EXPECT_OK(jesenrpc_request_parse(buf, strlen(buf), &parsed));
  assert(jesenrpc_request_is_notification(parsed));
  assert(parsed->id.kind == JESENRPC_ID_NONE);

  EXPECT_OK(jesenrpc_request_destroy(parsed));
  EXPECT_OK(jesenrpc_request_destroy(req));
}

static void test_response_result_roundtrip_string_id(void) {
  jesenrpc_id_t id = {0};
  const char *id_str = "abc123";
  EXPECT_OK(jesenrpc_id_set_string(&id, id_str, strlen(id_str)));

  jesenrpc_response_t *resp = NULL;
  EXPECT_OK(jesenrpc_response_create_with_id(&id, &resp));

  jesen_node_t *result = NULL;
  EXPECT_OK(jesen_object_create(&result));
  EXPECT_OK(jesen_object_add_bool(result, "ok", true));
  EXPECT_OK(jesenrpc_response_set_result(resp, result));

  char buf[256];
  EXPECT_OK(jesenrpc_response_serialize(resp, buf, sizeof buf));

  jesenrpc_response_t *parsed = NULL;
  EXPECT_OK(jesenrpc_response_parse(buf, strlen(buf), &parsed));
  assert(parsed->id.kind == JESENRPC_ID_STRING);
  assert(parsed->id.value.string.len == strlen(id_str));
  assert(strcmp(parsed->id.value.string.data, id_str) == 0);
  bool ok = false;
  EXPECT_OK(jesen_object_get_bool(parsed->result, "ok", &ok));
  assert(ok == true);

  EXPECT_OK(jesenrpc_response_destroy(parsed));
  EXPECT_OK(jesenrpc_response_destroy(resp));
}

static void test_response_error_roundtrip_with_data(void) {
  jesenrpc_id_t id = {0};
  EXPECT_OK(jesenrpc_id_set_number(&id, 7));

  jesenrpc_response_t *resp = NULL;
  EXPECT_OK(jesenrpc_response_create_with_id(&id, &resp));

  jesenrpc_error_object_t *err_obj = NULL;
  EXPECT_OK(jesenrpc_error_object_create(
      JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND, "not found", &err_obj));
  jesen_node_t *data = NULL;
  EXPECT_OK(jesen_object_create(&data));
  EXPECT_OK(jesen_object_add_string(data, "detail", "missing", 7));
  EXPECT_OK(jesenrpc_error_object_set_data(err_obj, data));
  EXPECT_OK(jesenrpc_response_set_error(resp, err_obj));

  char buf[256];
  EXPECT_OK(jesenrpc_response_serialize(resp, buf, sizeof buf));

  jesenrpc_response_t *parsed = NULL;
  EXPECT_OK(jesenrpc_response_parse(buf, strlen(buf), &parsed));
  assert(parsed->error != NULL);
  assert(parsed->error->code == JESENRPC_JSONRPC_ERROR_METHOD_NOT_FOUND);
  assert(strcmp(parsed->error->message, "not found") == 0);
  char detail[16];
  size_t len = 0;
  EXPECT_OK(jesen_object_get_string(parsed->error->data, "detail", detail,
                                    sizeof detail, &len));
  assert(len == 7);
  assert(strcmp(detail, "missing") == 0);

  EXPECT_OK(jesenrpc_response_destroy(parsed));
  EXPECT_OK(jesenrpc_response_destroy(resp));
}

static void test_request_batch_roundtrip(void) {
  jesenrpc_id_t id1 = {0};
  EXPECT_OK(jesenrpc_id_set_number(&id1, 1));
  jesenrpc_request_t *req1 = NULL;
  EXPECT_OK(jesenrpc_request_create_with_id("one", &id1, &req1));

  jesenrpc_request_t *req2 = NULL;
  EXPECT_OK(jesenrpc_request_create("notify", &req2));

  jesenrpc_request_t *reqs[2] = {req1, req2};
  char buf[512];
  EXPECT_OK(jesenrpc_request_batch_serialize((jesenrpc_request_t *const *)reqs,
                                             2, buf, sizeof buf));

  jesenrpc_request_batch_t parsed = {0};
  EXPECT_OK(jesenrpc_request_batch_parse(buf, strlen(buf), &parsed));
  assert(parsed.count == 2);
  assert(parsed.items[0]->id.kind == JESENRPC_ID_NUMBER);
  assert(parsed.items[0]->id.value.number == 1);
  assert(parsed.items[1]->id.kind == JESENRPC_ID_NONE);

  EXPECT_OK(jesenrpc_request_batch_destroy(&parsed));
  EXPECT_OK(jesenrpc_request_destroy(req1));
  EXPECT_OK(jesenrpc_request_destroy(req2));
}

static void test_response_batch_roundtrip(void) {
  jesenrpc_id_t id1 = {0};
  EXPECT_OK(jesenrpc_id_set_number(&id1, 10));
  jesenrpc_response_t *resp1 = NULL;
  EXPECT_OK(jesenrpc_response_create_with_id(&id1, &resp1));
  jesen_node_t *result = NULL;
  EXPECT_OK(jesen_object_create(&result));
  EXPECT_OK(jesen_object_add_int32(result, "value", 123));
  EXPECT_OK(jesenrpc_response_set_result(resp1, result));

  jesenrpc_id_t id2 = {0};
  EXPECT_OK(jesenrpc_id_set_string(&id2, "err", 3));
  jesenrpc_response_t *resp2 = NULL;
  EXPECT_OK(jesenrpc_response_create_with_id(&id2, &resp2));
  jesenrpc_error_object_t *err_obj = NULL;
  EXPECT_OK(jesenrpc_error_object_create(JESENRPC_JSONRPC_ERROR_INVALID_PARAMS,
                                         "bad params", &err_obj));
  EXPECT_OK(jesenrpc_response_set_error(resp2, err_obj));

  jesenrpc_response_t *resps[2] = {resp1, resp2};
  char buf[512];
  EXPECT_OK(jesenrpc_response_batch_serialize(
      (jesenrpc_response_t *const *)resps, 2, buf, sizeof buf));

  jesenrpc_response_batch_t parsed = {0};
  EXPECT_OK(jesenrpc_response_batch_parse(buf, strlen(buf), &parsed));
  assert(parsed.count == 2);
  int32_t out_val = 0;
  EXPECT_OK(jesen_object_get_int32(parsed.items[0]->result, "value", &out_val));
  assert(out_val == 123);
  assert(parsed.items[1]->error->code == JESENRPC_JSONRPC_ERROR_INVALID_PARAMS);

  EXPECT_OK(jesenrpc_response_batch_destroy(&parsed));
  EXPECT_OK(jesenrpc_response_destroy(resp1));
  EXPECT_OK(jesenrpc_response_destroy(resp2));
}

static void test_message_parse_single_request(void) {
  char buf[] = "{\"jsonrpc\":\"2.0\",\"id\":42,\"method\":\"echo\"}";
  jesenrpc_message_t msg;
  EXPECT_OK(jesenrpc_message_parse(buf, strlen(buf), &msg));
  assert(msg.kind == JESENRPC_MESSAGE_REQUEST_SINGLE);
  assert(msg.as.request != NULL);
  assert(msg.as.request->id.kind == JESENRPC_ID_NUMBER);
  assert(msg.as.request->id.value.number == 42);
  assert(strcmp(msg.as.request->method_name, "echo") == 0);
  EXPECT_OK(jesenrpc_message_destroy(&msg));
}

static void test_message_parse_single_response(void) {
  char buf[] = "{\"jsonrpc\":\"2.0\",\"id\":\"abc\",\"result\":true}";
  jesenrpc_message_t msg;
  EXPECT_OK(jesenrpc_message_parse(buf, strlen(buf), &msg));
  assert(msg.kind == JESENRPC_MESSAGE_RESPONSE_SINGLE);
  bool ok = false;
  EXPECT_OK(jesen_value_get_bool(msg.as.response->result, &ok));
  assert(ok == true);
  assert(msg.as.response->id.kind == JESENRPC_ID_STRING);
  assert(strcmp(msg.as.response->id.value.string.data, "abc") == 0);
  EXPECT_OK(jesenrpc_message_destroy(&msg));
}

static void test_message_parse_request_batch_and_peek(void) {
  jesenrpc_id_t id1 = {0};
  EXPECT_OK(jesenrpc_id_set_number(&id1, 1));
  jesenrpc_request_t *req1 = NULL;
  EXPECT_OK(jesenrpc_request_create_with_id("first", &id1, &req1));

  jesenrpc_request_t *req2 = NULL;
  EXPECT_OK(jesenrpc_request_create("second", &req2));

  jesenrpc_request_t *reqs[2] = {req1, req2};
  char buf[256];
  EXPECT_OK(jesenrpc_request_batch_serialize((jesenrpc_request_t *const *)reqs,
                                             2, buf, sizeof buf));

  char buf_copy[256];
  memcpy(buf_copy, buf, strlen(buf) + 1);
  jesenrpc_message_kind_t kind = JESENRPC_MESSAGE_UNKNOWN;
  EXPECT_OK(jesenrpc_message_peek_kind(buf_copy, strlen(buf_copy), &kind));
  assert(kind == JESENRPC_MESSAGE_REQUEST_BATCH);

  jesenrpc_message_t msg;
  EXPECT_OK(jesenrpc_message_parse(buf, strlen(buf), &msg));
  assert(msg.kind == JESENRPC_MESSAGE_REQUEST_BATCH);
  assert(msg.as.request_batch.count == 2);
  assert(msg.as.request_batch.items[0]->id.kind == JESENRPC_ID_NUMBER);
  assert(msg.as.request_batch.items[0]->id.value.number == 1);
  assert(jesenrpc_request_is_notification(msg.as.request_batch.items[1]));
  EXPECT_OK(jesenrpc_message_destroy(&msg));

  EXPECT_OK(jesenrpc_request_destroy(req1));
  EXPECT_OK(jesenrpc_request_destroy(req2));
}

static void test_message_parse_response_batch(void) {
  jesenrpc_response_t *resp1 = NULL;
  EXPECT_OK(jesenrpc_response_create(5, &resp1));
  jesen_node_t *result = NULL;
  EXPECT_OK(jesen_object_create(&result));
  EXPECT_OK(jesen_object_add_int32(result, "value", 10));
  EXPECT_OK(jesenrpc_response_set_result(resp1, result));

  jesenrpc_response_t *resp2 = NULL;
  EXPECT_OK(jesenrpc_response_create(6, &resp2));
  jesenrpc_error_object_t *err_obj = NULL;
  EXPECT_OK(jesenrpc_error_object_create(JESENRPC_JSONRPC_ERROR_INTERNAL,
                                         "oops", &err_obj));
  EXPECT_OK(jesenrpc_response_set_error(resp2, err_obj));

  jesenrpc_response_t *resps[2] = {resp1, resp2};
  char buf[256];
  EXPECT_OK(jesenrpc_response_batch_serialize(
      (jesenrpc_response_t *const *)resps, 2, buf, sizeof buf));

  jesenrpc_message_t msg;
  EXPECT_OK(jesenrpc_message_parse(buf, strlen(buf), &msg));
  assert(msg.kind == JESENRPC_MESSAGE_RESPONSE_BATCH);
  assert(msg.as.response_batch.count == 2);
  int32_t val = 0;
  EXPECT_OK(jesen_object_get_int32(msg.as.response_batch.items[0]->result,
                                   "value", &val));
  assert(val == 10);
  assert(msg.as.response_batch.items[1]->error->code ==
         JESENRPC_JSONRPC_ERROR_INTERNAL);
  EXPECT_OK(jesenrpc_message_destroy(&msg));

  EXPECT_OK(jesenrpc_response_destroy(resp1));
  EXPECT_OK(jesenrpc_response_destroy(resp2));
}

static void test_message_empty_batch_is_validation(void) {
  char buf[] = "[]";
  jesenrpc_message_t msg;
  jesenrpc_err_t err = jesenrpc_message_parse(buf, strlen(buf), &msg);
  assert(err == JESENRPC_ERR_VALIDATION);
}

int main(void) {
  test_request_roundtrip_with_params();
  test_notification_roundtrip();
  test_response_result_roundtrip_string_id();
  test_response_error_roundtrip_with_data();
  test_request_batch_roundtrip();
  test_response_batch_roundtrip();
  test_message_parse_single_request();
  test_message_parse_single_response();
  test_message_parse_request_batch_and_peek();
  test_message_parse_response_batch();
  test_message_empty_batch_is_validation();
  printf("All jesenrpc tests passed\n");
  return 0;
}
