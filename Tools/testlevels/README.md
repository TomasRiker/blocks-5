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


`bomb.xml`
----------
A wall of 29 bombs on one row with a `Fire` on the leftmost of them, so the
chain detonates on the first ticks and throws block debris across the whole
screen. It is there for judging the debris particles - their colour, how long
they last, and whether they fade out or brighten as they shrink.

Three things about it are deliberate. The bombs are *adjacent*: a bomb reaches
its neighbours only through the 3x3 loop in `Bomb::onUpdate`, so bombs two
cells apart do not chain. The `Fire` sits on the *same* cell as the first bomb,
because `onFire()` is what arms it and a fire beside it does nothing. And the
title begins `!!!` so the level sorts to the top of the single-levels list,
which is ordered by localized title - in a working tree that list also holds
the 42 campaign sources, and finding the right entry blind is otherwise fiddly.

What a recording of it cannot do is worth knowing before trying. The explosion
lands inside the crossfade from the menu, and under llvmpipe a rendered frame
costs a fifth of a second against a 20 ms logic tick, so the whole life of a
debris particle is a handful of frames. Two recordings cannot be compared frame
by frame either, even from the same seed: which tick a frame lands on depends
on load, so the same offset is a different moment in each run.
