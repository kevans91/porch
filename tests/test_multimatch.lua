local ltest = require('./libtest')
local porch = require('porch')

local cat = assert(porch.spawn("cat"))
cat.timeout = 3

assert(cat:write("PANIC: foo\r"))
assert(cat:write("login: \r"))

local panic_match, login_match

cat:match({
	["login:"] = {
		callback = function()
			login_match = true
		end,
	},
	["PANIC:.+"] = {
		callback = function()
			panic_match = true
		end,
	},
})

assert(panic_match, "Panic should have been found first")
assert(not login_match, "No matches should have happened after panic")

assert(cat:close())
