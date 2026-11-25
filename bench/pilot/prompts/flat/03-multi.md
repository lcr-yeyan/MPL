Return ONLY Flat Event JSON. Schema:
{"format":"mpl.flat-events","version":1,"logicalExtent":"<rational>","tempoBpm":<number>,"meterMap":[{"position":"0","numerator":<int>,"denominator":<int>,"grouping":"<text>"}],"tracks":[{"id":"<track>","events":[{"pitch":<0-127>,"onset":"<rational>","duration":"<rational>","velocity":1-127}]}]}

Task: one track `Piano`, logical extent 2, 120 bpm, 4/4. Twelve eighth-note events at onsets 0,1/8,...,11/8 (12 events), pitches cycling C4,E4,G4 starting C4 (60,64,67,60,64,67,...), duration 1/8, velocity 92. Note events beyond the 2-whole logical extent are not allowed.

No prose, no markdown.
