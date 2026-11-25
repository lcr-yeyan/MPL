project VoiceLeading {
  tempo 120;
  meter 4/4;
  key D minor;

  asset PianoAsset = instrument("GrandPiano");

  track Keys role harmony {
    clip Voicings {
      chord Dm9 = chord("Dm9");
      chord G7 = chord("G7");

      voicing v1 = voicing(Dm9, voices=4, range=D3..D5);
      voicing v2 = voiceLead(v1, G7, maxMove=7, maxSpacing=12, noCrossing=true);

      section S {
        bar 1 {
          play v1 rhythm [1/4 accent 2, 1/4, 1/4, 1/4];
        }
        bar 2 {
          play v2 rhythm [1/4 accent 2, 1/4, 1/4, 1/4];
        }
      }
    }
  }

  arrangement {
    play S;
  }

  export midi("voice-leading.mid");
}