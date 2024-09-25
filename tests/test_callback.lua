local ltest = require('./libtest')
local porch = require('porch')

local cat = assert(porch.spawn("cat"))
cat.timeout = 3

assert(cat:write("Hello, world\r"))

local invoked
cat:match({ Hello = { callback = function()
	invoked = true
end }})

assert(invoked, "Callback was not invoked")

assert(cat:close())
