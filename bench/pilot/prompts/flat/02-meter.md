Return ONLY Flat Event JSON. Schema:
{"format":"mpl.flat-events","version":1,"logicalExtent":"<rational>","tempoBpm":<number>,"meterMap":[{"position":"0","numerator":<int>,"denominator":<int>,"grouping":"<text>"}],"tracks":[{"id":"<track>","events":[{"pitch":<0-127>,"onset":"<rational>","duration":"<rational>","velocity":1-127}]}]}

Task: one track `Piano`, logical extent 7/8, 120 bpm, 7/8, grouping 2+2+3. Exactly seven eighth-note events at onsets 0,1/8,2/8,3/8,4/8,5/8,6/8 with pitches D4,E4,F4,G4,A4,Bb4,C5 (MIDI 62,64,65,67,69,70,72), velocities 104,88,88,96,88,88,92.

No prose, no markdown.
