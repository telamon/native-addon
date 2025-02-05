const binding = require('./binding')

// exports = binding (require() is crap)
for (const key in binding) exports[key] = binding[key]
