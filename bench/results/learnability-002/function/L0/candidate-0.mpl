project Function {
  tempo 120;
  meter 4/4;
  key C major;

  asset PianoAsset = instrument("GrandPiano");

  function cell(root: Pitch, velocity: Int) -> Fragment {
    fragment {
      note(root, 1/4, velocity);
      note(E4, 1/4, velocity);
    }
  }

  track Keys {
    role melodic;
    instrument PianoAsset;
  }

  section S {
    bars 1;
    track Keys {
      play cell(C4, 96);
    }
  }

  arrangement {
    play S;
  }
}

export midi("function.mid");