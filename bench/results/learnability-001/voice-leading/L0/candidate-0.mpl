project VoiceLeading {
  tempo 120;
  meter 4/4;
  key D minor;

  asset PianoAsset = instrument("GrandPiano");

  let v1 = voicing(chord("Dm9"), voices=4, range=D3..D5);
  let v2 = voiceLead(v1, chord("G7"), maxMove=7, maxSpacing=12, noCrossing=true);

  track Keys role harmony {
    instrument PianoAsset;
    section S {
      bars 2;
      play v1 rhythm [1/4 accent 2, 1/4, 1/4, 1/4];
      play v2 rhythm [1/4 accent 2, 1/4, 1/4, 1/4];
    }
  }

  arrangement {
    play S;
  }

  export midi("voice-leading.mid");
}