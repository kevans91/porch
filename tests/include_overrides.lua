--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local function life()
	return 42
end

return {
	expansion = 42,
	life = life,
}
