project Basic {
  tempo 120
  meter 4/4
  key C major

  asset PianoAsset = instrument("GrandPiano");

  track Keys role melodic {
    instrument PianoAsset;
    clip M {
      notes {
        C4 at 0 velocity 96 duration 1/4;
        D4 at 1/4 velocity 96 duration 1/4;
        E4 at 1/2 velocity 96 duration 1/4;
        F4 at 3/4 velocity 96 duration 1/4;
      }
    }
  }

  section S(1 bars) {
    clip Keys playing M;
  }

  arrangement {
    play S;
  }

  export midi("basic.mid");
}