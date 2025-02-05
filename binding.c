#include <assert.h>
#include <bare.h>
#include <js.h>
#include <utf.h>

static js_value_t *
addon_do_callback(js_env_t *env, js_callback_info_t *info) {
  const char *TAG = "[native:inner]";
  int err;

  size_t argc = 1;
  js_value_t *argv[argc];
  js_value_t *ctx;

  err = js_get_callback_info(env, info, &argc, argv, &ctx, NULL);
  assert(err == 0);
  assert(argc == 1);

  bool is_function;
  err = js_is_function(env, argv[0], &is_function);
  assert(err == 0);
  assert(is_function);

  size_t out_argc = 1;
  js_value_t *out_argv[out_argc];

  err = js_create_string_utf8(env, (utf8_t *) "Hello addon", -1, &out_argv[0]);
  assert(err == 0);

  printf("%s call function\n", TAG);

  js_value_t *cb_result;
  err = js_call_function(env, ctx, argv[0], out_argc, out_argv, &cb_result);

  printf("%s callback returned, err=%i\n", TAG, err);

  if (err == 0) {
    // cb_result is safe to use
    // but no-op here
  } else {
    // assert error sanity
    bool is_pending;
    err = js_is_exception_pending(env, &is_pending);
    assert(err == 0);
    printf("%s js_is_exception_pending=%i\n", TAG, is_pending);
  }

  js_value_t *result;
  err = js_create_int32(env, 0, &result);
  assert(err == 0);

  return result;
}

typedef struct action_s {
  js_env_t *env;
  js_ref_t *ctx_ref;
  js_ref_t *callback_ref;
  uv_async_t handle;
  int32_t expected_status;
} action_t;

static void
on_uv_close (uv_handle_t *handle) {
  printf("[info] handle_closed\n");
}

static void
on_uv_async (uv_async_t *handle) {
  const char *TAG = "[native:outer]";
  int err;
  action_t *action = handle->data;

  js_env_t *env = action->env;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  js_value_t *callback;
  err = js_get_reference_value(env, action->callback_ref, &callback);
  assert(err == 0);

  js_value_t *ctx;
  err = js_get_reference_value(env, action->ctx_ref, &ctx);
  assert(err == 0);

  size_t out_argc = 1;
  js_value_t *out_argv[out_argc];

  err = js_create_string_utf8(env, (utf8_t *) "Hello i/o addon", -1, &out_argv[0]);
  assert(err == 0);

  printf("%s call function w/ checkpoint\n", TAG);
  js_value_t *cb_result;
  int status = js_call_function_with_checkpoint(env, ctx, callback, out_argc, out_argv, &cb_result);

  printf("%s callback returned, err=%i expected=%i\n", TAG, status, action->expected_status);

  if (status == 0) {
    // cb_result is safe to use
    // but no-op here
  } else {
    // assert error sanity
    bool is_pending;
    err = js_is_exception_pending(env, &is_pending);
    assert(err == 0);
    printf("%s js_is_exception_pending=%i\n", TAG, is_pending);
  }
  assert(status == action->expected_status);

  // finally cleanup
  uv_close((uv_handle_t *) &action->handle, on_uv_close);

  err = js_delete_reference(env, action->ctx_ref);
  assert(err == 0);

  err = js_delete_reference(env, action->callback_ref);
  assert(err == 0);

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  printf("%s refs destroyed / native exit\n", TAG);
}

static js_value_t *
addon_do_async_callback(js_env_t *env, js_callback_info_t *info) {
  const char *TAG = "[native:inner_pre_io]";
  int err;

  size_t argc = 2;
  js_value_t *argv[argc];
  js_value_t *ctx;

  err = js_get_callback_info(env, info, &argc, argv, &ctx, NULL);
  assert(err == 0);
  assert(argc > 0);

  bool is_function;
  err = js_is_function(env, argv[0], &is_function);
  assert(err == 0);
  assert(is_function);

  js_value_t *action_arraybuffer;
  action_t *action;
  err = js_create_arraybuffer(env, sizeof(action_t), (void **) &action, &action_arraybuffer);
  assert(err == 0);

  action->env = env;

  action->expected_status = 0; // expect default status_ok
  if (argc > 1) { // status provided
    bool arg_is_number;
    err = js_is_number(env, argv[1], &arg_is_number);
    assert(err == 0);
    assert(arg_is_number);

    err = js_get_value_int32(env, argv[1], &action->expected_status);
    assert(err == 0);
  }

  err = js_create_reference(env, argv[0], 1, &action->callback_ref);
  assert(err == 0);

  err = js_create_reference(env, ctx, 1, &action->ctx_ref);
  assert(err == 0);

  uv_loop_t *loop;
  err = js_get_env_loop(env, &loop);
  assert(err == 0);

  err = uv_async_init(loop, &action->handle, on_uv_async);
  assert(err == 0);

  action->handle.data = action;

  printf("%s all rigged, triggering async_send\n", TAG);
  err = uv_async_send(&action->handle);
  assert(err == 0);

  return action_arraybuffer;
}

static uint32_t calls_untyped = 0;
static uint32_t calls_scoped = 0;
static uint32_t calls_view = 0;

static js_value_t *
typedarray_incr (js_env_t *env, js_callback_info_t *info) {
  calls_untyped++;
  int err;
  size_t argc = 1;
  js_value_t *argv[argc];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);
  assert(argc == 1);

  size_t len = 1024;
  uint8_t *data;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &data, &len, NULL, NULL);
  assert(err == 0);
  assert(len == 1024);

  data[0]++; // do nothing controversial

  js_value_t *res;
  err = js_create_uint32(env, data[0], &res);
  assert(err == 0);
  return res;
}

static uint32_t
typedarray_incr_typed_scoped (js_value_t *receiver, js_value_t *buffer, js_typed_callback_info_t *info) {
  calls_scoped++;
  int err;
  js_env_t *env = NULL;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  js_handle_scope_t *scope;
  err = js_open_handle_scope(env, &scope);
  assert(err == 0);

  size_t len = 1024;
  uint8_t *data;
  err = js_get_typedarray_info(env, buffer, NULL, (void **) &data, &len, NULL, NULL);
  assert(err == 0);
  assert(len == 1024);

  data[0]++; // do nothing controversial

  err = js_close_handle_scope(env, scope);
  assert(err == 0);

  return data[0];
}

static uint32_t
typedarray_incr_typed_view (js_value_t *receiver, js_value_t *buffer, js_typed_callback_info_t *info) {
  calls_view++;
  int err;
  js_env_t *env = NULL;
  err = js_get_typed_callback_info(info, &env, NULL);
  assert(err == 0);

  size_t len = 1024;
  uint8_t *data;
  js_typedarray_view_t *view;
  err = js_get_typedarray_view(env, buffer, NULL, (void **) &data, &len, &view);
  assert(err == 0);
  assert(len == 1024);

  data[0]++; // do nothing controversial

  return data[0];
}

static js_value_t *
typedarray_call_counters (js_env_t *env, js_callback_info_t *info) {
  int err;
  js_value_t *res;
  err = js_create_object(env, &res);
  assert(err == 0);
#define V(name, value) \
  { \
    js_value_t *val; \
    err = js_create_uint32(env, value, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, res, name, val); \
    assert(err == 0); \
  }

  V("untyped", calls_untyped)
  V("scoped", calls_scoped)
  V("view", calls_view)
#undef V
  return res;
}

static js_value_t *
bare_addon_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, untyped, signature, typed) \
  { \
    js_value_t *val; \
    if (signature) { \
      err = js_create_typed_function(env, name, -1, untyped, signature, typed, NULL, &val); \
      assert(err == 0); \
    } else { \
      err = js_create_function(env, name, -1, untyped, NULL, &val); \
      assert(err == 0); \
    } \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("do_callback", addon_do_callback, NULL, NULL)
  V("do_async_callback", addon_do_async_callback, NULL, NULL)

  V("typedarray_incr_scoped",
    typedarray_incr,
    &((js_callback_signature_t) {
      .version = 0,
      .args_len = 2,
      .args = (int[]) { js_object, js_object },
      .result = js_uint32
    }),
    typedarray_incr_typed_scoped
  )

  V("typedarray_incr_view",
    typedarray_incr,
    &((js_callback_signature_t) {
      .version = 0,
      .args_len = 2,
      .args = (int[]) { js_object, js_object },
      .result = js_uint32
    }),
    typedarray_incr_typed_view
  )

  V("typedarray_incr_untyped", typedarray_incr, NULL, NULL)

  V("typedarray_call_counters", typedarray_call_counters, NULL, NULL)
#undef V

  return exports;
}

BARE_MODULE(bare_addon, bare_addon_exports)
