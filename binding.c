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
  err = js_call_function_with_checkpoint(env, ctx, callback, out_argc, out_argv, &cb_result);

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

  js_value_t *action_arraybuffer;
  action_t *action;
  err = js_create_arraybuffer(env, sizeof(action_t), (void **) &action, &action_arraybuffer);
  assert(err == 0);

  action->env = env;

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

static js_value_t *
bare_addon_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("do_callback", addon_do_callback)
  V("do_async_callback", addon_do_async_callback)
#undef V

  return exports;
}

BARE_MODULE(bare_addon, bare_addon_exports)
