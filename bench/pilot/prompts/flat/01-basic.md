Return ONLY Flat Event JSON. Schema:
{"format":"mpl.flat-events","version":1,"logicalExtent":"<rational>","tempoBpm":<number>,"meterMap":[{"position":"0","numerator":<int>,"denominator":<int>,"grouping":"<text>"}],"tracks":[{"id":"<track>","events":[{"pitch":<0-127>,"onset":"<rational>","duration":"<rational>","velocity":1-127}]}]}

Task: one track `Piano`, logical extent 1, 120 bpm, 4/4, grouping 2+2. Exactly four quarter-note events at onsets 0, 1/4, 1/2, 3/4 with pitches C4, D4, E4, F4 (MIDI 60,62,64,65), velocity 96.

No prose, no markdown.
