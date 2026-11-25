# Flat Event Output Protocol

Return exactly one JSON object, no markdown, with this schema:
{"format":"mpl.flat-events","version":1,"logicalExtent":"<rational>","tempoBpm":<number>,"meterMap":[{"position":"0","numerator":<int>,"denominator":<int>,"grouping":"<text>"}],"tracks":[{"id":"<track>","events":[{"pitch":<0-127>,"onset":"<rational>","duration":"<rational>","velocity":1-127}]}]}
Times are exact rational whole-note strings, never floating point. There are no functions, modules or harmony provenance fields.