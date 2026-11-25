// MPL standard library: derived pitch/interval analysis.
// All functions are ordinary MPL; none require a model or native helper.
module StdAnalysis;

/// Pitch classes of a non-empty pitch list, in input order.
/// Complexity: O(n), result O(n).
export fn pitchClasses(ps: [Pitch]) -> [Int] {
  return for p in ps { pitchClass(p) };
}

/// Adjacent intervals of a pitch list. Requires length >= 2.
/// Complexity: O(n), result O(n-1).
export fn intervalsOf(ps: [Pitch]) -> [Int] {
  return for i in range(0, length(ps) - 1) {
    interval(ps[i], ps[i + 1])
  };
}

/// Sounding span of a non-empty pitch list.
/// Complexity: O(n) for the builtin range scan.
export fn spanOf(ps: [Pitch]) -> Int {
  return pitchRange(ps);
}
