project Theme {
  tempo 120;
  meter 4/4;
  key C major;

  asset PianoAsset = instrument("GrandPiano");

  track Keys role melodic {
    clip ThemeCell {
      notes {
        C4 quarter at 0 velocity 92;
        D4 quarter at 1/4 velocity 88;
        E4 quarter at 1/2 velocity 84;
        E4 quarter at 0 velocity 92;
        F4 quarter at 1/4 velocity 88;
        G4 quarter at 1/2 velocity 84;
      }
    }
  }

  section S bars 2 {
    clip Keys.ThemeCell;
  }

  arrangement {
    play S;
  }

  export midi("theme.mid");
}