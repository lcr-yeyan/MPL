project Function {
  tempo 120;
  meter 4/4;
  key C major;

  asset PianoAsset = instrument("GrandPiano");

  function cell(root: Pitch, velocity: Int) -> Fragment {
    return fragment {
      note(root, 1/4, velocity);
      note(E4, 1/4, velocity);
    };
  }

  let myCell = cell(C4, 96);

  track Keys role melodic {
    instrument PianoAsset;
    section S {
      play myCell;
    }
  }

  arrangement {
    play S;
  }

  export midi("function.mid");
}