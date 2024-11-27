local ltest = require('./libtest')
local porch = require('porch')
local rval

rval = assert(porch.run_script("test_include_file.script"))
assert(rval == 9, "Include-by-name failed to exit(9)")

local fh = assert(io.open("test_include_file.script"))
rval = assert(porch.run_script(fh))
assert(fh:close())
assert(rval == 9, "Include-stream failed to exit(9)")

local line
fh = assert(io.open("test_include_file.script"))

-- Now skip two lines, then include the file.  The first line is a comment, the
-- second line is an exit().
line = assert(fh:read("l"))
assert(line:match("^--"))
line = assert(fh:read("l"))
assert(line:match("^exit"))

rval = assert(porch.run_script(fh))
assert(fh:close())
assert(rval == 18, "Include-seeked-stream failed to exit(18)")
