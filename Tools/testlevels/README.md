Test levels
===========

Levels that are not shipped. They belong to something being looked at right
now, and so they live here and not in `Blocks5/levels` - what lies there
belongs to the game: it is read out of the game folder, it shows up in the
player's level list, and `isShippedContent` makes it undeletable and
un-overwritable.

To use one, copy the file to `My Documents\Blocks 5\levels` (under Linux
`~/.local/share/blocks5/levels`); it then appears in the level select screen
under "Single levels".

`diamondmachine.xml`
--------------------
Five rows of six diamond machines, a block on each. The electricity is off and
Bob stands beside the switch - one step to the left, and all the machines start
at once. Each *column* is a different block type, which shows how the colours
of different blocks come out; the five rows show the same thing five times and
are there to see the effect side by side instead of once in the middle of the
screen.

One more machine sits down in the floor, at x = 6, and the block on it lies at
Bob's own height. That one is there for aborting: push it away in the middle of
the conversion, or switch the electricity off again. The block must then be
fully opaque again instantly - a half-transparent block sliding away would be
exactly the bug that matters here - and the sparks still in flight must turn
round instead of vanishing.

Bob reaches this machine in a good second. It must not be any further away:
the conversion takes two, and what is through cannot be aborted any more.

One thing about the switch: `Player::move` sets a lockout of 20 ticks after a
touch. Holding the key longer than 0.4 s therefore switches the electricity
back off.


`contamination.xml`
-------------------
Bob stands in the toxic gas, and a little further right lie three syringes in a
row. Stand still until the Geiger counter crackles and the screen turns green,
then run through to the right.

The contamination goes below zero doing that, and it is meant to: a player who
collects syringes in reserve holds out longer in the gas afterwards. Nothing
may crackle then - the player is not contaminated but better than clean. That
is exactly what the bug hung on: `gs_game.cpp` asked `if(c)` instead of
`if(c > 0)` and rolled `random(0, 2000 + c)`. From four syringes on, that span
is negative, and because `MTRand::randInt()` takes it unsigned, four billion
came out of it - the Geiger counter crackled on until the level was restarted,
measured twenty-nine times a second. `random(int, int)` answers an empty span
with `min` now instead of reading it unsigned - that is the real trap, and it
would have caught every other caller the same way.
