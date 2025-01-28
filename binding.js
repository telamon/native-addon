// module.exports = require.addon()
require.addon = require('require-addon')
module.exports = require.addon('.', __filename)
