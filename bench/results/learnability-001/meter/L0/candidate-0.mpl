project Meter {
  tempo 120;
  meter 7/8;
  key D minor;

  asset PianoAsset = instrument("GrandPiano");

  track Keys role melodic {
    instrument PianoAsset;
    section S {
      meter 7/8 grouping 2+2+3;
      D4 at 0 dur 1/8 velocity 104;
      E4 at 1/8 dur 1/8 velocity 88;
      F4 at 2/8 dur 1/8 velocity 88;
      G4 at 3/8 dur 1/8 velocity 96;
      A4 at 4/8 dur 1/8 velocity 88;
      Bb4 at 5/8 dur 1/8 velocity 88;
      C5 at 6/8 dur 1/8 velocity 92;
    }
  }

  arrangement {
    play Keys;
  }
}

export midi("meter.mid");