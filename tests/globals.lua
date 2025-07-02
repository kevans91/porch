--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local global_env = {
	pairs = pairs,
}

local function process_dblist(idtxt)
	local db = {}
	local str = ""

	for k = 1, #idtxt do
		local chr = idtxt:byte(k)

		if chr ~= 0 then
			str = str .. idtxt:sub(k, k)
		else
			local name, id = str:match("^(.+)=([^=]+)$")

			if not name then
				assert(nil, "malformed printid line: " .. str)
			end

			db[tonumber(id)] = name
			str = ""
		end
	end

	return db
end

local function invoke_printid(argstr)
	local prog = (global_env.printid or "printid") .. " " .. argstr
	local fh = io.popen(prog)
	local output = fh:read("a")

	fh:close()
	return output

end

function global_env.system_groups()
	local output = invoke_printid("-ga")

	return process_dblist(output)
end
function global_env.system_unmapped_groups(limit)
	assert(limit > 0, "Must limit number of unmapped groups")
	local output = invoke_printid("-gv -c " .. limit)
	local tbl = {}

	for id in output:gmatch("[^%s]+") do
		tbl[#tbl + 1] = tonumber(id)
	end

	return tbl
end
function global_env.system_unmapped_users(limit)
	assert(limit > 0, "Must limit number of unmapped users")
	local output = invoke_printid("-uv -c " .. limit)
	local tbl = {}

	for id in output:gmatch("[^%s]+") do
		tbl[#tbl + 1] = tonumber(id)
	end

	return tbl
end
function global_env.system_users()
	local output = invoke_printid("-ua")

	return process_dblist(output)
end

return global_env
