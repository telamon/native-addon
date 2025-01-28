const test = require('brittle')
const addon = require('.')

test('inner call', t => {
  const err = addon.do_callback(val => {
    t.is(val, 'Hello addon', 'callback argument')
  })
  t.is(err, 0, 'return code ok')
})

test('outer call', async t => {
  const val = await new Promise(resolve => {
    const handle = addon.do_async_callback(resolve)
    t.ok(handle instanceof ArrayBuffer, 'Arraybuffer returned')
  })
  t.is(val, 'Hello i/o addon', 'callback argument')
})

test('uncaught exception on inner call', t => {
  t.plan(1)
  try {
    addon.do_callback(() => { throw new Error('boom 0') })
  } catch(err) {
    // exceptions in sync calls propagate to callee
    t.is(err.message, 'boom 0')
  }
})

test('uncaught exception on outer call', async t => {
  const expected_status = -2
  addon.do_async_callback(() => {
    console.log('JS: throwing error')
    throw new Error('boom 1')
  }, expected_status)

  const err = await new Promise(uncaught)
  t.is(err.message, 'boom 1', 'exception caught')
})

test('proc still running', async t => {
  await new Promise(resolve => setTimeout(resolve, 1000))
  t.ok('process not killed')
})

test('status does not leak', async t => {
  const expected_status = 0
  await new Promise(resolve => addon.do_async_callback(resolve, expected_status))

  // this context runs on the checkpoint of native-call above

  queueMicrotask(() => { throw new Error('boom 2') })
  // process.nextTick(() => { throw new Error('boom 2') }) // leaks -1 status into native call :arrow_up:

  const err = await new Promise(uncaught)
  t.is(err.message, 'boom 2', 'ex caught')
})

function uncaught (fn) {
  if (global.Bare) {
    global.Bare.once('uncaughtException', fn)
  } else {
    process.once('uncaughtException', fn)
  }
}
