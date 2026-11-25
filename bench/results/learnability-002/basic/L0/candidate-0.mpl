project Basic {
  tempo 120;
  meter 4/4;
  key C major;

  asset PianoAsset = instrument("GrandPiano");

  track Keys {
    role melodic;
    instrument PianoAsset;
    clip Notes {
      notes {
        C4 at 0 duration 1/4 velocity 96;
        D4 at 1/4 duration 1/4 velocity 96;
        E4 at 1/2 duration 1/4 velocity 96;
        F4 at 3/4 duration 1/4 velocity 96;
      }
    }
  }

  section S {
    bars 1;
    clip Keys;
  }

  arrangement {
    play S;
  }

  export midi("basic.mid");
}