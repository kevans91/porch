# What do we want?

An application that allows us to script some amounts of tty orchestration.
The most basic functionality we need is the ability to read some input from the
target application, and to be able to write back to the application.

# Language

Lua

# Script Structure

At its most basic level, orch(1) scripts are merely a set of match blocks that
have some configuration, including an optional callback.

```
-- match() returns a closure of orchlua_matchcfg
match "pattern" {
	<config>
}

match "pattern-two"

-- one() will be enqueued; when the queue gets to it,
-- it will execute the callback in a new protected
-- context and gather up the match blocks inside.
--
-- In a `one` context, we'll only ever take the first match
-- we can satisfy.  The rest are discarded.
one(function()
	match "foo"
	match "bar"
end)
```
## Execution

All match blocks will be satisfied in the order they are created
in.  Callbacks are invoked, if available, in which the script can setup
more match blocks or write to the `target`.  Callbacks are largely
aesthetic except in a one() context.  `target` is not available in
the one() context, except in a match context.
