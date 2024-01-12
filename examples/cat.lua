-- Executed like `cat cat.lua | ./orch cat`

-- We get released on first match anyways
-- release()

write "Send One\r"
one(function()
	match "One" {
		callback = function()
			write "LOL\r"

			debug "Matched one"
			match "LOL" {
				callback = function()
					debug "lol"
					write "Foo\r"
				end
			}
			-- Also valid:
			-- write "Foo"
			match "Foo" {
				callback = function()
					debug "foo matched too"
				end
			}
		end
	}
	match "Two" {
		callback = function()
		--	debug "Called two"
		end
	}
end)
